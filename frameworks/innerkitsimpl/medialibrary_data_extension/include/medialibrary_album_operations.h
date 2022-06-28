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

#ifndef OHOS_MEDIALIBRARY_ALBUM_OPERATIONS_H
#define OHOS_MEDIALIBRARY_ALBUM_OPERATIONS_H

#include <string>
#include <securec.h>

#include "medialibrary_command.h"
#include "native_album_asset.h"
#include "rdb_result_set_bridge.h"
#include "medialibrary_unistore_manager.h"

namespace OHOS {
namespace Media {
class MediaLibraryAlbumOperations {
public:
    MediaLibraryAlbumOperations();
    int32_t HandleAlbumOperation(MediaLibraryCommand &cmd);
    int32_t CreateAlbumOperation(MediaLibraryCommand &cmd);
    int32_t DeleteAlbumOperation(MediaLibraryCommand &cmd);
    int32_t ModifyAlbumOperation(MediaLibraryCommand &cmd);
    shared_ptr<NativeRdb::AbsSharedResultSet> QueryAlbumOperation(MediaLibraryCommand &cmd,
        std::vector<std::string> columns);

    std::shared_ptr<NativeAlbumAsset> nativeAlbumAsset_ = std::make_shared<NativeAlbumAsset>();
    void SetNativeAlbumAsset(std::shared_ptr<NativeAlbumAsset> nativeAlbumAsset)
    {
        nativeAlbumAsset_ = nativeAlbumAsset;
    }
    std::shared_ptr<NativeAlbumAsset> GetNativeAlbumAsset()
    {
        return nativeAlbumAsset_;
    }
private:
    std::string GetDistributedAlbumSql(const std::string &strQueryCondition, const std::string &tableName);
    std::shared_ptr<MediaLibraryUnistore> uniStore_{nullptr};
};
} // namespace Media
} // namespace OHOS
#endif // OHOS_MEDIALIBRARY_ALBUM_OPERATIONS_H
