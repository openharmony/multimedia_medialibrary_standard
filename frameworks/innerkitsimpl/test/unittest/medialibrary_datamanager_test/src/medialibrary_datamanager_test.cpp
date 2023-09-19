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
#define MLOG_TAG "FileExtUnitTest"

#include "medialibrary_datamanager_test.h"
#include "fetch_result.h"
#include "get_self_permissions.h"
#include "media_file_ext_ability.h"
#include "media_file_extention_utils.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "medialibrary_unittest_utils.h"
#include "medialibrary_uripermission_operations.h"
#include "uri.h"
#define private public
#include "medialibrary_data_manager.h"
#undef private

using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace Media {

class ArkJsRuntime : public AbilityRuntime::JsRuntime {
public:
    ArkJsRuntime() {};

    ~ArkJsRuntime() {};

    void StartDebugMode(bool needBreakPoint)  {};
    void FinishPreload() {};
    bool LoadRepairPatch(const string& patchFile, const string& baseFile)
    {
        return true;
    };
    bool NotifyHotReloadPage()
    {
        return true;
    };
    bool UnLoadRepairPatch(const string& patchFile)
    {
        return true;
    };
    bool RunScript(const string& path, const string& hapPath, bool useCommonChunk = false)
    {
        return true;
    };
    NativeValue* LoadJsModule(const string& path, const string& hapPath)
    {
        return nullptr;
    };
};

namespace {
    shared_ptr<FileAsset> g_pictures = nullptr;
    shared_ptr<FileAsset> g_download = nullptr;
}

void MediaLibraryDataManagerUnitTest::SetUpTestCase(void)
{
    MediaLibraryUnitTestUtils::Init();
    vector<string> perms = { "ohos.permission.MEDIA_LOCATION" };
    uint64_t tokenId = 0;
    PermissionUtilsUnitTest::SetAccessTokenPermission("MediaLibraryQueryPerfUnitTest", perms, tokenId);
    ASSERT_TRUE(tokenId != 0);
}

void MediaLibraryDataManagerUnitTest::TearDownTestCase(void) {}

void MediaLibraryDataManagerUnitTest::SetUp(void)
{
    MediaLibraryUnitTestUtils::CleanTestFiles();
    MediaLibraryUnitTestUtils::CleanBundlePermission();
    MediaLibraryUnitTestUtils::InitRootDirs();
    g_pictures = MediaLibraryUnitTestUtils::GetRootAsset(TEST_PICTURES);
    g_download = MediaLibraryUnitTestUtils::GetRootAsset(TEST_DOWNLOAD);
    MediaLibraryUnitTestUtils::Init();
}

void MediaLibraryDataManagerUnitTest::TearDown(void) {}

string ReturnUri(string UriType, string MainUri, string SubUri = "")
{
    if (SubUri.empty()) {
        return (UriType + "/" + MainUri);
    } else {
        return (UriType + "/" + MainUri + "/" + SubUri);
    }
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CreateAsset_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CreateAsset_Test_001::Start");
    Uri createAssetUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_FILEOPRN, MEDIA_FILEOPRN_CREATEASSET));
    DataShare::DataShareValuesBucket valuesBucket;
    string relativePath = "Pictures/";
    string displayName = "CreateAsset_Test_001.jpg";
    MediaType mediaType = MEDIA_TYPE_IMAGE;
    valuesBucket.Put(MEDIA_DATA_DB_MEDIA_TYPE, mediaType);
    valuesBucket.Put(MEDIA_DATA_DB_NAME, displayName);
    valuesBucket.Put(MEDIA_DATA_DB_RELATIVE_PATH, relativePath);
    MediaLibraryCommand cmd(createAssetUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_CreateAsset_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CloseAsset_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CloseAsset_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("OpenFile_test_001.jpg", g_pictures, fileAsset), true);
    Uri closeAssetUri(URI_CLOSE_FILE);
    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(MEDIA_DATA_DB_ID, fileAsset->GetId());
    MediaLibraryCommand cmd(closeAssetUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ(retVal, E_SUCCESS);
    MEDIA_INFO_LOG("DataManager_CloseAsset_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CreateAlbum_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CreateAlbum_Test_001::Start");
    Uri createAlbumUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_ALBUMOPRN, MEDIA_ALBUMOPRN_CREATEALBUM));
    DataShare::DataShareValuesBucket valuesBucket;
    string dirPath = ROOT_MEDIA_DIR + "Pictures/CreateAlbum_Test_001/";
    valuesBucket.Put(MEDIA_DATA_DB_NAME, "CreateAlbum_Test_001");
    valuesBucket.Put(MEDIA_DATA_DB_FILE_PATH, dirPath);
    MediaLibraryCommand cmd(createAlbumUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_CreateAlbum_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CreateDir_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CreateDir_Test_001::Start");
    Uri createDirUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_DIROPRN, MEDIA_DIROPRN_FMS_CREATEDIR));
    DataShare::DataShareValuesBucket valuesBucket;
    string relativePath = "Pictures/CreateDir_Test_001/";
    valuesBucket.Put(MEDIA_DATA_DB_RELATIVE_PATH, relativePath);
    MediaLibraryCommand cmd(createDirUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_CreateDir_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashDir_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashDir_Test_001::Start");
    shared_ptr<FileAsset> albumAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateAlbum("TrashDir_Test_001", g_pictures, albumAsset), true);
    Uri trashDirUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_DIROPRN, MEDIA_DIROPRN_FMS_TRASHDIR));
    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(MEDIA_DATA_DB_ID, albumAsset->GetId());
    MediaLibraryCommand cmd(trashDirUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_TrashDir_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_Favorite_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_Favorite_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("Favorite_Test_001.jpg", g_pictures, fileAsset), true);
    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(SMARTALBUMMAP_DB_ALBUM_ID, FAVOURITE_ALBUM_ID_VALUES);
    valuesBucket.Put(SMARTALBUMMAP_DB_CHILD_ASSET_ID, fileAsset->GetId());
    Uri addSmartAlbumUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_SMARTALBUMMAPOPRN,
        MEDIA_SMARTALBUMMAPOPRN_ADDSMARTALBUM));
    MediaLibraryCommand cmd(addSmartAlbumUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    Uri removeSmartAlbumUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_SMARTALBUMMAPOPRN,
        MEDIA_SMARTALBUMMAPOPRN_REMOVESMARTALBUM));
    MediaLibraryCommand removeCmd(removeSmartAlbumUri);
    retVal = MediaLibraryDataManager::GetInstance()->Insert(removeCmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_Favorite_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_Trash_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_Trash_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("Trash_Test_001.jpg", g_pictures, fileAsset), true);
    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(SMARTALBUMMAP_DB_ALBUM_ID, TRASH_ALBUM_ID_VALUES);
    valuesBucket.Put(SMARTALBUMMAP_DB_CHILD_ASSET_ID, fileAsset->GetId());
    Uri addSmartAlbumUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_SMARTALBUMMAPOPRN,
        MEDIA_SMARTALBUMMAPOPRN_ADDSMARTALBUM));
    MediaLibraryCommand cmd(addSmartAlbumUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    Uri removeSmartAlbumUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_SMARTALBUMMAPOPRN,
        MEDIA_SMARTALBUMMAPOPRN_REMOVESMARTALBUM));
    MediaLibraryCommand removeCmd(removeSmartAlbumUri);
    retVal = MediaLibraryDataManager::GetInstance()->Insert(removeCmd, valuesBucket);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_Trash_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_DeleteAsset_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_DeleteAsset_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("DeleteAsset_Test_001.jpg", g_pictures, fileAsset), true);
    Uri deleteAssetUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_FILEOPRN, MEDIA_FILEOPRN_DELETEASSET));
    DataShare::DataSharePredicates predicates;
    predicates.EqualTo(MEDIA_DATA_DB_ID, to_string(fileAsset->GetId()));
    MediaLibraryCommand cmd(deleteAssetUri, Media::OperationType::DELETE);
    auto retVal = MediaLibraryDataManager::GetInstance()->Delete(cmd, predicates);
    EXPECT_EQ(retVal, E_SUCCESS);
    MEDIA_INFO_LOG("DataManager_DeleteAsset_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_QueryDirTable_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_QueryDirTable_Test_001::Start");
    vector<string> columns;
    DataShare::DataSharePredicates predicates;
    string prefix = MEDIA_DATA_DB_MEDIA_TYPE + " <> " + to_string(MEDIA_TYPE_ALBUM);
    predicates.SetWhereClause(prefix);
    Uri queryFileUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIATYPE_DIRECTORY_TABLE));
    int errCode = 0;
    MediaLibraryCommand cmd(queryFileUri, Media::OperationType::QUERY);
    auto resultSet = MediaLibraryDataManager::GetInstance()->Query(cmd, columns, predicates, errCode);
    EXPECT_NE((resultSet == nullptr), true);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_QueryAlbum_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_QueryAlbum_Test_001::Start");
    vector<string> columns;
    DataShare::DataSharePredicates predicates;
    string prefix = MEDIA_DATA_DB_MEDIA_TYPE + " <> " + to_string(MEDIA_TYPE_ALBUM);
    predicates.SetWhereClause(prefix);
    Uri queryFileUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_ALBUMOPRN_QUERYALBUM));
    int errCode = 0;
    MediaLibraryCommand cmd(queryFileUri, Media::OperationType::QUERY);
    auto resultSet = MediaLibraryDataManager::GetInstance()->Query(cmd, columns, predicates, errCode);
    EXPECT_NE((resultSet == nullptr), true);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_QueryVolume_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_QueryVolume_Test_001::Start");
    vector<string> columns;
    DataShare::DataSharePredicates predicates;
    string prefix = MEDIA_DATA_DB_MEDIA_TYPE + " <> " + to_string(MEDIA_TYPE_ALBUM);
    predicates.SetWhereClause(prefix);
    Uri queryFileUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_QUERYOPRN_QUERYVOLUME));
    int errCode = 0;
    MediaLibraryCommand cmd(queryFileUri, Media::OperationType::QUERY);
    auto resultSet = MediaLibraryDataManager::GetInstance()->Query(cmd, columns, predicates, errCode);
    EXPECT_NE((resultSet == nullptr), true);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_QueryFiles_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_QueryFiles_Test_001::Start");
    vector<string> columns;
    DataShare::DataSharePredicates predicates;
    string prefix = MEDIA_DATA_DB_MEDIA_TYPE + " <> " + to_string(MEDIA_TYPE_ALBUM);
    predicates.SetWhereClause(prefix);
    Uri queryFileUri(MEDIALIBRARY_DATA_URI);
    int errCode = 0;
    MediaLibraryCommand cmd(queryFileUri, Media::OperationType::QUERY);
    auto resultSet = MediaLibraryDataManager::GetInstance()->Query(cmd, columns, predicates, errCode);
    EXPECT_NE((resultSet == nullptr), true);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UpdateFileAsset_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UpdateFileAsset_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("UpdateFileAsset_Test_001.jpg", g_pictures, fileAsset), true);
    DataShare::DataShareValuesBucket valuesBucketUpdate;
    valuesBucketUpdate.Put(MEDIA_DATA_DB_MEDIA_TYPE, fileAsset->GetMediaType());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_DATE_MODIFIED, MediaFileUtils::UTCTimeSeconds());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_URI, fileAsset->GetUri());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_NAME, "UpdateAsset_Test_001.jpg");
    valuesBucketUpdate.Put(MEDIA_DATA_DB_TITLE, "UpdateAsset_Test_001");
    valuesBucketUpdate.Put(MEDIA_DATA_DB_RELATIVE_PATH, fileAsset->GetRelativePath());
    DataShare::DataSharePredicates predicates;
    predicates.SetWhereClause(MEDIA_DATA_DB_ID + " = " + to_string(fileAsset->GetId()));
    Uri updateAssetUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_FILEOPRN, MEDIA_FILEOPRN_MODIFYASSET));
    MediaLibraryCommand cmd(updateAssetUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Update(cmd, valuesBucketUpdate, predicates);
    EXPECT_EQ((retVal > 0), true);
    MEDIA_INFO_LOG("DataManager_UpdateFileAsset_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UpdateAlbumAsset_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UpdateAlbumAsset_Test_001::Start");
    shared_ptr<FileAsset> albumAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateAlbum("UpdateAlbumAsset_Test_001", g_pictures, albumAsset), true);
    DataShare::DataSharePredicates predicates;
    string prefix = MEDIA_DATA_DB_ID + " = " + to_string(albumAsset->GetId());
    predicates.SetWhereClause(prefix);
    DataShare::DataShareValuesBucket valuesBucketUpdate;
    valuesBucketUpdate.Put(MEDIA_DATA_DB_MEDIA_TYPE, albumAsset->GetMediaType());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_URI, albumAsset->GetUri());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_RELATIVE_PATH, albumAsset->GetRelativePath());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_NAME, "U" + albumAsset->GetDisplayName());
    MEDIA_INFO_LOG("DataManager_UpdateAlbumAsset_Test_001::GetUri = %{public}s", albumAsset->GetUri().c_str());
    Uri updateAssetUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_ALBUMOPRN, MEDIA_ALBUMOPRN_MODIFYALBUM));
    MediaLibraryCommand cmd(updateAssetUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Update(cmd, valuesBucketUpdate, predicates);
    EXPECT_EQ(retVal, E_SUCCESS);
    MEDIA_INFO_LOG("DataManager_UpdateAlbumAsset_Test_001::retVal = %{public}d. End", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_OpenFile_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_OpenFile_Test_001::Start");
    ArkJsRuntime runtime;
    shared_ptr<MediaFileExtAbility> mediaFileExtAbility = make_shared<MediaFileExtAbility>(runtime);
    Uri fileAsset("");
    shared_ptr<FileAsset> albumAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateAlbum("CreateFile_test_001", g_download, albumAsset), true);
    Uri parentUri(albumAsset->GetUri());
    ASSERT_EQ(mediaFileExtAbility->CreateFile(parentUri, "OpenFile_test_001.jpg", fileAsset), E_SUCCESS);
    for (auto const &mode : MEDIA_OPEN_MODES) {
        string realUri = MediaFileUtils::GetRealUriFromVirtualUri(fileAsset.ToString());
        MediaLibraryCommand cmd(Uri(realUri), OperationType::OPEN);
        int32_t fd = MediaLibraryDataManager::GetInstance()->OpenFile(cmd, mode);
        EXPECT_GT(fd, 0);
        if (fd > 0) {
            close(fd);
        }
        MEDIA_INFO_LOG("DataManager_OpenFile_Test_001 mode: %{public}s, fd: %{public}d.", mode.c_str(), fd);
    }
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_OpenFile_Test_002, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_OpenFile_Test_002::Start");
    Uri fileAsset("");
    ArkJsRuntime runtime;
    Uri parentUri(g_pictures->GetUri());
    shared_ptr<MediaFileExtAbility> mediaFileExtAbility = make_shared<MediaFileExtAbility>(runtime);
    ASSERT_EQ(mediaFileExtAbility->CreateFile(parentUri, "OpenFile_Test_001.jpg", fileAsset), E_SUCCESS);
    MEDIA_INFO_LOG("fileAsset = %{public}s", fileAsset.ToString().c_str());

    string mode = "rt";
    string realUri = MediaFileUtils::GetRealUriFromVirtualUri(fileAsset.ToString());
    MediaLibraryCommand cmd(Uri(realUri), OperationType::OPEN);
    int32_t fd = MediaLibraryDataManager::GetInstance()->OpenFile(cmd, mode);
    EXPECT_LT(fd, 0);
    if (fd > 0) {
        close(fd);
    }
    MEDIA_INFO_LOG("DataManager_OpenFile_Test_002 mode: %{public}s, fd: %{public}d.", mode.c_str(), fd);

    mode = "ra";
    MediaLibraryCommand raCmd(Uri(realUri), OperationType::OPEN);
    fd = MediaLibraryDataManager::GetInstance()->OpenFile(raCmd, mode);
    EXPECT_LT(fd, 0);
    if (fd > 0) {
        close(fd);
    }
    MEDIA_INFO_LOG("DataManager_OpenFile_Test_002 mode: %{public}s, fd: %{public}d.", mode.c_str(), fd);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, Revert_Package_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_Revert_Package_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("Revert_Package_Test_001.jpg", g_pictures, fileAsset), true);
    
    DataShare::DataShareValuesBucket valuesBucketUpdate;
    valuesBucketUpdate.Put(MEDIA_DATA_DB_MEDIA_TYPE, fileAsset->GetMediaType());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_DATE_MODIFIED, MediaFileUtils::UTCTimeSeconds());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_URI, fileAsset->GetUri());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_NAME, "Revert_Package_Test_001.jpg");
    valuesBucketUpdate.Put(MEDIA_DATA_DB_TITLE, "Revert_Package_Test_001");
    valuesBucketUpdate.Put(MEDIA_DATA_DB_RELATIVE_PATH, fileAsset->GetRelativePath());
    int64_t time = MediaFileUtils::UTCTimeSeconds();
    valuesBucketUpdate.Put(MEDIA_DATA_DB_TIME_PENDING, to_string(time));
    DataShare::DataSharePredicates predicates;
    predicates.SetWhereClause(MEDIA_DATA_DB_ID + " = " + to_string(fileAsset->GetId()));
    Uri updateAssetUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_FILEOPRN, MEDIA_FILEOPRN_MODIFYASSET));
    MediaLibraryCommand cmd(updateAssetUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Update(cmd, valuesBucketUpdate, predicates);
    EXPECT_GT(retVal, 0);
    
    string package = fileAsset->GetOwnerPackage();
    MEDIA_INFO_LOG("DataManager_Revert_Package_Test_001 package:%{publc}s", package.c_str());

    MediaLibraryDataManager::GetInstance()->RevertPendingByPackage(package);

    vector<string> columns;
    Uri queryUri(MEDIALIBRARY_DATA_URI);
    MediaLibraryCommand queryCmd(queryUri, OperationType::QUERY);
    int32_t errCode = 0;
    auto resultSetPtr = MediaLibraryDataManager::GetInstance()->QueryRdb(queryCmd, columns, predicates, errCode);
    EXPECT_NE(resultSetPtr, nullptr);
    int count = 0;
    EXPECT_EQ(resultSetPtr->GetRowCount(count), E_OK);

    auto fetchFileResult = make_shared<FetchResult<FileAsset>>();
    for (int32_t row = 0; row < count; row++) {
        unique_ptr<FileAsset> fileAssetObj = fetchFileResult->GetObjectFromRdb(resultSetPtr, row);
        EXPECT_NE(fileAssetObj, nullptr);
        EXPECT_EQ(fileAssetObj->GetTimePending(), 0);

        MediaLibraryCommand deleteCmd(queryUri, Media::OperationType::DELETE);
        int ret =  MediaLibraryDataManager::GetInstance()->Delete(deleteCmd, predicates);
        EXPECT_EQ(ret, E_FAIL);
    }
    MEDIA_INFO_LOG("DataManager_Revert_Package_Test_001::End");
}

HWTEST_F(MediaLibraryDataManagerUnitTest, Revert_BY_DAY_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_Revert_BY_DAY_Test_001::Start");
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("Revert_BY_DAY_Test_001.jpg", g_pictures, fileAsset), true);
    DataShare::DataShareValuesBucket valuesBucketUpdate;
    valuesBucketUpdate.Put(MEDIA_DATA_DB_MEDIA_TYPE, fileAsset->GetMediaType());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_DATE_MODIFIED, MediaFileUtils::UTCTimeSeconds());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_URI, fileAsset->GetUri());
    valuesBucketUpdate.Put(MEDIA_DATA_DB_NAME, "Revert_BY_DAY_Test_001.jpg");
    valuesBucketUpdate.Put(MEDIA_DATA_DB_TITLE, "Revert_BY_DAY_Test_001");
    valuesBucketUpdate.Put(MEDIA_DATA_DB_RELATIVE_PATH, fileAsset->GetRelativePath());
    int64_t time = MediaFileUtils::UTCTimeSeconds() - 8 * 24 * 60 * 60;
    valuesBucketUpdate.Put(MEDIA_DATA_DB_TIME_PENDING, to_string(time));
    DataShare::DataSharePredicates predicates;
    predicates.SetWhereClause(MEDIA_DATA_DB_ID + " = " + to_string(fileAsset->GetId()));
    Uri updateAssetUri(ReturnUri(MEDIALIBRARY_DATA_URI, MEDIA_FILEOPRN, MEDIA_FILEOPRN_MODIFYASSET));
    MediaLibraryCommand cmd(updateAssetUri);
    auto retVal = MediaLibraryDataManager::GetInstance()->Update(cmd, valuesBucketUpdate, predicates);
    EXPECT_GT(retVal, 0);

    MediaLibraryDataManager::GetInstance()->HandleRevertPending();

    vector<string> columns;
    Uri queryUri(MEDIALIBRARY_DATA_URI);
    MediaLibraryCommand queryCmd(queryUri, OperationType::QUERY);
    int32_t errCode = 0;
    auto resultSetPtr = MediaLibraryDataManager::GetInstance()->QueryRdb(queryCmd, columns, predicates, errCode);
    EXPECT_NE(resultSetPtr, nullptr);
    int count = 0;
    EXPECT_EQ(resultSetPtr->GetRowCount(count), E_OK);

    auto fetchFileResult = make_shared<FetchResult<FileAsset>>();
    for (int32_t row = 0; row < count; row++) {
        unique_ptr<FileAsset> fileAssetObj = fetchFileResult->GetObjectFromRdb(resultSetPtr, row);
        EXPECT_NE(fileAssetObj, nullptr);
        EXPECT_EQ(fileAssetObj->GetTimePending(), 0);
        NativeRdb::AbsRdbPredicates absPredicates(MEDIALIBRARY_TABLE);
        absPredicates.SetWhereClause(predicates.GetWhereClause());
        int32_t deletedRows = -1;
        int ret = MediaLibraryDataManager::GetInstance()->rdbStore_->Delete(deletedRows, absPredicates);
        EXPECT_GT(ret, E_FAIL);
    }
    MEDIA_INFO_LOG("DataManager_Revert_BY_DAY_Test_001::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_File_Test_001
 * @tc.name      : test trash and recovery file: normal case
 * @tc.desc      : 1.Create the parent dir: trashRecovery_File_001
 *                 2.Create file1 in trashRecovery_File_001: trashRecovery_File_001/file1
 *                 3.trash file1
 *                 4.recovery file1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_File_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_001::Start");
    shared_ptr<FileAsset> trashRecovery_File_001 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_File_001", g_download, trashRecovery_File_001));
    shared_ptr<FileAsset> file1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", trashRecovery_File_001, file1));

    MediaLibraryUnitTestUtils::TrashFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), false);
    MediaLibraryUnitTestUtils::RecoveryFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_001::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_File_Test_002
 * @tc.name      : test trash and recovery file: there is the same name file in file system when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_File_002
 *                 2.Create file1 in trashRecovery_File_002: trashRecovery_File_002/file1
 *                 3.trash file1
 *                 4.recreate file1
 *                 5.recovery file1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_File_Test_002, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_002::Start");
    shared_ptr<FileAsset> trashRecovery_File_002 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_File_002", g_download, trashRecovery_File_002));
    shared_ptr<FileAsset> file1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", trashRecovery_File_002, file1));

    MediaLibraryUnitTestUtils::TrashFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), false);
    shared_ptr<FileAsset> sameFile = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", trashRecovery_File_002, sameFile));
    MediaLibraryUnitTestUtils::RecoveryFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_002::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_File_Test_003
 * @tc.name      : test trash and recovery file: the parent dir is not existed in db(is_trash != 0) when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_File_003
 *                 2.Create file1 in trashRecovery_File_003: trashRecovery_File_003/file1
 *                 3.trash file1
 *                 4.trash parent dir trashRecovery_File_003
 *                 5.recovery file1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_File_Test_003, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_003::Start");
    shared_ptr<FileAsset> trashRecovery_File_003 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_File_003", g_download, trashRecovery_File_003));
    shared_ptr<FileAsset> file1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", trashRecovery_File_003, file1));

    MediaLibraryUnitTestUtils::TrashFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), false);
    MediaLibraryUnitTestUtils::TrashFile(trashRecovery_File_003);
    MediaLibraryUnitTestUtils::RecoveryFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_003::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_File_Test_004
 * @tc.name      : test trash and recovery file: the parent dir is not existed in db(is_trash != 0) and
 *                                               there is the same name file in file system when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_File_004
 *                 2.Create file1 in trashRecovery_File_004: trashRecovery_File_004/file1
 *                 3.trash file1
 *                 4.trash parent dir trashRecovery_File_004
 *                 5.recreate trashRecovery_File_004
 *                 6.recreate file1
 *                 7.recovery file1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_File_Test_004, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_004::Start");
    shared_ptr<FileAsset> trashRecovery_File_004 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_File_004", g_download, trashRecovery_File_004));
    shared_ptr<FileAsset> file1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", trashRecovery_File_004, file1));

    MediaLibraryUnitTestUtils::TrashFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), false);
    MediaLibraryUnitTestUtils::TrashFile(trashRecovery_File_004);
    shared_ptr<FileAsset> sameParent = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_File_004", g_download, sameParent));
    shared_ptr<FileAsset> sameFile = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", sameParent, sameFile));
    MediaLibraryUnitTestUtils::RecoveryFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_004::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_File_Test_005
 * @tc.name      : test trash and recovery file: the parent dir is not existed in db(is_trash != 0) and
 *                                               there is the same name file in file system when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_File_005
 *                 2.Create file1 in trashRecovery_File_005: trashRecovery_File_005/file1
 *                 3.trash file1
 *                 4.rename parent dir trashRecovery_Dir_005
 *                 5.recovery dir1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_File_Test_005, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_005::Start");
    shared_ptr<FileAsset> trashRecovery_File_005 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_File_005", g_download, trashRecovery_File_005));
    shared_ptr<FileAsset> file1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file1.txt", trashRecovery_File_005, file1));

    MediaLibraryUnitTestUtils::TrashFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), false);
    Uri parent(trashRecovery_File_005->GetUri());
    Uri newUri("");
    ASSERT_EQ(MediaFileExtentionUtils::Rename(parent, "trashRecovery_File_005_renamed.jpg", newUri), E_SUCCESS);
    MediaLibraryUnitTestUtils::RecoveryFile(file1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(file1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_File_Test_005::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_Dir_Test_001
 * @tc.name      : test trash and recovery dir: normal case
 * @tc.desc      : 1.Create dir: trashRecovery_Dir_001
 *                 2.Create childAsset in trashRecovery_Dir_001
 *                 3.trash trashRecovery_Dir_001
 *                 4.recovery trashRecovery_Dir_001
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_Dir_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_001::Start");
    shared_ptr<FileAsset> trashRecovery_Dir_001 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_001", g_download, trashRecovery_Dir_001));
    shared_ptr<FileAsset> childAsset = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file2.txt", trashRecovery_Dir_001, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir2", trashRecovery_Dir_001, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file3.txt", childAsset, childAsset));

    MediaLibraryUnitTestUtils::TrashFile(trashRecovery_Dir_001);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(trashRecovery_Dir_001->GetPath()), false);
    MediaLibraryUnitTestUtils::RecoveryFile(trashRecovery_Dir_001);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(trashRecovery_Dir_001->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_001::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_Dir_Test_002
 * @tc.name      : test trash and recovery dir: there is the same name dir in file system when recovering
 * @tc.desc      : 1.Create dir: trashRecovery_Dir_002
 *                 2.Create childAsset in trashRecovery_Dir_002
 *                 3.trash trashRecovery_Dir_002
 *                 4.recreate trashRecovery_Dir_002
 *                 5.recovery trashRecovery_Dir_002
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_Dir_Test_002, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_002::Start");
    shared_ptr<FileAsset> trashRecovery_Dir_002 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_002", g_download, trashRecovery_Dir_002));
    shared_ptr<FileAsset> childAsset = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file2.txt", trashRecovery_Dir_002, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir2", trashRecovery_Dir_002, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file3.txt", childAsset, childAsset));

    MediaLibraryUnitTestUtils::TrashFile(trashRecovery_Dir_002);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(trashRecovery_Dir_002->GetPath()), false);
    shared_ptr<FileAsset> sameDir = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_002", g_download, sameDir));
    MediaLibraryUnitTestUtils::RecoveryFile(trashRecovery_Dir_002);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(trashRecovery_Dir_002->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_002::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_Dir_Test_003
 * @tc.name      : test trash and recovery dir: the parent dir is not existed in db(is_trash != 0) when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_Dir_003
 *                 2.Create dir1 in trashRecovery_Dir_003: trashRecovery_Dir_003/dir1
 *                 3.Create childAsset in dir1
 *                 4.trash dir1
 *                 5.trash parent dir trashRecovery_Dir_003
 *                 6.recovery dir1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_Dir_Test_003, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_003::Start");
    shared_ptr<FileAsset> trashRecovery_Dir_003 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_003", g_download, trashRecovery_Dir_003));
    shared_ptr<FileAsset> dir1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir1", trashRecovery_Dir_003, dir1));
    shared_ptr<FileAsset> childAsset = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file2.txt", dir1, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir2", dir1, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file3.txt", childAsset, childAsset));

    MediaLibraryUnitTestUtils::TrashFile(dir1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(dir1->GetPath()), false);
    MediaLibraryUnitTestUtils::TrashFile(trashRecovery_Dir_003);
    MediaLibraryUnitTestUtils::RecoveryFile(dir1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(dir1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_003::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_Dir_Test_004
 * @tc.name      : test trash and recovery dir: the parent dir is not existed in db(is_trash != 0) and
 *                                               there is the same name dir in file system when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_Dir_004
 *                 2.Create dir1 in trashRecovery_Dir_004: trashRecovery_Dir_004/dir1
 *                 3.Create childAsset in dir1
 *                 4.trash dir1
 *                 5.trash parent dir trashRecovery_Dir_004
 *                 6.recreate trashRecovery_Dir_004
 *                 7.recreate dir1
 *                 8.recovery dir1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_Dir_Test_004, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_004::Start");
    shared_ptr<FileAsset> trashRecovery_Dir_004 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_004", g_download, trashRecovery_Dir_004));
    shared_ptr<FileAsset> dir1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir1", trashRecovery_Dir_004, dir1));
    shared_ptr<FileAsset> childAsset = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file2.txt", dir1, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir2", dir1, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file3.txt", childAsset, childAsset));

    MediaLibraryUnitTestUtils::TrashFile(dir1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(dir1->GetPath()), false);
    MediaLibraryUnitTestUtils::TrashFile(trashRecovery_Dir_004);
    shared_ptr<FileAsset> sameParent = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_004", g_download, sameParent));
    shared_ptr<FileAsset> sameDir = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir1", sameParent, sameDir));
    MediaLibraryUnitTestUtils::RecoveryFile(dir1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(dir1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_004::End");
}

/**
 * @tc.number    : DataManager_TrashRecovery_Dir_Test_005
 * @tc.name      : test trash and recovery dir: the parent dir is renamed when recovering
 * @tc.desc      : 1.Create the parent dir: trashRecovery_Dir_005
 *                 2.Create dir1 in trashRecovery_Dir_005: trashRecovery_Dir_005/dir1
 *                 3.Create childAsset in dir1
 *                 4.trash dir1
 *                 5.rename parent dir trashRecovery_Dir_005
 *                 6.recovery dir1
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_TrashRecovery_Dir_Test_005, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_005::Start");
    shared_ptr<FileAsset> trashRecovery_Dir_005 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("trashRecovery_Dir_005", g_download, trashRecovery_Dir_005));
    shared_ptr<FileAsset> dir1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir1", trashRecovery_Dir_005, dir1));
    shared_ptr<FileAsset> childAsset = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file2.txt", dir1, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir2", dir1, childAsset));
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file3.txt", childAsset, childAsset));

    MediaLibraryUnitTestUtils::TrashFile(dir1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(dir1->GetPath()), false);
    Uri parent(trashRecovery_Dir_005->GetUri());
    Uri newUri("");
    ASSERT_EQ(MediaFileExtentionUtils::Rename(parent, "trashRecovery_Dir_005_renamed.png", newUri), E_SUCCESS);
    MediaLibraryUnitTestUtils::RecoveryFile(dir1);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(dir1->GetPath()), true);
    MEDIA_INFO_LOG("DataManager_TrashRecovery_Dir_Test_005::End");
}

/**
 * @tc.number    : DataManager_Delete_Dir_Test_001
 * @tc.name      : test delete dir: normal case
 * @tc.desc      : 1.Create the parent dir to be deleted: delete_Dir_001
 *                 2.Create dir1 in delete_Dir_001: delete_Dir_001/dir1
 *                 3.Create file2 in dir1: delete_Dir_001/dir1/file2.txt
 *                 4.Create dir2 in dir1: delete_Dir_001/dir1/dir2
 *                 5.Create file3 in dir2: delete_Dir_001/dir1/dir2/file3.txt
 *                 6.Delete delete_Dir_001
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_Delete_Dir_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_001::Start");
    shared_ptr<FileAsset> delete_Dir_001 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("delete_Dir_001", g_download, delete_Dir_001));
    shared_ptr<FileAsset> dir1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("delete_Dir_001", delete_Dir_001, dir1));
    shared_ptr<FileAsset> file2 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("delete_Dir_001.txt", dir1, file2));
    shared_ptr<FileAsset> dir2 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("delete_Dir_001", dir1, dir2));
    shared_ptr<FileAsset> file3 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("delete_Dir_001.txt", dir2, file3));

    string deleteUri = MEDIALIBRARY_DATA_URI + "/" + MEDIA_FILEOPRN + "/" + MEDIA_FILEOPRN_DELETEASSET;
    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_001::deleteUri: %s", deleteUri.c_str());
    Uri deleteAssetUri(deleteUri);
    DataShare::DataSharePredicates predicates;
    predicates.EqualTo(MEDIA_DATA_DB_ID, to_string(delete_Dir_001->GetId()));
    MediaLibraryCommand cmd(deleteAssetUri, Media::OperationType::DELETE);
    int retVal = MediaLibraryDataManager::GetInstance()->Delete(cmd, predicates);
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(delete_Dir_001->GetPath()), false);
    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_001::delete end, retVal: %d", retVal);
}

/**
 * @tc.number    : DataManager_Delete_Dir_Test_002
 * @tc.name      : test delete dir: exists trashed children
 * @tc.desc      : 1.Create the parent dir to be deleted: delete_Dir_002
 *                 2.Create dir in delete_Dir_002: delete_Dir_002/dir1
 *                 3.Create file in dir1: delete_Dir_002/dir1/file2.txt
 *                 4.Create dir in dir1: delete_Dir_002/dir1/dir2
 *                 5.Create file in dir2: delete_Dir_002/dir1/dir2/file3.txt
 *                 6.Trash file3.txt
 *                 7.Trash dir1
 *                 8.Trash delete_Dir_002
 *                 9.Delete delete_Dir_002
 */
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_Delete_Dir_Test_002, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_002::Start");
    shared_ptr<FileAsset> delete_Dir_002 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("delete_Dir_002", g_download, delete_Dir_002));
    shared_ptr<FileAsset> dir1 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir1", delete_Dir_002, dir1));
    shared_ptr<FileAsset> file2 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file2.txt", dir1, file2));
    shared_ptr<FileAsset> dir2 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateAlbum("dir2", dir1, dir2));
    shared_ptr<FileAsset> file3 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("file3.txt", dir2, file3));

    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_002::trash start");
    MediaLibraryUnitTestUtils::TrashFile(file3);
    MediaLibraryUnitTestUtils::TrashFile(dir1);
    MediaLibraryUnitTestUtils::TrashFile(delete_Dir_002);

    string deleteUri = MEDIALIBRARY_DATA_URI + "/" + MEDIA_FILEOPRN + "/" + MEDIA_FILEOPRN_DELETEASSET;
    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_002::deleteUri: %s", deleteUri.c_str());
    Uri deleteAssetUri(deleteUri);
    DataShare::DataSharePredicates predicates;
    predicates.EqualTo(MEDIA_DATA_DB_ID, to_string(delete_Dir_002->GetId()));
    MediaLibraryCommand cmd(deleteAssetUri, Media::OperationType::DELETE);
    int retVal = MediaLibraryDataManager::GetInstance()->Delete(cmd, {});
    EXPECT_EQ(MediaLibraryUnitTestUtils::IsFileExists(delete_Dir_002->GetPath()), false);
    MEDIA_INFO_LOG("DataManager_Delete_Dir_Test_002::delete end, retVal: %d", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UriPermission_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_001::Start");
    shared_ptr<FileAsset> UriPermission001 = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("UriPermission001.txt", g_download, UriPermission001));

    int32_t fileId = UriPermission001->GetId();
    string bundleName = BUNDLE_NAME;
    int32_t tableType = static_cast<int32_t>(TableType::TYPE_FILES);
    for (const auto &mode : MEDIA_OPEN_MODES) {
        EXPECT_EQ(MediaLibraryUnitTestUtils::GrantUriPermission(fileId, bundleName, mode, tableType), E_SUCCESS);
    }

    vector<string> columns;
    DataShare::DataSharePredicates predicates;
    predicates.EqualTo(PERMISSION_FILE_ID, to_string(fileId))->And()->EqualTo(PERMISSION_BUNDLE_NAME, bundleName);
    Uri queryUri(MEDIALIBRARY_BUNDLEPERM_URI);
    int errCode = 0;
    MediaLibraryCommand cmd(queryUri, Media::OperationType::QUERY);
    auto resultSet = MediaLibraryDataManager::GetInstance()->Query(cmd, columns, predicates, errCode);
    ASSERT_NE(resultSet, nullptr);
    int count = -1;
    ASSERT_EQ(resultSet->GetRowCount(count), E_OK);
    EXPECT_EQ(count, 0);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UriPermission_Test_002, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_002::Start");
    Uri addPermission(ReturnUri(MEDIALIBRARY_BUNDLEPERM_URI, BUNDLE_PERMISSION_INSERT));
    DataShare::DataShareValuesBucket values;
    MediaLibraryCommand cmd(addPermission);
    int retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, values);
    EXPECT_EQ(retVal, E_INVALID_VALUES);
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_002::ret: %d", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UriPermission_Test_003, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_003::Start");
    Uri addPermission(ReturnUri(MEDIALIBRARY_BUNDLEPERM_URI, BUNDLE_PERMISSION_INSERT));
    DataShare::DataShareValuesBucket values;
    values.Put(PERMISSION_FILE_ID, 1);
    MediaLibraryCommand cmd(addPermission);
    int retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, values);
    EXPECT_EQ(retVal, E_INVALID_VALUES);
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_003::ret: %d", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UriPermission_Test_004, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_004::Start");
    Uri addPermission(ReturnUri(MEDIALIBRARY_BUNDLEPERM_URI, BUNDLE_PERMISSION_INSERT));
    DataShare::DataShareValuesBucket values;
    values.Put(PERMISSION_FILE_ID, 1);
    values.Put(PERMISSION_BUNDLE_NAME, BUNDLE_NAME);
    MediaLibraryCommand cmd(addPermission);
    int retVal = MediaLibraryDataManager::GetInstance()->Insert(cmd, values);
    EXPECT_EQ(retVal, E_INVALID_VALUES);
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_004::ret: %d", retVal);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_UriPermission_Test_005, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_UriPermission_Test_005::Start");
    int32_t fileId = 1;
    string bundleName = BUNDLE_NAME;
    string mode = "ra";
    int32_t tableType = static_cast<int32_t>(TableType::TYPE_FILES);
    EXPECT_EQ(MediaLibraryUnitTestUtils::GrantUriPermission(fileId, bundleName, mode, tableType), E_INVALID_MODE);

    mode = "rt";
    EXPECT_EQ(MediaLibraryUnitTestUtils::GrantUriPermission(fileId, bundleName, mode, tableType), E_INVALID_MODE);

    fileId = -1;
    bundleName = "";
    mode = MEDIA_FILEMODE_READONLY;
    EXPECT_EQ(MediaLibraryUnitTestUtils::GrantUriPermission(fileId, bundleName, mode, tableType), E_SUCCESS);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CheckUriPermission_Test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CheckUriPermission_Test_001::Start");
    Uri fileAsset("");
    ArkJsRuntime runtime;
    Uri parentUri(g_download->GetUri());
    shared_ptr<MediaFileExtAbility> mediaFileExtAbility = make_shared<MediaFileExtAbility>(runtime);
    ASSERT_EQ(mediaFileExtAbility->CreateFile(parentUri, "OpenFile_Test_001.jpg", fileAsset), E_SUCCESS);
    unordered_map<string, int32_t> expect {
        { MEDIA_FILEMODE_READONLY, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_WRITEONLY, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_READWRITE, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_WRITETRUNCATE, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_WRITEAPPEND, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_READWRITETRUNCATE, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_READWRITEAPPEND, E_PERMISSION_DENIED },
    };
    for (const auto &inputMode : MEDIA_OPEN_MODES) {
        auto ret = UriPermissionOperations::CheckUriPermission(fileAsset.ToString(), inputMode);
        EXPECT_EQ(ret, expect[inputMode]);
    }
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CheckUriPermission_Test_002, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CheckUriPermission_Test_002::Start");
    Uri fileAsset("");
    ArkJsRuntime runtime;
    Uri parentUri(g_download->GetUri());
    shared_ptr<MediaFileExtAbility> mediaFileExtAbility = make_shared<MediaFileExtAbility>(runtime);
    ASSERT_EQ(mediaFileExtAbility->CreateFile(parentUri, "OpenFile_Test_002.jpg", fileAsset), E_SUCCESS);
    unordered_map<string, int32_t> expect {
        { MEDIA_FILEMODE_READONLY, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_WRITEONLY, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_READWRITE, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_WRITETRUNCATE, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_WRITEAPPEND, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_READWRITETRUNCATE, E_PERMISSION_DENIED },
        { MEDIA_FILEMODE_READWRITEAPPEND, E_PERMISSION_DENIED },
    };
    for (const auto &inputMode : MEDIA_OPEN_MODES) {
        auto ret = UriPermissionOperations::CheckUriPermission(fileAsset.ToString(), inputMode);
        EXPECT_EQ(ret, expect[inputMode]);
    }
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CheckUriPermission_Test_003, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CheckUriPermission_Test_003::Start");
    shared_ptr<FileAsset> file = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("CheckUriPermission003.txt", g_download, file));

    int32_t fileId = file->GetId();
    string bundleName = BUNDLE_NAME;
    string mode = MEDIA_FILEMODE_READWRITE;
    int32_t tableType = static_cast<int32_t>(TableType::TYPE_FILES);
    EXPECT_EQ(MediaLibraryUnitTestUtils::GrantUriPermission(fileId, bundleName, mode, tableType), E_SUCCESS);

    string uri = MediaFileUtils::GetFileMediaTypeUri(MEDIA_TYPE_FILE, "") + SLASH_CHAR + to_string(fileId);
    for (const auto &inputMode : MEDIA_OPEN_MODES) {
        auto ret = UriPermissionOperations::CheckUriPermission(uri, inputMode);
        EXPECT_EQ(ret, E_PERMISSION_DENIED);
        MEDIA_ERR_LOG("CheckUriPermission permissionMode: %{public}s, inputMode: %{public}s, ret: %{public}d",
            mode.c_str(), inputMode.c_str(), ret);
    }
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CheckUriPermission_Test_004, TestSize.Level0)
{
    MEDIA_INFO_LOG("DataManager_CheckUriPermission_Test_004::Start");
    shared_ptr<FileAsset> file = nullptr;
    ASSERT_TRUE(MediaLibraryUnitTestUtils::CreateFile("CheckUriPermission004.txt", g_download, file));

    int32_t fileId = file->GetId();
    string bundleName = BUNDLE_NAME;
    string mode = "Rw";
    int32_t tableType = static_cast<int32_t>(TableType::TYPE_FILES);
    EXPECT_EQ(MediaLibraryUnitTestUtils::GrantUriPermission(fileId, bundleName, mode, tableType), E_SUCCESS);

    string uri = MediaFileUtils::GetFileMediaTypeUri(MEDIA_TYPE_FILE, "") + SLASH_CHAR + to_string(fileId);
    string inputMode = "rWt";
    auto ret = UriPermissionOperations::CheckUriPermission(uri, inputMode);
    EXPECT_EQ(ret, E_PERMISSION_DENIED);
    MEDIA_ERR_LOG("CheckUriPermission permissionMode: %{public}s, inputMode: %{public}s, ret: %{public}d",
        mode.c_str(), inputMode.c_str(), ret);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_LcdDistributeAging_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int32_t ret = mediaLibraryDataManager->LcdDistributeAging();
    EXPECT_EQ(ret, E_OK);
    shared_ptr<OHOS::AbilityRuntime::Context> extensionContext;
    mediaLibraryDataManager->InitialiseThumbnailService(extensionContext);
    ret = mediaLibraryDataManager->LcdDistributeAging();
    EXPECT_EQ(ret, E_OK);
}
#endif

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_GetDirQuerySetMap_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    mediaLibraryDataManager->GetDirQuerySetMap();
    shared_ptr<MediaDataShareExtAbility> datashareExternsion =  nullptr;
    mediaLibraryDataManager->SetOwner(datashareExternsion);
    auto ret = mediaLibraryDataManager->GetOwner();
    EXPECT_EQ(ret, nullptr);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_CreateThumbnailAsync_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    string uri = "";
    mediaLibraryDataManager->CreateThumbnailAsync(uri, "Download");
    EXPECT_NE(mediaLibraryDataManager->thumbnailService_, nullptr);
    string uriTest = "CreateThumbnailAsync";
    mediaLibraryDataManager->CreateThumbnailAsync(uriTest, "Download");
    EXPECT_NE(mediaLibraryDataManager->thumbnailService_, nullptr);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_MakeDirQuerySetMap_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int32_t ret = mediaLibraryDataManager->MakeDirQuerySetMap(MediaLibraryDataManager::dirQuerySetMap_);
    EXPECT_EQ(ret, E_SUCCESS);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_DoTrashAging_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int32_t ret = mediaLibraryDataManager->DoTrashAging();
    EXPECT_EQ(ret, E_SUCCESS);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_DoAging_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int32_t ret = mediaLibraryDataManager->DoAging();
    EXPECT_EQ(ret, E_OK);
    shared_ptr<OHOS::AbilityRuntime::Context> extensionContext;
    mediaLibraryDataManager->InitialiseThumbnailService(extensionContext);
    mediaLibraryDataManager->GenerateThumbnails();
    ret = mediaLibraryDataManager->DoAging();
    EXPECT_EQ(ret, E_OK);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_DistributeDeviceAging_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int32_t ret = mediaLibraryDataManager->DistributeDeviceAging();
    EXPECT_EQ(ret, E_FAIL);
    shared_ptr<OHOS::AbilityRuntime::Context> extensionContext;
    mediaLibraryDataManager->InitialiseThumbnailService(extensionContext);
    ret = mediaLibraryDataManager->DistributeDeviceAging();
    EXPECT_EQ(ret, E_FAIL);
}
#endif

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_NeedQuerySync_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    string networkId = "";
    OperationObject oprnObject = OperationObject::SMART_ALBUM;
    mediaLibraryDataManager->NeedQuerySync(networkId, oprnObject);
    string networkIdTest = "NeedQuerySync";
    mediaLibraryDataManager->NeedQuerySync(networkIdTest, oprnObject);
    oprnObject = OperationObject::SMART_ALBUM_MAP;
    mediaLibraryDataManager->NeedQuerySync(networkIdTest, oprnObject);
    oprnObject = OperationObject::FILESYSTEM_PHOTO;
    mediaLibraryDataManager->NeedQuerySync(networkIdTest, oprnObject);
    oprnObject = OperationObject::FILESYSTEM_AUDIO;
    mediaLibraryDataManager->NeedQuerySync(networkIdTest, oprnObject);
    oprnObject = OperationObject::PHOTO_ALBUM;
    mediaLibraryDataManager->NeedQuerySync(networkIdTest, oprnObject);
    EXPECT_NE(networkIdTest, "");
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_SolveInsertCmd_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    MediaLibraryCommand cmdOne(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    int32_t ret = mediaLibraryDataManager->SolveInsertCmd(cmdOne);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
    MediaLibraryCommand cmdTwo(OperationObject::FILESYSTEM_PHOTO, OperationType::OPEN);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdTwo);
    EXPECT_EQ(ret, E_ERR);
    MediaLibraryCommand cmdThree(OperationObject::FILESYSTEM_AUDIO, OperationType::OPEN);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdThree);
    EXPECT_EQ(ret, E_ERR);
    MediaLibraryCommand cmdFour(OperationObject::FILESYSTEM_ALBUM, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdFour);
    EXPECT_EQ(ret, E_INVALID_PATH);
    MediaLibraryCommand cmdFive(OperationObject::PHOTO_ALBUM, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdFive);
    EXPECT_NE(ret, E_OK);
    MediaLibraryCommand cmdSix(OperationObject::FILESYSTEM_DIR, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdSix);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_SolveInsertCmd_Test_002, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    MediaLibraryCommand cmdOne(OperationObject::SMART_ALBUM, OperationType::CREATE);
    int32_t ret = mediaLibraryDataManager->SolveInsertCmd(cmdOne);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
    MediaLibraryCommand cmdTwo(OperationObject::SMART_ALBUM_MAP, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdTwo);
    EXPECT_EQ(ret, E_SMARTALBUM_IS_NOT_EXISTED);
    MediaLibraryCommand cmdThree(OperationObject::THUMBNAIL, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdThree);
    EXPECT_EQ(ret, E_FAIL);
    MediaLibraryCommand cmdFour(OperationObject::BUNDLE_PERMISSION, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdFour);
    EXPECT_EQ(ret, E_FAIL);
    MediaLibraryCommand cmdFive(OperationObject::UNKNOWN_OBJECT, OperationType::CREATE);
    ret = mediaLibraryDataManager->SolveInsertCmd(cmdFive);
    EXPECT_EQ(ret, E_FAIL);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_SetCmdBundleAndDevice_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    MediaLibraryCommand cmd(Uri(MEDIALIBRARY_BUNDLEPERM_URI), OperationType::QUERY);
    int32_t ret = mediaLibraryDataManager->SetCmdBundleAndDevice(cmd);
    EXPECT_EQ(ret, E_GET_CLIENTBUNDLE_FAIL);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_GetThumbnail_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    string uri = "GetThumbnail";
    auto ret = mediaLibraryDataManager->GetThumbnail(uri);
    EXPECT_LT(ret, 0);
    shared_ptr<OHOS::AbilityRuntime::Context> extensionContext;
    mediaLibraryDataManager->InitialiseThumbnailService(extensionContext);
    ret = mediaLibraryDataManager->GetThumbnail(uri);
    EXPECT_LT(ret, 0);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_BatchInsert_Test_001, TestSize.Level0)
{
    Uri uri("");
    vector<DataShare::DataShareValuesBucket> values;
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    MediaLibraryCommand cmd(uri);
    int32_t ret = mediaLibraryDataManager->BatchInsert(cmd, values);
    EXPECT_EQ(ret, E_INVALID_URI);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_BatchInsert_Test_002, TestSize.Level0)
{
    Uri uri(UFM_PHOTO_ALBUM_ADD_ASSET);
    vector<DataShare::DataShareValuesBucket> values;
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    MediaLibraryCommand cmd(uri);
    int32_t ret = mediaLibraryDataManager->BatchInsert(cmd, values);
    EXPECT_EQ(ret, E_SUCCESS);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_BatchInsert_Test_003, TestSize.Level0)
{
    Uri uri(MEDIALIBRARY_DATA_URI);
    vector<DataShare::DataShareValuesBucket> values;
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    MediaLibraryCommand cmd(uri);
    int32_t ret = mediaLibraryDataManager->BatchInsert(cmd, values);
    EXPECT_EQ(ret, E_SUCCESS);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_QueryRdb_Test_001, TestSize.Level0)
{
    Uri uri("");
    MediaLibraryCommand cmd(uri, OperationType::QUERY);
    vector<string> columns;
    DataShare::DataSharePredicates predicates;
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int errCode = 0;
    auto ret = mediaLibraryDataManager->QueryRdb(cmd, columns, predicates, errCode);
    EXPECT_NE(ret, nullptr);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_GetType_Test_001, TestSize.Level0)
{
    Uri uri(MEDIALIBRARY_DATA_URI);
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    auto ret = mediaLibraryDataManager->GetType(uri);
    EXPECT_EQ(ret, "");
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_NotifyChange_Test_001, TestSize.Level0)
{
    Uri uri(MEDIALIBRARY_DATA_URI);
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    mediaLibraryDataManager->NotifyChange(uri);
    EXPECT_EQ(mediaLibraryDataManager->extension_, nullptr);
}

HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_GenerateThumbnails_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    int32_t ret = mediaLibraryDataManager->GenerateThumbnails();
    EXPECT_EQ(ret, NativeRdb::E_EMPTY_VALUES_BUCKET);
    mediaLibraryDataManager->ClearMediaLibraryMgr();
    ret = mediaLibraryDataManager->GenerateThumbnails();
    EXPECT_EQ(ret, NativeRdb::E_EMPTY_VALUES_BUCKET);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryDataManagerUnitTest, DataManager_QuerySync_Test_001, TestSize.Level0)
{
    auto mediaLibraryDataManager = MediaLibraryDataManager::GetInstance();
    string networkId = "";
    string tableName = "";
    bool ret = mediaLibraryDataManager->QuerySync(networkId, tableName);
    EXPECT_EQ(ret, false);
    string networkIdTest = "QuerySync";
    string tableNameTest = "QuerySync";
    ret = mediaLibraryDataManager->QuerySync(networkIdTest, tableNameTest);
    EXPECT_EQ(ret, false);
}
#endif
} // namespace Media
} // namespace OHOS
