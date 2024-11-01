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

#ifndef FRAMEWORKS_SERVICE_MEDIA_ASYNC_WORKER_INCLUDE_MEDIALIBRARY_ASYNC_WORKER_H_
#define FRAMEWORKS_SERVICE_MEDIA_ASYNC_WORKER_INCLUDE_MEDIALIBRARY_ASYNC_WORKER_H_

#include <condition_variable>
#include <mutex>
#include <thread>

#define ASYNC_WORKER_API_EXPORT __attribute__ ((visibility ("default")))
namespace OHOS {
namespace Media {
class TrashAsyncTaskWorker {
public:
    virtual ~TrashAsyncTaskWorker();
    ASYNC_WORKER_API_EXPORT static std::shared_ptr<TrashAsyncTaskWorker> GetInstance();
    ASYNC_WORKER_API_EXPORT void Interrupt();
    ASYNC_WORKER_API_EXPORT void Init();
private:
    TrashAsyncTaskWorker();
    void StartWorker();
    static std::mutex instanceLock_;
    static std::shared_ptr<TrashAsyncTaskWorker> asyncWorkerInstance_;
};
} // namespace Media
} // namespace OHOS

#endif  // FRAMEWORKS_SERVICE_MEDIA_ASYNC_WORKER_INCLUDE_MEDIALIBRARY_ASYNC_WORKER_H_