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

#include "thumbnail_datashare_bridge.h"
#include "medialibrary_errno.h"
#include "medialibrary_tracer.h"
#include "media_log.h"

namespace OHOS {
namespace Media {
using namespace DataShare;
using namespace DistributedKv;
ThumbnailSemaphore::ThumbnailSemaphore(int32_t count) : count_(count)
{}

void ThumbnailSemaphore::Signal()
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++count_;
    }
    cv_.notify_one();
}

void ThumbnailSemaphore::Wait()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [=] { return count_ > 0; });
    --count_;
}

static constexpr int32_t THUMBNAIL_SEM_NUM = 4;
ThumbnailSemaphore ThumbnailDataShareBridge::sem_(THUMBNAIL_SEM_NUM);
int ThumbnailDataShareBridge::GetRowCount(int32_t &count)
{
    count = 1;
    return E_OK;
}

int ThumbnailDataShareBridge::GetAllColumnNames(std::vector<std::string> &columnsName)
{
    columnsName = { "key", "velue" };
    return E_OK;
}

bool ThumbnailDataShareBridge::FillBlock(int pos, ResultSetBridge::Writer &writer)
{
    if (singleKvStorePtr_ == nullptr) {
        MEDIA_ERR_LOG("singleKvStorePtr_ nullptr");
        return false;
    }

    MediaLibraryTracer tracer;
    tracer.Start("ThumbnailDataShareBridge::Get");
    sem_.Wait();
    Key key(thumbnailKey_);
    Value value;
    Status status = singleKvStorePtr_->Get(key, value);
    if (status != Status::SUCCESS) {
        MEDIA_ERR_LOG("GetEntry failed: %{public}d", status);
        sem_.Signal();
        return false;
    }
    sem_.Signal();
    tracer.Finish();

    tracer.Start("ThumbnailDataShareBridge::Writer");
    int statusAlloc = writer.AllocRow();
    if (statusAlloc != E_OK) {
        MEDIA_ERR_LOG("ShraedBlock is full: %{public}d", statusAlloc);
        return false;
    }
    int keyStatus = writer.Write(0, key.ToString().c_str(), key.Size() + 1);
    if (keyStatus != E_OK) {
        MEDIA_ERR_LOG("WriterBlob key error: %{public}d", keyStatus);
        return false;
    }
    int valueStatus = writer.Write(1, value.ToString().c_str(), value.Size() + 1);
    if (valueStatus != E_OK) {
        MEDIA_ERR_LOG("WriterBlob key error: %{public}d", valueStatus);
        return false;
    }

    return true;
}

int ThumbnailDataShareBridge::OnGo(int32_t start, int32_t target, ResultSetBridge::Writer &writer)
{
    if ((start < 0) || (target < 0) || (start > target)) {
        MEDIA_ERR_LOG("nowRowIndex out of line: %{pubilc}d", target);
        return -1;
    }
    for (int pos = start; pos <= target; pos++) {
        bool ret = FillBlock(pos, writer);
        if (!ret) {
            MEDIA_ERR_LOG("nowRowIndex out of line: %{pubilc}d", target);
            return pos - 1;
        }
    }
    return target;
}

ThumbnailDataShareBridge::ThumbnailDataShareBridge(const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore,
    const std::string &key)
{
    singleKvStorePtr_ = kvStore;
    thumbnailKey_ = key;
}

std::shared_ptr<ResultSetBridge> ThumbnailDataShareBridge::Create(const std::shared_ptr<SingleKvStore> &kvStore,
    const std::string &key)
{
    if (kvStore == nullptr) {
        MEDIA_ERR_LOG("param error, kvStore nullptr");
        return nullptr;
    }

    return std::shared_ptr<ResultSetBridge>(
        new (std::nothrow) ThumbnailDataShareBridge(kvStore, key));
}
} // namespace Media
} // namespace OHOS
