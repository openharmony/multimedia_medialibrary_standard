/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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
#define MLOG_TAG "Scanner"

#include "metadata_extractor.h"

#include <fcntl.h>
#include "hitrace_meter.h"
#include "media_exif.h"
#include "media_log.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "medialibrary_tracer.h"
#include "nlohmann/json.hpp"

namespace OHOS {
namespace Media {
using namespace std;

const double DEGREES2MINUTES = 60.0;
const double DEGREES2SECONDS = 3600.0;
constexpr int32_t OFFSET_NUM = 2;

template <class Type>
static Type stringToNum(const string &str)
{
    std::istringstream iss(str);
    Type num;
    iss >> num;
    return num;
}

double GetLongitudeLatitude(string inputStr)
{
    auto pos = inputStr.find(',');
    if (pos == string::npos) {
        return 0;
    }
    double ret = stringToNum<double>(inputStr.substr(0, pos));

    inputStr = inputStr.substr(pos + OFFSET_NUM);
    pos = inputStr.find(',');
    if (pos == string::npos) {
        return 0;
    }
    ret += stringToNum<double>(inputStr.substr(0, pos)) / DEGREES2MINUTES;

    inputStr = inputStr.substr(pos + OFFSET_NUM);
    ret += stringToNum<double>(inputStr) / DEGREES2SECONDS;
    return ret;
}

static time_t convertTimeStr2TimeStamp(string &timeStr)
{
    struct tm timeinfo;
    strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S",  &timeinfo);
    time_t timeStamp = mktime(&timeinfo);
    return timeStamp;
}

int32_t MetadataExtractor::ExtractImageExif(std::unique_ptr<ImageSource> &imageSource, std::unique_ptr<Metadata> &data)
{
    if (imageSource == nullptr) {
        MEDIA_ERR_LOG("Failed to obtain image source");
        return E_OK;
    }

    int32_t intTempMeta = 0;
    string propertyStr;
    nlohmann::json exifJson;
    uint32_t err = imageSource->GetImagePropertyInt(0, PHOTO_DATA_IMAGE_ORIENTATION, intTempMeta);
    exifJson[PHOTO_DATA_IMAGE_ORIENTATION] = (err == 0) ? intTempMeta: 0;

    err = imageSource->GetImagePropertyString(0, PHOTO_DATA_IMAGE_GPS_LONGITUDE, propertyStr);
    exifJson[PHOTO_DATA_IMAGE_GPS_LONGITUDE] = (err == 0) ? GetLongitudeLatitude(propertyStr): 0;

    err = imageSource->GetImagePropertyString(0, PHOTO_DATA_IMAGE_GPS_LATITUDE, propertyStr);
    exifJson[PHOTO_DATA_IMAGE_GPS_LONGITUDE] = (err == 0) ? GetLongitudeLatitude(propertyStr): 0;

    for (auto &exifKey : exifInfoKeys) {
        err = imageSource->GetImagePropertyString(0, exifKey, propertyStr);
        exifJson[exifKey] = (err == 0) ? propertyStr: "";
    }
    data->SetAllExif(exifJson.dump());

    err = imageSource->GetImagePropertyString(0, PHOTO_DATA_IMAGE_USER_COMMENT, propertyStr);
    if (err == 0) {
        data->SetUserComment(propertyStr);
    }

    return E_OK;
}

int32_t MetadataExtractor::ExtractImageMetadata(std::unique_ptr<Metadata> &data)
{
    uint32_t err = 0;

    SourceOptions opts;
    opts.formatHint = "image/" + data->GetFileExtension();
    std::unique_ptr<ImageSource> imageSource =
        ImageSource::CreateImageSource(data->GetFilePath(), opts, err);
    if (err != 0 || imageSource == nullptr) {
        MEDIA_ERR_LOG("Failed to obtain image source, err = %{public}d", err);
        return E_OK;
    }

    ImageInfo imageInfo;
    err = imageSource->GetImageInfo(0, imageInfo);
    if (err == 0) {
        data->SetFileWidth(imageInfo.size.width);
        data->SetFileHeight(imageInfo.size.height);
    } else {
        MEDIA_ERR_LOG("Failed to get image info, err = %{public}d", err);
    }

    string propertyStr;
    int64_t int64TempMeta = 0;
    err = imageSource->GetImagePropertyString(0, MEDIA_DATA_IMAGE_DATE_TIME_ORIGINAL, propertyStr);
    if (err == 0) {
        int64TempMeta = convertTimeStr2TimeStamp(propertyStr);
        if (int64TempMeta < 0) {
            data->SetDateTaken(data->GetFileDateModified());
        } else {
            data->SetDateTaken(int64TempMeta);
        }
    } else {
        // use modified time as date taken time when date taken not set
        data->SetDateTaken(data->GetFileDateModified());
    }

    int32_t intTempMeta = 0;
    err = imageSource->GetImagePropertyInt(0, MEDIA_DATA_IMAGE_ORIENTATION, intTempMeta);
    if (err == 0) {
        data->SetOrientation(intTempMeta);
    }

    double dbleTempMeta = -1;
    err = imageSource->GetImagePropertyString(0, MEDIA_DATA_IMAGE_GPS_LONGITUDE, propertyStr);
    if (err == 0) {
        dbleTempMeta = GetLongitudeLatitude(propertyStr);
        data->SetLongitude(dbleTempMeta);
    }

    err = imageSource->GetImagePropertyString(0, MEDIA_DATA_IMAGE_GPS_LATITUDE, propertyStr);
    if (err == 0) {
        dbleTempMeta = GetLongitudeLatitude(propertyStr);
        data->SetLatitude(dbleTempMeta);
    }

    ExtractImageExif(imageSource, data);

    return E_OK;
}

void MetadataExtractor::FillExtractedMetadata(const std::unordered_map<int32_t, std::string> &resultMap,
    std::unique_ptr<Metadata> &data)
{
    string strTemp;
    int32_t intTempMeta;
    int64_t int64TempMeta;

    strTemp = resultMap.at(AV_KEY_ALBUM);
    if (strTemp != "") {
        data->SetAlbum(strTemp);
    }

    strTemp = resultMap.at(AV_KEY_ARTIST);
    if (strTemp != "") {
        data->SetFileArtist(strTemp);
    }

    strTemp = resultMap.at(AV_KEY_DURATION);
    if (strTemp != "") {
        intTempMeta = stringToNum<int32_t>(strTemp);
        data->SetFileDuration(intTempMeta);
    }

    strTemp = resultMap.at(AV_KEY_VIDEO_HEIGHT);
    if (strTemp != "") {
        intTempMeta = stringToNum<int32_t>(strTemp);
        data->SetFileHeight(intTempMeta);
    }

    strTemp = resultMap.at(AV_KEY_VIDEO_WIDTH);
    if (strTemp != "") {
        intTempMeta = stringToNum<int32_t>(strTemp);
        data->SetFileWidth(intTempMeta);
    }

    strTemp = resultMap.at(AV_KEY_MIME_TYPE);
    if (strTemp != "") {
        data->SetFileMimeType(strTemp);
    }

    strTemp = resultMap.at(AV_KEY_DATE_TIME_FORMAT);
    if (strTemp != "") {
        int64TempMeta = convertTimeStr2TimeStamp(strTemp);
        if (int64TempMeta < 0) {
            data->SetDateTaken(data->GetFileDateModified());
        } else {
            data->SetDateTaken(int64TempMeta);
        }
    } else {
        // use modified time as date taken time when date taken not set
        data->SetDateTaken(data->GetFileDateModified());
    }

    strTemp = resultMap.at(AV_KEY_VIDEO_ORIENTATION);
    if (strTemp == "") {
        intTempMeta = 0;
    } else {
        intTempMeta = stringToNum<int32_t>(strTemp);
    }
    data->SetOrientation(intTempMeta);

    strTemp = resultMap.at(AV_KEY_TITLE);
    if (!strTemp.empty()) {
        data->SetFileTitle(strTemp);
    }
}

int32_t MetadataExtractor::ExtractAVMetadata(std::unique_ptr<Metadata> &data)
{
    MediaLibraryTracer tracer;
    tracer.Start("ExtractAVMetadata");

    tracer.Start("CreateAVMetadataHelper");
    std::shared_ptr<AVMetadataHelper> avMetadataHelper = AVMetadataHelperFactory::CreateAVMetadataHelper();
    tracer.Finish();
    if (avMetadataHelper == nullptr) {
        MEDIA_ERR_LOG("AV metadata helper is null");
        return E_AVMETADATA;
    }

    string filePath = data->GetFilePath();
    if (filePath.empty()) {
        MEDIA_ERR_LOG("AV metadata file path is empty");
        return E_AVMETADATA;
    }

    int32_t fd = open(filePath.c_str(), O_RDONLY);
    if (fd <= 0) {
        MEDIA_ERR_LOG("Open file descriptor failed, errno = %{public}d", errno);
        return E_SYSCALL;
    }

    struct stat64 st;
    if (fstat64(fd, &st) != 0) {
        MEDIA_ERR_LOG("Get file state failed for the given fd");
        (void)close(fd);
        return E_SYSCALL;
    }

    tracer.Start("avMetadataHelper->SetSource");
    int32_t err = avMetadataHelper->SetSource(fd, 0, static_cast<int64_t>(st.st_size), AV_META_USAGE_META_ONLY);
    tracer.Finish();
    if (err != 0) {
        MEDIA_ERR_LOG("SetSource failed for the given file descriptor, err = %{public}d", err);
        (void)close(fd);
        return E_AVMETADATA;
    } else {
        tracer.Start("avMetadataHelper->ResolveMetadata");
        std::unordered_map<int32_t, std::string> resultMap = avMetadataHelper->ResolveMetadata();
        tracer.Finish();
        if (!resultMap.empty()) {
            FillExtractedMetadata(resultMap, data);
        }
    }

    (void)close(fd);

    return E_OK;
}

int32_t MetadataExtractor::Extract(std::unique_ptr<Metadata> &data)
{
    if (data->GetFileMediaType() == MEDIA_TYPE_IMAGE) {
        return ExtractImageMetadata(data);
    } else {
        return ExtractAVMetadata(data);
    }
}
} // namespace Media
} // namespace OHOS
