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

#include "thumbnail_service.h"

#include "display_manager.h"
#include "media_column.h"
#include "medialibrary_async_worker.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "media_log.h"
#include "result_set_utils.h"
#include "thumbnail_aging_helper.h"
#include "thumbnail_const.h"
#include "thumbnail_generate_helper.h"
#include "thumbnail_helper_factory.h"
#include "thumbnail_uri_utils.h"

using namespace std;
using namespace OHOS::DistributedKv;
using namespace OHOS::NativeRdb;
using namespace OHOS::AbilityRuntime;

namespace OHOS {
namespace Media {
std::shared_ptr<ThumbnailService> ThumbnailService::thumbnailServiceInstance_{nullptr};
std::mutex ThumbnailService::instanceLock_;
ThumbnailService::ThumbnailService(void)
{
    rdbStorePtr_ = nullptr;
    kvStorePtr_ = nullptr;
}

shared_ptr<ThumbnailService> ThumbnailService::GetInstance()
{
    if (thumbnailServiceInstance_ == nullptr) {
        std::lock_guard<std::mutex> lockGuard(instanceLock_);
        if (thumbnailServiceInstance_ != nullptr) {
            return thumbnailServiceInstance_;
        }
        thumbnailServiceInstance_ = shared_ptr<ThumbnailService>(new ThumbnailService());
    }

    return thumbnailServiceInstance_;
}

static int32_t GetDefaultWindowSize(Size &size)
{
    auto &displayMgr = OHOS::Rosen::DisplayManager::GetInstance();
    auto display = displayMgr.GetDefaultDisplay();
    if (display == nullptr) {
        return E_ERR;
    }
    size.width = display->GetWidth();
    size.height = display->GetHeight();
    MEDIA_INFO_LOG("display window size::w %{public}d, h %{public}d", size.width, size.height);

    return E_OK;
}

int32_t ThumbnailService::Init(const shared_ptr<RdbStore> &rdbStore,
    const shared_ptr<SingleKvStore> &kvStore,
    const shared_ptr<Context> &context)
{
    rdbStorePtr_ = rdbStore;
    kvStorePtr_ = kvStore;
    context_ = context;

    return GetDefaultWindowSize(screenSize_);
}

void ThumbnailService::ReleaseService()
{
    StopAllWorker();
    rdbStorePtr_ = nullptr;
    kvStorePtr_ = nullptr;
    context_ = nullptr;
    thumbnailServiceInstance_ = nullptr;
}

#ifdef MEDIALIBRARY_COMPATIBILITY
static int32_t GetPathFromDb(const shared_ptr<NativeRdb::RdbStore> &rdbStorePtr, const string &fileId,
    const string &table, string &path)
{
    if (rdbStorePtr == nullptr) {
        return E_HAS_DB_ERROR;
    }
    if (!all_of(fileId.begin(), fileId.end(), ::isdigit)) {
        return E_INVALID_FILEID;
    }
    string querySql = "SELECT " + MediaColumn::MEDIA_FILE_PATH + " FROM " + table +
        " WHERE " + MediaColumn::MEDIA_ID + "=?";
    vector<string> selectionArgs = { fileId };
    auto resultSet = rdbStorePtr->QuerySql(querySql, selectionArgs);
    if (resultSet == nullptr || resultSet->GoToFirstRow() != NativeRdb::E_OK) {
        return E_HAS_DB_ERROR;
    }
    path = GetStringVal(MediaColumn::MEDIA_FILE_PATH, resultSet);
    if (path.empty()) {
        return E_INVALID_PATH;
    }
    return E_OK;
}
#endif

int ThumbnailService::GetThumbnailFd(const string &uri)
{
    string id, path, table;
    Size size;
    if (!ThumbnailUriUtils::ParseThumbnailInfo(uri, id, size, path, table)) {
        return E_FAIL;
    }
#ifdef MEDIALIBRARY_COMPATIBILITY
    if (path.empty()) {
        int32_t errCode = GetPathFromDb(rdbStorePtr_, id, table, path);
        if (errCode != E_OK) {
            MEDIA_ERR_LOG("GetPathFromDb failed, errCode = %{public}d", errCode);
            return errCode;
        }
    }
#endif
    ThumbRdbOpt opts = {
        .store = rdbStorePtr_,
        .path = path,
        .table = table,
        .row = id,
        .uri = uri,
    };
    shared_ptr<IThumbnailHelper> thumbnailHelper = ThumbnailHelperFactory::GetThumbnailHelper(size);
    if (thumbnailHelper == nullptr) {
        return E_NO_MEMORY;
    }
    if (!IsThumbnail(size.width, size.height)) {
        opts.screenSize = screenSize_;
    }
    int fd = thumbnailHelper->GetThumbnailPixelMap(opts);
    if (fd < 0) {
        MEDIA_ERR_LOG("GetThumbnailPixelMap failed : %{public}d", fd);
    }
    return fd;
}

int32_t ThumbnailService::CreateThumbnail(const std::string &uri, const string &path, bool isSync)
{
    string fileId;
    string networkId;
    string tableName;
    if (!ThumbnailUriUtils::ParseFileUri(uri, fileId, networkId, tableName)) {
        MEDIA_ERR_LOG("ParseThumbnailInfo faild");
        return E_ERR;
    }

    ThumbRdbOpt opts = {
        .store = rdbStorePtr_,
        .path = path,
        .table = tableName,
        .row = fileId,
        .screenSize = screenSize_
    };
    Size size = { DEFAULT_THUMB_SIZE, DEFAULT_THUMB_SIZE };
    shared_ptr<IThumbnailHelper> thumbnailHelper = ThumbnailHelperFactory::GetThumbnailHelper(size);
    if (thumbnailHelper == nullptr) {
        MEDIA_ERR_LOG("thumbnailHelper nullptr");
        return E_ERR;
    }
    int32_t err = thumbnailHelper->CreateThumbnail(opts, isSync);
    if (err != E_OK) {
        MEDIA_ERR_LOG("CreateThumbnail failed : %{public}d", err);
        return err;
    }

    size = { DEFAULT_LCD_SIZE, DEFAULT_LCD_SIZE };
    shared_ptr<IThumbnailHelper> lcdHelper = ThumbnailHelperFactory::GetThumbnailHelper(size);
    if (lcdHelper == nullptr) {
        MEDIA_ERR_LOG("lcdHelper nullptr");
        return E_ERR;
    }
    err = lcdHelper->CreateThumbnail(opts, isSync);
    if (err != E_OK) {
        MEDIA_ERR_LOG("CreateLcd failed : %{public}d", err);
        return err;
    }
    return err;
}

void ThumbnailService::InterruptBgworker()
{
    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker != nullptr) {
        asyncWorker->Interrupt();
    }
}

void ThumbnailService::StopAllWorker()
{
    shared_ptr<MediaLibraryAsyncWorker> asyncWorker = MediaLibraryAsyncWorker::GetInstance();
    if (asyncWorker != nullptr) {
        asyncWorker->Stop();
    }
}

int32_t ThumbnailService::GenerateThumbnails()
{
    int32_t err = 0;
    vector<string> tableList;
    tableList.emplace_back(PhotoColumn::PHOTOS_TABLE);
    tableList.emplace_back(AudioColumn::AUDIOS_TABLE);
    tableList.emplace_back(MEDIALIBRARY_TABLE);

    for (const auto &tableName : tableList) {
        ThumbRdbOpt opts = {
            .store = rdbStorePtr_,
            .kvStore = kvStorePtr_,
            .table = tableName
        };

        err = ThumbnailGenerateHelper::CreateThumbnailBatch(opts);
        if (err != E_OK) {
            MEDIA_ERR_LOG("CreateThumbnailBatch failed : %{public}d", err);
        }
        if (tableName != AudioColumn::AUDIOS_TABLE) {
            err = ThumbnailGenerateHelper::CreateLcdBatch(opts);
            if (err != E_OK) {
                MEDIA_ERR_LOG("CreateLcdBatch failed : %{public}d", err);
            }
        }
    }

    return err;
}

int32_t ThumbnailService::LcdAging()
{
    int32_t err = 0;
    vector<string> tableList;
    tableList.emplace_back(PhotoColumn::PHOTOS_TABLE);
    tableList.emplace_back(AudioColumn::AUDIOS_TABLE);
    tableList.emplace_back(MEDIALIBRARY_TABLE);

    for (const auto &tableName : tableList) {
        ThumbRdbOpt opts = {
            .store = rdbStorePtr_,
            .kvStore = kvStorePtr_,
            .table = tableName,
        };
        err = ThumbnailAgingHelper::AgingLcdBatch(opts);
        if (err != E_OK) {
            MEDIA_ERR_LOG("AgingLcdBatch failed : %{public}d", err);
        }
    }

    return E_OK;
}

#ifdef DISTRIBUTED
int32_t ThumbnailService::LcdDistributeAging(const string &udid)
{
    ThumbRdbOpt opts = {
        .store = rdbStorePtr_,
        .kvStore = kvStorePtr_,
        .udid = udid
    };
    int32_t err = ThumbnailAgingHelper::AgingDistributeLcdBatch(opts);
    if (err != E_OK) {
        MEDIA_ERR_LOG("AgingDistributeLcdBatch failed : %{public}d", err);
        return err;
    }
    return E_OK;
}

int32_t ThumbnailService::InvalidateDistributeThumbnail(const string &udid)
{
    ThumbRdbOpt opts = {
        .store = rdbStorePtr_,
        .kvStore = kvStorePtr_,
        .udid = udid
    };
    int32_t err = ThumbnailAgingHelper::InvalidateDistributeBatch(opts);
    if (err != E_OK) {
        MEDIA_ERR_LOG("InvalidateDistributeBatch failed : %{public}d", err);
    }
    return err;
}
#endif

void ThumbnailService::InvalidateThumbnail(const std::string &id, const std::string &tableName, const std::string &path)
{
    ThumbRdbOpt opts = {
        .store = rdbStorePtr_,
        .path = path,
        .table = tableName,
        .row = id,
    };
    ThumbnailData thumbnailData;
    ThumbnailUtils::DeleteOriginImage(opts);
}
} // namespace Media
} // namespace OHOS
