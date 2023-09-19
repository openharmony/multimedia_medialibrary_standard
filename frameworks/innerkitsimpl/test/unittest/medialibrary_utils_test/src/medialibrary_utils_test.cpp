/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "avmetadatahelper.h"
#include "foundation/ability/form_fwk/test/mock/include/mock_single_kv_store.h"
#include "kvstore.h"
#include "thumbnail_service.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "medialibrary_object_utils.h"
#include "medialibrary_sync_operation.h"
#include "medialibrary_utils_test.h"
#define private public
#include "thumbnail_utils.h"
#undef private

using namespace std;
using namespace OHOS;
using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {

class ConfigTestOpenCall : public NativeRdb::RdbOpenCallback {
public:
    int OnCreate(NativeRdb::RdbStore &rdbStore) override;
    int OnUpgrade(NativeRdb::RdbStore &rdbStore, int oldVersion, int newVersion) override;
    static const string CREATE_TABLE_TEST;
};

const string ConfigTestOpenCall::CREATE_TABLE_TEST = string("CREATE TABLE IF NOT EXISTS test ") +
    "(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, age INTEGER, salary REAL, blobType BLOB)";

int ConfigTestOpenCall::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(CREATE_TABLE_TEST);
}

int ConfigTestOpenCall::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return 0;
}
shared_ptr<NativeRdb::RdbStore> storePtr = nullptr;
void MediaLibraryUtilsTest::SetUpTestCase(void)
{
    const string dbPath = "/data/test/medialibrary_utils_test.db";
    NativeRdb::RdbStoreConfig config(dbPath);
    ConfigTestOpenCall helper;
    int errCode = 0;
    shared_ptr<NativeRdb::RdbStore> store = NativeRdb::RdbHelper::GetRdbStore(config, 1, helper, errCode);
    storePtr = store;
}
void MediaLibraryUtilsTest::TearDownTestCase(void) {}

// SetUp:Execute before each test case
void MediaLibraryUtilsTest::SetUp() {}

void MediaLibraryUtilsTest::TearDown(void) {}

HWTEST_F(MediaLibraryUtilsTest, medialib_UpdateRemotePath_test_001, TestSize.Level0)
{
    string path = "";
    string networkId = "";
    bool ret = ThumbnailUtils::UpdateRemotePath(path, networkId);
    EXPECT_EQ(ret, false);
    path = "medialib_UpdateRemotePath_test_001";
    networkId = "1d3cb099659d53b3ee15faaab3c00a8ff983382ebc8b01aabde039ed084e167b";
    ret = ThumbnailUtils::UpdateRemotePath(path, networkId);
    EXPECT_EQ(ret, false);
    path = MEDIA_DATA_DEVICE_PATH;
    ret = ThumbnailUtils::UpdateRemotePath(path, networkId);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryThumbnailSet_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string row = "medialib_QueryThumbnailSet_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = MEDIALIBRARY_TABLE,
        .row = row
    };

    auto resultSetPtr = ThumbnailUtils::QueryThumbnailSet(opts);
    EXPECT_NE(resultSetPtr, nullptr);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryThumbnailInfo_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string row = "medialib_QueryThumbnailInfo_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = MEDIALIBRARY_TABLE,
        .row = row
    };
    ThumbnailData data;
    int err  = 0;
    auto resultSetPtr = ThumbnailUtils::QueryThumbnailInfo(opts, data, err);
    EXPECT_EQ(resultSetPtr, nullptr);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryLcdCount_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = MEDIALIBRARY_TABLE
    };
    int outLcdCount = 0;
    int err = 0;
    bool ret = ThumbnailUtils::QueryLcdCount(opts, outLcdCount, err);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryDistributeLcdCount_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts = {
        .store = storePtr,
    };
    int outLcdCount = 0;
    int err = 0;
    bool ret = ThumbnailUtils::QueryDistributeLcdCount(opts, outLcdCount, err);
    EXPECT_EQ(ret, true);
}


HWTEST_F(MediaLibraryUtilsTest, medialib_QueryHasLcdFiles_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = MEDIALIBRARY_TABLE;
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = table
    };
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryHasLcdFiles(opts, infos, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryHasThumbnailFiles_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = "medialib_QueryHasThumbnailFiles_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = table
    };
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryHasThumbnailFiles(opts, infos, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryAgingDistributeLcdInfos_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string udid = "medialib_QueryAgingDistributeLcdInfos_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .udid = udid
    };
    int LcdLimit = 0;
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryAgingDistributeLcdInfos(opts, LcdLimit, infos, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryAgingLcdInfos_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = "medialib_QueryAgingLcdInfos_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = table
    };
    int LcdLimit = 0;
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryAgingLcdInfos(opts, LcdLimit, infos, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryNoLcdInfos_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = "medialib_QueryNoLcdInfos_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = table
    };
    int LcdLimit = 0;
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryNoLcdInfos(opts, LcdLimit, infos, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryNoThumbnailInfos_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = "medialib_QueryNoThumbnailInfos_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = table
    };
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryNoThumbnailInfos(opts, infos, err);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_UpdateLcdInfo_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string row = "medialib_UpdateLcdInfo_test_001";
    string table = "medialib_UpdateLcdInfo_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = table,
        .row = row
    };
    ThumbnailData data;
    int err = 0;
    bool ret = ThumbnailUtils::UpdateLcdInfo(opts, data, err);
    EXPECT_EQ(ret, false);
    ret = ThumbnailUtils::UpdateLcdInfo(opts, data, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_UpdateVisitTime_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts = {
        .store = storePtr,
        .networkId = "",
    };
    ThumbnailData data;
    int err = 0;
    bool ret = ThumbnailUtils::UpdateVisitTime(opts, data, err);
    EXPECT_EQ(ret, false);
    opts.networkId = "UpdateVisitTime";
    ret = ThumbnailUtils::UpdateVisitTime(opts, data, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryDeviceThumbnailRecords_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string udid = "medialib_QueryDeviceThumbnailRecords_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .udid = udid
    };
    vector<ThumbnailData> infos;
    int err = 0;
    bool ret = ThumbnailUtils::QueryDeviceThumbnailRecords(opts, infos, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_QueryRemoteThumbnail_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbnailData data;
    ThumbRdbOpt opts = {
    .store = storePtr,
    };
    int err = 0;
    bool ret = ThumbnailUtils::QueryRemoteThumbnail(opts, data, err);
    EXPECT_EQ(ret, false);
    opts.networkId = "medialib_QueryRemoteThumbnail_test";
    ret = ThumbnailUtils::QueryRemoteThumbnail(opts, data, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_CleanThumbnailInfo_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts = {
    .store = storePtr,
    };
    bool withThumb = true;
    bool withLcd = true;
    bool ret = ThumbnailUtils::CleanThumbnailInfo(opts, withThumb, withLcd);
    EXPECT_EQ(ret, false);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryUtilsTest, medialib_SyncPushTable_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = "medialib_SyncPushTable_test_001";
    MediaLibrarySyncOpts syncOpts = {
        .rdbStore = storePtr,
        .table = table,
        .bundleName = "medialib_SyncPushTable_test_001",
    };
    vector<string> devices;
    bool isBlock = true;
    bool ret = MediaLibrarySyncOperation::SyncPushTable(syncOpts, devices, isBlock);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_SyncPullTable_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string table = "medialib_SyncPullTable_test_001";
    MediaLibrarySyncOpts syncOpts = {
        .rdbStore = storePtr,
        .table = table,
        .bundleName = "medialib_SyncPullTable_test_001",
    };
    vector<string> devices;
    bool ret = MediaLibrarySyncOperation::SyncPullTable(syncOpts, devices);
    EXPECT_EQ(ret, false);
}
#endif

HWTEST_F(MediaLibraryUtilsTest, medialib_ResizeImage_test_001, TestSize.Level0)
{
    vector<uint8_t> data;
    Size size;
    unique_ptr<PixelMap> pixelMap = nullptr;
    bool ret = ThumbnailUtils::ResizeImage(data, size, pixelMap);
    EXPECT_EQ(ret, false);
    data.push_back(8);
    ret = ThumbnailUtils::ResizeImage(data, size, pixelMap);
    EXPECT_EQ(ret, false);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryUtilsTest, medialib_WaitFor_test_001, TestSize.Level0)
{
    MediaLibrarySyncCallback mediaLibrary;
    bool ret = mediaLibrary.WaitFor();
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_DeleteLcdData_test_001, TestSize.Level0)
{
    ThumbnailData thumbnailData;
    ThumbRdbOpt opts;
    bool ret = ThumbnailUtils::DeleteLcdData(opts, thumbnailData);
    EXPECT_EQ(ret, false);
    thumbnailData.lcdKey = "DeleteLcdData";
    ret = ThumbnailUtils::DeleteLcdData(opts, thumbnailData);
    EXPECT_EQ(ret, true);
    opts.networkId = "medialib_DeleteLcdData_test_001";
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    opts.kvStore = kvStorePtr;
    ret = ThumbnailUtils::DeleteLcdData(opts, thumbnailData);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_DeleteDistributeLcdData_test_001, TestSize.Level0)
{
    ThumbnailData thumbnailData;
    ThumbRdbOpt opts;
    bool ret = ThumbnailUtils::DeleteDistributeLcdData(opts, thumbnailData);
    EXPECT_EQ(ret, false);
    thumbnailData.lcdKey = "DeleteDistributeLcdData";
    ret = ThumbnailUtils::DeleteDistributeLcdData(opts, thumbnailData);
    EXPECT_EQ(ret, true);
    opts.networkId = "medialib_DeleteDistributeLcdData_test_001";
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    opts.kvStore = kvStorePtr;
    ret = ThumbnailUtils::DeleteDistributeLcdData(opts, thumbnailData);
    EXPECT_EQ(ret, true);
}
#endif

HWTEST_F(MediaLibraryUtilsTest, medialib_DoUpdateRemoteThumbnail_test_001, TestSize.Level0)
{
    ThumbnailData thumbnailData;
    ThumbRdbOpt opts;
    int err = 0;
    bool ret = ThumbnailUtils::DoUpdateRemoteThumbnail(opts, thumbnailData, err);
    EXPECT_EQ(ret, false);
    opts.networkId = "medialib_DoUpdateRemoteThumbnail_test_001";
    ret = ThumbnailUtils::DoUpdateRemoteThumbnail(opts, thumbnailData, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_DeleteOriginImage_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbnailData thumbnailData;
    string row = "medialib_DeleteOriginImage_test_001";
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = MEDIALIBRARY_TABLE,
        .row = row
    };
    bool ret = ThumbnailUtils::DeleteOriginImage(opts);
    EXPECT_EQ(ret, false);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryUtilsTest, medialib_SyncPullKvstore_test_001, TestSize.Level0)
{
    vector<string> thumbnailKeys = {"SyncPullKvstore"};
    auto ret = MediaLibrarySyncOperation::SyncPullKvstore(nullptr, thumbnailKeys, "");
    EXPECT_EQ(ret, DistributedKv::Status::ERROR);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    ret = MediaLibrarySyncOperation::SyncPullKvstore(kvStorePtr, thumbnailKeys, "");
    EXPECT_EQ(ret, DistributedKv::Status::ERROR);
    string networkId = "Kvstore";
    ret = MediaLibrarySyncOperation::SyncPullKvstore(kvStorePtr, thumbnailKeys, networkId);
    EXPECT_EQ(ret, DistributedKv::Status::ERROR);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_SyncPushKvstore_test_001, TestSize.Level0)
{
    vector<string> thumbnailKeys = {"SyncPushKvstore"};
    auto ret = MediaLibrarySyncOperation::SyncPushKvstore(nullptr, thumbnailKeys, "");
    EXPECT_EQ(ret, DistributedKv::Status::ERROR);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    ret = MediaLibrarySyncOperation::SyncPushKvstore(kvStorePtr, thumbnailKeys, "");
    EXPECT_EQ(ret, DistributedKv::Status::ERROR);
    string networkId = "Kvstore";
    ret = MediaLibrarySyncOperation::SyncPushKvstore(kvStorePtr, thumbnailKeys, networkId);
    EXPECT_NE(ret, DistributedKv::Status::ERROR);
}
#endif

HWTEST_F(MediaLibraryUtilsTest, medialib_GetKvResultSet_test_001, TestSize.Level0)
{
    string networkId = "medialib_GetKvResultSet_test_001";
    shared_ptr<DataShare::ResultSetBridge> outResultSet = nullptr;
    bool ret = ThumbnailUtils::GetKvResultSet(nullptr, "", networkId, outResultSet);
    EXPECT_EQ(ret, false);
    string key = "GetKvResultSet";
    ret = ThumbnailUtils::GetKvResultSet(nullptr, key, networkId, outResultSet);
    EXPECT_EQ(ret, false);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    ret = ThumbnailUtils::GetKvResultSet(kvStorePtr, key, networkId, outResultSet);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_RemoveDataFromKv_test_001, TestSize.Level0)
{
    bool ret = ThumbnailUtils::RemoveDataFromKv(nullptr, "");
    EXPECT_EQ(ret, false);
    string key = "RemoveDataFromKv";
    ret = ThumbnailUtils::RemoveDataFromKv(nullptr, key);
    EXPECT_EQ(ret, false);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    ret = ThumbnailUtils::RemoveDataFromKv(kvStorePtr, key);
    EXPECT_EQ(ret, true);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryUtilsTest, medialib_IsImageExist_test_001, TestSize.Level0)
{
    string networkId = "medialib_IsImageExist_test_001";
    bool ret = ThumbnailUtils::IsImageExist("", networkId, nullptr);
    EXPECT_EQ(ret, false);
    string key = "IsImageExist";
    ret = ThumbnailUtils::IsImageExist(key, networkId, nullptr);
    EXPECT_EQ(ret, false);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    ret = ThumbnailUtils::IsImageExist(key, networkId, kvStorePtr);
    EXPECT_EQ(ret, false);
}
#endif

HWTEST_F(MediaLibraryUtilsTest, medialib_compressImage_test_001, TestSize.Level0)
{
    ThumbnailData data;
    data.thumbnail.push_back(0);
    data.source = make_shared<PixelMap>();
    bool ret = ThumbnailUtils::CompressImage(data.source, data.thumbnail);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_LoadSourceImage_test_001, TestSize.Level0)
{
    Size desiredSize;
    desiredSize.width = 20;
    desiredSize.height = 20;
    ThumbnailData data;
    data.source = nullptr;
    data.mediaType = MEDIA_TYPE_VIDEO;
    bool isThumbnail = true;
    bool ret = ThumbnailUtils::LoadSourceImage(data, desiredSize, isThumbnail);
    EXPECT_EQ(ret, false);
    data.mediaType = MEDIA_TYPE_AUDIO;
    ret = ThumbnailUtils::LoadSourceImage(data, desiredSize, isThumbnail);
    EXPECT_EQ(ret, false);
    data.mediaType = MEDIA_TYPE_MEDIA;
    data.path = "Documents/";
    ret = ThumbnailUtils::LoadSourceImage(data, desiredSize, isThumbnail);
    EXPECT_EQ(ret, false);
    shared_ptr<AVMetadataHelper> avMetadataHelper = AVMetadataHelperFactory::CreateAVMetadataHelper();
    data.source = make_shared<PixelMap>();
    ret = ThumbnailUtils::LoadSourceImage(data, desiredSize, isThumbnail);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_setSource_test_001, TestSize.Level0)
{
    int32_t ret = ThumbnailUtils::SetSource(nullptr, "");
    EXPECT_EQ(ret, E_ERR);
    shared_ptr<AVMetadataHelper> avMetadataHelper = AVMetadataHelperFactory::CreateAVMetadataHelper();
    ret = ThumbnailUtils::SetSource(avMetadataHelper, "");
    EXPECT_EQ(ret, E_ERR);
    string path = "//storage/cloud/files";
    ret = ThumbnailUtils::SetSource(avMetadataHelper, path);
    EXPECT_EQ(ret, E_ERR);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_uTCTimeSeconds_test_001, TestSize.Level0)
{
    int64_t ret = ThumbnailUtils::UTCTimeMilliSeconds();
    EXPECT_GT(ret, 0);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_parseQueryResult_test_001, TestSize.Level0)
{
    vector<string> column = {
        REMOTE_THUMBNAIL_DB_FILE_ID,
        MEDIA_DATA_DB_LCD
    };
    ThumbnailData data;
    data.id = "0";
    data.path = "/storage/cloud/files";
    data.mediaType = 0;
    int err = 0;
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = REMOTE_THUMBNAIL_TABLE,
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_LCD);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, opts.udid);
    rdbPredicates.Limit(0);
    rdbPredicates.OrderByAsc(MEDIA_DATA_DB_TIME_VISIT);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    ThumbnailUtils::ParseQueryResult(resultSet, data, err);
    EXPECT_NE(err, 0);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_deleteDistributeThumbnailInfo_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts;
    opts.store = storePtr;
    opts.networkId = "GetUdidByNetworkId";
    opts.udid = "GetUdidByNetworkId";
    string key = "RemoveDataFromKv";
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    opts.kvStore = kvStorePtr;
    ThumbnailUtils::RemoveDataFromKv(opts.kvStore, key);
    bool ret = ThumbnailUtils::DeleteDistributeThumbnailInfo(opts);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_parseStringResult_test_001, TestSize.Level0)
{
    vector<string> column = {
        REMOTE_THUMBNAIL_DB_FILE_ID,
        MEDIA_DATA_DB_LCD
    };
    ThumbRdbOpt opts = {
        .store = storePtr,
    };
    int index = 0;
    string data = "ParseStringResult";
    int err = 0;
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_LCD);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, opts.udid);
    rdbPredicates.Limit(0);
    rdbPredicates.OrderByAsc(MEDIA_DATA_DB_TIME_VISIT);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    string dataTest = "";
    ThumbnailUtils::ParseStringResult(resultSet, -1, dataTest, err);
    EXPECT_GT(err, E_OK);
    ThumbnailUtils::ParseStringResult(resultSet, index, data, err);
    EXPECT_GT(err, E_OK);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_checkResultSetCount_test_001, TestSize.Level0)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_THUMBNAIL,
        MEDIA_DATA_DB_LCD,
        MEDIA_DATA_DB_MEDIA_TYPE,
        MEDIA_DATA_DB_DATE_MODIFIED
    };
    ThumbRdbOpt opts = {
        .store = storePtr,
        .table = MEDIALIBRARY_TABLE,
        .row = "CheckResultSetCount",
    };
    int err = 0;
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_LCD);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, opts.udid);
    rdbPredicates.Limit(0);
    rdbPredicates.OrderByAsc(MEDIA_DATA_DB_TIME_VISIT);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    bool ret = ThumbnailUtils::CheckResultSetCount(nullptr, err);
    EXPECT_EQ(ret, false);
    ret = ThumbnailUtils::CheckResultSetCount(resultSet, err);
    EXPECT_EQ(ret, false);
    auto resultSetTest = ThumbnailUtils::QueryThumbnailSet(opts);
    ret = ThumbnailUtils::CheckResultSetCount(resultSetTest, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_loadImageFile_test_001, TestSize.Level0)
{
    ThumbnailData data;
    bool isThumbnail = false;
    Size desiredSize;
    desiredSize.width = 20;
    desiredSize.height = 20;
    data.path = "/storage/cloud/files";
    data.source = make_shared<PixelMap>();
    bool ret = ThumbnailUtils::LoadImageFile(data, isThumbnail, desiredSize);
    EXPECT_EQ(ret, false);
    ret = ThumbnailUtils::LoadVideoFile(data, isThumbnail, desiredSize);
    EXPECT_EQ(ret, false);
    ret = ThumbnailUtils::LoadAudioFile(data, isThumbnail, desiredSize);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_saveImage_test_001, TestSize.Level0)
{
    vector<uint8_t> image;
    DistributedKv::Status ret = ThumbnailUtils::SaveImage(nullptr, "", image);
    EXPECT_EQ(ret, DistributedKv::Status::ERROR);
    string key = "SaveImage";
    image.push_back(0);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    ret = ThumbnailUtils::SaveImage(kvStorePtr, key, image);
    EXPECT_EQ(ret, DistributedKv::Status::SUCCESS);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_getUdidByNetworkId_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts = {
        .store = storePtr,
        .networkId = "GetUdidByNetworkId",
        .table = MEDIALIBRARY_TABLE,
        .row = "GetUdidByNetworkId",
    };
    opts.store = storePtr;
    string networkIdTest = "";
    string outUdidTest = "";
    int err = 0;
    bool ret = ThumbnailUtils::GetUdidByNetworkId(opts, networkIdTest, outUdidTest, err);
    EXPECT_EQ(ret, false);
    string outUdid = "GetUdidByNetworkId";
    ret = ThumbnailUtils::GetUdidByNetworkId(opts, opts.networkId, outUdid, err);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryUtilsTest, medialib_updateRemoteThumbnailInfo_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    ThumbRdbOpt opts;
    ThumbnailData data;
    int err = 0;
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    opts.kvStore = kvStorePtr;
    opts.store = storePtr;
    opts.networkId = "InsertRemoteThumbnailInfo";
    opts.row = "InsertRemoteThumbnailInfo";
    bool ret = ThumbnailUtils::UpdateRemoteThumbnailInfo(opts, data, err);
    EXPECT_EQ(ret, false);
    data.id = "0";
    data.udid = "InsertRemoteThumbnailInfo";
    opts.store = storePtr;
    ThumbnailUtils::InsertRemoteThumbnailInfo(opts, data, err);
    ret = ThumbnailUtils::UpdateRemoteThumbnailInfo(opts, data, err);
    EXPECT_EQ(ret, false);
    ret = ThumbnailUtils::CleanDistributeLcdInfo(opts);
    EXPECT_EQ(ret, false);
}
} // namespace Media
} // namespace OHOS