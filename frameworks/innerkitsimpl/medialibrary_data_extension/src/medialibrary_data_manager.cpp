/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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

#define MLOG_TAG "DataManager"

#include "medialibrary_data_manager.h"

#include <shared_mutex>
#include <unordered_set>

#include "ability_scheduler_interface.h"
#include "abs_rdb_predicates.h"
#include "datashare_abs_result_set.h"
#ifdef DISTRIBUTED
#include "device_manager.h"
#include "device_manager_callback.h"
#endif
#include "hitrace_meter.h"
#include "ipc_skeleton.h"
#include "media_column.h"
#include "media_datashare_ext_ability.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "media_scanner_manager.h"
#include "medialibrary_album_operations.h"
#include "medialibrary_asset_operations.h"
#include "medialibrary_audio_operations.h"
#include "medialibrary_bundle_manager.h"
#include "medialibrary_common_utils.h"
#ifdef DISTRIBUTED
#include "medialibrary_device.h"
#include "medialibrary_device_info.h"
#endif
#include "medialibrary_dir_operations.h"
#include "medialibrary_errno.h"
#include "medialibrary_file_operations.h"
#include "medialibrary_inotify.h"
#include "medialibrary_object_utils.h"
#include "medialibrary_smartalbum_map_operations.h"
#include "medialibrary_smartalbum_operations.h"
#include "medialibrary_sync_operation.h"
#include "medialibrary_tracer.h"
#include "medialibrary_unistore_manager.h"
#include "medialibrary_uripermission_operations.h"
#include "mimetype_utils.h"
#include "permission_utils.h"
#include "photo_map_operations.h"
#include "rdb_store.h"
#include "rdb_utils.h"
#include "result_set_utils.h"
#include "system_ability_definition.h"
#include "timer.h"
#include "trash_async_worker.h"
#include "value_object.h"

using namespace std;
using namespace OHOS::AppExecFwk;
using namespace OHOS::AbilityRuntime;
using namespace OHOS::NativeRdb;
using namespace OHOS::DistributedKv;
using namespace OHOS::DataShare;
using namespace OHOS::RdbDataShareAdapter;

namespace {
const OHOS::DistributedKv::AppId KVSTORE_APPID = {"com.ohos.medialibrary.medialibrarydata"};
const OHOS::DistributedKv::StoreId KVSTORE_STOREID = {"medialibrary_thumbnail"};
};

namespace OHOS {
namespace Media {

shared_ptr<MediaLibraryDataManager> MediaLibraryDataManager::instance_ = nullptr;
unordered_map<string, DirAsset> MediaLibraryDataManager::dirQuerySetMap_ = {};
mutex MediaLibraryDataManager::mutex_;
static constexpr int REVERT_DAYS = 7;
static constexpr int DAY_HOURS = 24;
static constexpr int PER_HOUR_MINUTES = 60;
static constexpr int PER_MINUTE_SECONDS = 60;
#ifdef DISTRIBUTED
static constexpr int MAX_QUERY_THUMBNAIL_KEY_COUNT = 20;
#endif
MediaLibraryDataManager::MediaLibraryDataManager(void)
{
}

MediaLibraryDataManager::~MediaLibraryDataManager(void)
{
    if (kvStorePtr_ != nullptr) {
        dataManager_.CloseKvStore(KVSTORE_APPID, kvStorePtr_);
        kvStorePtr_ = nullptr;
    }
}

shared_ptr<MediaLibraryDataManager> MediaLibraryDataManager::GetInstance()
{
    if (instance_ == nullptr) {
        lock_guard<mutex> lock(mutex_);
        if (instance_ == nullptr) {
            instance_ = make_shared<MediaLibraryDataManager>();
        }
    }
    return instance_;
}

static DataShare::DataShareExtAbility *MediaDataShareCreator(const unique_ptr<Runtime> &runtime)
{
    MEDIA_DEBUG_LOG("MediaLibraryCreator::%{public}s", __func__);
    return  MediaDataShareExtAbility::Create(runtime);
}

__attribute__((constructor)) void RegisterDataShareCreator()
{
    MEDIA_DEBUG_LOG("MediaLibraryDataMgr::%{public}s", __func__);
    DataShare::DataShareExtAbility::SetCreator(MediaDataShareCreator);
}

static void MakeRootDirs()
{
    for (auto &dir : PRESET_ROOT_DIRS) {
        Uri createAlbumUri(MEDIALIBRARY_DATA_URI + "/" + MEDIA_ALBUMOPRN + "/" + MEDIA_ALBUMOPRN_CREATEALBUM);
        ValuesBucket valuesBucket;
        valuesBucket.PutString(MEDIA_DATA_DB_FILE_PATH, ROOT_MEDIA_DIR + dir);
        MediaLibraryCommand cmd(createAlbumUri, valuesBucket);
        auto ret = MediaLibraryAlbumOperations::CreateAlbumOperation(cmd);
        if (ret == E_FILE_EXIST) {
            MEDIA_INFO_LOG("Root dir: %{public}s is exist", dir.c_str());
        } else if (ret <= 0) {
            MEDIA_ERR_LOG("Failed to preset root dir: %{public}s", dir.c_str());
        }
        if (dir == DOC_DIR_VALUES || dir == DOWNLOAD_DIR_VALUES) {
            continue;
        }
        MediaFileUtils::CreateDirectory(ROOT_MEDIA_DIR + dir + ".recycle");
    }
}

int32_t MediaLibraryDataManager::InitMediaLibraryMgr(const shared_ptr<OHOS::AbilityRuntime::Context> &context,
    const shared_ptr<OHOS::AbilityRuntime::Context> &extensionContext)
{
    lock_guard<shared_mutex> lock(mgrSharedMutex_);

    if (refCnt_.load() > 0) {
        MEDIA_DEBUG_LOG("already initialized");
        refCnt_++;
        return E_OK;
    }

    context_ = context;
    int32_t errCode = InitMediaLibraryRdbStore();
    CHECK_AND_RETURN_RET_LOG(errCode == E_OK, errCode, "failed at InitMediaLibraryRdbStore");

#ifdef DISTRIBUTED
    errCode = InitDeviceData();
    CHECK_AND_RETURN_RET_LOG(errCode == E_OK, errCode, "failed at InitDeviceData");
#endif

    MimeTypeUtils::InitMimeTypeMap();
    errCode = MakeDirQuerySetMap(dirQuerySetMap_);
    CHECK_AND_RETURN_RET_LOG(errCode == E_OK, errCode, "failed at MakeDirQuerySetMap");

    MakeRootDirs();

    errCode = InitialiseThumbnailService(extensionContext);
    CHECK_AND_RETURN_RET_LOG(errCode == E_OK, errCode, "failed at InitialiseThumbnailService");

    errCode = DoTrashAging();
    if (errCode != E_OK) {
        MEDIA_WARN_LOG("Ignore trash aging failures, just continue");
    }
    refCnt_++;
    return E_OK;
}

#ifdef DISTRIBUTED
int32_t MediaLibraryDataManager::InitDeviceData()
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("MediaLibraryDataManager InitDeviceData rdbStore is null");
        return E_ERR;
    }

    MediaLibraryTracer tracer;
    tracer.Start("InitDeviceRdbStoreTrace");
    if (!MediaLibraryDevice::GetInstance()->InitDeviceRdbStore(rdbStore_)) {
        MEDIA_ERR_LOG("MediaLibraryDataManager InitDeviceData failed!");
        return E_ERR;
    }
    return E_OK;
}
#endif

void MediaLibraryDataManager::ClearMediaLibraryMgr()
{
    lock_guard<shared_mutex> lock(mgrSharedMutex_);

    refCnt_--;
    if (refCnt_.load() > 0) {
        MEDIA_DEBUG_LOG("still other extension exist");
        return;
    }

    rdbStore_ = nullptr;

    if (kvStorePtr_ != nullptr) {
        dataManager_.CloseKvStore(KVSTORE_APPID, kvStorePtr_);
        kvStorePtr_ = nullptr;
    }

#ifdef DISTRIBUTED
    if (MediaLibraryDevice::GetInstance()) {
        MediaLibraryDevice::GetInstance()->Stop();
    };
#endif

    if (thumbnailService_ != nullptr) {
        thumbnailService_->ReleaseService();
        thumbnailService_ = nullptr;
    }
    auto watch = MediaLibraryInotify::GetInstance();
    if (watch != nullptr) {
        watch->DoStop();
    }
    MediaLibraryUnistoreManager::GetInstance().Stop();
    extension_ = nullptr;
}

int32_t MediaLibraryDataManager::InitMediaLibraryRdbStore()
{
    if (rdbStore_) {
        return E_OK;
    }

    int32_t ret = MediaLibraryUnistoreManager::GetInstance().Init(context_);
    if (ret != E_OK) {
        MEDIA_ERR_LOG("init MediaLibraryUnistoreManager failed");
        return ret;
    }
    rdbStore_ = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw();
    if (ret != E_OK) {
        MEDIA_ERR_LOG("rdbStore is nullptr");
        return E_ERR;
    }

    return E_OK;
}

int32_t MediaLibraryDataManager::InitialiseKvStore()
{
    if (kvStorePtr_ != nullptr) {
        return E_OK;
    }

    Options options = {
        .createIfMissing = true,
        .encrypt = false,
        .backup = false,
        .autoSync = false,
        .securityLevel = DistributedKv::SecurityLevel::S3,
        .area = DistributedKv::Area::EL2,
        .kvStoreType = KvStoreType::SINGLE_VERSION,
        .baseDir = context_->GetDatabaseDir(),
    };

    Status status = dataManager_.GetSingleKvStore(options, KVSTORE_APPID, KVSTORE_STOREID, kvStorePtr_);
    if (status != Status::SUCCESS || kvStorePtr_ == nullptr) {
        MEDIA_ERR_LOG("MediaLibraryDataManager::InitialiseKvStore failed %{private}d", status);
        return E_ERR;
    }
#ifdef DISTRIBUTED
    if (!MediaLibraryDevice::GetInstance()->InitDeviceKvStore(kvStorePtr_)) {
        return E_FAIL;
    }
#endif
    return E_OK;
}

shared_ptr<MediaDataShareExtAbility> MediaLibraryDataManager::GetOwner()
{
    return extension_;
}

void MediaLibraryDataManager::SetOwner(const shared_ptr<MediaDataShareExtAbility> &datashareExternsion)
{
    extension_ = datashareExternsion;
}

string MediaLibraryDataManager::GetType(const Uri &uri)
{
    MEDIA_INFO_LOG("GetType uri: %{public}s", uri.ToString().c_str());
    return "";
}

int32_t MediaLibraryDataManager::MakeDirQuerySetMap(unordered_map<string, DirAsset> &outDirQuerySetMap)
{
    int32_t count = -1;
    vector<string> columns;
    AbsRdbPredicates dirAbsPred(MEDIATYPE_DIRECTORY_TABLE);
    auto queryResultSet = rdbStore_->QueryByStep(dirAbsPred, columns);
    auto ret = queryResultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("rdb failed");
        return E_ERR;
    }
    MEDIA_INFO_LOG("MakeDirQuerySetMap count = %{public}d", count);
    if (count == 0) {
        MEDIA_ERR_LOG("can not find any dirAsset");
        return E_ERR;
    }
    DirAsset dirAsset;
    string dirVal;
    outDirQuerySetMap.clear();
    while (queryResultSet->GoToNextRow() == NativeRdb::E_OK) {
        dirVal = get<string>(
            ResultSetUtils::GetValFromColumn(DIRECTORY_DB_DIRECTORY, queryResultSet, TYPE_STRING));
        dirAsset.SetDirectory(dirVal);
        dirAsset.SetDirType(get<int32_t>(
            ResultSetUtils::GetValFromColumn(DIRECTORY_DB_DIRECTORY_TYPE, queryResultSet, TYPE_INT32)));
        dirAsset.SetMediaTypes(get<string>(
            ResultSetUtils::GetValFromColumn(DIRECTORY_DB_MEDIA_TYPE, queryResultSet, TYPE_STRING)));
        dirAsset.SetExtensions(get<string>(
            ResultSetUtils::GetValFromColumn(DIRECTORY_DB_EXTENSION, queryResultSet, TYPE_STRING)));
        outDirQuerySetMap.insert(make_pair(dirVal, dirAsset));
    }
    return E_OK;
}

unordered_map<string, DirAsset> MediaLibraryDataManager::GetDirQuerySetMap()
{
    return dirQuerySetMap_;
}

#ifdef MEDIALIBRARY_COMPATIBILITY
static void ChangeUriFromValuesBucket(ValuesBucket &values)
{
    if (!values.HasColumn(MEDIA_DATA_DB_URI)) {
        return;
    }

    ValueObject value;
    if (!values.GetObject(MEDIA_DATA_DB_URI, value)) {
        return;
    }
    string oldUri;
    if (value.GetString(oldUri) != NativeRdb::E_OK) {
        return;
    }
    string newUri = MediaFileUtils::GetRealUriFromVirtualUri(oldUri);
    values.Delete(MEDIA_DATA_DB_URI);
    values.PutString(MEDIA_DATA_DB_URI, newUri);
}
#endif

int32_t MediaLibraryDataManager::SolveInsertCmd(MediaLibraryCommand &cmd)
{
    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_ASSET: {
            return MediaLibraryFileOperations::HandleFileOperation(cmd);
        }
        case OperationObject::FILESYSTEM_PHOTO:
        case OperationObject::FILESYSTEM_AUDIO: {
            return MediaLibraryAssetOperations::HandleInsertOperation(cmd);
        }
        case OperationObject::FILESYSTEM_ALBUM: {
            return MediaLibraryAlbumOperations::CreateAlbumOperation(cmd);
        }
        case OperationObject::PHOTO_ALBUM: {
            return MediaLibraryAlbumOperations::HandlePhotoAlbumOperations(cmd);
        }
        case OperationObject::FILESYSTEM_DIR: {
            return MediaLibraryDirOperations::HandleDirOperation(cmd);
        }
        case OperationObject::SMART_ALBUM: {
            return MediaLibrarySmartAlbumOperations::HandleSmartAlbumOperation(cmd);
        }
        case OperationObject::SMART_ALBUM_MAP: {
            return MediaLibrarySmartAlbumMapOperations::HandleSmartAlbumMapOperation(cmd);
        }
        case OperationObject::THUMBNAIL: {
            return HandleThumbnailOperations(cmd);
        }
        case OperationObject::BUNDLE_PERMISSION: {
            return UriPermissionOperations::HandleUriPermOperations(cmd);
        }
        default: {
            MEDIA_ERR_LOG("MediaLibraryDataManager SolveInsertCmd: unsupported OperationObject: %{public}d",
                cmd.GetOprnObject());
            return E_FAIL;
        }
    }
}

int32_t MediaLibraryDataManager::Insert(MediaLibraryCommand &cmd, const DataShareValuesBucket &dataShareValue)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return E_FAIL;
    }

    ValuesBucket value = RdbUtils::ToValuesBucket(dataShareValue);
    if (value.IsEmpty()) {
        MEDIA_ERR_LOG("MediaLibraryDataManager Insert: Input parameter is invalid");
        return E_INVALID_VALUES;
    }
#ifdef MEDIALIBRARY_COMPATIBILITY
    ChangeUriFromValuesBucket(value);
#endif
    cmd.SetValueBucket(value);

    OperationType oprnType = cmd.GetOprnType();
    if (oprnType == OperationType::CREATE) {
        if (SetCmdBundleAndDevice(cmd) != ERR_OK) {
            MEDIA_ERR_LOG("MediaLibraryDataManager SetCmdBundleAndDevice failed.");
        }
    }
    // boardcast operation
    if (oprnType == OperationType::SCAN) {
        return MediaScannerManager::GetInstance()->ScanDir(ROOT_MEDIA_DIR, nullptr);
    } else if (oprnType == OperationType::DELETE_TOOL) {
        return MediaLibraryAssetOperations::DeleteToolOperation(cmd);
    }
    return SolveInsertCmd(cmd);
}

int32_t MediaLibraryDataManager::InsertExt(MediaLibraryCommand &cmd, const DataShareValuesBucket &dataShareValue,
    string &result)
{
    int32_t res = Insert(cmd, dataShareValue);
    result = cmd.GetResult();
    return res;
}

int32_t MediaLibraryDataManager::HandleThumbnailOperations(MediaLibraryCommand &cmd)
{
    if (thumbnailService_ == nullptr) {
        return E_THUMBNAIL_SERVICE_NULLPTR;
    }
    int32_t result = E_FAIL;
    switch (cmd.GetOprnType()) {
        case OperationType::GENERATE:
            result = thumbnailService_->GenerateThumbnails();
            break;
        case OperationType::AGING:
            result = thumbnailService_->LcdAging();
            break;
#ifdef DISTRIBUTED
        case OperationType::DISTRIBUTE_AGING:
            result = DistributeDeviceAging();
            break;
#endif
        default:
            MEDIA_ERR_LOG("bad operation type %{public}u", cmd.GetOprnType());
    }
    return result;
}

int32_t MediaLibraryDataManager::BatchInsert(MediaLibraryCommand &cmd, const vector<DataShareValuesBucket> &values)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_ERR_LOG("MediaLibraryDataManager is not initialized");
        return E_FAIL;
    }

    string uriString = cmd.GetUri().ToString();
    if (uriString == UFM_PHOTO_ALBUM_ADD_ASSET || uriString == PAH_PHOTO_ALBUM_ADD_ASSET) {
        return PhotoMapOperations::AddPhotoAssets(values);
    }
    if (uriString.find(MEDIALIBRARY_DATA_URI) == string::npos) {
        MEDIA_ERR_LOG("MediaLibraryDataManager BatchInsert: Input parameter is invalid");
        return E_INVALID_URI;
    }
    int32_t rowCount = 0;
    for (auto it = values.begin(); it != values.end(); it++) {
        if (Insert(cmd, *it) >= 0) {
            rowCount++;
        }
    }

    return rowCount;
}

int32_t MediaLibraryDataManager::Delete(MediaLibraryCommand &cmd, const DataSharePredicates &predicates)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return E_FAIL;
    }
    if (cmd.GetUri().ToString().find(MEDIALIBRARY_DATA_URI) == string::npos) {
        MEDIA_ERR_LOG("Not Data ability Uri");
        return E_INVALID_URI;
    }
    MediaLibraryTracer tracer;
    tracer.Start("CheckWhereClause");
    auto whereClause = predicates.GetWhereClause();
    if (!MediaLibraryCommonUtils::CheckWhereClause(whereClause)) {
        MEDIA_ERR_LOG("illegal query whereClause input %{public}s", whereClause.c_str());
        return E_SQL_CHECK_FAIL;
    }
    tracer.Finish();

    // MEDIALIBRARY_TABLE just for RdbPredicates
    NativeRdb::RdbPredicates rdbPredicate = RdbUtils::ToPredicates(predicates,
        cmd.GetTableName());
    cmd.GetAbsRdbPredicates()->SetWhereClause(rdbPredicate.GetWhereClause());
    cmd.GetAbsRdbPredicates()->SetWhereArgs(rdbPredicate.GetWhereArgs());

    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_ASSET:
        case OperationObject::FILESYSTEM_DIR:
        case OperationObject::FILESYSTEM_ALBUM: {
            vector<string> columns = { MEDIA_DATA_DB_ID, MEDIA_DATA_DB_FILE_PATH, MEDIA_DATA_DB_PARENT_ID,
                MEDIA_DATA_DB_MEDIA_TYPE, MEDIA_DATA_DB_IS_TRASH, MEDIA_DATA_DB_RELATIVE_PATH };
            auto fileAsset = MediaLibraryObjectUtils::GetFileAssetByPredicates(*cmd.GetAbsRdbPredicates(), columns);
            CHECK_AND_RETURN_RET_LOG(fileAsset != nullptr, E_INVALID_ARGUMENTS, "Get fileAsset failed.");
            if (fileAsset->GetRelativePath() == "") {
                return E_DELETE_DENIED;
            }
            return (fileAsset->GetMediaType() != MEDIA_TYPE_ALBUM) ?
                MediaLibraryObjectUtils::DeleteFileObj(move(fileAsset)) :
                MediaLibraryObjectUtils::DeleteDirObj(move(fileAsset));
        }
        case OperationObject::PHOTO_ALBUM: {
            return MediaLibraryAlbumOperations::DeletePhotoAlbum(rdbPredicate);
        }
        case OperationObject::PHOTO_MAP: {
            return PhotoMapOperations::RemovePhotoAssets(rdbPredicate);
        }
        case OperationObject::FILESYSTEM_PHOTO:
        case OperationObject::FILESYSTEM_AUDIO: {
            return MediaLibraryAssetOperations::DeleteOperation(cmd);
        }
        default:
            break;
    }

    return E_FAIL;
}

int32_t MediaLibraryDataManager::Update(MediaLibraryCommand &cmd, const DataShareValuesBucket &dataShareValue,
    const DataSharePredicates &predicates)
{
    MEDIA_DEBUG_LOG("MediaLibraryDataManager::Update");
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return E_FAIL;
    }

    ValuesBucket value = RdbUtils::ToValuesBucket(dataShareValue);
    if (value.IsEmpty()) {
        MEDIA_ERR_LOG("MediaLibraryDataManager Update:Input parameter is invalid ");
        return E_INVALID_VALUES;
    }

#ifdef MEDIALIBRARY_COMPATIBILITY
    ChangeUriFromValuesBucket(value);
#endif

    cmd.SetValueBucket(value);
    cmd.SetDataSharePred(predicates);
    cmd.GetAbsRdbPredicates()->SetWhereClause(predicates.GetWhereClause());
    cmd.GetAbsRdbPredicates()->SetWhereArgs(predicates.GetWhereArgs());

    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_ASSET: {
            auto ret = MediaLibraryFileOperations::ModifyFileOperation(cmd);
            if (ret == E_SAME_PATH) {
                break;
            } else {
                return ret;
            }
        }
        case OperationObject::FILESYSTEM_DIR:
            // supply a ModifyDirOperation here to replace
            // modify in the HandleDirOperations in Insert function, if need
            break;
        case OperationObject::FILESYSTEM_ALBUM: {
            return MediaLibraryAlbumOperations::ModifyAlbumOperation(cmd);
        }
        case OperationObject::FILESYSTEM_PHOTO:
        case OperationObject::FILESYSTEM_AUDIO: {
            return MediaLibraryAssetOperations::UpdateOperation(cmd);
        }
        case OperationObject::PHOTO_ALBUM: {
            return MediaLibraryAlbumOperations::HandlePhotoAlbum(cmd.GetOprnType(), value, predicates);
        }
        default:
            break;
    }
    // ModifyInfoByIdInDb can finish the default update of smartalbum and smartmap,
    // so no need to distinct them in switch-case deliberately
    cmd.SetValueBucket(value);
    return MediaLibraryObjectUtils::ModifyInfoByIdInDb(cmd);
}

void MediaLibraryDataManager::InterruptBgworker()
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return;
    }

    if (thumbnailService_ == nullptr) {
        return;
    }
    thumbnailService_->InterruptBgworker();
    shared_ptr<TrashAsyncTaskWorker> asyncWorker = TrashAsyncTaskWorker::GetInstance();
    if (asyncWorker == nullptr) {
        MEDIA_ERR_LOG("asyncWorker null");
        return;
    }
    asyncWorker->Interrupt();
}

int32_t MediaLibraryDataManager::GenerateThumbnails()
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return E_FAIL;
    }

    if (thumbnailService_ == nullptr) {
        return E_THUMBNAIL_SERVICE_NULLPTR;
    }
    return thumbnailService_->GenerateThumbnails();
}

int32_t MediaLibraryDataManager::DoAging()
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    MEDIA_DEBUG_LOG("MediaLibraryDataManager::DoAging IN");
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return E_FAIL;
    }

    if (thumbnailService_ == nullptr) {
        return E_THUMBNAIL_SERVICE_NULLPTR;
    }
    int32_t errorCode = thumbnailService_->LcdAging();
    if (errorCode != 0) {
        MEDIA_ERR_LOG("LcdAging exist error %{public}d", errorCode);
    }

    shared_ptr<TrashAsyncTaskWorker> asyncWorker = TrashAsyncTaskWorker::GetInstance();
    if (asyncWorker == nullptr) {
        MEDIA_ERR_LOG("asyncWorker null");
        return E_FAIL;
    }
    asyncWorker->Init();
    return errorCode;
}

#ifdef DISTRIBUTED
int32_t MediaLibraryDataManager::LcdDistributeAging()
{
    MEDIA_DEBUG_LOG("MediaLibraryDataManager::LcdDistributeAging IN");
    auto deviceInstance = MediaLibraryDevice::GetInstance();
    if ((thumbnailService_ == nullptr) || (deviceInstance == nullptr)) {
        return E_THUMBNAIL_SERVICE_NULLPTR;
    }
    int32_t result = E_SUCCESS;
    vector<string> deviceUdids;
    deviceInstance->QueryAllDeviceUdid(deviceUdids);
    for (string &udid : deviceUdids) {
        result = thumbnailService_->LcdDistributeAging(udid);
        if (result != E_SUCCESS) {
            MEDIA_ERR_LOG("LcdDistributeAging fail result is %{public}d", result);
            break;
        }
    }
    return result;
}

int32_t MediaLibraryDataManager::DistributeDeviceAging()
{
    MEDIA_DEBUG_LOG("MediaLibraryDataManager::DistributeDeviceAging IN");
    auto deviceInstance = MediaLibraryDevice::GetInstance();
    if ((thumbnailService_ == nullptr) || (deviceInstance == nullptr)) {
        return E_FAIL;
    }
    int32_t result = E_FAIL;
    vector<MediaLibraryDeviceInfo> deviceDataBaseList;
    deviceInstance->QueryAgingDeviceInfos(deviceDataBaseList);
    MEDIA_DEBUG_LOG("MediaLibraryDevice InitDeviceRdbStore deviceDataBaseList size =  %{public}d",
        (int) deviceDataBaseList.size());
    for (MediaLibraryDeviceInfo deviceInfo : deviceDataBaseList) {
        result = thumbnailService_->InvalidateDistributeThumbnail(deviceInfo.deviceUdid);
        if (result != E_SUCCESS) {
            MEDIA_ERR_LOG("invalidate fail %{public}d", result);
            continue;
        }
    }
    return result;
}
#endif

int MediaLibraryDataManager::GetThumbnail(const string &uri)
{
    if (thumbnailService_ == nullptr) {
        return E_THUMBNAIL_SERVICE_NULLPTR;
    }
    if (!uri.empty() && MediaLibraryObjectUtils::CheckUriPending(uri)) {
        MEDIA_ERR_LOG("failed to get thumbnail, the file:%{public}s is pending", uri.c_str());
        return E_FAIL;
    }
    return thumbnailService_->GetThumbnailFd(uri);
}

void MediaLibraryDataManager::CreateThumbnailAsync(const string &uri, const string &path)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return;
    }

    if (thumbnailService_ == nullptr) {
        return;
    }
    if (!uri.empty()) {
        if (MediaLibraryObjectUtils::CheckUriPending(uri)) {
            MEDIA_ERR_LOG("failed to get thumbnail, the file:%{public}s is pending", uri.c_str());
            return;
        }
        int32_t err = thumbnailService_->CreateThumbnail(uri, path);
        if (err != E_SUCCESS) {
            MEDIA_ERR_LOG("ThumbnailService CreateThumbnail failed : %{public}d", err);
        }
    }
}

void MediaLibraryDataManager::NeedQuerySync(const string &networkId, OperationObject oprnObject)
{
    if (networkId.empty()) {
        return;
    }
    // tabletype mapping into tablename
    string tableName = MEDIALIBRARY_TABLE;
    switch (oprnObject) {
        case OperationObject::SMART_ALBUM:
            tableName = SMARTALBUM_TABLE;
            break;
        case OperationObject::SMART_ALBUM_MAP:
            tableName = SMARTALBUM_MAP_TABLE;
            break;
        case OperationObject::FILESYSTEM_PHOTO:
            tableName = PhotoColumn::PHOTOS_TABLE;
            break;
        case OperationObject::FILESYSTEM_AUDIO:
            tableName = AudioColumn::AUDIOS_TABLE;
            break;
        default:
            break;
    }

#ifdef DISTRIBUTED
    if ((oprnObject != OperationObject::ASSETMAP) && (oprnObject != OperationObject::SMART_ALBUM_ASSETS)) {
        MediaLibraryTracer tracer;
        tracer.Start("QuerySync");
        auto ret = QuerySync(networkId, tableName);
        MEDIA_INFO_LOG("MediaLibraryDataManager QuerySync result = %{private}d", ret);
    }
#endif
}

shared_ptr<ResultSetBridge> MediaLibraryDataManager::Query(MediaLibraryCommand &cmd,
    const vector<string> &columns, const DataSharePredicates &predicates, int &errCode)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        errCode = E_FAIL;
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return nullptr;
    }

    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryDataManager::Query");
    if (rdbStore_ == nullptr) {
        errCode = E_FAIL;
        MEDIA_ERR_LOG("Rdb Store is not initialized");
        return nullptr;
    }

    auto absResultSet = QueryRdb(cmd, columns, predicates, errCode);
    if (absResultSet == nullptr) {
        errCode = (errCode != E_OK) ? errCode : E_FAIL;
        return nullptr;
    }
    return RdbUtils::ToResultSetBridge(absResultSet);
}

#ifdef DISTRIBUTED
int32_t MediaLibraryDataManager::SyncPullThumbnailKeys(const Uri &uri)
{
    if (MediaLibraryDevice::GetInstance() == nullptr || !MediaLibraryDevice::GetInstance()->IsHasActiveDevice()) {
        return E_ERR;
    }
    if (kvStorePtr_ == nullptr) {
        return E_ERR;
    }

    MediaLibraryCommand cmd(uri, OperationType::QUERY);
    cmd.GetAbsRdbPredicates()->BeginWrap()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_IMAGE))
        ->Or()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_VIDEO))->EndWrap()
        ->And()->EqualTo(MEDIA_DATA_DB_DATE_TRASHED, to_string(0))
        ->And()->NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM))
        ->OrderByDesc(MEDIA_DATA_DB_DATE_ADDED);
    vector<string> columns = { MEDIA_DATA_DB_THUMBNAIL, MEDIA_DATA_DB_LCD };
    auto resultset = MediaLibraryFileOperations::QueryFileOperation(cmd, columns);
    if (resultset == nullptr) {
        return E_HAS_DB_ERROR;
    }

    vector<string> thumbnailKeys;
    int count = 0;
    while (resultset->GoToNextRow() == NativeRdb::E_OK && count < MAX_QUERY_THUMBNAIL_KEY_COUNT) {
        string thumbnailKey =
            get<string>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_THUMBNAIL, resultset, TYPE_STRING));
        thumbnailKeys.push_back(thumbnailKey);
        string lcdKey = get<string>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_LCD, resultset, TYPE_STRING));
        thumbnailKeys.push_back(lcdKey);
        count++;
    }

    if (thumbnailKeys.empty()) {
        return E_NO_SUCH_FILE;
    }
    MediaLibrarySyncOperation::SyncPullKvstore(kvStorePtr_, thumbnailKeys,
        MediaFileUtils::GetNetworkIdFromUri(uri.ToString()));
    return E_SUCCESS;
}
#endif

shared_ptr<NativeRdb::ResultSet> MediaLibraryDataManager::QueryRdb(MediaLibraryCommand &cmd,
    const vector<string> &columns, const DataSharePredicates &predicates, int &errCode)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        errCode = E_FAIL;
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return nullptr;
    }
    MediaLibraryTracer tracer;
    tracer.Start("QueryRdb");
    static const map<OperationObject, string> queryConditionMap {
        { OperationObject::SMART_ALBUM, SMARTALBUM_DB_ID },
        { OperationObject::SMART_ALBUM_MAP, SMARTALBUMMAP_DB_ALBUM_ID },
        { OperationObject::FILESYSTEM_DIR, MEDIA_DATA_DB_ID },
        { OperationObject::ALL_DEVICE, "" },
        { OperationObject::ACTIVE_DEVICE, "" },
        { OperationObject::ASSETMAP, "" },
        { OperationObject::SMART_ALBUM_ASSETS, "" },
        { OperationObject::BUNDLE_PERMISSION, "" },
    };

    tracer.Start("CheckWhereClause");
    auto whereClause = predicates.GetWhereClause();
    if (!MediaLibraryCommonUtils::CheckWhereClause(whereClause)) {
        errCode = E_INVALID_VALUES;
        MEDIA_ERR_LOG("illegal query whereClause input %{public}s", whereClause.c_str());
        return nullptr;
    }
    tracer.Finish();

    cmd.SetDataSharePred(predicates);
    // MEDIALIBRARY_TABLE just for RdbPredicates
    NativeRdb::RdbPredicates rdbPredicate = RdbUtils::ToPredicates(predicates,
        MEDIALIBRARY_TABLE);
    cmd.GetAbsRdbPredicates()->SetWhereClause(rdbPredicate.GetWhereClause());
    cmd.GetAbsRdbPredicates()->SetWhereArgs(rdbPredicate.GetWhereArgs());
    cmd.GetAbsRdbPredicates()->SetOrder(rdbPredicate.GetOrder());

    shared_ptr<NativeRdb::ResultSet> queryResultSet;
    OperationObject oprnObject = cmd.GetOprnObject();
    auto it = queryConditionMap.find(oprnObject);
    if (it != queryConditionMap.end()) {
        queryResultSet = MediaLibraryObjectUtils::QueryWithCondition(cmd, columns, it->second);
    } else if (oprnObject == OperationObject::FILESYSTEM_ALBUM || oprnObject == OperationObject::MEDIA_VOLUME) {
        queryResultSet = MediaLibraryAlbumOperations::QueryAlbumOperation(cmd, columns);
    } else if (oprnObject == OperationObject::PHOTO_ALBUM) {
        queryResultSet = MediaLibraryAlbumOperations::QueryPhotoAlbum(cmd, columns);
    } else if (oprnObject == OperationObject::PHOTO_MAP) {
        queryResultSet = PhotoMapOperations::QueryPhotoAssets(
            RdbUtils::ToPredicates(predicates, PhotoColumn::PHOTOS_TABLE), columns);
    } else if (oprnObject == OperationObject::FILESYSTEM_PHOTO || oprnObject == OperationObject::FILESYSTEM_AUDIO) {
        queryResultSet = MediaLibraryAssetOperations::QueryOperation(cmd, columns);
    } else {
        tracer.Start("QueryFile");
        queryResultSet = MediaLibraryFileOperations::QueryFileOperation(cmd, columns);
    }
    return queryResultSet;
}

#ifdef DISTRIBUTED
bool MediaLibraryDataManager::QuerySync(const string &networkId, const string &tableName)
{
    if (networkId.empty() || tableName.empty()) {
        return false;
    }

    OHOS::DistributedHardware::DmDeviceInfo deviceInfo;
    auto &deviceManager = OHOS::DistributedHardware::DeviceManager::GetInstance();
    auto ret = deviceManager.GetLocalDeviceInfo(bundleName_, deviceInfo);
    if (ret != ERR_OK) {
        MEDIA_ERR_LOG("MediaLibraryDataManager QuerySync Failed to get local device info.");
        return false;
    }

    if (networkId == string(deviceInfo.networkId)) {
        return true;
    }

    int32_t syncStatus = DEVICE_SYNCSTATUSING;
    auto result = MediaLibraryDevice::GetInstance()->GetDeviceSyncStatus(networkId, tableName, syncStatus);
    if (result && syncStatus == DEVICE_SYNCSTATUS_COMPLETE) {
        return true;
    }

    vector<string> devices = { networkId };
    MediaLibrarySyncOpts syncOpts;
    syncOpts.rdbStore = rdbStore_;
    syncOpts.table = tableName;
    syncOpts.bundleName = bundleName_;
    return MediaLibrarySyncOperation::SyncPullTable(syncOpts, devices);
}
#endif

int32_t MediaLibraryDataManager::OpenFile(MediaLibraryCommand &cmd, const string &mode)
{
    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryDataManager::OpenFile");
    auto oprnObject = cmd.GetOprnObject();
    if (oprnObject == OperationObject::FILESYSTEM_PHOTO || oprnObject == OperationObject::FILESYSTEM_AUDIO) {
        return MediaLibraryAssetOperations::OpenOperation(cmd, mode);
    }

#ifdef MEDIALIBRARY_COMPATIBILITY
    if (oprnObject != OperationObject::THUMBNAIL) {
        string opObject = MediaFileUri::GetPathFirstDentry(const_cast<Uri &>(cmd.GetUri()));
        if (opObject == IMAGE_ASSET_TYPE || opObject == VIDEO_ASSET_TYPE || opObject == URI_TYPE_PHOTO) {
            cmd.SetOprnObject(OperationObject::FILESYSTEM_PHOTO);
            return MediaLibraryAssetOperations::OpenOperation(cmd, mode);
        }
        if (opObject == AUDIO_ASSET_TYPE || opObject == URI_TYPE_AUDIO_V10) {
            cmd.SetOprnObject(OperationObject::FILESYSTEM_AUDIO);
            return MediaLibraryAssetOperations::OpenOperation(cmd, mode);
        }
    }
#endif

    return MediaLibraryObjectUtils::OpenFile(cmd, mode);
}

void MediaLibraryDataManager::NotifyChange(const Uri &uri)
{
    shared_lock<shared_mutex> sharedLock(mgrSharedMutex_);
    if (refCnt_.load() <= 0) {
        MEDIA_DEBUG_LOG("MediaLibraryDataManager is not initialized");
        return;
    }

    if (extension_ == nullptr) {
        MEDIA_ERR_LOG("MediaLibraryDataManager::NotifyChange failed");
        return;
    }

    extension_->NotifyChange(uri);
}

int32_t MediaLibraryDataManager::InitialiseThumbnailService(
    const shared_ptr<OHOS::AbilityRuntime::Context> &extensionContext)
{
    if (thumbnailService_ != nullptr) {
        return E_OK;
    }
    thumbnailService_ = ThumbnailService::GetInstance();
    if (thumbnailService_ == nullptr) {
        return E_THUMBNAIL_SERVICE_NULLPTR;
    }
    int ret = thumbnailService_->Init(rdbStore_, kvStorePtr_, extensionContext);
    if (ret != E_OK) {
        MEDIA_ERR_LOG("Failed to init ThumbnailService, err %{public}d", ret);
        return E_ERR;
    }
    return E_OK;
}

int32_t ScanFileCallback::OnScanFinished(const int32_t status, const string &uri, const string &path)
{
    auto instance = MediaLibraryDataManager::GetInstance();
    if (instance != nullptr) {
        instance->CreateThumbnailAsync(uri, path);
    }
    return E_OK;
}

int32_t MediaLibraryDataManager::SetCmdBundleAndDevice(MediaLibraryCommand &outCmd)
{
    string clientBundle = MediaLibraryBundleManager::GetInstance()->GetClientBundleName();
    if (clientBundle.empty()) {
        MEDIA_ERR_LOG("GetClientBundleName failed");
        return E_GET_CLIENTBUNDLE_FAIL;
    }
    outCmd.SetBundleName(clientBundle);
#ifdef DISTRIBUTED
    OHOS::DistributedHardware::DmDeviceInfo deviceInfo;
    auto &deviceManager = OHOS::DistributedHardware::DeviceManager::GetInstance();
    int32_t ret = deviceManager.GetLocalDeviceInfo(bundleName_, deviceInfo);
    if (ret < 0) {
        MEDIA_ERR_LOG("GetLocalDeviceInfo ret = %{public}d", ret);
    } else {
        outCmd.SetDeviceName(deviceInfo.deviceName);
    }
    return ret;
#endif
    return 0;
}

int32_t MediaLibraryDataManager::DoTrashAging()
{
    MediaLibrarySmartAlbumMapOperations::HandleAgingOperation();
    MediaLibraryAlbumOperations::HandlePhotoAlbum(OperationType::AGING, {}, {});
    MediaLibraryAudioOperations::TrashAging();
    return E_SUCCESS;
}

int32_t MediaLibraryDataManager::RevertPendingByFileId(const std::string &fileId)
{
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::UPDATE);
    ValuesBucket values;
    values.PutLong(Media::MEDIA_DATA_DB_TIME_PENDING, 0);
    cmd.SetValueBucket(values);
    int32_t retVal = MediaLibraryObjectUtils::ModifyInfoByIdInDb(cmd, fileId);
    if (retVal <= 0) {
        MEDIA_ERR_LOG("failed to revert pending error, fileId:%{public}s", fileId.c_str());
        return retVal;
    }
    auto fileAsset = MediaLibraryObjectUtils::GetFileAssetFromId(fileId);
    string srcPath = fileAsset->GetPath();
    MediaLibraryObjectUtils::ScanFileAsync(srcPath, fileId, MediaLibraryApi::API_10);
    return E_SUCCESS;
}

int32_t MediaLibraryDataManager::RevertPendingByPackage(const std::string &bundleName)
{
    MediaLibraryCommand queryCmd(OperationObject::FILESYSTEM_ASSET, OperationType::QUERY);
    queryCmd.GetAbsRdbPredicates()
        ->EqualTo(MEDIA_DATA_DB_OWNER_PACKAGE, bundleName)
        ->And()
        ->NotEqualTo(MEDIA_DATA_DB_TIME_PENDING, to_string(0));
    vector<string> columns = { MEDIA_DATA_DB_ID };
    auto result = MediaLibraryObjectUtils::QueryWithCondition(queryCmd, columns);
    if (result == nullptr) {
        return E_HAS_DB_ERROR;
    }

    int32_t ret = E_SUCCESS;
    while (result->GoToNextRow() == NativeRdb::E_OK) {
        int32_t id = GetInt32Val(MEDIA_DATA_DB_ID, result);
        int32_t retVal = RevertPendingByFileId(to_string(id));
        if (retVal != E_SUCCESS) {
            ret = retVal;
            MEDIA_ERR_LOG("Revert file %{public}d failed, ret=%{public}d", id, retVal);
            continue;
        }
    }
    return ret;
}

int32_t MediaLibraryDataManager::HandleRevertPending()
{
    int64_t time = MediaFileUtils::UTCTimeSeconds();
    time -= REVERT_DAYS * DAY_HOURS * PER_MINUTE_SECONDS * PER_HOUR_MINUTES;
    if (time < 0) {
        MEDIA_ERR_LOG("the time of revert is error, time=%{public}ld", (long)time);
        return E_INVALID_VALUES;
    }
    MediaLibraryCommand queryCmd(OperationObject::FILESYSTEM_ASSET, OperationType::QUERY);
    queryCmd.GetAbsRdbPredicates()->LessThan(MEDIA_DATA_DB_TIME_PENDING, to_string(time))
        ->And()->GreaterThan(MEDIA_DATA_DB_TIME_PENDING, to_string(0));
    vector<string> columns = { MEDIA_DATA_DB_ID };
    auto result = MediaLibraryObjectUtils::QueryWithCondition(queryCmd, columns);
    if (result == nullptr) {
        return E_HAS_DB_ERROR;
    }

    int32_t ret = E_SUCCESS;
    while (result->GoToNextRow() == NativeRdb::E_OK) {
        int32_t id = GetInt32Val(MEDIA_DATA_DB_ID, result);
        int32_t retVal = RevertPendingByFileId(to_string(id));
        if (retVal != E_SUCCESS) {
            ret = retVal;
            MEDIA_ERR_LOG("Revert file %{public}d failed, ret=%{public}d", id, retVal);
            continue;
        }
    }
    return ret;
}
}  // namespace Media
}  // namespace OHOS
