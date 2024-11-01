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

#ifndef FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_ITHUMBNAIL_HELPER_H_
#define FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_ITHUMBNAIL_HELPER_H_

#include <map>
#include <memory>
#include <shared_mutex>

#include "ability_connect_callback_stub.h"
#include "ability_context.h"
#include "datashare_proxy.h"
#include "datashare_values_bucket.h"
#include "medialibrary_async_worker.h"
#include "medialibrary_sync_operation.h"
#include "result_set_bridge.h"
#include "thumbnail_const.h"
#include "thumbnail_utils.h"
#include "pixel_map.h"

namespace OHOS {
namespace Media {
class GenerateAsyncTaskData : public AsyncTaskData {
public:
    GenerateAsyncTaskData() = default;
    virtual ~GenerateAsyncTaskData() override = default;
    ThumbRdbOpt opts;
    ThumbnailData thumbnailData;
};

enum WaitStatus {
    INSERT,
    WAIT_SUCCESS,
    TIMEOUT,
};

using ThumbnailMap = std::map<std::string, std::shared_ptr<SyncStatus>>;
class ThumbnailWait {
public:
    ThumbnailWait(bool release);
    ~ThumbnailWait();

    WaitStatus InsertAndWait(const std::string &id, bool isLcd);
    void CheckAndWait(const std::string &id, bool isLcd);

private:
    void Notify();
    std::string id_;
    bool needRelease_{false};
    static ThumbnailMap thumbnailMap_;
    static std::shared_mutex mutex_;
};

class IThumbnailHelper {
public:
    IThumbnailHelper() = default;
    virtual ~IThumbnailHelper() = default;
    virtual int32_t CreateThumbnail(ThumbRdbOpt &opts, bool isSync = false);
    virtual int32_t GetThumbnailPixelMap(ThumbRdbOpt &opts, const Size &size);
    static void DeleteThumbnailKv(ThumbRdbOpt &opts);
    static void CreateLcd(AsyncTaskData *data);
    static void CreateThumbnail(AsyncTaskData *data);
    static void AddAsyncTask(MediaLibraryExecute executor, ThumbRdbOpt &opts, ThumbnailData &data, bool isFront);
protected:
    static void GetThumbnailInfo(ThumbRdbOpt &opts, ThumbnailData &outData);
    static std::unique_ptr<PixelMap> GetPixelMap(const std::vector<uint8_t> &image, Size &size);
    static bool DoCreateLcd(ThumbRdbOpt &opts, ThumbnailData &data);
    static bool DoCreateThumbnail(ThumbRdbOpt &opts, ThumbnailData &data);
private:
    static bool GenThumbnail(ThumbRdbOpt &opts, ThumbnailData &data, const ThumbnailType type);
    static bool TryLoadSource(ThumbRdbOpt &opts, ThumbnailData &data, const Size &size, const std::string &suffix);
};

// copy from foundation/distributeddatamgr/data_share/frameworks/native/common/include/idatashare.h
class IDataShareThumb : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"OHOS.DataShare.IDataShare");
    enum {
        CMD_INSERT = 4,
    };
    virtual int Insert(const Uri &uri, const DataShare::DataShareValuesBucket &value) = 0;
};
} // namespace Media
} // namespace OHOS

#endif  // FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_ITHUMBNAIL_HELPER_H_
