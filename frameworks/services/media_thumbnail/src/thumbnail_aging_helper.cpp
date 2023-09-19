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
#define MLOG_TAG "Thumbnail"

#include "thumbnail_aging_helper.h"

#include "medialibrary_errno.h"
#include "media_log.h"
#include "thumbnail_const.h"

using namespace std;
using namespace OHOS::DistributedKv;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {
static void AgingLcd(AsyncTaskData *data)
{
    if (data == nullptr) {
        return;
    }
    AgingAsyncTaskData* taskData = static_cast<AgingAsyncTaskData*>(data);
    int32_t err = ThumbnailAgingHelper::ClearLcdFromFileTable(taskData->opts);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to ClearLcdFormFileTable %{public}d", err);
    }
}

#ifdef DISTRIBUTED
static void AgingDistributeLcd(AsyncTaskData* data)
{
    if (data == nullptr) {
        return;
    }
    AgingAsyncTaskData* taskData = static_cast<AgingAsyncTaskData*>(data);
    int32_t err = ThumbnailAgingHelper::ClearRemoteLcdFromFileTable(taskData->opts);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to ClearRemoteLcdFormFileTable %{public}d", err);
    }
}
#endif

#ifdef DISTRIBUTED
static void ClearThumbnailRecordTask(AsyncTaskData* data)
{
    if (data == nullptr) {
        return;
    }
    AgingAsyncTaskData* taskData = static_cast<AgingAsyncTaskData*>(data);
    int32_t err = ThumbnailAgingHelper::ClearKeyAndRecordFromMap(taskData->opts);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to ClearKeyAndRecordFromMap %{public}d", err);
    }
}
#endif

int32_t ThumbnailAgingHelper::AgingLcdBatch(ThumbRdbOpt &opts)
{
    MEDIA_INFO_LOG("IN %{public}s", opts.table.c_str());
    if (opts.store == nullptr) {
        return E_HAS_DB_ERROR;
    }

    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        return E_ERR;
    }
    AgingAsyncTaskData* taskData = new (std::nothrow)AgingAsyncTaskData();
    if (taskData == nullptr) {
        return E_ERR;
    }
    taskData->opts = opts;
    shared_ptr<MediaLibraryAsyncTask> agingAsyncTask = make_shared<MediaLibraryAsyncTask>(AgingLcd, taskData);
    if (agingAsyncTask != nullptr) {
        asyncWorker->AddTask(agingAsyncTask, false);
    }
    return E_OK;
}

int32_t ThumbnailAgingHelper::ClearLcdFromFileTable(ThumbRdbOpt &opts)
{
    int lcdCount = 0;
    int32_t err = GetLcdCount(opts, lcdCount);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to GetLcdCount %{public}d", err);
        return err;
    }
    MEDIA_DEBUG_LOG("lcdCount %{public}d", lcdCount);
    if (lcdCount <= THUMBNAIL_LCD_AGING_THRESHOLD) {
        MEDIA_INFO_LOG("Not need aging Lcd. lcdCount: %{lcdCount}d", lcdCount);
        return E_OK;
    }
    vector<ThumbnailData> infos;
    err = GetAgingLcdData(opts, lcdCount - THUMBNAIL_LCD_AGING_THRESHOLD, infos);
    if ((err != E_OK) || infos.empty()) {
        MEDIA_ERR_LOG("Failed to GetAgingLcdData %{public}d", err);
        return err;
    }

    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        return E_ERR;
    }
    for (uint32_t i = 0; i < infos.size(); i++) {
        opts.row = infos[i].id;
        if (ThumbnailUtils::DeleteThumbFile(infos[i], ThumbnailType::LCD)) {
            ThumbnailUtils::CleanThumbnailInfo(opts, false, true);
        }
    }

    return E_OK;
}

#ifdef DISTRIBUTED
int32_t ThumbnailAgingHelper::AgingDistributeLcdBatch(ThumbRdbOpt &opts)
{
    if (opts.store == nullptr) {
        MEDIA_ERR_LOG("opts.store is not init");
        return E_ERR;
    }

    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        return E_ERR;
    }
    AgingAsyncTaskData* taskData = new AgingAsyncTaskData();
    if (taskData == nullptr) {
        return E_ERR;
    }
    taskData->opts = opts;
    shared_ptr<MediaLibraryAsyncTask> agingAsyncTask = make_shared<MediaLibraryAsyncTask>(AgingDistributeLcd, taskData);
    if (agingAsyncTask != nullptr) {
        asyncWorker->AddTask(agingAsyncTask, false);
    }
    return E_OK;
}

int32_t ThumbnailAgingHelper::ClearRemoteLcdFromFileTable(ThumbRdbOpt &opts)
{
    int lcdCount = 0;
    int32_t err = GetDistributeLcdCount(opts, lcdCount);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to GetDistributeLcdCount %{public}d", err);
        return err;
    }
    MEDIA_DEBUG_LOG("GetDistributeLcdCount %{public}d", lcdCount);
    if (lcdCount <= THUMBNAIL_LCD_AGING_THRESHOLD) {
        MEDIA_INFO_LOG("Not need aging Lcd. GetDistributeLcdCount: %{lcdCount}d", lcdCount);
        return E_OK;
    }
    vector<ThumbnailData> infos;
    err = GetAgingDistributeLcdData(opts, lcdCount - THUMBNAIL_LCD_GENERATE_THRESHOLD, infos);
    if ((err != E_OK) || infos.empty()) {
        MEDIA_ERR_LOG("Failed to GetAgingDistributeLcdData %{public}d", err);
        return err;
    }

    for (uint32_t i = 0; i < infos.size(); i++) {
        opts.row = infos[i].id;
        ThumbnailUtils::DeleteDistributeLcdData(opts, infos[i]);
    }

    return E_OK;
}
#endif

int32_t ThumbnailAgingHelper::GetLcdCount(ThumbRdbOpt &opts, int &outLcdCount)
{
    int32_t err = E_ERR;
    if (!ThumbnailUtils::QueryLcdCount(opts, outLcdCount, err)) {
        MEDIA_ERR_LOG("Failed to QueryLcdCount %{public}d", err);
        return err;
    }
    return E_OK;
}
#ifdef DISTRIBUTED
int32_t ThumbnailAgingHelper::GetDistributeLcdCount(ThumbRdbOpt &opts, int &outLcdCount)
{
    int32_t err = E_ERR;
    if (!ThumbnailUtils::QueryDistributeLcdCount(opts, outLcdCount, err)) {
        MEDIA_ERR_LOG("Failed to QueryLcdCount %{public}d", err);
        return err;
    }
    return E_OK;
}
#endif

int32_t ThumbnailAgingHelper::GetAgingLcdData(ThumbRdbOpt &opts, int lcdLimit, vector<ThumbnailData> &outDatas)
{
    int32_t err = E_ERR;
    if (!ThumbnailUtils::QueryAgingLcdInfos(opts, lcdLimit, outDatas, err)) {
        MEDIA_ERR_LOG("Failed to QueryAgingLcdInfos %{public}d", err);
        return err;
    }
    return E_OK;
}

#ifdef DISTRIBUTED
int32_t ThumbnailAgingHelper::GetAgingDistributeLcdData(ThumbRdbOpt &opts,
    int lcdLimit, vector<ThumbnailData> &outDatas)
{
    int32_t err = E_ERR;
    if (!ThumbnailUtils::QueryAgingDistributeLcdInfos(opts, lcdLimit, outDatas, err)) {
        MEDIA_ERR_LOG("Failed to QueryAgingLcdInfos %{public}d", err);
        return err;
    }
    return E_OK;
}

int32_t ThumbnailAgingHelper::InvalidateDistributeBatch(ThumbRdbOpt &opts)
{
    if (opts.store == nullptr) {
        MEDIA_ERR_LOG("opts.store is not init");
        return E_ERR;
    }

    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        return E_ERR;
    }
    AgingAsyncTaskData* taskData = new (std::nothrow)AgingAsyncTaskData();
    if (taskData == nullptr) {
        return E_ERR;
    }
    taskData->opts = opts;
    shared_ptr<MediaLibraryAsyncTask> agingAsyncTask = make_shared<MediaLibraryAsyncTask>(
        ClearThumbnailRecordTask, taskData);
    if (agingAsyncTask != nullptr) {
        asyncWorker->AddTask(agingAsyncTask, true);
    }
    return E_OK;
}

int32_t ThumbnailAgingHelper::ClearKeyAndRecordFromMap(ThumbRdbOpt &opts)
{
    int32_t err = E_ERR;
    vector<ThumbnailData> infos;
    if (!ThumbnailUtils::QueryDeviceThumbnailRecords(opts, infos, err)) {
        MEDIA_ERR_LOG("Failed to QueryDeviceThumbnailRecords %{public}d", err);
        return err;
    }

    for (uint32_t i = 0; i < infos.size(); i++) {
        opts.row = infos[i].id;
        if (ThumbnailUtils::DeleteOriginImage(opts)) {
            ThumbnailUtils::DeleteDistributeThumbnailInfo(opts);
        }
    }
    return E_OK;
}
#endif
} // namespace Media
} // namespace OHOS
