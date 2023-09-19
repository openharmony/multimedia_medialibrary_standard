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

#ifndef FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_SERVICE_H_
#define FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_SERVICE_H_

#include <mutex>

#include "ability_context.h"
#include "media_file_uri.h"
#include "pixel_map.h"
#include "rdb_helper.h"
#include "result_set_bridge.h"
#include "single_kvstore.h"
#include "userfile_manager_types.h"
#include "thumbnail_const.h"

#define THUMBNAIL_API_EXPORT __attribute__ ((visibility ("default")))
namespace OHOS {
namespace Media {
class ThumbnailService {
public:
    virtual ~ThumbnailService() = default;
    THUMBNAIL_API_EXPORT static std::shared_ptr<ThumbnailService> GetInstance();
    THUMBNAIL_API_EXPORT void ReleaseService();

    THUMBNAIL_API_EXPORT int GetThumbnailFd(const std::string &uri);
    THUMBNAIL_API_EXPORT int32_t LcdAging();
#ifdef DISTRIBUTED
    THUMBNAIL_API_EXPORT int32_t LcdDistributeAging(const std::string &udid);
#endif
    THUMBNAIL_API_EXPORT int32_t GenerateThumbnails();
    THUMBNAIL_API_EXPORT void InterruptBgworker();
    THUMBNAIL_API_EXPORT void StopAllWorker();
#ifdef DISTRIBUTED
    THUMBNAIL_API_EXPORT int32_t InvalidateDistributeThumbnail(const std::string &udid);
#endif
    THUMBNAIL_API_EXPORT int32_t CreateThumbnail(const std::string &uri, const std::string &path,
        bool isSync = false);
    THUMBNAIL_API_EXPORT void InvalidateThumbnail(const std::string &id, const std::string &tableName,
        const std::string &path = "");
    THUMBNAIL_API_EXPORT int32_t Init(const std::shared_ptr<NativeRdb::RdbStore> &rdbStore,
        const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore,
        const std::shared_ptr<OHOS::AbilityRuntime::Context> &context);
private:
    ThumbnailService();

    static std::shared_ptr<ThumbnailService> thumbnailServiceInstance_;
    static std::mutex instanceLock_;
    std::shared_ptr<DistributedKv::SingleKvStore> kvStorePtr_;
    std::shared_ptr<NativeRdb::RdbStore> rdbStorePtr_;
    std::shared_ptr<OHOS::AbilityRuntime::Context> context_;
    Size screenSize_;
};
} // namespace Media
} // namespace OHOS

#endif  // FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_SERVICE_H_
