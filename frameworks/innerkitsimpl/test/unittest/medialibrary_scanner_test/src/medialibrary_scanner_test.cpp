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

#include "media_log.h"
#include "medialibrary_db_const.h"
#include "medialibrary_scanner_test.h"
#include "medialibrary_unittest_utils.h"
#include "scanner_utils.h"
#include "userfile_manager_types.h"

using namespace std;
using namespace OHOS;
using namespace testing::ext;

namespace OHOS {
namespace Media {

shared_ptr<FileAsset> g_pictures = nullptr;
void MediaLibraryExtUnitTest::SetUpTestCase(void)
{
    MediaLibraryUnitTestUtils::Init();
}

void MediaLibraryExtUnitTest::TearDownTestCase(void) {}

// SetUp:Execute before each test case
void MediaLibraryExtUnitTest::SetUp()
{
    MediaLibraryUnitTestUtils::CleanTestFiles();
    MediaLibraryUnitTestUtils::InitRootDirs();
    g_pictures = MediaLibraryUnitTestUtils::GetRootAsset(TEST_PICTURES);
}

void MediaLibraryExtUnitTest::TearDown(void) {}


HWTEST_F(MediaLibraryExtUnitTest, medialib_IsExists_test_001, TestSize.Level0)
{
    if (!MediaLibraryUnitTestUtils::IsValid()) {
        MEDIA_ERR_LOG("MediaLibraryDataManager invalid");
        exit(1);
    }
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("IsExists_test_001.jpg", g_pictures, fileAsset), true);
    string path = "";
    bool ret = ScannerUtils::IsExists(path);
    EXPECT_EQ(ret, false);
    path = "medialib_GetFileName_test_001";
    ret = ScannerUtils::IsExists(path);
    EXPECT_EQ(ret, false);
    path= "/storage/media/local/files/Pictures/IsExists_test_001.jpg";
    ret = ScannerUtils::IsExists(path);
    EXPECT_EQ(ret, true);
}


HWTEST_F(MediaLibraryExtUnitTest, medialib_GetFileNameFromUri_test_001, TestSize.Level0)
{
    string path = "";
    string ret = ScannerUtils::GetFileNameFromUri(path);
    EXPECT_EQ(ret, "");
    path = "medialib_GetFileName_test_001/test";
    ret = ScannerUtils::GetFileNameFromUri(path);
    EXPECT_EQ(ret, "test");
    path = "medialib_GetFileName_test_001";
    ret = ScannerUtils::GetFileNameFromUri(path);
    EXPECT_EQ(ret, "");
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetFileExtensionFromFileUri_test_001, TestSize.Level0)
{
    string path = "";
    string ret = ScannerUtils::GetFileExtensionFromFileUri(path);
    EXPECT_EQ(ret, "");
    path = "medialib_GetFileExtensionFromFileUri_001.test";
    ret = ScannerUtils::GetFileExtensionFromFileUri(path);
    EXPECT_EQ(ret, "test");
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetMediatypeFromMimetype_test_001, TestSize.Level0)
{
    string path = "";
    MediaType mediaType = ScannerUtils::GetMediatypeFromMimetype(path);
    EXPECT_EQ(mediaType, MEDIA_TYPE_FILE);
    path = DEFAULT_AUDIO_MIME_TYPE;
    mediaType = ScannerUtils::GetMediatypeFromMimetype(path);
    EXPECT_EQ(mediaType, MEDIA_TYPE_AUDIO);
    path = DEFAULT_VIDEO_MIME_TYPE;
    mediaType = ScannerUtils::GetMediatypeFromMimetype(path);
    EXPECT_EQ(mediaType, MEDIA_TYPE_VIDEO);
    path = DEFAULT_IMAGE_MIME_TYPE;
    mediaType = ScannerUtils::GetMediatypeFromMimetype(path);
    EXPECT_EQ(mediaType, MEDIA_TYPE_IMAGE);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetMimeTypeFromExtension_test_001, TestSize.Level0)
{
    string extension = "";
    string mediaType = ScannerUtils::GetMimeTypeFromExtension(extension);
    EXPECT_EQ(mediaType, DEFAULT_FILE_MIME_TYPE);
    extension = "medialib_GetMimeTypeFromExtension_test";
    mediaType = ScannerUtils::GetMimeTypeFromExtension(extension);
    EXPECT_EQ(mediaType, DEFAULT_FILE_MIME_TYPE);
    extension = AUDIO_CONTAINER_TYPE_WAV;
    mediaType = ScannerUtils::GetMimeTypeFromExtension(extension);
    EXPECT_EQ(mediaType, DEFAULT_AUDIO_MIME_TYPE);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_IsDirectory_test_001, TestSize.Level0)
{
    std::string path = "";
    bool ret = ScannerUtils::IsDirectory(path);
    EXPECT_EQ(ret, false);
    path = "medialib_IsDirectory_test_001";
    ret = ScannerUtils::IsDirectory(path);
    EXPECT_EQ(ret, false);
    path = "/storage/media";
    ret = ScannerUtils::IsDirectory(path);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_IsRegularFile_test_001, TestSize.Level0)
{
    if (!MediaLibraryUnitTestUtils::IsValid()) {
        MEDIA_ERR_LOG("MediaLibraryDataManager invalid");
        exit(1);
    }
    std::string path = "";
    bool ret = ScannerUtils::IsRegularFile(path);
    EXPECT_EQ(ret, false);
    path = "medialib_IsRegularFile_test_001";
    ret = ScannerUtils::IsRegularFile(path);
    EXPECT_EQ(ret, false);
    shared_ptr<FileAsset> fileAsset = nullptr;
    ASSERT_EQ(MediaLibraryUnitTestUtils::CreateFile("IsRegularFile_test_001.jpg", g_pictures, fileAsset), true);
    path = "/storage/media/local/files/Pictures/IsRegularFile_test_001.jpg";
    ret = ScannerUtils::IsRegularFile(path);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_IsFileHidden_test_001, TestSize.Level0)
{
    std::string path = "";
    bool ret = ScannerUtils::IsFileHidden(path);
    EXPECT_EQ(ret, false);
    path = "medialib_IsFileHidden_test_001/.test";
    ret = ScannerUtils::IsFileHidden(path);
    EXPECT_EQ(ret, true);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetParentPath_test_001, TestSize.Level0)
{
    string path = "";
    string ret = ScannerUtils::GetParentPath(path);
    EXPECT_EQ(ret, "");
    path = "medialib_GetParentPath_test_001/test";
    ret = ScannerUtils::GetParentPath(path);
    EXPECT_EQ(ret, "medialib_GetParentPath_test_001");
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetRootMediaDir_test_001, TestSize.Level0)
{
    std::string path = "";
    ScannerUtils::GetRootMediaDir(path);
    EXPECT_EQ(path, ROOT_MEDIA_DIR);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_GetFileTitle_test_001, TestSize.Level0)
{
    std::string path = "medialib_GetFileTitle_test_001";
    string ret = ScannerUtils::GetFileTitle(path);
    EXPECT_EQ(path, ret);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_IsDirHidden_test_001, TestSize.Level0)
{
    string path = "medialib_IsDirHidden_test_001/.test";
    bool ret = ScannerUtils::IsDirHidden(path);
    EXPECT_EQ(ret, true);
    path = "";
    ret = ScannerUtils::IsDirHidden(path);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_IsDirHiddenRecursive_test_001, TestSize.Level0)
{
    std::string path = "medialib_IsDirHiddenRecursive_test_001/.test";
    bool ret = ScannerUtils::IsDirHiddenRecursive(path);
    EXPECT_EQ(ret, true);
    path = "";
    ret = ScannerUtils::IsDirHiddenRecursive(path);
    EXPECT_EQ(ret, false);
}

HWTEST_F(MediaLibraryExtUnitTest, medialib_CheckSkipScanList_test_001, TestSize.Level0)
{
    std::string path = "medialib_CheckSkipScanList_test_001";
    bool ret = ScannerUtils::CheckSkipScanList(path);
    EXPECT_EQ(ret, false);
}

}// namespace Media
} // namespace OHOS