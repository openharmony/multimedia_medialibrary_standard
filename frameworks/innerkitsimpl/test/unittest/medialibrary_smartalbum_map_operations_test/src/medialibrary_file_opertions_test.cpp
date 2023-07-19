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

#include "medialibrary_smartalbum_map_operations_test.h"
#include "medialibrary_unistore_manager.h"
#include "ability_context_impl.h"
#include "uri.h"
#define private public
#include "medialibrary_file_operations.h"
#undef private

using namespace std;
using namespace OHOS;
using namespace testing::ext;

namespace OHOS {
namespace Media {

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_HandleFileOperation_test_001, TestSize.Level0)
{
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    int32_t ret = MediaLibraryFileOperations::HandleFileOperation(cmd);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);

    MediaLibraryCommand cmd1(OperationObject::FILESYSTEM_ASSET, OperationType::CLOSE);
    ret = MediaLibraryFileOperations::HandleFileOperation(cmd1);
    EXPECT_EQ(ret, E_INVALID_FILEID);

    MediaLibraryCommand cmd2(OperationObject::FILESYSTEM_ASSET, OperationType::GETCAPACITY);
    ret = MediaLibraryFileOperations::HandleFileOperation(cmd2);
    EXPECT_EQ(ret, E_FAIL);

    MediaLibraryCommand cmd3(OperationObject::FILESYSTEM_ASSET, OperationType::COPY);
    ret = MediaLibraryFileOperations::HandleFileOperation(cmd3);
    EXPECT_EQ(ret, E_INVALID_URI);
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_CreateFileOperation_test_001, TestSize.Level0)
{
    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);

    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CREATE);
    int32_t ret = MediaLibraryFileOperations::CreateFileOperation(cmd);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);

    ValuesBucket values;
    cmd.SetValueBucket(values);
    ret = MediaLibraryFileOperations::CreateFileOperation(cmd);
    EXPECT_EQ(ret, E_HAS_DB_ERROR);

    values.PutString(MEDIA_DATA_DB_NAME, "medialib_CopyAsset_test_001");
    values.PutString(MEDIA_DATA_DB_NAME, "testCase");
    values.PutInt("media_type", 0);
    cmd.SetValueBucket(values);
    ret = MediaLibraryFileOperations::CreateFileOperation(cmd);
    EXPECT_EQ(ret, E_INVALID_ARGUMENTS);

    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_CloseFileOperation_test_001, TestSize.Level0)
{
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::CLOSE);
    int32_t ret = MediaLibraryFileOperations::CloseFileOperation(cmd);
    EXPECT_EQ(ret, E_INVALID_FILEID);

    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);
    string queryUri = MEDIALIBRARY_DATA_URI;
    Uri uri(queryUri);
    ValuesBucket values;
    values.PutInt("file_id", -1);
    MediaLibraryCommand cmd1(OperationObject::FILESYSTEM_ASSET, OperationType::CLOSE, values);
    ret = MediaLibraryFileOperations::CloseFileOperation(cmd1);
    EXPECT_EQ(ret, E_INVALID_FILEID);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_ModifyFileOperation_test_001, TestSize.Level0)
{
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::GETCAPACITY);
    int32_t ret = MediaLibraryFileOperations::ModifyFileOperation(cmd);
    EXPECT_EQ(ret, E_INVALID_FILEID);

    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);

    ValuesBucket values;
    values.PutInt("file_id", 1);
    values.PutString(MEDIA_DATA_DB_NAME, "medialibrary_ModifyFileOperation_test_001");
    MediaLibraryCommand cmd3(OperationObject::FILESYSTEM_ASSET, OperationType::GETCAPACITY, values);
    cmd3.SetValueBucket(values);
    ret = MediaLibraryFileOperations::ModifyFileOperation(cmd3);
    EXPECT_EQ(ret, E_INVALID_FILEID);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_GetAlbumCapacityOperation_test_001, TestSize.Level0)
{
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::GETCAPACITY);
    int32_t ret = MediaLibraryFileOperations::GetAlbumCapacityOperation(cmd);
    EXPECT_EQ(ret, E_FAIL);

    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);
    ValuesBucket values;
    MediaLibraryCommand cmd1(OperationObject::FILESYSTEM_ASSET, OperationType::GETCAPACITY, values);
    ret = MediaLibraryFileOperations::GetAlbumCapacityOperation(cmd1);
    EXPECT_EQ(ret, E_FAIL);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_CopyFileOperation_test_001, TestSize.Level0)
{
    MediaLibraryCommand cmd(OperationObject::FILESYSTEM_ASSET, OperationType::COPY);
    int32_t ret = MediaLibraryFileOperations::CopyFileOperation(cmd);
    EXPECT_EQ(ret, E_INVALID_URI);
    
    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);
    ValuesBucket values;
    MediaLibraryCommand cmd1(OperationObject::FILESYSTEM_ASSET, OperationType::GETCAPACITY, values);
    ret = MediaLibraryFileOperations::GetAlbumCapacityOperation(cmd1);
    EXPECT_EQ(ret, E_FAIL);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_QueryFileOperation_test_001, TestSize.Level0)
{
    string queryUri = MEDIALIBRARY_DATA_URI;
    Uri uri(queryUri);
    MediaLibraryCommand cmd(uri, OperationType::QUERY);
    vector<string> columns;
    auto resultset = MediaLibraryFileOperations::QueryFileOperation(cmd, columns);
    EXPECT_EQ((resultset != nullptr), false);

    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);
    columns = { MEDIA_DATA_DB_THUMBNAIL, MEDIA_DATA_DB_LCD };
    MediaLibraryCommand cmd1(uri, OperationType::QUERY);
    cmd1.GetAbsRdbPredicates()->BeginWrap()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_IMAGE))
        ->Or()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_VIDEO))->EndWrap()
        ->And()->EqualTo(MEDIA_DATA_DB_DATE_TRASHED, to_string(0))
        ->And()->NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM))
        ->OrderByDesc(MEDIA_DATA_DB_DATE_ADDED);
    resultset = MediaLibraryFileOperations::QueryFileOperation(cmd1, columns);
    EXPECT_EQ((resultset != nullptr), true);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_QueryFavFiles_test_001, TestSize.Level0)
{
    string queryUri = MEDIALIBRARY_DATA_URI;
    Uri uri(queryUri);
    MediaLibraryCommand cmd(uri, OperationType::QUERY);
    auto resultset = MediaLibraryFileOperations::QueryFavFiles(cmd);
    EXPECT_EQ((resultset != nullptr), false);

    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);
    MediaLibraryCommand cmd1(uri, OperationType::QUERY);
    cmd1.GetAbsRdbPredicates()->BeginWrap()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_IMAGE))
        ->Or()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_VIDEO))->EndWrap()
        ->And()->EqualTo(MEDIA_DATA_DB_DATE_TRASHED, to_string(0))
        ->And()->NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM))
        ->OrderByDesc(MEDIA_DATA_DB_DATE_ADDED);
    resultset = MediaLibraryFileOperations::QueryFavFiles(cmd1);
    EXPECT_EQ((resultset != nullptr), true);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

HWTEST_F(MediaLibraryExtUnitTest, medialibrary_QueryTrashFiles_test_001, TestSize.Level0)
{
    string queryUri = MEDIALIBRARY_DATA_URI;
    Uri uri(queryUri);
    MediaLibraryCommand cmd(uri, OperationType::QUERY);
    auto resultset = MediaLibraryFileOperations::QueryTrashFiles(cmd);
    EXPECT_EQ((resultset != nullptr), false);

    auto context = std::make_shared<OHOS::AbilityRuntime::AbilityContextImpl>();
    MediaLibraryUnistoreManager::GetInstance().Init(context);
    MediaLibraryCommand cmd1(uri, OperationType::QUERY);
    cmd1.GetAbsRdbPredicates()->BeginWrap()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_IMAGE))
        ->Or()->EqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_VIDEO))->EndWrap()
        ->And()->EqualTo(MEDIA_DATA_DB_DATE_TRASHED, to_string(0))
        ->And()->NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM))
        ->OrderByDesc(MEDIA_DATA_DB_DATE_ADDED);
    resultset = MediaLibraryFileOperations::QueryTrashFiles(cmd1);
    EXPECT_EQ((resultset != nullptr), true);
    MediaLibraryUnistoreManager::GetInstance().Stop();
}

}
}