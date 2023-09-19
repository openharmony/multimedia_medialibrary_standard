/*
 * Copyright (C) 2022-2023 Huawei Device Co., Ltd.
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

#ifndef FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_UTILS_H_
#define FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_UTILS_H_

#include <mutex>
#include <condition_variable>

#include "ability_context.h"
#include "avmetadatahelper.h"
#include "datashare_result_set.h"
#include "rdb_helper.h"
#include "single_kvstore.h"
#include "thumbnail_const.h"
#include "thumbnail_datashare_bridge.h"

namespace OHOS {
namespace Media {
struct ThumbRdbOpt {
    std::shared_ptr<NativeRdb::RdbStore> store;
    std::shared_ptr<DistributedKv::SingleKvStore> kvStore;
    std::shared_ptr<AbilityRuntime::Context> context;
    std::string networkId;
    std::string path;
    std::string table;
    std::string udid;
    std::string row;
    std::string uri;
    Size screenSize;
};

struct ThumbnailData {
    ThumbnailData() {}
    virtual ~ThumbnailData()
    {
        source = nullptr;
        thumbnail.clear();
        lcd.clear();
    }
    int mediaType{-1};
    int64_t dateModified{0};
    float degrees;
    std::shared_ptr<PixelMap> source;
    std::vector<uint8_t> thumbnail;
    std::vector<uint8_t> lcd;
    std::string id;
    std::string cloudId;
    std::string udid;
    std::string path;
    std::string thumbnailKey;
    std::string lcdKey;
};

class ThumbnailUtils {
public:
    ThumbnailUtils() = delete;
    ~ThumbnailUtils() = delete;
    // utils
    static bool ResizeImage(const std::vector<uint8_t> &data, const Size &size, std::unique_ptr<PixelMap> &pixelMap);
    static bool CompressImage(std::shared_ptr<PixelMap> &pixelMap, std::vector<uint8_t> &data);
    static bool CleanThumbnailInfo(ThumbRdbOpt &opts, bool withThumb, bool withLcd = false);
    static int GetPixelMapFromResult(const std::shared_ptr<DataShare::DataShareResultSet> &resultSet, const Size &size,
        std::unique_ptr<PixelMap> &outPixelMap);

    // URI utils
    static bool UpdateRemotePath(std::string &path, const std::string &networkId);

    // RDB Store Query
    static std::shared_ptr<NativeRdb::ResultSet> QueryThumbnailSet(ThumbRdbOpt &opts);
    static std::shared_ptr<NativeRdb::ResultSet> QueryThumbnailInfo(ThumbRdbOpt &opts,
        ThumbnailData &data, int &err);
    static bool QueryRemoteThumbnail(ThumbRdbOpt &opts, ThumbnailData &data, int &err);

    // KV Store
    static bool RemoveDataFromKv(const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore, const std::string &key);
#ifdef DISTRIBUTED
    static bool IsImageExist(const std::string &key, const std::string &networkId,
        const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore);
    static bool DeleteLcdData(ThumbRdbOpt &opts, ThumbnailData &data);
    static bool DeleteDistributeLcdData(ThumbRdbOpt &opts, ThumbnailData &thumbnailData);
#endif
    static bool DeleteThumbFile(ThumbnailData &data, ThumbnailType type);
    static bool DeleteDistributeThumbnailInfo(ThumbRdbOpt &opts);

    static bool GetKvResultSet(const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore, const std::string &key,
        const std::string &networkId, std::shared_ptr<DataShare::ResultSetBridge> &outResultSet);
    static bool DeleteOriginImage(ThumbRdbOpt &opts);
    static std::string GetThumbPath(const std::string &path, const std::string &key);
    // Steps
    static bool LoadSourceImage(ThumbnailData &data, const Size &desiredSize, const bool isThumbnail = true);
    static bool GenTargetPixelmap(ThumbnailData &data, const Size &desiredSize);
    static DistributedKv::Status SaveThumbnailData(ThumbnailData &data, const std::string &networkId,
        const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore);

    static DistributedKv::Status SaveLcdData(ThumbnailData &data, const std::string &networkId,
        const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore);
    static int TrySaveFile(ThumbnailData &Data, ThumbnailType type);
    static bool UpdateLcdInfo(ThumbRdbOpt &opts, ThumbnailData &data, int &err);
    static bool UpdateVisitTime(ThumbRdbOpt &opts, ThumbnailData &data, int &err);
    static bool DoUpdateRemoteThumbnail(ThumbRdbOpt &opts, ThumbnailData &data, int &err);

    // RDB Store generate and aging
    static bool QueryHasLcdFiles(ThumbRdbOpt &opts, std::vector<ThumbnailData> &infos, int &err);
    static bool QueryHasThumbnailFiles(ThumbRdbOpt &opts, std::vector<ThumbnailData> &infos, int &err);
    static bool QueryLcdCount(ThumbRdbOpt &opts, int &outLcdCount, int &err);
    static bool QueryDistributeLcdCount(ThumbRdbOpt &opts, int &outLcdCount, int &err);
    static bool QueryAgingLcdInfos(ThumbRdbOpt &opts, int LcdLimit,
        std::vector<ThumbnailData> &infos, int &err);
    static bool QueryAgingDistributeLcdInfos(ThumbRdbOpt &opts, int LcdLimit,
        std::vector<ThumbnailData> &infos, int &err);
    static bool QueryNoLcdInfos(ThumbRdbOpt &opts, int LcdLimit, std::vector<ThumbnailData> &infos, int &err);
    static bool QueryNoThumbnailInfos(ThumbRdbOpt &opts, std::vector<ThumbnailData> &infos, int &err);
    static bool QueryDeviceThumbnailRecords(ThumbRdbOpt &opts, std::vector<ThumbnailData> &infos, int &err);

private:
    static int32_t SetSource(std::shared_ptr<AVMetadataHelper> avMetadataHelper, const std::string &path);
    static int64_t UTCTimeMilliSeconds();
    static void ParseQueryResult(const std::shared_ptr<NativeRdb::ResultSet> &resultSet,
        ThumbnailData &data, int &err);
    static void ParseStringResult(const std::shared_ptr<NativeRdb::ResultSet> &resultSet,
        int index, std::string &data, int &err);

    static bool CheckResultSetCount(const std::shared_ptr<NativeRdb::ResultSet> &resultSet, int &err);
    // utils
    static Size ConvertDecodeSize(const Size &sourceSize, const Size &desiredSize, const bool isThumbnail);
    static bool LoadImageFile(ThumbnailData &data, const bool isThumbnail, const Size &desiredSize);
    static bool LoadVideoFile(ThumbnailData &data, const bool isThumbnail, const Size &desiredSize);
    static bool LoadAudioFile(ThumbnailData &data, const bool isThumbnail, const Size &desiredSize);
    static std::string GetUdid();
    // KV Store
    static DistributedKv::Status SaveImage(const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore,
        const std::string &key, const std::vector<uint8_t> &image);

    // RDB Store
    static bool GetRemoteThumbnailInfo(ThumbRdbOpt &opts, const std::string &id,
        const std::string &udid, int &err);
    static bool GetUdidByNetworkId(ThumbRdbOpt &opts, const std::string &networkId,
        std::string &outUdid, int &err);
    static bool UpdateRemoteThumbnailInfo(ThumbRdbOpt &opts, ThumbnailData &data, int &err);
    static bool InsertRemoteThumbnailInfo(ThumbRdbOpt &opts, ThumbnailData &data, int &err);
    static bool CleanDistributeLcdInfo(ThumbRdbOpt &opts);
};
} // namespace Media
} // namespace OHOS

#endif // FRAMEWORKS_SERVICES_THUMBNAIL_SERVICE_INCLUDE_THUMBNAIL_UTILS_H_
