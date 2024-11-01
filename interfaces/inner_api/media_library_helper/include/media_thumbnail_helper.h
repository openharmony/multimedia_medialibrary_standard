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

#ifndef INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_THUMBNAIL_HELPER_H_
#define INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_THUMBNAIL_HELPER_H_

#include <securec.h>
#include "pixel_map.h"

#define EXPORT __attribute__ ((visibility ("default")))
namespace OHOS {
namespace Media {
class MediaThumbnailHelper {
public:
    MediaThumbnailHelper();
    ~MediaThumbnailHelper() = default;
    static bool ResizeImage(const std::vector<uint8_t> &data, const Size &size, std::unique_ptr<PixelMap> &pixelMap);
};
} // namespace Media
} // namespace  OHOS
#endif  // INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_THUMBNAIL_HELPER_H_
