/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
#define MLOG_TAG "FileExtUnitTest"

#include <chrono>
#include <thread>
#include "medialibrary_device.h"
#include "medialibrary_rdb_test.h"
#include "context.h"
#include "ability_context_impl.h"
#include "js_runtime.h"
#include "photo_album_column.h"
#include "media_file_utils.h"
#include "medialibrary_sync_operation.h"
#define private public
#include "medialibrary_object_utils.h"
#include "medialibrary_rdbstore.h"
#undef private

using namespace std;
using namespace OHOS;
using namespace testing::ext;

namespace OHOS {
namespace Media {
shared_ptr <MediaLibraryRdbStore> rdbStorePtr = nullptr;
void MediaLibraryRdbTest::SetUpTestCase(void)
{
    auto stageContext = std::make_shared<AbilityRuntime::ContextImpl>();
    auto abilityContextImpl = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    abilityContextImpl->SetStageContext(stageContext);
    rdbStorePtr = std::make_shared<MediaLibraryRdbStore>(abilityContextImpl);
    int32_t ret = rdbStorePtr->Init();
    MEDIA_INFO_LOG("rdbstore start ret = %{public}d", ret);
}
void MediaLibraryRdbTest::TearDownTestCase(void) {}

// SetUp:Execute before each test case
void MediaLibraryRdbTest::SetUp() {}

void MediaLibraryRdbTest::TearDown(void) {}

HWTEST_F(MediaLibraryRdbTest, medialib_Insert_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    cmd.SetTableName(MEDIALIBRARY_TABLE);
    ValuesBucket values;
    string name = "test name";
    values.PutString(MEDIA_DATA_DB_NAME, name);
    string data = "medialib_Insert_test_001";
    values.PutString(MEDIA_DATA_DB_FILE_PATH, data);
    string title = "insert test";
    values.PutString(MEDIA_DATA_DB_TITLE, title);
    int32_t fileId = 3;
    values.PutInt(MEDIA_DATA_DB_ID, fileId);
    cmd.SetValueBucket(values);
    int64_t rowId = 1;
    rdbStorePtr->Init();
    int32_t ret = rdbStorePtr->Insert(cmd, rowId);
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Insert_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    cmd.SetTableName(MEDIALIBRARY_TABLE);
    ValuesBucket values;
    string name = "medialib_Insert_test_002";
    values.PutString(MEDIA_DATA_DB_NAME, name);
    string displayname = "medialib_Insert_test_002/test";
    values.PutString(MEDIA_DATA_DB_NAME, displayname);
    cmd.SetValueBucket(values);
    int64_t rowId = 1;
    int32_t ret = rdbStorePtr->Insert(cmd, rowId);
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Insert_test_003, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    int64_t rowId = 1;
    int32_t ret = rdbStorePtr->Insert(cmd, rowId);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Insert_test_004, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    cmd.SetTableName(MEDIALIBRARY_TABLE);
    ValuesBucket values;
    string name = "medialib_Insert_test_004";
    values.PutString(MEDIA_DATA_DB_NAME, name);
    cmd.SetValueBucket(values);
    int64_t rowId = 1;
    rdbStorePtr->Stop();
    int32_t ret = rdbStorePtr->Insert(cmd, rowId);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Query_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::QUERY);
    vector<string> columns;
    columns.push_back(MEDIA_DATA_DB_RECYCLE_PATH);
    rdbStorePtr->Init();
    auto queryResultSet = rdbStorePtr->Query(cmd, columns);
    EXPECT_NE(queryResultSet, nullptr);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Query_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::QUERY);
    vector<string> columns;
    columns.push_back(MEDIA_DATA_DB_DATE_TRASHED);
    rdbStorePtr->Stop();
    auto queryResultSet = rdbStorePtr->Query(cmd, columns);
    EXPECT_EQ(queryResultSet, nullptr);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Delete_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::DELETE);
    string selection = MEDIA_DATA_DB_ID + " = ? OR "+ MEDIA_DATA_DB_PARENT_ID + " = ?";
    cmd.GetAbsRdbPredicates()->SetWhereClause(selection);
    vector<string> selectionArgs;
    int32_t rowId = 3;
    selectionArgs.push_back(to_string(rowId));
    cmd.GetAbsRdbPredicates()->SetWhereArgs(selectionArgs);
    rdbStorePtr->Init();
    int32_t ret = rdbStorePtr->Delete(cmd, rowId);
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Delete_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Stop();
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::DELETE);
    int32_t rowId = 1;
    int32_t ret = rdbStorePtr->Delete(cmd, rowId);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Update_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::UPDATE);
    ValuesBucket valuesBucket;
    string title = "medialib_Update_test_001";
    valuesBucket.PutString(MEDIA_DATA_DB_TITLE, title);
    cmd.SetValueBucket(valuesBucket);
    int32_t updatedRows = E_HAS_DB_ERROR;
    rdbStorePtr->Init();
    int32_t ret = rdbStorePtr->Update(cmd, updatedRows);
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Update_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::UPDATE);
    int32_t updatedRows = -1;
    int32_t ret = rdbStorePtr->Update(cmd, updatedRows);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Update_test_003, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Stop();
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::UPDATE);
    int32_t updatedRows = E_HAS_DB_ERROR;
    int32_t ret = rdbStorePtr->Update(cmd, updatedRows);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_QuerySql_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Init();
    auto queryResultSet = rdbStorePtr->QuerySql(QUERY_MEDIA_VOLUME);
    EXPECT_NE(queryResultSet, nullptr);
}

HWTEST_F(MediaLibraryRdbTest, medialib_QuerySql_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Stop();
    auto queryResultSet = rdbStorePtr->QuerySql(QUERY_MEDIA_VOLUME);
    EXPECT_EQ(queryResultSet, nullptr);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Transaction_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Init();
    int32_t ret = rdbStorePtr->BeginTransaction();
    EXPECT_EQ(ret, E_OK);
    ret = rdbStorePtr->BeginTransaction();
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::UPDATE);
    ValuesBucket valuesBucket;
    string title = "medialib_Update_test_001";
    valuesBucket.PutString(MEDIA_DATA_DB_TITLE, title);
    cmd.SetValueBucket(valuesBucket);
    int32_t updatedRows = E_HAS_DB_ERROR;
    ret = rdbStorePtr->Update(cmd, updatedRows);
    ret = rdbStorePtr->Commit();
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Transaction_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Stop();
    int32_t ret = rdbStorePtr->BeginTransaction();
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Transaction_test_003, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Init();
    int32_t ret = rdbStorePtr->Commit();
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
    ret = rdbStorePtr->RollBack();
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryRdbTest, medialib_MediaLibraryRdbStoreObserver_test_001, TestSize.Level0)
{
    string bundleName = "medialib_MediaLibraryRdbStoreObserver_test_001";
    std::shared_ptr<MediaLibraryRdbStoreObserver> test = make_shared<MediaLibraryRdbStoreObserver>(bundleName);
    EXPECT_NE(test, nullptr);
}
#endif

HWTEST_F(MediaLibraryRdbTest, medialib_ExecuteSql_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    string modifySql = "UPDATE " + MEDIALIBRARY_TABLE + " SET ";
    int32_t ret = rdbStorePtr->ExecuteSql(modifySql);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_ExecuteSql_test_002, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Stop();
    string modifySql = "UPDATE " + MEDIALIBRARY_TABLE + " SET ";
    int32_t ret = rdbStorePtr->ExecuteSql(modifySql);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_Stop_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    rdbStorePtr->Stop();
    EXPECT_NE(rdbStorePtr, nullptr);
}

inline void PrepareUserAlbum(const string &albumName, const string &relativePath, ValuesBucket &values)
{
    values.PutString(PhotoAlbumColumns::ALBUM_NAME, albumName);
    values.PutInt(PhotoAlbumColumns::ALBUM_TYPE, PhotoAlbumType::USER);
    values.PutInt(PhotoAlbumColumns::ALBUM_SUBTYPE, PhotoAlbumSubType::USER_GENERIC);
    values.PutLong(PhotoAlbumColumns::ALBUM_DATE_MODIFIED, MediaFileUtils::UTCTimeSeconds());

    if (!relativePath.empty()) {
        values.PutString(PhotoAlbumColumns::ALBUM_RELATIVE_PATH, relativePath);
    }
}

HWTEST_F(MediaLibraryRdbTest, medialib_BuildValuesSql_test_001, TestSize.Level0)
{
    if (rdbStorePtr == nullptr) {
        exit(1);
    }
    string sql;
    vector<ValueObject> bindArgs;
    sql.append("INSERT").append(" OR ROLLBACK").append(" INTO ").append(PhotoAlbumColumns::TABLE).append(" ");
    ValuesBucket albumValues;
    PrepareUserAlbum("BuildValuesSql", "Documents/", albumValues);
    MediaLibraryRdbStore::BuildValuesSql(albumValues, bindArgs, sql);
    AbsRdbPredicates predicates(PhotoAlbumColumns::TABLE);
    vector<std::string> columns;
    MediaLibraryRdbStore::BuildQuerySql(predicates, columns, bindArgs, sql);
    int32_t ret = MediaLibraryRdbStore::ExecuteForLastInsertedRowId(sql, bindArgs);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
    rdbStorePtr->Init();
    ret = MediaLibraryRdbStore::DeleteFromDisk(predicates);
    EXPECT_EQ(ret, E_SUCCESS);
    string retTest = MediaLibraryRdbStore::CloudSyncTriggerFunc(columns);
    EXPECT_EQ(retTest, "");
    retTest = MediaLibraryRdbStore::IsCallerSelfFunc(columns);
    EXPECT_EQ(retTest, "true");
    rdbStorePtr->Stop();
    ret = MediaLibraryRdbStore::ExecuteForLastInsertedRowId(sql, bindArgs);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
}

HWTEST_F(MediaLibraryRdbTest, medialib_TransactionOperations_test_001, TestSize.Level0)
{
    MEDIA_INFO_LOG("medialib_TransactionOperations_test_001 begin");
    TransactionOperations trans1;
    rdbStorePtr->Init();
    trans1.rdbStore_ = rdbStorePtr;
    trans1.Finish();
    int32_t ret = trans1.Start();
    EXPECT_EQ(ret, E_OK);
    TransactionOperations trans2;
    trans2.rdbStore_ = rdbStorePtr;
    ret = trans2.Start();
    EXPECT_EQ(ret, E_HAS_DB_ERROR);
    trans1.Finish();
    trans2.Finish();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    rdbStorePtr->Stop();
    MEDIA_INFO_LOG("medialib_TransactionOperations_test_001 end");
}

void TransactionTestFunc(shared_ptr<MediaLibraryRdbStore> rdbStorePtr,
    int* startSignal, int* endSignal, int32_t sleepTimeMs)
{
    TransactionOperations transactionOperations;
    transactionOperations.rdbStore_ = rdbStorePtr;
    int32_t ret = transactionOperations.Start();
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Start failed, ret=%{public}d", ret);
        return;
    }
    (*startSignal)++;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
    transactionOperations.Finish();
    (*endSignal)++;
}

HWTEST_F(MediaLibraryRdbTest, medialib_TransactionOperations_test_002, TestSize.Level0)
{
    // test Transcation success
    MEDIA_INFO_LOG("medialib_TransactionOperations_test_002 begin");
    rdbStorePtr->Init();
    int startSignal = 0;
    int endSignal = 0;
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 0).detach();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 10).detach();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 50).detach();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 100).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(startSignal, 4);
    EXPECT_EQ(endSignal, 4);
    rdbStorePtr->Stop();
    MEDIA_INFO_LOG("medialib_TransactionOperations_test_002 finish");
}

HWTEST_F(MediaLibraryRdbTest, medialib_TransactionOperations_test_003, TestSize.Level0)
{
    // test Transcation failed
    MEDIA_INFO_LOG("medialib_TransactionOperations_test_003 begin");
    int startSignal = 0;
    int endSignal = 0;
    rdbStorePtr->Init();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 3000).detach();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 3000).detach();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 3000).detach();
    thread(TransactionTestFunc, rdbStorePtr, (&startSignal), (&endSignal), 3000).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    EXPECT_EQ(startSignal, 1);
    EXPECT_EQ(endSignal, 1);
    rdbStorePtr->Stop();
    MEDIA_INFO_LOG("medialib_TransactionOperations_test_003 finish");
}

} // namespace Media
} // namespace OHOS