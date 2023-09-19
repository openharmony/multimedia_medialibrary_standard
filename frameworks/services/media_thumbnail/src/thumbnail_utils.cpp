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
#define MLOG_TAG "Thumbnail"

#include "thumbnail_utils.h"

#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>

#include "cloud_sync_helper.h"
#include "datashare_abs_result_set.h"
#ifdef DISTRIBUTED
#include "device_manager.h"
#endif
#include "distributed_kv_data_manager.h"
#include "hitrace_meter.h"
#include "image_packer.h"
#include "medialibrary_common_utils.h"
#include "medialibrary_errno.h"
#include "medialibrary_sync_operation.h"
#include "medialibrary_tracer.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "mimetype_utils.h"
#include "parameter.h"
#include "post_proc.h"
#include "rdb_errno.h"
#include "rdb_predicates.h"
#include "thumbnail_const.h"
#include "unique_fd.h"

using namespace std;
using namespace OHOS::DistributedKv;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {
constexpr int32_t KEY_INDEX = 0;
constexpr int32_t VALUE_INDEX = 1;
constexpr float EPSILON = 1e-6;
bool ThumbnailUtils::UpdateRemotePath(string &path, const string &networkId)
{
    MEDIA_DEBUG_LOG("ThumbnailUtils::UpdateRemotePath IN path = %{private}s, networkId = %{private}s",
        path.c_str(), networkId.c_str());
    if (path.empty() || networkId.empty()) {
        return false;
    }

    size_t pos = path.find(MEDIA_DATA_DEVICE_PATH);
    if (pos == string::npos) {
        return false;
    }

    path.replace(pos, MEDIA_DATA_DEVICE_PATH.size(), networkId);
    return true;
}

#ifdef DISTRIBUTED
bool ThumbnailUtils::DeleteLcdData(ThumbRdbOpt &opts, ThumbnailData &thumbnailData)
{
    if (thumbnailData.lcdKey.empty()) {
        MEDIA_ERR_LOG("lcd Key is empty");
        return false;
    }

    if (IsImageExist(thumbnailData.lcdKey, opts.networkId, opts.kvStore)) {
        if (!RemoveDataFromKv(opts.kvStore, thumbnailData.lcdKey)) {
            MEDIA_ERR_LOG("ThumbnailUtils::RemoveDataFromKv faild");
            return false;
        }
        if (!CleanThumbnailInfo(opts, false, true)) {
            return false;
        }
    }

    return true;
}

bool ThumbnailUtils::DeleteDistributeLcdData(ThumbRdbOpt &opts, ThumbnailData &thumbnailData)
{
    if (thumbnailData.lcdKey.empty()) {
        MEDIA_ERR_LOG("lcd Key is empty");
        return false;
    }

    if (IsImageExist(thumbnailData.lcdKey, opts.networkId, opts.kvStore)) {
        if (!RemoveDataFromKv(opts.kvStore, thumbnailData.lcdKey)) {
            MEDIA_ERR_LOG("ThumbnailUtils::RemoveDataFromKv faild");
            return false;
        }
        if (!CleanDistributeLcdInfo(opts)) {
            return false;
        }
    }

    return true;
}
#endif

static string GetThumbnailSuffix(ThumbnailType type)
{
    string suffix;
    switch (type) {
        case ThumbnailType::YEAR:
            suffix = THUMBNAIL_YEAR_SUFFIX;
            break;
        case ThumbnailType::MTH:
            suffix = THUMBNAIL_MTH_SUFFIX;
            break;
        case ThumbnailType::THUMB:
            suffix = THUMBNAIL_THUMB_SUFFIX;
            break;
        case ThumbnailType::LCD:
            suffix = THUMBNAIL_LCD_SUFFIX;
            break;
        default:
            return "";
    }
    return suffix;
}

bool ThumbnailUtils::DeleteThumbFile(ThumbnailData &data, ThumbnailType type)
{
    string fileName = GetThumbnailPath(data.path, GetThumbnailSuffix(type));
    if (!MediaFileUtils::DeleteFile(fileName)) {
        MEDIA_ERR_LOG("delete file faild %{public}d", errno);
        return false;
    }
    return true;
}

bool ThumbnailUtils::LoadAudioFile(ThumbnailData &data, const bool isThumbnail, const Size &desiredSize)
{
    shared_ptr<AVMetadataHelper> avMetadataHelper = AVMetadataHelperFactory::CreateAVMetadataHelper();
    string path = data.path;
    int32_t err = SetSource(avMetadataHelper, path);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Av meta data helper set source failed %{public}d", err);
        return false;
    }

    auto audioPicMemory = avMetadataHelper->FetchArtPicture();
    if (audioPicMemory == nullptr) {
        MEDIA_ERR_LOG("FetchArtPicture failed!");
        return false;
    }

    SourceOptions opts;
    uint32_t errCode = 0;
    unique_ptr<ImageSource> audioImageSource = ImageSource::CreateImageSource(audioPicMemory->GetBase(),
        audioPicMemory->GetSize(), opts, errCode);
    if (audioImageSource == nullptr) {
        MEDIA_ERR_LOG("Failed to create image source! path %{private}s errCode %{public}d",
            path.c_str(), errCode);
        return false;
    }

    ImageInfo imageInfo;
    errCode = audioImageSource->GetImageInfo(0, imageInfo);
    if (errCode != E_OK) {
        MEDIA_ERR_LOG("Failed to get image info, path: %{private}s err: %{public}d", path.c_str(), errCode);
        return false;
    }

    DecodeOptions decOpts;
    decOpts.desiredSize = ConvertDecodeSize(imageInfo.size, desiredSize, isThumbnail);
    decOpts.desiredPixelFormat = PixelFormat::BGRA_8888;
    data.source = audioImageSource->CreatePixelMap(decOpts, errCode);
    if ((errCode != E_OK) || (data.source == nullptr)) {
        MEDIA_ERR_LOG("Av meta data helper fetch frame at time failed");
        return false;
    }
    return true;
}

bool ThumbnailUtils::LoadVideoFile(ThumbnailData &data, const bool isThumbnail, const Size &desiredSize)
{
    shared_ptr<AVMetadataHelper> avMetadataHelper = AVMetadataHelperFactory::CreateAVMetadataHelper();
    string path = data.path;
    int32_t err = SetSource(avMetadataHelper, path);
    if (err != 0) {
        MEDIA_ERR_LOG("Av meta data helper set source failed path %{private}s err %{public}d",
            path.c_str(), err);
        return false;
    }
    PixelMapParams param;
    param.colorFormat = PixelFormat::RGBA_8888;
    data.source = avMetadataHelper->FetchFrameAtTime(AV_FRAME_TIME, AVMetadataQueryOption::AV_META_QUERY_NEXT_SYNC,
        param);
    if (data.source == nullptr) {
        MEDIA_ERR_LOG("Av meta data helper fetch frame at time failed");
        return false;
    }

    auto resultMap = avMetadataHelper->ResolveMetadata();
    string videoOrientation = resultMap.at(AV_KEY_VIDEO_ORIENTATION);
    if (!videoOrientation.empty()) {
        std::istringstream iss(videoOrientation);
        iss >> data.degrees;
    }
    return true;
}

// gen pixelmap from data.souce, should ensure source is not null
bool ThumbnailUtils::GenTargetPixelmap(ThumbnailData &data, const Size &desiredSize)
{
    MediaLibraryTracer tracer;
    tracer.Start("GenTargetPixelmap");
    if (data.source == nullptr) {
        return false;
    }
    float widthScale = (1.0f * desiredSize.width) / data.source->GetWidth();
    float heightScale = (1.0f * desiredSize.height) / data.source->GetHeight();
    data.source->scale(widthScale, heightScale);
    return true;
}

bool ThumbnailUtils::LoadImageFile(ThumbnailData &data, const bool isThumbnail, const Size &desiredSize)
{
    mallopt(M_SET_THREAD_CACHE, M_THREAD_CACHE_DISABLE);
    mallopt(M_DELAYED_FREE, M_DELAYED_FREE_DISABLE);

    MediaLibraryTracer tracer;
    tracer.Start("ImageSource::CreateImageSource");

    uint32_t err = 0;
    SourceOptions opts;
    string path = data.path;
    unique_ptr<ImageSource> imageSource = ImageSource::CreateImageSource(path, opts, err);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to create image source, path: %{private}s err: %{public}d", path.c_str(), err);
        return false;
    }
    tracer.Finish();

    tracer.Start("imageSource->CreatePixelMap");
    ImageInfo imageInfo;
    err = imageSource->GetImageInfo(0, imageInfo);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to get image info, path: %{private}s err: %{public}d", path.c_str(), err);
        return false;
    }

    DecodeOptions decodeOpts;
    decodeOpts.desiredSize = ConvertDecodeSize(imageInfo.size, desiredSize, isThumbnail);
    decodeOpts.desiredPixelFormat = PixelFormat::BGRA_8888;
    data.source = imageSource->CreatePixelMap(decodeOpts, err);
    if ((err != E_OK) || (data.source == nullptr)) {
        MEDIA_ERR_LOG("Failed to create pixelmap path %{private}s err %{public}d",
            path.c_str(), err);
        return false;
    }
    tracer.Finish();

    int intTempMeta;
    err = imageSource->GetImagePropertyInt(0, MEDIA_DATA_IMAGE_ORIENTATION, intTempMeta);
    if (err == E_OK) {
        data.degrees = static_cast<float>(intTempMeta);
    }
    return true;
}

string ThumbnailUtils::GetUdid()
{
#ifdef DISTRIBUTED
    static string innerUdid;

    if (!innerUdid.empty()) {
        return innerUdid;
    }

    auto &deviceManager = OHOS::DistributedHardware::DeviceManager::GetInstance();
    OHOS::DistributedHardware::DmDeviceInfo deviceInfo;
    auto ret = deviceManager.GetLocalDeviceInfo(BUNDLE_NAME, deviceInfo);
    if (ret != ERR_OK) {
        MEDIA_ERR_LOG("get local device info failed, ret %{public}d", ret);
        return string();
    }

    ret = deviceManager.GetUdidByNetworkId(BUNDLE_NAME, deviceInfo.networkId, innerUdid);
    if (ret != 0) {
        MEDIA_ERR_LOG("GetDeviceUdid error networkId = %{private}s, ret %{public}d",
            deviceInfo.networkId, ret);
        return string();
    }
    return innerUdid;
#endif
    return "";
}

bool ThumbnailUtils::CompressImage(shared_ptr<PixelMap> &pixelMap, vector<uint8_t> &data)
{
    PackOption option = {
        .format = THUMBNAIL_FORMAT,
        .quality = THUMBNAIL_QUALITY,
        .numberHint = NUMBER_HINT_1
    };
    data.resize(pixelMap->GetByteCount());

    MediaLibraryTracer tracer;
    tracer.Start("imagePacker.StartPacking");
    ImagePacker imagePacker;
    uint32_t err = imagePacker.StartPacking(data.data(), data.size(), option);
    tracer.Finish();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to StartPacking %{public}d", err);
        return false;
    }

    tracer.Start("imagePacker.AddImage");
    err = imagePacker.AddImage(*pixelMap);
    tracer.Finish();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to StartPacking %{public}d", err);
        return false;
    }

    tracer.Start("imagePacker.FinalizePacking");
    int64_t packedSize = 0;
    err = imagePacker.FinalizePacking(packedSize);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to StartPacking %{public}d", err);
        return false;
    }

    data.resize(packedSize);
    return true;
}

Status ThumbnailUtils::SaveImage(const shared_ptr<SingleKvStore> &kvStore, const string &key,
    const vector<uint8_t> &image)
{
    MEDIA_DEBUG_LOG("ThumbnailUtils::SaveImage IN key [%{public}s]", key.c_str());
    Status status = Status::ERROR;
    if (kvStore == nullptr) {
        MEDIA_ERR_LOG("KvStore is not init");
        return status;
    }

    MediaLibraryTracer tracer;
    tracer.Start("SaveImage kvStore->Put");
    Value val(image);
    status = kvStore->Put(key, val);
    return status;
}

shared_ptr<ResultSet> ThumbnailUtils::QueryThumbnailSet(ThumbRdbOpt &opts)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_MEDIA_TYPE,
    };

    vector<string> selectionArgs;
    string strQueryCondition = MEDIA_DATA_DB_ID + " = " + opts.row;

    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.SetWhereClause(strQueryCondition);
    rdbPredicates.SetWhereArgs(selectionArgs);
    return opts.store->QueryByStep(rdbPredicates, column);
}

shared_ptr<ResultSet> ThumbnailUtils::QueryThumbnailInfo(ThumbRdbOpt &opts,
    ThumbnailData &data, int &err)
{
    MediaLibraryTracer tracer;
    tracer.Start("QueryThumbnailInfo");
    auto resultSet = QueryThumbnailSet(opts);
    if (!CheckResultSetCount(resultSet, err)) {
        return nullptr;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        return nullptr;
    }

    ParseQueryResult(resultSet, data, err);
    return resultSet;
}

bool ThumbnailUtils::QueryLcdCount(ThumbRdbOpt &opts, int &outLcdCount, int &err)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_TIME_VISIT, "0");
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_FILE));
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM));
    auto resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (resultSet == nullptr) {
        return false;
    }
    int rowCount = 0;
    err = resultSet->GetRowCount(rowCount);
    resultSet.reset();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to get row count %{public}d", err);
        return false;
    }
    MEDIA_DEBUG_LOG("rowCount is %{public}d", rowCount);
    if (rowCount <= 0) {
        MEDIA_INFO_LOG("No match! %{public}s", rdbPredicates.ToString().c_str());
        rowCount = 0;
    }

    outLcdCount = rowCount;
    return true;
}

bool ThumbnailUtils::QueryDistributeLcdCount(ThumbRdbOpt &opts, int &outLcdCount, int &err)
{
    vector<string> column = {
        REMOTE_THUMBNAIL_DB_ID,
    };
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, opts.udid);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_LCD);
    auto resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (resultSet == nullptr) {
        return false;
    }
    int rowCount = 0;
    err = resultSet->GetRowCount(rowCount);
    resultSet.reset();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to get row count %{public}d", err);
        return false;
    }
    MEDIA_INFO_LOG("rowCount is %{public}d", rowCount);
    if (rowCount <= 0) {
        MEDIA_INFO_LOG("No match! %{public}s", rdbPredicates.ToString().c_str());
        rowCount = 0;
    }
    outLcdCount = rowCount;
    return true;
}

bool ThumbnailUtils::QueryHasLcdFiles(ThumbRdbOpt &opts, vector<ThumbnailData> &infos, int &err)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_THUMBNAIL,
        MEDIA_DATA_DB_LCD,
        MEDIA_DATA_DB_MEDIA_TYPE,
        MEDIA_DATA_DB_DATE_MODIFIED
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_LCD);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        if (!data.path.empty()) {
            infos.push_back(data);
        }
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::QueryHasThumbnailFiles(ThumbRdbOpt &opts, vector<ThumbnailData> &infos, int &err)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_THUMBNAIL,
        MEDIA_DATA_DB_LCD,
        MEDIA_DATA_DB_MEDIA_TYPE,
        MEDIA_DATA_DB_DATE_MODIFIED
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_THUMBNAIL);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        if (!data.path.empty()) {
            infos.push_back(data);
        }
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::QueryAgingDistributeLcdInfos(ThumbRdbOpt &opts, int LcdLimit,
    vector<ThumbnailData> &infos, int &err)
{
    vector<string> column = {
        REMOTE_THUMBNAIL_DB_FILE_ID,
        MEDIA_DATA_DB_LCD
    };
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.IsNotNull(MEDIA_DATA_DB_LCD);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, opts.udid);

    rdbPredicates.Limit(LcdLimit);
    rdbPredicates.OrderByAsc(MEDIA_DATA_DB_TIME_VISIT);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        if (!data.lcdKey.empty()) {
            infos.push_back(data);
        }
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::QueryAgingLcdInfos(ThumbRdbOpt &opts, int LcdLimit,
    vector<ThumbnailData> &infos, int &err)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_MEDIA_TYPE,
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_FILE));
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM));

    rdbPredicates.Limit(LcdLimit);
    rdbPredicates.OrderByAsc(MEDIA_DATA_DB_TIME_VISIT);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        if (!data.path.empty()) {
            infos.push_back(data);
        }
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::QueryNoLcdInfos(ThumbRdbOpt &opts, int LcdLimit, vector<ThumbnailData> &infos, int &err)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_MEDIA_TYPE,
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.EqualTo(MEDIA_DATA_DB_TIME_VISIT, "0");
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_FILE));
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM));
    rdbPredicates.EqualTo(MEDIA_DATA_DB_IS_TRASH, "0");
    rdbPredicates.EqualTo(MEDIA_DATA_DB_TIME_PENDING, "0");

    rdbPredicates.Limit(LcdLimit);
    rdbPredicates.OrderByDesc(MEDIA_DATA_DB_DATE_ADDED);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        if (!data.path.empty()) {
            infos.push_back(data);
        }
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::QueryNoThumbnailInfos(ThumbRdbOpt &opts, vector<ThumbnailData> &infos, int &err)
{
    vector<string> column = {
        MEDIA_DATA_DB_ID,
        MEDIA_DATA_DB_FILE_PATH,
        MEDIA_DATA_DB_MEDIA_TYPE,
    };
    RdbPredicates rdbPredicates(opts.table);
    rdbPredicates.EqualTo(MEDIA_DATA_DB_TIME_VISIT, "0");
    rdbPredicates.EqualTo(MEDIA_DATA_DB_IS_TRASH, "0");
    rdbPredicates.EqualTo(MEDIA_DATA_DB_TIME_PENDING, "0");
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_ALBUM));
    rdbPredicates.NotEqualTo(MEDIA_DATA_DB_MEDIA_TYPE, to_string(MEDIA_TYPE_FILE));

    rdbPredicates.Limit(THUMBNAIL_QUERY_MAX);
    rdbPredicates.OrderByDesc(MEDIA_DATA_DB_DATE_ADDED);

    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        if (err == E_EMPTY_VALUES_BUCKET) {
            return true;
        }
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        if (!data.path.empty()) {
            infos.push_back(data);
        }
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::UpdateLcdInfo(ThumbRdbOpt &opts, ThumbnailData &data, int &err)
{
    ValuesBucket values;
    int changedRows;

    int64_t timeNow = UTCTimeMilliSeconds();
    values.PutLong(MEDIA_DATA_DB_TIME_VISIT, timeNow);

    MediaLibraryTracer tracer;
    tracer.Start("UpdateLcdInfo opts.store->Update");
    err = opts.store->Update(changedRows, opts.table, values, MEDIA_DATA_DB_ID + " = ?",
        vector<string> { opts.row });
    if (err != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Update failed! %{public}d", err);
        return false;
    }
    return true;
}

bool ThumbnailUtils::UpdateVisitTime(ThumbRdbOpt &opts, ThumbnailData &data, int &err)
{
    if (!opts.networkId.empty()) {
        return DoUpdateRemoteThumbnail(opts, data, err);
    }

    ValuesBucket values;
    int changedRows;
    int64_t timeNow = UTCTimeMilliSeconds();
    values.PutLong(MEDIA_DATA_DB_TIME_VISIT, timeNow);
    err = opts.store->Update(changedRows, opts.table, values, MEDIA_DATA_DB_ID + " = ?",
        vector<string> { opts.row });
    if (err != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Update failed! %{public}d", err);
        return false;
    }
    return true;
}

bool ThumbnailUtils::QueryDeviceThumbnailRecords(ThumbRdbOpt &opts, vector<ThumbnailData> &infos,
    int &err)
{
    vector<string> column = {
        REMOTE_THUMBNAIL_DB_FILE_ID,
        MEDIA_DATA_DB_THUMBNAIL,
        MEDIA_DATA_DB_LCD
    };
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, opts.udid);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    ThumbnailData data;
    do {
        ParseQueryResult(resultSet, data, err);
        infos.push_back(data);
    } while (resultSet->GoToNextRow() == E_OK);
    return true;
}

bool ThumbnailUtils::GetRemoteThumbnailInfo(ThumbRdbOpt &opts, const string &id,
    const string &udid, int &err)
{
    vector<string> column = {
        REMOTE_THUMBNAIL_DB_ID
    };
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_FILE_ID, id);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, udid);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }
    return true;
}

bool ThumbnailUtils::GetUdidByNetworkId(ThumbRdbOpt &opts, const string &networkId,
    string &outUdid, int &err)
{
    vector<string> column = {
        DEVICE_DB_ID,
        DEVICE_DB_UDID
    };
    RdbPredicates rdbPredicates(DEVICE_TABLE);
    rdbPredicates.EqualTo(DEVICE_DB_NETWORK_ID, networkId);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }
    int index;
    err = resultSet->GetColumnIndex(DEVICE_DB_UDID, index);
    if (err == NativeRdb::E_OK) {
        ParseStringResult(resultSet, index, outUdid, err);
    } else {
        MEDIA_ERR_LOG("Get column index error %{public}d", err);
    }
    return true;
}

bool ThumbnailUtils::QueryRemoteThumbnail(ThumbRdbOpt &opts, ThumbnailData &data, int &err)
{
    if (data.udid.empty() && !GetUdidByNetworkId(opts, opts.networkId, data.udid, err)) {
        MEDIA_ERR_LOG("GetUdidByNetworkId failed! %{public}d", err);
        return false;
    }

    vector<string> column = {
        REMOTE_THUMBNAIL_DB_ID,
        MEDIA_DATA_DB_THUMBNAIL,
        MEDIA_DATA_DB_LCD
    };
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_FILE_ID, data.id);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, data.udid);
    shared_ptr<ResultSet> resultSet = opts.store->QueryByStep(rdbPredicates, column);
    if (!CheckResultSetCount(resultSet, err)) {
        MEDIA_ERR_LOG("CheckResultSetCount failed %{public}d", err);
        return false;
    }

    err = resultSet->GoToFirstRow();
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed GoToFirstRow %{public}d", err);
        return false;
    }

    int index;
    err = resultSet->GetColumnIndex(MEDIA_DATA_DB_LCD, index);
    if (err == NativeRdb::E_OK) {
        ParseStringResult(resultSet, index, data.lcdKey, err);
    }

    err = resultSet->GetColumnIndex(MEDIA_DATA_DB_THUMBNAIL, index);
    if (err == NativeRdb::E_OK) {
        ParseStringResult(resultSet, index, data.thumbnailKey, err);
    }
    return true;
}

static inline bool IsKeyNotSame(const string &newKey, const string &oldKey)
{
    return !newKey.empty() && !oldKey.empty() && (newKey != oldKey);
}

bool ThumbnailUtils::DoUpdateRemoteThumbnail(ThumbRdbOpt &opts, ThumbnailData &data, int &err)
{
    if (opts.networkId.empty()) {
        return false;
    }
    if (data.thumbnailKey.empty() && data.lcdKey.empty()) {
        return false;
    }
    ThumbnailData tmpData = data;
    auto isGot = ThumbnailUtils::QueryRemoteThumbnail(opts, tmpData, err);
    if (isGot) {
        if (IsKeyNotSame(data.thumbnailKey, tmpData.thumbnailKey)) {
            if (!RemoveDataFromKv(opts.kvStore, tmpData.thumbnailKey)) {
                return false;
            }
        }
        if (IsKeyNotSame(data.lcdKey, tmpData.lcdKey)) {
            if (!RemoveDataFromKv(opts.kvStore, tmpData.lcdKey)) {
                return false;
            }
        }
    }

    data.udid = tmpData.udid;
    if (isGot) {
        return UpdateRemoteThumbnailInfo(opts, data, err);
    } else {
        return InsertRemoteThumbnailInfo(opts, data, err);
    }
}

bool ThumbnailUtils::UpdateRemoteThumbnailInfo(ThumbRdbOpt &opts, ThumbnailData &data, int &err)
{
    RdbPredicates rdbPredicates(REMOTE_THUMBNAIL_TABLE);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_FILE_ID, data.id);
    rdbPredicates.EqualTo(REMOTE_THUMBNAIL_DB_UDID, data.udid);

    ValuesBucket values;
    if (!data.thumbnailKey.empty()) {
        values.PutString(MEDIA_DATA_DB_THUMBNAIL, data.thumbnailKey);
    }

    if (!data.lcdKey.empty()) {
        values.PutString(MEDIA_DATA_DB_LCD, data.lcdKey);
        int64_t timeNow = UTCTimeMilliSeconds();
        values.PutLong(MEDIA_DATA_DB_TIME_VISIT, timeNow);
    }

    int changedRows;
    err = opts.store->Update(changedRows, values, rdbPredicates);
    if (err != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Update failed! %{public}d", err);
        return false;
    }

    return true;
}

bool ThumbnailUtils::InsertRemoteThumbnailInfo(ThumbRdbOpt &opts, ThumbnailData &data, int &err)
{
    ValuesBucket values;
    values.PutInt(REMOTE_THUMBNAIL_DB_FILE_ID, stoi(data.id));
    values.PutString(REMOTE_THUMBNAIL_DB_UDID, data.udid);
    if (!data.thumbnailKey.empty()) {
        values.PutString(MEDIA_DATA_DB_THUMBNAIL, data.thumbnailKey);
    }

    if (!data.lcdKey.empty()) {
        values.PutString(MEDIA_DATA_DB_LCD, data.lcdKey);
        int64_t timeNow = UTCTimeMilliSeconds();
        values.PutLong(MEDIA_DATA_DB_TIME_VISIT, timeNow);
    }

    int64_t outRowId = -1;
    err = opts.store->Insert(outRowId, REMOTE_THUMBNAIL_TABLE, values);
    if (err != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Update failed! %{public}d", err);
        return false;
    }
    return true;
}

bool ThumbnailUtils::CleanThumbnailInfo(ThumbRdbOpt &opts, bool withThumb, bool withLcd)
{
    ValuesBucket values;
    if (withThumb) {
        values.PutNull(MEDIA_DATA_DB_THUMBNAIL);
    }
    if (withLcd) {
        values.PutNull(MEDIA_DATA_DB_LCD);
        values.PutLong(MEDIA_DATA_DB_TIME_VISIT, 0);
        values.PutInt(MEDIA_DATA_DB_DIRTY, static_cast<int32_t>(DirtyType::TYPE_SYNCED));
    }
    int changedRows;
    auto err = opts.store->Update(changedRows, opts.table, values, MEDIA_DATA_DB_ID + " = ?",
        vector<string> { opts.row });
    if (err != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Update failed! %{public}d", err);
        return false;
    }
    return true;
}

bool ThumbnailUtils::CleanDistributeLcdInfo(ThumbRdbOpt &opts)
{
    string udid;
    int err;
    if (!GetUdidByNetworkId(opts, opts.networkId, udid, err)) {
        MEDIA_ERR_LOG("GetUdidByNetworkId failed! %{public}d", err);
        return false;
    }

    ValuesBucket values;
    values.PutNull(MEDIA_DATA_DB_LCD);
    values.PutLong(MEDIA_DATA_DB_TIME_VISIT, 0);
    int changedRows;
    vector<string> whereArgs = { udid, opts.row };
    string deleteCondition = REMOTE_THUMBNAIL_DB_UDID + " = ? AND " +
        REMOTE_THUMBNAIL_DB_FILE_ID + " = ?";
    auto ret = opts.store->Update(changedRows, REMOTE_THUMBNAIL_TABLE, values, deleteCondition, whereArgs);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Delete failed! %{public}d", ret);
        return false;
    }
    return true;
}

bool ThumbnailUtils::DeleteDistributeThumbnailInfo(ThumbRdbOpt &opts)
{
    int changedRows;
    vector<string> whereArgs = { opts.udid, opts.row };
    string deleteCondition = REMOTE_THUMBNAIL_DB_UDID + " = ? AND " +
        REMOTE_THUMBNAIL_DB_FILE_ID + " = ?";
    auto err = opts.store->Delete(changedRows, REMOTE_THUMBNAIL_TABLE, deleteCondition, whereArgs);
    if (err != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("RdbStore Delete failed! %{public}d", err);
        return false;
    }
    return true;
}

Size ThumbnailUtils::ConvertDecodeSize(const Size &sourceSize, const Size &desiredSize, const bool isThumbnail)
{
    float desiredScale = static_cast<float>(desiredSize.height) / static_cast<float>(desiredSize.width);
    float sourceScale = static_cast<float>(sourceSize.height) / static_cast<float>(sourceSize.width);
    float scale = 1.0f;
    if ((sourceScale - desiredScale > EPSILON) ^ isThumbnail) {
        scale = (float)desiredSize.height / sourceSize.height;
    } else {
        scale = (float)desiredSize.width / sourceSize.width;
    }
    scale = scale < 1.0f ? scale : 1.0f;
    Size decodeSize = {
        static_cast<int32_t> (scale * sourceSize.width),
        static_cast<int32_t> (scale * sourceSize.height),
    };
    return decodeSize;
}

bool ThumbnailUtils::LoadSourceImage(ThumbnailData &data, const Size &desiredSize, const bool isThumbnail)
{
    if (data.source != nullptr) {
        return true;
    }
    MediaLibraryTracer tracer;
    tracer.Start("LoadSourceImage");
    if (data.mediaType == -1) {
        auto extension = MediaFileUtils::GetExtensionFromPath(data.path);
        auto mimeType = MimeTypeUtils::GetMimeTypeFromExtension(extension);
        data.mediaType = MimeTypeUtils::GetMediaTypeFromMimeType(mimeType);
    }

    bool ret = false;
    data.degrees = 0.0;
    if (data.mediaType == MEDIA_TYPE_VIDEO) {
        ret = LoadVideoFile(data, isThumbnail, desiredSize);
    } else if (data.mediaType == MEDIA_TYPE_AUDIO) {
        ret = LoadAudioFile(data, isThumbnail, desiredSize);
    } else {
        ret = LoadImageFile(data, isThumbnail, desiredSize);
    }
    if (!ret || (data.source == nullptr)) {
        return false;
    }
    tracer.Finish();

    if (isThumbnail) {
        tracer.Start("CenterScale");
        PostProc postProc;
        if (!postProc.CenterScale(desiredSize, *data.source)) {
            MEDIA_ERR_LOG("thumbnail center crop failed [%{public}s]", data.id.c_str());
            return false;
        }
    }
    data.source->SetAlphaType(AlphaType::IMAGE_ALPHA_TYPE_UNPREMUL);
    data.source->rotate(data.degrees);
    return true;
}

static int SaveFile(const string &fileName, uint8_t *output, int writeSize)
{
    const mode_t fileMode = 0664;
    mode_t mask = umask(0);
    UniqueFd fd(open(fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, fileMode));
    umask(mask);
    if (fd.Get() < 0) {
        if (errno == EEXIST) {
            UniqueFd fd(open(fileName.c_str(), O_WRONLY | O_TRUNC, fileMode));
        }
        if (fd.Get() < 0) {
            MEDIA_ERR_LOG("save failed! filePath %{private}s status %{public}d", fileName.c_str(), errno);
            return -errno;
        }
    }
    int ret = write(fd.Get(), output, writeSize);
    if (ret < 0) {
        return -errno;
    } else {
        return ret;
    }
}

int ThumbnailUtils::TrySaveFile(ThumbnailData &data, ThumbnailType type)
{
    string suffix;
    uint8_t *output;
    int writeSize;
    switch (type) {
        case ThumbnailType::MTH:
        case ThumbnailType::YEAR:
            suffix = (type == ThumbnailType::MTH) ? THUMBNAIL_MTH_SUFFIX : THUMBNAIL_YEAR_SUFFIX;
            output = const_cast<uint8_t *>(data.source->GetPixels());
            writeSize = data.source->GetByteCount();
            break;
        case ThumbnailType::THUMB:
            suffix = THUMBNAIL_THUMB_SUFFIX;
            output = data.thumbnail.data();
            writeSize = data.thumbnail.size();
            break;
        case ThumbnailType::LCD:
            suffix = THUMBNAIL_LCD_SUFFIX;
            output = data.lcd.data();
            writeSize = data.lcd.size();
            break;
        default:
            return E_INVALID_ARGUMENTS;
    }
    if (writeSize <= 0) {
        return E_THUMBNAIL_LOCAL_CREATE_FAIL;
    }
    string fileName = GetThumbnailPath(data.path, suffix);
    string dir = MediaFileUtils::GetParentPath(fileName);
    if (!MediaFileUtils::CreateDirectory(dir)) {
        return -errno;
    }

    int ret = SaveFile(fileName, output, writeSize);
    if (ret < 0) {
        DeleteThumbFile(data, type);
        return ret;
    } else if (ret != writeSize) {
        DeleteThumbFile(data, type);
        return E_NO_SPACE;
    }
    return E_OK;
}

Status ThumbnailUtils::SaveThumbnailData(ThumbnailData &data, const string &networkId,
    const shared_ptr<SingleKvStore> &kvStore)
{
    Status status = SaveImage(kvStore, data.thumbnailKey, data.thumbnail);
    if (status != DistributedKv::Status::SUCCESS) {
        MEDIA_ERR_LOG("SaveImage failed! status %{public}d", status);
    }

    return status;
}

Status ThumbnailUtils::SaveLcdData(ThumbnailData &data, const string &networkId,
    const shared_ptr<SingleKvStore> &kvStore)
{
    Status status = SaveImage(kvStore, data.lcdKey, data.lcd);
    if (status != DistributedKv::Status::SUCCESS) {
        MEDIA_ERR_LOG("SaveLcdData SaveImage failed! status %{public}d", status);
    }
    return status;
}

int32_t ThumbnailUtils::SetSource(shared_ptr<AVMetadataHelper> avMetadataHelper, const string &path)
{
    if (avMetadataHelper == nullptr) {
        MEDIA_ERR_LOG("avMetadataHelper == nullptr");
        return E_ERR;
    }
    MEDIA_DEBUG_LOG("path = %{private}s", path.c_str());
    int32_t fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        MEDIA_ERR_LOG("Open file failed, err %{public}d", errno);
        return E_ERR;
    }

    struct stat64 st;
    if (fstat64(fd, &st) != 0) {
        MEDIA_ERR_LOG("Get file state failed, err %{public}d", errno);
        (void)close(fd);
        return E_ERR;
    }
    int64_t length = static_cast<int64_t>(st.st_size);
    int32_t ret = avMetadataHelper->SetSource(fd, 0, length, AV_META_USAGE_PIXEL_MAP);
    if (ret != 0) {
        MEDIA_ERR_LOG("SetSource fail");
        (void)close(fd);
        return E_ERR;
    }
    (void)close(fd);
    return SUCCESS;
}

bool ThumbnailUtils::ResizeImage(const vector<uint8_t> &data, const Size &size, unique_ptr<PixelMap> &pixelMap)
{
    MediaLibraryTracer tracer;
    tracer.Start("ResizeImage");
    if (data.size() == 0) {
        MEDIA_ERR_LOG("Data is empty");
        return false;
    }

    tracer.Start("ImageSource::CreateImageSource");
    uint32_t err = E_OK;
    SourceOptions opts;
    unique_ptr<ImageSource> imageSource = ImageSource::CreateImageSource(data.data(),
        data.size(), opts, err);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to create image source %{public}d", err);
        return false;
    }
    tracer.Finish();

    tracer.Start("imageSource->CreatePixelMap");
    DecodeOptions decodeOpts;
    decodeOpts.desiredSize.width = size.width;
    decodeOpts.desiredSize.height = size.height;
    decodeOpts.allocatorType = AllocatorType::SHARE_MEM_ALLOC;
    pixelMap = imageSource->CreatePixelMap(decodeOpts, err);
    if (err != Media::SUCCESS) {
        MEDIA_ERR_LOG("Failed to create pixelmap %{public}d", err);
        return false;
    }

    return true;
}

int ThumbnailUtils::GetPixelMapFromResult(const shared_ptr<DataShare::DataShareResultSet> &resultSet, const Size &size,
    unique_ptr<PixelMap> &outPixelMap)
{
    MediaLibraryTracer tracer;
    tracer.Start("ThumbnailUtils::GetKv");
    int ret = resultSet->GoToFirstRow();
    if (ret != DataShare::E_OK) {
        MEDIA_ERR_LOG("GoToFirstRow error %{public}d", ret);
        return ret;
    }

    vector<uint8_t> key;
    ret = resultSet->GetBlob(KEY_INDEX, key);
    if (ret != DataShare::E_OK) {
        MEDIA_ERR_LOG("GetBlob key error %{public}d", ret);
        return ret;
    }

    vector<uint8_t> image;
    ret = resultSet->GetBlob(VALUE_INDEX, image);
    if (ret != DataShare::E_OK) {
        MEDIA_ERR_LOG("GetBlob image error %{public}d", ret);
        return ret;
    }

    resultSet->Close();
    tracer.Finish();

    MEDIA_DEBUG_LOG("key %{public}s key len %{public}d len %{public}d", string(key.begin(),
        key.end()).c_str(), static_cast<int>(key.size()), static_cast<int>(image.size()));

    tracer.Start("ThumbnailUtils::ResizeImage");
    if (!ResizeImage(image, size, outPixelMap)) {
        MEDIA_ERR_LOG("ResizeImage error");
        return E_FAIL;
    }

    return ret;
}

bool ThumbnailUtils::GetKvResultSet(const shared_ptr<SingleKvStore> &kvStore, const string &key,
    const string &networkId, shared_ptr<DataShare::ResultSetBridge> &outResultSet)
{
    if (key.empty()) {
        MEDIA_ERR_LOG("key empty");
        return false;
    }

    if (kvStore == nullptr) {
        MEDIA_ERR_LOG("KvStore is not init");
        return false;
    }

    MediaLibraryTracer tracer;
    tracer.Start("GetKey kvStore->Get");
    shared_ptr<SingleKvStore> singleKv = kvStore;
    outResultSet = shared_ptr<DataShare::ResultSetBridge>(ThumbnailDataShareBridge::Create(singleKv, key));
    return true;
}

bool ThumbnailUtils::RemoveDataFromKv(const shared_ptr<SingleKvStore> &kvStore, const string &key)
{
    if (key.empty()) {
        MEDIA_ERR_LOG("RemoveLcdFromKv key empty");
        return false;
    }

    if (kvStore == nullptr) {
        MEDIA_ERR_LOG("KvStore is not init");
        return false;
    }

    MediaLibraryTracer tracer;
    tracer.Start("RemoveLcdFromKv kvStore->Get");
    auto status = kvStore->Delete(key);
    if (status != Status::SUCCESS) {
        MEDIA_ERR_LOG("Failed to get key [%{public}s] ret [%{public}d]", key.c_str(), status);
        return false;
    }
    return true;
}

// notice: return value is whether thumb/lcd is deleted
bool ThumbnailUtils::DeleteOriginImage(ThumbRdbOpt &opts)
{
    ThumbnailData tmpData;
    tmpData.path = opts.path;
    bool isDelete = false;
    if (opts.path.empty()) {
        int err = 0;
        auto rdbSet = QueryThumbnailInfo(opts, tmpData, err);
        if (rdbSet == nullptr) {
            MEDIA_ERR_LOG("QueryThumbnailInfo Faild [ %{public}d ]", err);
            return isDelete;
        }
    }
    if (DeleteThumbFile(tmpData, ThumbnailType::YEAR)) {
        isDelete = true;
    }
    if (DeleteThumbFile(tmpData, ThumbnailType::MTH)) {
        isDelete = true;
    }
    if (DeleteThumbFile(tmpData, ThumbnailType::THUMB)) {
        isDelete = true;
    }
    if (DeleteThumbFile(tmpData, ThumbnailType::LCD)) {
        isDelete = true;
    }
    string fileName = GetThumbnailPath(tmpData.path, "");
    MediaFileUtils::DeleteFile(MediaFileUtils::GetParentPath(fileName));
    return isDelete;
}

#ifdef DISTRIBUTED
bool ThumbnailUtils::IsImageExist(const string &key, const string &networkId, const shared_ptr<SingleKvStore> &kvStore)
{
    if (key.empty()) {
        return false;
    }

    if (kvStore == nullptr) {
        MEDIA_ERR_LOG("KvStore is not init");
        return false;
    }

    bool ret = false;
    DataQuery query;
    query.InKeys({key});
    int count = 0;
    auto status = kvStore->GetCount(query, count);
    if (status == Status::SUCCESS && count > 0) {
        ret = true;
    }

    if (!ret) {
        if (!networkId.empty()) {
            MediaLibraryTracer tracer;
            tracer.Start("SyncPullKvstore");
            vector<string> keys = { key };
            auto syncStatus = MediaLibrarySyncOperation::SyncPullKvstore(kvStore, keys, networkId);
            if (syncStatus == DistributedKv::Status::SUCCESS) {
                MEDIA_DEBUG_LOG("SyncPullKvstore SUCCESS");
                return true;
            } else {
                MEDIA_ERR_LOG("SyncPullKvstore failed! ret %{public}d", syncStatus);
                return false;
            }
        }
    }
    return ret;
}
#endif

int64_t ThumbnailUtils::UTCTimeMilliSeconds()
{
    struct timespec t;
    constexpr int64_t SEC_TO_MSEC = 1e3;
    constexpr int64_t MSEC_TO_NSEC = 1e6;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * SEC_TO_MSEC + t.tv_nsec / MSEC_TO_NSEC;
}

bool ThumbnailUtils::CheckResultSetCount(const shared_ptr<ResultSet> &resultSet, int &err)
{
    if (resultSet == nullptr) {
        return false;
    }
    int rowCount = 0;
    err = resultSet->GetRowCount(rowCount);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to get row count %{public}d", err);
        return false;
    }

    if (rowCount <= 0) {
        MEDIA_ERR_LOG("CheckCount No match!");
        err = E_EMPTY_VALUES_BUCKET;
        return false;
    }

    return true;
}

void ThumbnailUtils::ParseStringResult(const shared_ptr<ResultSet> &resultSet, int index, string &data, int &err)
{
    bool isNull = true;
    err = resultSet->IsColumnNull(index, isNull);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to check column %{public}d null %{public}d", index, err);
    }

    if (!isNull) {
        err = resultSet->GetString(index, data);
        if (err != E_OK) {
            MEDIA_ERR_LOG("Failed to get column %{public}d string %{public}d", index, err);
        }
    }
}

void ThumbnailUtils::ParseQueryResult(const shared_ptr<ResultSet> &resultSet, ThumbnailData &data, int &err)
{
    int index;
    err = resultSet->GetColumnIndex(MEDIA_DATA_DB_ID, index);
    if (err == NativeRdb::E_OK) {
        ParseStringResult(resultSet, index, data.id, err);
    }

    err = resultSet->GetColumnIndex(MEDIA_DATA_DB_FILE_PATH, index);
    if (err == NativeRdb::E_OK) {
        ParseStringResult(resultSet, index, data.path, err);
    }

    err = resultSet->GetColumnIndex(MEDIA_DATA_DB_MEDIA_TYPE, index);
    if (err == NativeRdb::E_OK) {
        data.mediaType = MediaType::MEDIA_TYPE_ALL;
        err = resultSet->GetInt(index, data.mediaType);
    }
}
} // namespace Media
} // namespace OHOS
