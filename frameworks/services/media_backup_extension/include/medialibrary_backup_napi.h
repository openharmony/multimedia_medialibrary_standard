/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_MEDIA_MEDIALIBRARY_BACKUP_NAPI_H_
#define OHOS_MEDIA_MEDIALIBRARY_BACKUP_NAPI_H_

#include <mutex>
#include <vector>
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "napi_error.h"
#include "napi_remote_object.h"
#include "backup_defines.h"

namespace OHOS {
namespace Media {
static const std::string MEDIA_LIB_BACKUP_NAPI_CLASS_NAME = "MediaLibraryBackup";

class MediaLibraryBackupNapi {
public:
    static napi_value Init(napi_env env, napi_value exports);

    MediaLibraryBackupNapi() = default;
    ~MediaLibraryBackupNapi() = default;

private:
    static napi_value JSStartRestore(napi_env env, napi_callback_info info);
    static napi_value JSMoveFiles(napi_env env, napi_callback_info info);
};
} // namespace Media
} // namespace OHOS

#endif  // OHOS_MEDIA_MEDIALIBRARY_BACKUP_NAPI_H_
