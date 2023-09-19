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

#ifndef FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_AGING_HELPER_H_
#define FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_AGING_HELPER_H_

#include "medialibrary_async_worker.h"
#include "rdb_helper.h"
#include "single_kvstore.h"
#include "thumbnail_utils.h"

namespace OHOS {
namespace Media {
class AgingAsyncTaskData : public AsyncTaskData {
public:
    AgingAsyncTaskData() = default;
    virtual ~AgingAsyncTaskData() override = default;
    ThumbRdbOpt opts;
    ThumbnailData thumbnailData;
};

class ThumbnailAgingHelper {
public:
    ThumbnailAgingHelper() = delete;
    virtual ~ThumbnailAgingHelper() = delete;

    static int32_t AgingLcdBatch(ThumbRdbOpt &opts);

#ifdef DISTRIBUTED
    static int32_t AgingDistributeLcdBatch(ThumbRdbOpt &opts);
    static int32_t InvalidateDistributeBatch(ThumbRdbOpt &opts);
    static int32_t ClearKeyAndRecordFromMap(ThumbRdbOpt &opts);
    static int32_t ClearRemoteLcdFromFileTable(ThumbRdbOpt &opts);
#endif
    static int32_t ClearLcdFromFileTable(ThumbRdbOpt &opts);
private:
    static int32_t GetLcdCount(ThumbRdbOpt &opts, int &outLcdCount);
    static int32_t GetAgingLcdData(ThumbRdbOpt &opts, int LcdLimit, std::vector<ThumbnailData> &outDatas);
    static int32_t GetDistributeLcdCount(ThumbRdbOpt &opts, int &outLcdCount);
    static int32_t GetAgingDistributeLcdData(ThumbRdbOpt &opts,
        int LcdLimit, std::vector<ThumbnailData> &outDatas);
};
} // namespace Media
} // namespace OHOS

#endif  // FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_AGING_HELPER_H_
