/*
 * Copyright (C) 2021-2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "medialibrary_asset_operations.h"
#define MLOG_TAG "Distributed"
#include "medialibrary_sync_operation.h"
#include "datashare_helper.h"
#include "device_manager.h"
#include "media_column.h"
#include "media_log.h"
#include "medialibrary_async_worker.h"
#include "medialibrary_errno.h"
#include "medialibrary_tracer.h"
#include "result_set_utils.h"

namespace OHOS {
namespace Media {
using namespace std;
using namespace OHOS::AppExecFwk;
using namespace OHOS::DistributedKv;

namespace {
static constexpr int RETRY_COUNT = 3;
static constexpr int32_t WAIT_FOR_MS = 1000;
static constexpr int32_t ALBUM_THUMBNAIL_MAX_COUNT = 50;
static vector<string> table_arr = {
    MEDIALIBRARY_TABLE, PhotoColumn::PHOTOS_TABLE, AudioColumn::AUDIOS_TABLE,
    SMARTALBUM_TABLE, SMARTALBUM_MAP_TABLE, CATEGORY_SMARTALBUM_MAP_TABLE };

class DistributedAsyncTaskData : public AsyncTaskData {
public:
    DistributedAsyncTaskData() = default;
    virtual ~DistributedAsyncTaskData() = default;
    MediaLibrarySyncOpts syncOpts_;
    vector<string> networkIds_;
    string sqlStatement_;
};
}

void MediaLibrarySyncCallback::SyncCompleted(const map<string, Status> &results)
{
    for (auto &item : results) {
        if (item.second == Status::SUCCESS) {
            unique_lock<mutex> lock(status_.mtx_);
            status_.isSyncComplete_ = true;
            break;
        }
    }
    status_.cond_.notify_one();
}

bool MediaLibrarySyncCallback::WaitFor()
{
    unique_lock<mutex> lock(status_.mtx_);
    bool ret =
        status_.cond_.wait_for(lock, chrono::milliseconds(WAIT_FOR_MS), [this]() { return status_.isSyncComplete_; });
    return ret;
}

static void SyncPullTableByNetworkId(AsyncTaskData* data)
{
    DistributedAsyncTaskData* taskData = static_cast<DistributedAsyncTaskData*>(data);
    MediaLibrarySyncOperation::SyncPullTable(taskData->syncOpts_, taskData->networkIds_);
}

bool MediaLibrarySyncOperation::SyncPullAllTableByNetworkId(MediaLibrarySyncOpts &syncOpts, vector<string> &devices)
{
    if (syncOpts.rdbStore == nullptr) {
        MEDIA_ERR_LOG("MediaLibrarySyncOperation SyncPullAllTable rdbStore is null");
        return false;
    }

    for (auto &table_name : table_arr) {
        shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
        if (asyncWorker == nullptr) {
            continue;
        }
        DistributedAsyncTaskData *taskData = new (nothrow) DistributedAsyncTaskData();
        if (taskData == nullptr) {
            continue;
        }
        syncOpts.table = table_name;
        taskData->syncOpts_ = syncOpts;
        taskData->networkIds_ = devices;
        auto distributedAsyncTask = make_shared<MediaLibraryAsyncTask>(SyncPullTableByNetworkId, taskData);
        asyncWorker->AddTask(distributedAsyncTask, false);
    }
    return true;
}

static string GetDeviceUdidByNetworkId(const shared_ptr<RdbStore> &rdbStore, const string &networkId)
{
    vector<string> columns;
    AbsRdbPredicates absPredDevice(DEVICE_TABLE);
    absPredDevice.EqualTo(DEVICE_DB_NETWORK_ID, networkId);
    auto queryResultSet = rdbStore->QueryByStep(absPredDevice, columns);
    auto count = 0;
    auto ret = queryResultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        return "";
    }

    if (count <= 0) {
        return "";
    }

    ret = queryResultSet->GoToFirstRow();
    if (ret != NativeRdb::E_OK) {
        return "";
    }

    return get<string>(ResultSetUtils::GetValFromColumn(DEVICE_DB_UDID, queryResultSet, TYPE_STRING));
}

static int32_t UpdateDeviceSyncStatus(const shared_ptr<RdbStore> &rdbStore, const string &networkId, int32_t syncStatus)
{
    string deviceUdid = GetDeviceUdidByNetworkId(rdbStore, networkId);
    if (deviceUdid.empty()) {
        return E_FAIL;
    }

    vector<string> columns;
    AbsRdbPredicates absPredDevice(DEVICE_TABLE);
    absPredDevice.EqualTo(DEVICE_DB_UDID, deviceUdid);
    auto queryResultSet = rdbStore->QueryByStep(absPredDevice, columns);

    auto count = 0;
    int32_t ret = queryResultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        return ret;
    }
    if (count <= 0) {
        return E_HAS_DB_ERROR;
    }

    ValuesBucket valuesBucket;
    valuesBucket.PutString(DEVICE_DB_UDID, deviceUdid);
    valuesBucket.PutInt(DEVICE_DB_SYNC_STATUS, syncStatus);
    int32_t updatedRows(0);
    vector<string> whereArgs = {deviceUdid};
    ret = rdbStore->Update(updatedRows, DEVICE_TABLE, valuesBucket, DEVICE_DB_UDID + " = ?", whereArgs);
    if (ret != E_OK) {
        return ret;
    }
    return (updatedRows > 0) ? E_OK : E_FAIL;
}

static string GetDistributedTableName(const shared_ptr<RdbStore> &rdbStore, const string &networkId)
{
    string distributedTableName;
    int errCode = E_ERR;
    if (!networkId.empty()) {
        distributedTableName = rdbStore->ObtainDistributedTableName(networkId, MEDIALIBRARY_TABLE, errCode);
    }
    return distributedTableName;
}

static int32_t GetAlbumCoverThumbnailKeys(const shared_ptr<NativeRdb::RdbStore> &rdbStore,
    const string &sqlStatement, vector<string> &keys)
{
    shared_ptr<NativeRdb::ResultSet> rdbResultSet = rdbStore->QuerySql(sqlStatement);
    auto count = 0;
    int32_t ret = rdbResultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        return ret;
    }

    if (count == 0) {
        return E_FAIL;
    }

    int32_t queryBucketId = -1;
    while (rdbResultSet->GoToNextRow() == NativeRdb::E_OK) {
        int32_t bucketId =
            get<int32_t>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_BUCKET_ID, rdbResultSet, TYPE_INT32));
        if (bucketId == 0) {
            continue;
        }

        if (queryBucketId == bucketId) {
            continue;
        }

        string thumbnailKey =
            get<string>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_THUMBNAIL, rdbResultSet, TYPE_STRING));
        keys.push_back(thumbnailKey);
        queryBucketId = bucketId;
    }
    return E_SUCCESS;
}

static void SyncPullAlbumCoverThumbnailKeys(AsyncTaskData* data)
{
    DistributedAsyncTaskData* taskData = static_cast<DistributedAsyncTaskData*>(data);
    vector<string> thumbnailKeys;
    GetAlbumCoverThumbnailKeys(taskData->syncOpts_.rdbStore, taskData->sqlStatement_, thumbnailKeys);
    MediaLibrarySyncOperation::SyncPullKvstore(taskData->syncOpts_.kvStore, thumbnailKeys, taskData->networkIds_[0]);
}

static void SyncPullAlbumCover(const MediaLibrarySyncOpts &syncOpts, const string &networkId)
{
    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        return;
    }
    DistributedAsyncTaskData* taskData = new (nothrow)DistributedAsyncTaskData();
    if (taskData == nullptr) {
        return;
    }
    taskData->syncOpts_ = syncOpts;
    taskData->networkIds_ = {networkId};
    string distributedTableName = GetDistributedTableName(syncOpts.rdbStore, networkId);
    taskData->sqlStatement_ = "SELECT " + MEDIA_DATA_DB_BUCKET_ID + ", " + "max(" + MEDIA_DATA_DB_DATE_ADDED + "), " +
                              MEDIA_DATA_DB_THUMBNAIL + " FROM " + distributedTableName + " WHERE " +
                              MEDIA_DATA_DB_MEDIA_TYPE + " <> " + to_string(MEDIA_TYPE_FILE) + " AND " +
                              MEDIA_DATA_DB_MEDIA_TYPE + " <> " + to_string(MEDIA_TYPE_ALBUM) + " GROUP BY " +
                              MEDIA_DATA_DB_BUCKET_ID + " , " + MEDIA_DATA_DB_THUMBNAIL + " ORDER BY " +
                              MEDIA_DATA_DB_DATE_ADDED + " DESC";
    auto distributedAsyncTask = make_shared<MediaLibraryAsyncTask>(SyncPullAlbumCoverThumbnailKeys, taskData);
    asyncWorker->AddTask(distributedAsyncTask, false);
}

static bool SyncPullTableCallbackExec(const MediaLibrarySyncOpts &syncOpts, const string &networkId, int syncResult)
{
    if (networkId.empty()) {
        MEDIA_ERR_LOG("SyncPullTable networkId is empty");
        return false;
    }
    if (syncResult != 0) {
        MEDIA_ERR_LOG("SyncPullTable tableName = %{public}s device = %{private}s syncResult = %{private}d",
                      syncOpts.table.c_str(), networkId.c_str(), syncResult);
        return false;
    }
    if (syncOpts.table == MEDIALIBRARY_TABLE) {
        UpdateDeviceSyncStatus(syncOpts.rdbStore, networkId, DEVICE_SYNCSTATUS_COMPLETE);
        if (syncOpts.row.empty()) {
            SyncPullAlbumCover(syncOpts, networkId);
        }
    }
    return true;
}

bool MediaLibrarySyncOperation::SyncPullTable(MediaLibrarySyncOpts &syncOpts, vector<string> &devices)
{
    CHECK_AND_RETURN_RET_LOG(syncOpts.rdbStore != nullptr, false, "Rdb Store is not initialized");
    DistributedRdb::SyncOption option;
    option.mode = DistributedRdb::SyncMode::PULL;
    option.isBlock = true;

    vector<string> onlineDevices;
    GetOnlineDevices(syncOpts.bundleName, devices, onlineDevices);
    if (onlineDevices.size() == 0) {
        MEDIA_ERR_LOG("SyncPullTable there is no online device");
        return false;
    }
    NativeRdb::AbsRdbPredicates predicate(syncOpts.table);
    predicate.InDevices(onlineDevices);
    if (syncOpts.table == MEDIALIBRARY_TABLE && !syncOpts.row.empty()) {
        predicate.EqualTo(MEDIA_DATA_DB_TIME_PENDING, to_string(0))->And()->EqualTo(MEDIA_DATA_DB_ID, syncOpts.row);
    } else if (syncOpts.table == MEDIALIBRARY_TABLE && syncOpts.row.empty()) {
        predicate.EqualTo(MEDIA_DATA_DB_TIME_PENDING, to_string(0));
    } else if (!syncOpts.row.empty()) {
        predicate.EqualTo(MEDIA_DATA_DB_ID, syncOpts.row);
    }

    DistributedRdb::SyncCallback callback = [syncOpts](const DistributedRdb::SyncResult &syncResult) {
        for (auto iter = syncResult.begin(); iter != syncResult.end(); iter++) {
            SyncPullTableCallbackExec(syncOpts, iter->first, iter->second);
        }
    };

    uint32_t count = 0;
    int ret = -1;
    while (count++ < RETRY_COUNT && ret != E_OK) {
        MediaLibraryTracer tracer;
        tracer.Start("abilityHelper->Query");
        ret = syncOpts.rdbStore->Sync(option, predicate, callback);
    }
    return ret == E_OK;
}

static void GetCameraThumbnailKeys(const shared_ptr<NativeRdb::RdbStore> &rdbStore,
    const string &sqlStatement, vector<string> &keys)
{
    shared_ptr<NativeRdb::ResultSet> rdbResultSet = rdbStore->QuerySql(sqlStatement);
    auto count = 0;
    auto ret = rdbResultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        return;
    }
    if (count != 1) {
        return;
    }

    while (rdbResultSet->GoToNextRow() == NativeRdb::E_OK) {
        string relativePath =
            get<string>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_RELATIVE_PATH, rdbResultSet, TYPE_STRING));
        if (relativePath != CAMERA_PATH) {
            MEDIA_ERR_LOG("This sync is not for camera");
            return;
        }
        string thumbnailKey =
            get<string>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_THUMBNAIL, rdbResultSet, TYPE_STRING));
        keys.push_back(thumbnailKey);
        string lcdKey = get<string>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_LCD, rdbResultSet, TYPE_STRING));
        keys.push_back(lcdKey);
    }
}

static void SyncPushCameraThumbnailKeys(AsyncTaskData* data)
{
    DistributedAsyncTaskData* taskData = static_cast<DistributedAsyncTaskData*>(data);
    vector<string> thumbnailKeys;
    GetCameraThumbnailKeys(taskData->syncOpts_.rdbStore, taskData->sqlStatement_, thumbnailKeys);
    MediaLibrarySyncOperation::SyncPushKvstore(taskData->syncOpts_.kvStore, thumbnailKeys, taskData->networkIds_[0]);
}

static void SyncPushCameraThumbnail(const MediaLibrarySyncOpts &syncOpts, const string &networkId)
{
    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        return;
    }
    DistributedAsyncTaskData* taskData = new (nothrow)DistributedAsyncTaskData();
    if (taskData == nullptr) {
        return;
    }
    taskData->syncOpts_ = syncOpts;
    taskData->networkIds_ = {networkId};
    taskData->sqlStatement_ = "SELECT " + MEDIA_DATA_DB_ID + ", " + MEDIA_DATA_DB_THUMBNAIL + ", " + MEDIA_DATA_DB_LCD +
                              ", " + MEDIA_DATA_DB_RELATIVE_PATH + " FROM " + syncOpts.table + " WHERE " +
                              MEDIA_DATA_DB_ID + " = " + syncOpts.row;
    auto distributedAsyncTask = make_shared<MediaLibraryAsyncTask>(SyncPushCameraThumbnailKeys, taskData);
    asyncWorker->AddTask(distributedAsyncTask, false);
}

static bool SyncPushTableCallbackExec(const MediaLibrarySyncOpts &syncOpts, const string &networkId, int syncResult)
{
    if (networkId.empty()) {
        return false;
    }
    if (syncResult != 0) {
        MEDIA_ERR_LOG("SyncPushTable tableName = %{public}s device = %{private}s syncResult = %{private}d",
                      syncOpts.table.c_str(), networkId.c_str(), syncResult);
        return false;
    }

    if (syncOpts.table == MEDIALIBRARY_TABLE) {
        SyncPushCameraThumbnail(syncOpts, networkId);
    }
    return true;
}

bool MediaLibrarySyncOperation::SyncPushTable(MediaLibrarySyncOpts &syncOpts, vector<string> &devices, bool isBlock)
{
    CHECK_AND_RETURN_RET_LOG(syncOpts.rdbStore != nullptr, false, "Rdb Store is not initialized");
    DistributedRdb::SyncOption option;
    option.mode = DistributedRdb::SyncMode::PUSH;
    option.isBlock = isBlock;

    vector<string> onlineDevices;
    GetOnlineDevices(syncOpts.bundleName, devices, onlineDevices);
    if (onlineDevices.size() == 0) {
        return false;
    }
    NativeRdb::AbsRdbPredicates predicate(syncOpts.table);
    predicate.InDevices(onlineDevices);
    if (syncOpts.table == MEDIALIBRARY_TABLE && !syncOpts.row.empty()) {
        predicate.EqualTo(MEDIA_DATA_DB_TIME_PENDING, to_string(0))->And()->EqualTo(MEDIA_DATA_DB_ID, syncOpts.row);
    } else if (syncOpts.table == MEDIALIBRARY_TABLE && syncOpts.row.empty()) {
        predicate.EqualTo(MEDIA_DATA_DB_TIME_PENDING, to_string(0));
    } else if (!syncOpts.row.empty()) {
        predicate.EqualTo(MEDIA_DATA_DB_ID, syncOpts.row);
    }

    DistributedRdb::SyncCallback callback = [syncOpts](const DistributedRdb::SyncResult& syncResult) {
        for (auto iter = syncResult.begin(); iter != syncResult.end(); iter++) {
            SyncPushTableCallbackExec(syncOpts, iter->first, iter->second);
        }
    };

    MediaLibraryTracer tracer;
    tracer.Start("SyncPushTable rdbStore->Sync");
    return syncOpts.rdbStore->Sync(option, predicate, callback) == E_OK;
}

void MediaLibrarySyncOperation::GetOnlineDevices(const string &bundleName, const vector<string> &originalDevices,
    vector<string> &onlineDevices)
{
    vector<OHOS::DistributedHardware::DmDeviceInfo> deviceList;
    string extra = "";
    auto &deviceManager = OHOS::DistributedHardware::DeviceManager::GetInstance();
    int32_t ret = deviceManager.GetTrustedDeviceList(bundleName, extra, deviceList);
    if (ret != 0) {
        MEDIA_ERR_LOG("get trusted device list failed, ret %{public}d", ret);
        return;
    }

    for (auto &device : originalDevices) {
        for (auto &deviceInfo : deviceList) {
            string networkId = deviceInfo.networkId;
            if (networkId.compare(device) == 0) {
                onlineDevices.push_back(device);
            }
        }
    }
}

Status MediaLibrarySyncOperation::SyncPullKvstore(const shared_ptr<SingleKvStore> &kvStore,
    const vector<string> &keys, const string &networkId)
{
    if (kvStore == nullptr) {
        return Status::ERROR;
    }
    if (networkId.empty()) {
        return Status::ERROR;
    }

    if (keys.empty()) {
        return Status::ERROR;
    }
    DataQuery dataQuery;
    dataQuery.InKeys(keys);
    dataQuery.Limit(ALBUM_THUMBNAIL_MAX_COUNT, 0);
    vector<string> devices = {networkId};
    MediaLibraryTracer tracer;
    tracer.Start("SyncPullKvstore kvStore->SyncPull");
    auto callback = make_shared<MediaLibrarySyncCallback>();
    Status status = kvStore->Sync(devices, OHOS::DistributedKv::SyncMode::PULL, dataQuery, callback);
    if (!callback->WaitFor()) {
        MEDIA_DEBUG_LOG("wait_for timeout");
        status = Status::ERROR;
    }
    return status;
}

Status MediaLibrarySyncOperation::SyncPushKvstore(const shared_ptr<SingleKvStore> &kvStore,
    const vector<string> &keys, const string &networkId)
{
    if (kvStore == nullptr) {
        return Status::ERROR;
    }
    if (networkId.empty()) {
        return Status::ERROR;
    }
    if (keys.empty()) {
        return Status::ERROR;
    }
    DataQuery dataQuery;
    dataQuery.InKeys(keys);
    dataQuery.Limit(ALBUM_THUMBNAIL_MAX_COUNT, 0);
    vector<string> devices = { networkId };
    MediaLibraryTracer tracer;
    tracer.Start("SyncPushKvstore kvStore->SyncPush");
    return kvStore->Sync(devices, OHOS::DistributedKv::SyncMode::PUSH, dataQuery);
}
} // namespace Media
} // namespace OHOS
