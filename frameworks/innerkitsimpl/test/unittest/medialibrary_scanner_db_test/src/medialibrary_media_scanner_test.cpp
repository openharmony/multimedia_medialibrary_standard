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
#include "medialibrary_device.h"
#include "medialibrary_errno.h"
#include "medialibrary_scanner_db_test.h"
#include "media_scanner_db.h"
#define private public
#include "media_scanner.h"
#undef private

using namespace std;
using namespace OHOS;
using namespace testing::ext;

namespace OHOS {
namespace Media {
HWTEST_F(MediaLibraryExtUnitTest, medialib_Scan_test_001, TestSize.Level0)
{
    string path = "Scan";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(path, callback, MediaScannerObj::FILE);
    mediaScannerObj.Scan();
    MediaScannerObj mediaScannerObjOne(path, callback, MediaScannerObj::DIRECTORY);
    mediaScannerObjOne.Scan();
    MediaScannerObj mediaScannerObjTwo(path, callback, MediaScannerObj::START);
    MediaScannerObj mediaScannerObjThree(path, callback, MediaScannerObj::ERROR);
    mediaScannerObjThree.Scan();
    shared_ptr<bool> flag = make_shared<bool>();
    mediaScannerObjThree.SetStopFlag(flag);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_ScanFileInternal_test_001, TestSize.Level0)
{
    string path = "medialib_ScanFileInternal_test_001/.test";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(path, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.ScanFileInternal();
    EXPECT_EQ(ret, E_FILE_HIDDEN);
    string pathOne = "/storage/media/local";
    MediaScannerObj mediaScannerObjOne(pathOne, callback, MediaScannerObj::FILE);
    ret = mediaScannerObjOne.ScanFileInternal();
    EXPECT_EQ(ret, E_DATA);
    string pathTwo = "medialib_ScanFileInternal_test_001/test";
    MediaScannerObj mediaScannerObjTwo(pathTwo, callback, MediaScannerObj::FILE);
    ret = mediaScannerObjTwo.ScanFileInternal();
    EXPECT_EQ(ret, E_SYSCALL);
    string pathThree = "/storage/media/local/files/test";
    MediaScannerObj mediaScannerObjThree(pathThree, callback, MediaScannerObj::FILE);
    ret = mediaScannerObjThree.ScanFileInternal();
    EXPECT_EQ(ret, E_SYSCALL);
    string pathFour= "/storage/media/local/files/";
    MediaScannerObj mediaScannerObjFour(pathFour, callback, MediaScannerObj::FILE);
    ret = mediaScannerObjFour.ScanFileInternal();
    EXPECT_EQ(ret, E_DATA);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_ScanFile_test_001, TestSize.Level0)
{
    string path = "/storage/media/local/files/";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(path, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.ScanFile();
    EXPECT_EQ(ret, E_DATA);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetFileMetadata_test_001, TestSize.Level0)
{
    string path = "/storage/media/local/files/";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(path, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.GetFileMetadata();
    EXPECT_EQ(ret, E_OK);
    MediaScannerObj mediaScannerObjOne("", callback, MediaScannerObj::FILE);
    ret = mediaScannerObjOne.GetFileMetadata();
    EXPECT_EQ(ret, E_SYSCALL);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetParentDirInfo_test_001, TestSize.Level0)
{
    string path = "GetParentDirInfo";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(path, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.GetParentDirInfo(path, UNKNOWN_ID);
    EXPECT_EQ(ret, E_DATA);
    string pathOne = "/storage/media/local/files";
    MediaScannerObj mediaScannerObjOne(pathOne, callback, MediaScannerObj::FILE);
    ret = mediaScannerObjOne.GetParentDirInfo(pathOne, UNKNOWN_ID);
    EXPECT_EQ(ret, E_DATA);
    string pathTwo = "/storage/media/local/file/";
    MediaScannerObj mediaScannerObjFour(pathTwo, callback, MediaScannerObj::FILE);
    ret = mediaScannerObjFour.GetParentDirInfo(pathTwo, UNKNOWN_ID);
    EXPECT_EQ(ret, E_DATA);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetMediaInfo_test_001, TestSize.Level0)
{
    string path = "/storage/media/local/files/";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(path, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.GetFileMetadata();
    EXPECT_EQ(ret, E_OK);
    ret = mediaScannerObj.GetMediaInfo();
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_ScanFileInTraversal_test_001, TestSize.Level0)
{
    string dir = "/storage/media/local/files/";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    string parent = "ScanDirInternal";
    MediaScannerObj mediaScannerObj(dir, callback, MediaScannerObj::DIRECTORY);
    int32_t ret = mediaScannerObj.ScanFileInTraversal(dir, parent, UNKNOWN_ID);
    EXPECT_EQ(ret, E_DATA);
    string path = "medialib_ScanDirInternal_test_001/.test";
    string pathTest = "medialib_ScanDirInternal_test_001/test";
    ret = mediaScannerObj.ScanFileInTraversal(path, parent, UNKNOWN_ID);
    EXPECT_EQ(ret, E_FILE_HIDDEN);
    ret = mediaScannerObj.ScanFileInTraversal(pathTest, parent, UNKNOWN_ID);
    EXPECT_EQ(ret, E_SYSCALL);
    string dirTest = "/storage/media/local/files";
    ret = mediaScannerObj.ScanFileInTraversal(dir, dirTest, UNKNOWN_ID);
    EXPECT_EQ(ret, E_DATA);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_WalkFileTree_test_001, TestSize.Level0)
{
    string dir = "/storage/media/local/files";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    string path(4096, 'a');
    MediaScannerObj mediaScannerObj(dir, callback, MediaScannerObj::DIRECTORY);
    int32_t ret = mediaScannerObj.WalkFileTree(path, 0);
    EXPECT_EQ(ret, ERR_INCORRECT_PATH);
    ret = mediaScannerObj.WalkFileTree("", 0);
    EXPECT_EQ(ret, ERR_NOT_ACCESSIBLE);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_CleanupDirectory_test_001, TestSize.Level0)
{
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj("", callback, MediaScannerObj::DIRECTORY);
    int32_t ret = mediaScannerObj.CleanupDirectory();
    EXPECT_EQ(ret, E_OK);
    string dirTest = "/storage/media/local/files";
    MediaScannerObj mediaScannerObjOne(dirTest, callback, MediaScannerObj::DIRECTORY);
    ret = mediaScannerObjOne.CleanupDirectory();
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_InsertOrUpdateAlbumInfo_test_001, TestSize.Level0)
{
    string dir = "/storage/media/local/files";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(dir, callback, MediaScannerObj::DIRECTORY);
    int32_t parentId= -1;
    const string albumName = "InsertOrUpdateAlbumInfo";
    int32_t ret = mediaScannerObj.InsertOrUpdateAlbumInfo("", parentId, albumName);
    EXPECT_EQ(ret, UNKNOWN_ID);
    string dirTest = "/storage/media/local/files";
    ret = mediaScannerObj.InsertOrUpdateAlbumInfo(dirTest, parentId, albumName);
    EXPECT_GT(ret, E_OK);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_Commit_test_001, TestSize.Level0)
{
    string dir = "/storage/media/local/files";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(dir, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.GetFileMetadata();
    EXPECT_EQ(ret, E_OK);
    ret = mediaScannerObj.Commit();
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_AddToTransaction_test_001, TestSize.Level0)
{
    string dir = "/storage/media/local/files";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(dir, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.GetFileMetadata();
    EXPECT_EQ(ret, E_OK);
    ret = mediaScannerObj.AddToTransaction();
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_CommitTransaction_test_001, TestSize.Level0)
{
    string dir = "/storage/media/local/files";
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj(dir, callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.GetFileMetadata();
    EXPECT_EQ(ret, E_OK);
    ret = mediaScannerObj.CommitTransaction();
    EXPECT_EQ(ret, E_OK);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_InvokeCallback_test_001, TestSize.Level0)
{
    shared_ptr<IMediaScannerCallback> callback = nullptr;
    MediaScannerObj mediaScannerObj("", callback, MediaScannerObj::FILE);
    int32_t ret = mediaScannerObj.InvokeCallback(0);
    EXPECT_EQ(ret, E_OK);
}
} // namespace Media
} // namespace OHOS