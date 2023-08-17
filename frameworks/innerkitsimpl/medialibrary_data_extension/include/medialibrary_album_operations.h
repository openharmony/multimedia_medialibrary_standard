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

#include <securec.h>
#include <string>

#include "abs_shared_result_set.h"
#include "datashare_predicates.h"
#include "datashare_values_bucket.h"
#include "medialibrary_command.h"
#include "native_album_asset.h"
#include "rdb_predicates.h"
#include "rdb_result_set_bridge.h"

namespace OHOS {
namespace Media {
class MediaLibraryAlbumOperations {
public:
    static int32_t CreateAlbumOperation(MediaLibraryCommand &cmd);
    static int32_t ModifyAlbumOperation(MediaLibraryCommand &cmd);
    static shared_ptr<NativeRdb::ResultSet> QueryAlbumOperation(MediaLibraryCommand &cmd,
        const std::vector<std::string> &columns);

    static int32_t HandlePhotoAlbumOperations(MediaLibraryCommand &cmd);
    static std::shared_ptr<NativeRdb::ResultSet> QueryPhotoAlbum(MediaLibraryCommand &cmd,
        const std::vector<std::string> &columns);
    static int32_t DeletePhotoAlbum(NativeRdb::RdbPredicates &predicates);
    static int32_t AddPhotoAssets(const vector<DataShare::DataShareValuesBucket> &values);
    static int32_t HandlePhotoAlbum(const OperationType &opType, const NativeRdb::ValuesBucket &values,
        const DataShare::DataSharePredicates &predicates);

private:
    static std::string GetDistributedAlbumSql(const std::string &strQueryCondition, const std::string &tableName);
};
} // namespace Media
} // namespace OHOS
#endif // OHOS_MEDIALIBRARY_ALBUM_OPERATIONS_H
