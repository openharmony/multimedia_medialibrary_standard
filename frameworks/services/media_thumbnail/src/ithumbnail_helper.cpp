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

#include "ithumbnail_helper.h"

#include "ability_manager_client.h"
#include "hitrace_meter.h"
#include "ipc_skeleton.h"
#include "media_column.h"
#include "medialibrary_errno.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "rdb_helper.h"
#include "single_kvstore.h"
#include "thumbnail_const.h"
#include "post_event_utils.h"

using namespace std;
using namespace OHOS::DistributedKv;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {
void IThumbnailHelper::GetThumbnailInfo(ThumbRdbOpt &opts, ThumbnailData &outData)
{
    if (opts.store == nullptr) {
        return;
    }
    if (!opts.path.empty()) {
        outData.path = opts.path;
        outData.id = opts.row;
        return;
    }
    string filesTableName = opts.table;
    int errCode = E_ERR;
    if (!opts.networkId.empty()) {
        filesTableName = opts.store->ObtainDistributedTableName(opts.networkId, opts.table, errCode);
    }
    if (filesTableName.empty()) {
        return;
    }
    opts.table = filesTableName;
    int err;
    ThumbnailUtils::QueryThumbnailInfo(opts, outData, err);
    if (err != E_OK) {
        MEDIA_ERR_LOG("query fail [%{public}d]", err);
    }
    return;
}

void IThumbnailHelper::CreateLcd(AsyncTaskData* data)
{
    GenerateAsyncTaskData* taskData = static_cast<GenerateAsyncTaskData*>(data);
    DoCreateLcd(taskData->opts, taskData->thumbnailData);
}

void IThumbnailHelper::CreateThumbnail(AsyncTaskData* data)
{
    GenerateAsyncTaskData* taskData = static_cast<GenerateAsyncTaskData*>(data);
    DoCreateThumbnail(taskData->opts, taskData->thumbnailData);
}

void IThumbnailHelper::AddAsyncTask(MediaLibraryExecute executor, ThumbRdbOpt &opts, ThumbnailData &data, bool isFront)
{
    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker == nullptr) {
        MEDIA_DEBUG_LOG("IThumbnailHelper::AddAsyncTask asyncWorker is null");
        return;
    }
    GenerateAsyncTaskData* taskData = new (nothrow)GenerateAsyncTaskData();
    if (taskData == nullptr) {
        MEDIA_DEBUG_LOG("IThumbnailHelper::GenerateAsyncTaskData taskData is null");
        return;
    }
    taskData->opts = opts;
    taskData->thumbnailData = data;

    shared_ptr<MediaLibraryAsyncTask> generateAsyncTask = make_shared<MediaLibraryAsyncTask>(executor, taskData);
    asyncWorker->AddTask(generateAsyncTask, isFront);
}

ThumbnailWait::ThumbnailWait(bool release) : needRelease_(release)
{}

ThumbnailWait::~ThumbnailWait()
{
    if (needRelease_) {
        Notify();
    }
}

ThumbnailMap ThumbnailWait::thumbnailMap_;
std::shared_mutex ThumbnailWait::mutex_;

static bool WaitFor(const shared_ptr<SyncStatus>& thumbnailWait, int waitMs, unique_lock<mutex> &lck)
{
    bool ret = thumbnailWait->cond_.wait_for(lck, chrono::milliseconds(waitMs),
        [thumbnailWait]() { return thumbnailWait->isSyncComplete_; });
    if (!ret) {
        MEDIA_INFO_LOG("IThumbnailHelper::Wait wait for lock timeout");
    }
    return ret;
}

WaitStatus ThumbnailWait::InsertAndWait(const string &id, bool isLcd)
{
    id_ = id;

    if (isLcd) {
        id_ += THUMBNAIL_LCD_SUFFIX;
    } else {
        id_ += THUMBNAIL_THUMB_SUFFIX;
    }
    unique_lock<shared_mutex> writeLck(mutex_);
    auto iter = thumbnailMap_.find(id_);
    if (iter != thumbnailMap_.end()) {
        auto thumbnailWait = iter->second;
        unique_lock<mutex> lck(thumbnailWait->mtx_);
        writeLck.unlock();
        bool ret = WaitFor(thumbnailWait, WAIT_FOR_MS, lck);
        if (ret) {
            return WaitStatus::WAIT_SUCCESS;
        } else {
            return WaitStatus::TIMEOUT;
        }
    } else {
        shared_ptr<SyncStatus> thumbnailWait = make_shared<SyncStatus>();
        thumbnailMap_.insert(ThumbnailMap::value_type(id_, thumbnailWait));
        return WaitStatus::INSERT;
    }
}

void ThumbnailWait::CheckAndWait(const string &id, bool isLcd)
{
    id_ = id;

    if (isLcd) {
        id_ += THUMBNAIL_LCD_SUFFIX;
    } else {
        id_ += THUMBNAIL_THUMB_SUFFIX;
    }
    shared_lock<shared_mutex> readLck(mutex_);
    auto iter = thumbnailMap_.find(id_);
    if (iter != thumbnailMap_.end()) {
        auto thumbnailWait = iter->second;
        unique_lock<mutex> lck(thumbnailWait->mtx_);
        readLck.unlock();
        WaitFor(thumbnailWait, WAIT_FOR_MS, lck);
    }
}

void ThumbnailWait::Notify()
{
    unique_lock<shared_mutex> writeLck(mutex_);
    auto iter = thumbnailMap_.find(id_);
    if (iter != thumbnailMap_.end()) {
        auto thumbnailWait = iter->second;
        thumbnailMap_.erase(iter);
        {
            unique_lock<mutex> lck(thumbnailWait->mtx_);
            writeLck.unlock();
            thumbnailWait->isSyncComplete_ = true;
        }
        thumbnailWait->cond_.notify_all();
    }
}

bool IThumbnailHelper::TryLoadSource(ThumbRdbOpt &opts, ThumbnailData &data, const Size &size, const string &suffix)
{
    if (!ThumbnailUtils::LoadSourceImage(data, size, suffix == THUMBNAIL_THUMB_SUFFIX)) {
        if (opts.path.empty()) {
            MEDIA_ERR_LOG("LoadSourceImage faild, %{private}s", data.path.c_str());
            VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
                {KEY_OPT_FILE, data.path}, {KEY_OPT_TYPE, OptType::THUMB}};
            PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
            return false;
        } else {
            opts.path = "";
            GetThumbnailInfo(opts, data);
            string fileName = GetThumbnailPath(data.path, suffix);
            if (access(fileName.c_str(), F_OK) == 0) {
                return true;
            }
            if (!ThumbnailUtils::LoadSourceImage(data, size, suffix == THUMBNAIL_THUMB_SUFFIX)) {
                VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__},
                    {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN}, {KEY_OPT_FILE, data.path}, {KEY_OPT_TYPE, OptType::THUMB}};
                PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
                return false;
            }
        }
    }
    return true;
}


bool IThumbnailHelper::DoCreateLcd(ThumbRdbOpt &opts, ThumbnailData &data)
{
    ThumbnailWait thumbnailWait(true);
    auto ret = thumbnailWait.InsertAndWait(data.id, true);
    if (ret == WaitStatus::WAIT_SUCCESS) {
        return true;
    }

    if (!TryLoadSource(opts, data, opts.screenSize, THUMBNAIL_LCD_SUFFIX)) {
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
            {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
        return false;
    }

    if (data.source == nullptr) {
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
            {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
        return false;
    }

    shared_ptr<string> pathPtr = make_shared<string>(data.path);
    if (!ThumbnailUtils::CompressImage(data.source, data.lcd, data.mediaType == MEDIA_TYPE_AUDIO, pathPtr)) {
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
            {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
        MEDIA_ERR_LOG("CompressImage faild");
        return false;
    }

    int err = ThumbnailUtils::TrySaveFile(data, ThumbnailType::LCD);
    if (err < 0) {
        MEDIA_ERR_LOG("SaveLcd faild %{public}d", err);
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, err},
            {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
        return false;
    }

    data.lcd.clear();
    if (!ThumbnailUtils::UpdateLcdInfo(opts, data, err)) {
        MEDIA_INFO_LOG("UpdateLcdInfo faild err : %{public}d", err);
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, err},
            {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::DB_OPT_ERR, map);
        return false;
    }

    return true;
}

bool IThumbnailHelper::GenThumbnail(ThumbRdbOpt &opts, ThumbnailData &data, const ThumbnailType type)
{
    bool isThumb = false;
    if (type == ThumbnailType::THUMB) {
        isThumb = true;
    }
    Size size;
    if (isThumb) {
        size = { DEFAULT_THUMB_SIZE, DEFAULT_THUMB_SIZE };
        if (!TryLoadSource(opts, data, size, THUMBNAIL_THUMB_SUFFIX)) {
            VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
                {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
            PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
            return false;
        }

        if (data.source == nullptr) {
            return false;
        }

        if (!ThumbnailUtils::CompressImage(data.source, data.thumbnail)) {
            MEDIA_ERR_LOG("CompressImage faild id %{private}s", opts.row.c_str());
            VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
                {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
            PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
            return false;
        }
    } else {
        if (type == ThumbnailType::MTH) {
            size = {DEFAULT_MTH_SIZE, DEFAULT_MTH_SIZE };
        } else {
            size = { DEFAULT_YEAR_SIZE, DEFAULT_YEAR_SIZE };
        }
        ThumbnailUtils::GenTargetPixelmap(data, size);
    }

    int err = ThumbnailUtils::TrySaveFile(data, type);
    if (err < 0) {
        MEDIA_ERR_LOG("SaveThumbnailData faild %{public}d", err);
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, err},
            {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
        return false;
    }
    data.thumbnail.clear();
    return true;
}

bool IThumbnailHelper::DoCreateThumbnail(ThumbRdbOpt &opts, ThumbnailData &data)
{
    ThumbnailWait thumbnailWait(true);
    auto ret = thumbnailWait.InsertAndWait(data.id, false);
    if (ret == WaitStatus::WAIT_SUCCESS) {
        return true;
    }

    if (!GenThumbnail(opts, data, ThumbnailType::THUMB)) {
        VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
            {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
        PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
        return false;
    }
    if (opts.table != AudioColumn::AUDIOS_TABLE) {
        if (!GenThumbnail(opts, data, ThumbnailType::MTH)) {
            VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
                {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
            PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
            return false;
        }
        if (!GenThumbnail(opts, data, ThumbnailType::YEAR)) {
            VariantMap map = {{KEY_ERR_FILE, __FILE__}, {KEY_ERR_LINE, __LINE__}, {KEY_ERR_CODE, E_THUMBNAIL_UNKNOWN},
                {KEY_OPT_FILE, opts.path}, {KEY_OPT_TYPE, OptType::THUMB}};
            PostEventUtils::GetInstance().PostErrorProcess(ErrType::FILE_OPT_ERR, map);
            return false;
        }
    }

    return true;
}
} // namespace Media
} // namespace OHOS