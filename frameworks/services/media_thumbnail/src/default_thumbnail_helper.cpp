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
#define MLOG_TAG "Thumbnail"

#include <fcntl.h>

#include "default_thumbnail_helper.h"
#include "media_column.h"
#include "medialibrary_errno.h"
#include "media_log.h"
#include "thumbnail_const.h"
#include "thumbnail_utils.h"

using namespace std;
using namespace OHOS::DistributedKv;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {
int32_t DefaultThumbnailHelper::CreateThumbnail(ThumbRdbOpt &opts, bool isSync)
{
    ThumbnailData thumbnailData;
    GetThumbnailInfo(opts, thumbnailData);

    string fileName = GetThumbnailPath(thumbnailData.path, THUMBNAIL_THUMB_SUFFIX);
    if (access(fileName.c_str(), F_OK) == 0) {
        MEDIA_DEBUG_LOG("CreateThumbnail key is same, no need generate");
        return E_OK;
    }

    if (isSync) {
        DoCreateThumbnail(opts, thumbnailData);
    } else {
        IThumbnailHelper::AddAsyncTask(IThumbnailHelper::CreateThumbnail, opts, thumbnailData, true);
    }
    return E_OK;
}

int32_t DefaultThumbnailHelper::GetThumbnailPixelMap(ThumbRdbOpt &opts, const Size &size)
{
    ThumbnailWait thumbnailWait(false);
    thumbnailWait.CheckAndWait(opts.row, false);
    ThumbnailData thumbnailData;
    GetThumbnailInfo(opts, thumbnailData);

    ThumbnailType type = GetThumbType(size.width, size.height);
    if (opts.table == AudioColumn::AUDIOS_TABLE) {
        type = ThumbnailType::THUMB;
    }
    string fileName = GetThumbnailPath(thumbnailData.path, GetThumbSuffix(type));
    if (access(fileName.c_str(), F_OK) != 0) {
        if (!DoCreateThumbnail(opts, thumbnailData)) {
            return E_THUMBNAIL_LOCAL_CREATE_FAIL;
        }
        if (!opts.path.empty()) {
            fileName = GetThumbnailPath(thumbnailData.path, THUMBNAIL_THUMB_SUFFIX);
        }
    }
    auto fd = open(fileName.c_str(), O_RDONLY);
    if (fd >= 0) {
        return fd;
    }
    return -errno;
}
} // namespace Media
} // namespace OHOS
