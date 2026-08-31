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
#define MLOG_TAG "MediaLibraryManager"

#include "media_library_manager.h"

#include <charconv>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "album_asset.h"
#include "datashare_abs_result_set.h"
#include "datashare_predicates.h"
#include "fetch_result.h"
#include "file_asset.h"
#include "image_source.h"
#include "media_file_uri.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "medialibrary_tracer.h"
#include "medialibrary_type_const.h"
#include "post_proc.h"
#include "result_set_utils.h"
#include "string_ex.h"
#include "thumbnail_const.h"
#include "unique_fd.h"

#ifdef IMAGE_PURGEABLE_PIXELMAP
#include "purgeable_pixelmap_builder.h"
#endif

using namespace std;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {
shared_ptr<DataShare::DataShareHelper> MediaLibraryManager::sDataShareHelper_ = nullptr;
constexpr int32_t DEFAULT_THUMBNAIL_SIZE = 256;

MediaLibraryManager *MediaLibraryManager::GetMediaLibraryManager()
{
    static MediaLibraryManager mediaLibMgr;
    return &mediaLibMgr;
}

void MediaLibraryManager::InitMediaLibraryManager(const sptr<IRemoteObject> &token)
{
    if (sDataShareHelper_ == nullptr) {
        sDataShareHelper_ = DataShare::DataShareHelper::Creator(token, MEDIALIBRARY_DATA_URI);
    }
}

int32_t MediaLibraryManager::CloseAsset(const string &uri, const int32_t fd)
{
    int32_t retVal = E_FAIL;
    DataShareValuesBucket valuesBucket;
    valuesBucket.Put(MEDIA_DATA_DB_URI, uri);

    if (sDataShareHelper_ != nullptr) {
        string abilityUri = MEDIALIBRARY_DATA_URI;
        Uri closeAssetUri(abilityUri + "/" + MEDIA_FILEOPRN + "/" + MEDIA_FILEOPRN_CLOSEASSET);

        if (close(fd) == E_SUCCESS) {
            retVal = sDataShareHelper_->Insert(closeAssetUri, valuesBucket);
        }

        if (retVal == E_FAIL) {
            MEDIA_ERR_LOG("Failed to close the file");
        }
    }

    return retVal;
}

int32_t MediaLibraryManager::QueryTotalSize(MediaVolume &outMediaVolume)
{
    if (sDataShareHelper_ == nullptr) {
        MEDIA_ERR_LOG("sDataShareHelper_ is null");
        return E_FAIL;
    }
    vector<string> columns;
    Uri uri(MEDIALIBRARY_DATA_URI + "/" + MEDIA_QUERYOPRN_QUERYVOLUME + "/" + MEDIA_QUERYOPRN_QUERYVOLUME);
    DataSharePredicates predicates;
    auto queryResultSet = sDataShareHelper_->Query(uri, predicates, columns);
    if (queryResultSet == nullptr) {
        MEDIA_ERR_LOG("queryResultSet is null!");
        return E_FAIL;
    }
    auto count = 0;
    auto ret = queryResultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("get rdbstore failed");
        return E_HAS_DB_ERROR;
    }
    MEDIA_INFO_LOG("count = %{public}d", (int)count);
    if (count >= 0) {
        while (queryResultSet->GoToNextRow() == NativeRdb::E_OK) {
            int mediatype = get<int32_t>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_MEDIA_TYPE,
                queryResultSet, TYPE_INT32));
            int64_t size = get<int64_t>(ResultSetUtils::GetValFromColumn(MEDIA_DATA_DB_SIZE,
                queryResultSet, TYPE_INT64));
            outMediaVolume.SetSize(mediatype, size);
        }
    }
    MEDIA_INFO_LOG("Size:Files:%{public}" PRId64 " Videos:%{public}" PRId64 " Images:%{public} " PRId64
        " Audio:%{public}" PRId64,
        outMediaVolume.GetFilesSize(), outMediaVolume.GetVideosSize(),
        outMediaVolume.GetImagesSize(), outMediaVolume.GetAudiosSize());
    return E_SUCCESS;
}

std::shared_ptr<DataShareResultSet> MediaLibraryManager::GetResultSetFromDb(string columnName, const string &value,
    vector<string> &columns)
{
    Uri uri(MEDIALIBRARY_MEDIA_PREFIX);
    DataSharePredicates predicates;
    predicates.EqualTo(columnName, value);
    predicates.And()->EqualTo(MEDIA_DATA_DB_IS_TRASH, to_string(NOT_TRASHED));
    DatashareBusinessError businessError;

    if (sDataShareHelper_ == nullptr) {
        MEDIA_ERR_LOG("sDataShareHelper_ is null");
        return nullptr;
    }
    return sDataShareHelper_->Query(uri, predicates, columns, &businessError);
}

static int32_t SolvePath(const string &filePath, string &tempPath, string &userId)
{
    if (filePath.empty()) {
        return E_INVALID_PATH;
    }

    string prePath = PRE_PATH_VALUES;
    if (filePath.find(prePath) != 0) {
        return E_CHECK_ROOT_DIR_FAIL;
    }
    string postpath = filePath.substr(prePath.length());
    auto pos = postpath.find('/');
    if (pos == string::npos) {
        return E_INVALID_ARGUMENTS;
    }
    userId = postpath.substr(0, pos);
    postpath = postpath.substr(pos + 1);
    tempPath = prePath + postpath;

    return E_SUCCESS;
}

static int32_t CheckResultSet(std::shared_ptr<DataShareResultSet> &resultSet)
{
    int count = 0;
    auto ret = resultSet->GetRowCount(count);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to get resultset row count, ret: %{public}d", ret);
        return ret;
    }
    if (count <= 0) {
        MEDIA_ERR_LOG("Failed to get count, count: %{public}d", count);
        return E_FAIL;
    }
    ret = resultSet->GoToFirstRow();
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to go to first row, ret: %{public}d", ret);
        return ret;
    }
    return E_SUCCESS;
}


int32_t MediaLibraryManager::GetFilePathFromUri(const Uri &fileUri, string &filePath, string userId)
{
    string uri = fileUri.ToString();
    MediaFileUri virtualUri(uri);
    if (!virtualUri.IsValid()) {
        return E_URI_INVALID;
    }
    string virtualId = virtualUri.GetFileId();
#ifdef MEDIALIBRARY_COMPATIBILITY
    if (MediaFileUtils::GetTableFromVirtualUri(uri) != MEDIALIBRARY_TABLE) {
        MEDIA_INFO_LOG("uri:%{private}s does not match Files Table", uri.c_str());
        return E_URI_INVALID;
    }
#endif
    vector<string> columns = { MEDIA_DATA_DB_FILE_PATH };
    auto resultSet = MediaLibraryManager::GetResultSetFromDb(MEDIA_DATA_DB_ID, virtualId, columns);
    CHECK_AND_RETURN_RET_LOG(resultSet != nullptr, E_INVALID_URI,
        "GetFilePathFromUri::uri is not correct: %{private}s", uri.c_str());
    if (CheckResultSet(resultSet) != E_SUCCESS) {
        return E_FAIL;
    }

    std::string tempPath = ResultSetUtils::GetStringValFromColumn(1, resultSet);
    if (tempPath.find(ROOT_MEDIA_DIR) != 0) {
        return E_CHECK_ROOT_DIR_FAIL;
    }
    string relativePath = tempPath.substr(ROOT_MEDIA_DIR.length());
    auto pos = relativePath.find('/');
    if (pos == string::npos) {
        return E_INVALID_ARGUMENTS;
    }
    relativePath = relativePath.substr(0, pos + 1);
    if ((relativePath != DOC_DIR_VALUES) && (relativePath != DOWNLOAD_DIR_VALUES)) {
        return E_DIR_CHECK_DIR_FAIL;
    }

    string prePath = PRE_PATH_VALUES;
    string postpath = tempPath.substr(prePath.length());
    tempPath = prePath + userId + "/" + postpath;
    filePath = tempPath;
    return E_SUCCESS;
}

int32_t MediaLibraryManager::GetUriFromFilePath(const string &filePath, Uri &fileUri, string &userId)
{
    if (filePath.empty()) {
        return E_INVALID_PATH;
    }

    string tempPath;
    SolvePath(filePath, tempPath, userId);
    if (tempPath.find(ROOT_MEDIA_DIR) != 0) {
        return E_CHECK_ROOT_DIR_FAIL;
    }
    string relativePath = tempPath.substr(ROOT_MEDIA_DIR.length());
    auto pos = relativePath.find('/');
    if (pos == string::npos) {
        return E_INVALID_ARGUMENTS;
    }
    relativePath = relativePath.substr(0, pos + 1);
    if ((relativePath != DOC_DIR_VALUES) && (relativePath != DOWNLOAD_DIR_VALUES)) {
        return E_DIR_CHECK_DIR_FAIL;
    }

    vector<string> columns = { MEDIA_DATA_DB_ID};
    auto resultSet = MediaLibraryManager::GetResultSetFromDb(MEDIA_DATA_DB_FILE_PATH, tempPath, columns);
    CHECK_AND_RETURN_RET_LOG(resultSet != nullptr, E_INVALID_URI,
        "GetUriFromFilePath::tempPath is not correct: %{private}s", tempPath.c_str());
    if (CheckResultSet(resultSet) != E_SUCCESS) {
        return E_FAIL;
    }

    int32_t fileId = ResultSetUtils::GetIntValFromColumn(0, resultSet);
#ifdef MEDIALIBRARY_COMPATIBILITY
    int64_t virtualId = MediaFileUtils::GetVirtualIdByType(fileId, MediaType::MEDIA_TYPE_FILE);
    fileUri = MediaFileUri(MediaType::MEDIA_TYPE_FILE, to_string(virtualId), "", MEDIA_API_VERSION_V9);
#else
    fileUri = MediaFileUri(MediaType::MEDIA_TYPE_FILE, to_string(fileId), "", MEDIA_API_VERSION_V9);
#endif
    return E_SUCCESS;
}

static inline bool IsSmallThumb(const int32_t width, const int32_t height)
{
    if (((width == DEFAULT_MTH_SIZE) && (height == DEFAULT_MTH_SIZE)) ||
        ((width == DEFAULT_YEAR_SIZE) && (height == DEFAULT_YEAR_SIZE))) {
        return true;
    }
    return false;
}

static inline std::string GetSandboxPath(const std::string &path, const Size &size, bool isAudio)
{
    if (path.length() < ROOT_MEDIA_DIR.length()) {
        return "";
    }
    std::string suffixStr = path.substr(ROOT_MEDIA_DIR.length()) + "/";
    if (isAudio) {
        if (size.width > DEFAULT_THUMBNAIL_SIZE || size.height > DEFAULT_THUMBNAIL_SIZE) {
            suffixStr += "LCD.jpg";
        } else {
            suffixStr += "THM.jpg";
        }
    } else {
        if (size.width > DEFAULT_THUMBNAIL_SIZE || size.height > DEFAULT_THUMBNAIL_SIZE) {
            suffixStr += "LCD.jpg";
        } else if (size.width == DEFAULT_MTH_SIZE && size.height == DEFAULT_MTH_SIZE) {
            suffixStr += "MTH.jpg";
        } else if (size.width == DEFAULT_YEAR_SIZE && size.height == DEFAULT_YEAR_SIZE) {
            suffixStr += "YEAR.jpg";
        } else {
            suffixStr += "THM.jpg";
        }
    }

    return ROOT_SANDBOX_DIR + ".thumbs/" + suffixStr;
}

int MediaLibraryManager::OpenThumbnail(string &uriStr, const string &path, const Size &size)
{
    if (sDataShareHelper_ == nullptr) {
        MEDIA_ERR_LOG("Failed to open thumbnail, datashareHelper is nullptr");
        return E_ERR;
    }
    if (!path.empty()) {
        string sandboxPath = GetSandboxPath(path, size,
            (MediaFileUri::GetMediaTypeFromUri(uriStr) == MediaType::MEDIA_TYPE_AUDIO));
        int fd = -1;
        if (!sandboxPath.empty()) {
            fd = open(sandboxPath.c_str(), O_RDONLY);
        }
        if (fd > 0) {
            return fd;
        }
        if (IsAsciiString(path)) {
            uriStr += "&" + THUMBNAIL_PATH + "=" + path;
        }
    }
    Uri openUri(uriStr);
    return sDataShareHelper_->OpenFile(openUri, "R");
}

/**
 * Get the file uri prefix with id
 * eg. Input: file://media/Photo/10/IMG_xxx/01.jpg
 *     Output: file://media/Photo/10
 */
static void GetUriIdPrefix(std::string &fileUri)
{
    MediaFileUri mediaUri(fileUri);
    if (!mediaUri.IsApi10()) {
        return;
    }
    auto slashIdx = fileUri.rfind('/');
    if (slashIdx == std::string::npos) {
        return;
    }
    auto tmpUri = fileUri.substr(0, slashIdx);
    slashIdx = tmpUri.rfind('/');
    if (slashIdx == std::string::npos) {
        return;
    }
    fileUri = tmpUri.substr(0, slashIdx);
}

static bool GetParamsFromUri(const string &uri, string &fileUri, const bool isOldVer, Size &size, string &path)
{
    MediaFileUri mediaUri(uri);
    if (!mediaUri.IsValid()) {
        return false;
    }
    if (isOldVer) {
        auto index = uri.find("thumbnail");
        if (index == string::npos) {
            return false;
        }
        fileUri = uri.substr(0, index - 1);
        GetUriIdPrefix(fileUri);
        index += strlen("thumbnail");
        index = uri.find('/', index);
        if (index == string::npos) {
            return false;
        }
        index += 1;
        auto tmpIdx = uri.find('/', index);
        if (tmpIdx == string::npos) {
            return false;
        }

        int32_t width = 0;
        StrToInt(uri.substr(index, tmpIdx - index), width);
        int32_t height = 0;
        StrToInt(uri.substr(tmpIdx + 1), height);
        size = { .width = width, .height = height };
    } else {
        auto qIdx = uri.find('?');
        if (qIdx == string::npos) {
            return false;
        }
        fileUri = uri.substr(0, qIdx);
        GetUriIdPrefix(fileUri);
        auto &queryKey = mediaUri.GetQueryKeys();
        if (queryKey.count(THUMBNAIL_PATH) != 0) {
            path = queryKey[THUMBNAIL_PATH];
        }
        if (queryKey.count(THUMBNAIL_WIDTH) != 0) {
            const auto &width = queryKey[THUMBNAIL_WIDTH];
            auto result = std::from_chars(width.data(), width.data() + width.size(), size.width);
            if (result.ec != std::errc{} || result.ptr != width.data() + width.size()) {
                return false;
            }
        }
        if (queryKey.count(THUMBNAIL_HEIGHT) != 0) {
            const auto &height = queryKey[THUMBNAIL_HEIGHT];
            auto result = std::from_chars(height.data(), height.data() + height.size(), size.height);
            if (result.ec != std::errc{} || result.ptr != height.data() + height.size()) {
                return false;
            }
        }
    }
    return true;
}

static bool IfSizeEqualsRatio(const Size &imageSize, const Size &targetSize)
{
    if (imageSize.height <= 0 || targetSize.height <= 0) {
        return false;
    }

    return imageSize.width / imageSize.height == targetSize.width / targetSize.height;
}

unique_ptr<PixelMap> MediaLibraryManager::DecodeThumbnail(UniqueFd& uniqueFd, const Size& size)
{
    MediaLibraryTracer tracer;
    tracer.Start("ImageSource::CreateImageSource");
    SourceOptions opts;
    uint32_t err = 0;
    unique_ptr<ImageSource> imageSource = ImageSource::CreateImageSource(uniqueFd.Get(), opts, err);
    if (imageSource  == nullptr) {
        MEDIA_ERR_LOG("CreateImageSource err %{public}d", err);
        return nullptr;
    }

    ImageInfo imageInfo;
    err = imageSource->GetImageInfo(0, imageInfo);
    if (err != E_OK) {
        MEDIA_ERR_LOG("GetImageInfo err %{public}d", err);
        return nullptr;
    }

    bool isEqualsRatio = IfSizeEqualsRatio(imageInfo.size, size);
    DecodeOptions decodeOpts;
    decodeOpts.desiredSize = isEqualsRatio ? size : imageInfo.size;
    decodeOpts.allocatorType = AllocatorType::SHARE_MEM_ALLOC;
    unique_ptr<PixelMap> pixelMap = imageSource->CreatePixelMap(decodeOpts, err);
    if (pixelMap == nullptr) {
        MEDIA_ERR_LOG("CreatePixelMap err %{public}d", err);
        return nullptr;
    }
#ifdef IMAGE_PURGEABLE_PIXELMAP
    PurgeableBuilder::MakePixelMapToBePurgeable(pixelMap, uniqueFd.Get(), opts, decodeOpts);
#endif
    PostProc postProc;
    if (size.width != DEFAULT_ORIGINAL && !isEqualsRatio && !postProc.CenterScale(size, *pixelMap)) {
        return nullptr;
    }
    return pixelMap;
}

unique_ptr<PixelMap> MediaLibraryManager::GetPixelMapWithoutDecode(UniqueFd &uniqueFd, const Size &size)
{
    struct stat statInfo;
    if (fstat(uniqueFd.Get(), &statInfo) == E_ERR) {
        return nullptr;
    }
    uint32_t *buffer = static_cast<uint32_t*>(malloc(statInfo.st_size));
    if (buffer == nullptr) {
        return nullptr;
    }
    read(uniqueFd.Get(), buffer, statInfo.st_size);
    InitializationOptions option;
    option.size = size;
    
    unique_ptr<PixelMap> pixelMap = PixelMap::Create(buffer, statInfo.st_size, option);
    free(buffer);
    return pixelMap;
}

unique_ptr<PixelMap> MediaLibraryManager::QueryThumbnail(const std::string &uri, Size &size, const string &path)
{
    MediaLibraryTracer tracer;
    tracer.Start("QueryThumbnail uri:" + uri);

    string openUriStr = uri + "?" + MEDIA_OPERN_KEYWORD + "=" + MEDIA_DATA_DB_THUMBNAIL + "&" + MEDIA_DATA_DB_WIDTH +
        "=" + to_string(size.width) + "&" + MEDIA_DATA_DB_HEIGHT + "=" + to_string(size.height);
    tracer.Start("DataShare::OpenThumbnail");
    UniqueFd uniqueFd(MediaLibraryManager::OpenThumbnail(openUriStr, path, size));
    if (uniqueFd.Get() < 0) {
        MEDIA_ERR_LOG("queryThumb is null, errCode is %{public}d", uniqueFd.Get());
        return nullptr;
    }
    tracer.Finish();
    if (IsSmallThumb(size.width, size.height) &&
        (MediaFileUri::GetMediaTypeFromUri(uri) != MediaType::MEDIA_TYPE_AUDIO)) {
        return GetPixelMapWithoutDecode(uniqueFd, size);
    } else {
        return DecodeThumbnail(uniqueFd, size);
    }
}

std::unique_ptr<PixelMap> MediaLibraryManager::GetThumbnail(const Uri &uri)
{
    // uri is dataability:///media/image/<id>/thumbnail/<width>/<height>
    string uriStr = uri.ToString();
    auto thumbLatIdx = uriStr.find("thumbnail") + strlen("thumbnail");
    if (thumbLatIdx == string::npos || thumbLatIdx > uriStr.length()) {
        return nullptr;
    }
    bool isOldVersion = uriStr[thumbLatIdx] == '/';
    string path;
    string fileUri;
    Size size;
    if (!GetParamsFromUri(uriStr, fileUri, isOldVersion, size, path)) {
        return nullptr;
    }
    return QueryThumbnail(fileUri, size, path);
}
} // namespace Media
} // namespace OHOS
