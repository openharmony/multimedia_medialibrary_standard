/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
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
#ifndef OHOS_FILEMANAGEMENT_USERFILEMGR_TYPES_H
#define OHOS_FILEMANAGEMENT_USERFILEMGR_TYPES_H

#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace OHOS {
namespace Media {
enum class ResultNapiType {
    TYPE_MEDIALIBRARY,
    TYPE_USERFILE_MGR,
    TYPE_PHOTOACCESS_HELPER,
    TYPE_NAPI_MAX
};

enum MediaType {
    MEDIA_TYPE_FILE,
    MEDIA_TYPE_IMAGE,
    MEDIA_TYPE_VIDEO,
    MEDIA_TYPE_AUDIO,
    MEDIA_TYPE_MEDIA,
    MEDIA_TYPE_ALBUM_LIST,
    MEDIA_TYPE_ALBUM_LIST_INFO,
    MEDIA_TYPE_ALBUM,
    MEDIA_TYPE_SMARTALBUM,
    MEDIA_TYPE_DEVICE,
    MEDIA_TYPE_REMOTEFILE,
    MEDIA_TYPE_NOFILE,
    MEDIA_TYPE_PHOTO,
    MEDIA_TYPE_ALL,
    MEDIA_TYPE_DEFAULT,
};

enum PhotoAlbumType : int32_t {
    USER = 0,
    SYSTEM = 1024
};

enum PhotoAlbumSubType : int32_t {
    USER_GENERIC = 1,

    SYSTEM_START = 1025,
    FAVORITE = SYSTEM_START,
    VIDEO,
    HIDDEN,
    TRASH,
    SCREENSHOT,
    CAMERA,
    IMAGES,
    SYSTEM_END = IMAGES,
    ANY = std::numeric_limits<int32_t>::max()
};

const std::vector<std::string> ALL_SYS_PHOTO_ALBUM = {
    std::to_string(PhotoAlbumSubType::FAVORITE),
    std::to_string(PhotoAlbumSubType::VIDEO),
    std::to_string(PhotoAlbumSubType::HIDDEN),
    std::to_string(PhotoAlbumSubType::TRASH),
    std::to_string(PhotoAlbumSubType::SCREENSHOT),
    std::to_string(PhotoAlbumSubType::CAMERA),
    std::to_string(PhotoAlbumSubType::IMAGES),
};

enum class PhotoSubType : int32_t {
    DEFAULT,
    SCREENSHOT,
    CAMERA
};

const std::string URI_PARAM_API_VERSION = "api_version";

enum class MediaLibraryApi : uint32_t {
    API_START = 8,
    API_OLD = 9,
    API_10,
    API_END
};

enum NotifyType {
    NOTIFY_ADD,
    NOTIFY_UPDATE,
    NOTIFY_REMOVE,
    NOTIFY_ALBUM_ADD_ASSERT,
    NOTIFY_ALBUM_REMOVE_ASSET
};

enum class RequestPhotoType : int32_t {
    REQUEST_ALL = 0,
    REQUEST_FAST_THUMB = 1,
    REQUEST_QUALITY_THUMB = 2,
};
} // namespace Media
} // namespace OHOS
#endif // OHOS_FILEMANAGEMENT_USERFILEMGR_TYPES_H
