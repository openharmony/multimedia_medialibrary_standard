/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
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

#ifndef INTERFACES_KITS_JS_MEDIALIBRARY_INCLUDE_THUMBNAIL_MANAGER_H
#define INTERFACES_KITS_JS_MEDIALIBRARY_INCLUDE_THUMBNAIL_MANAGER_H

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "image_type.h"
#include "napi/native_api.h"
#include "nocopyable.h"
#include "safe_map.h"
#include "safe_queue.h"
#include "pixel_map.h"
#include "userfile_manager_types.h"

namespace OHOS {
namespace Media {
class ThumbnailRequest;
class ThumbnailManager;
using RequestSharedPtr = std::shared_ptr<ThumbnailRequest>;
using PixelMapPtr = std::unique_ptr<PixelMap>;

enum class ThumbnailStatus : int32_t {
    THUMB_INITIAL = 0,
    THUMB_FAST,
    THUMB_QUALITY,
    THUMB_REMOVE,
};

struct RequestPhotoParams {
    std::string uri;
    std::string path;
    Size size;
    RequestPhotoType type;
};

class ThumbnailCallback {
public:
    ThumbnailCallback(napi_env env, napi_ref callback) : env_(env), callBackRef_(callback)
    { }
    virtual ~ThumbnailCallback() = default;
    napi_env env_;
    napi_ref callBackRef_;
};

class ThumnailUv {
public:
    ThumnailUv(const RequestSharedPtr &request, ThumbnailManager *manager, bool isFastImage) : request_(request),
        manager_(manager), isFastImage_(isFastImage) {}
    RequestSharedPtr request_;
    ThumbnailManager *manager_;
    bool isFastImage_ = false;
};

class ThumbnailRequest {
public:
    explicit ThumbnailRequest(const RequestPhotoParams &params, napi_env env, napi_ref callback);
    virtual ~ThumbnailRequest();
    bool UpdateStatus(ThumbnailStatus status);
    ThumbnailStatus GetStatus();
    bool NeedContinue();

    std::string GetUri() const
    {
        return uri_;
    }

    std::string GetPath() const
    {
        return path_;
    }

    Size GetRequestSize() const
    {
        return requestSize_;
    }

    PixelMapPtr GetPixelMap()
    {
        return std::move(pixelMap);
    }

    void SetPixelMap(PixelMapPtr ptr)
    {
        pixelMap = std::move(ptr);
    }

    PixelMapPtr GetFastPixelMap()
    {
        return std::move(fastPixelMap);
    }

    void SetFastPixelMap(PixelMapPtr ptr)
    {
        fastPixelMap = std::move(ptr);
    }

    void SetUUID(const std::string &uuid)
    {
        uuid_ = uuid;
    }

    std::string GetUUID() const
    {
        return uuid_;
    }

    ThumbnailCallback callback_;
    RequestPhotoType requestPhotoType;
private:
    std::string uri_;
    std::string path_;
    Size requestSize_;
    ThumbnailStatus status_ = ThumbnailStatus::THUMB_INITIAL;
    std::mutex mutex_;
    std::string uuid_;
    
    PixelMapPtr fastPixelMap;
    PixelMapPtr pixelMap;
};

class MMapFdPtr {
public:
    explicit MMapFdPtr(int32_t fd);
    ~MMapFdPtr();
    void* GetFdPtr();
    off_t GetFdSize();
    bool IsValid();
private:
    void* fdPtr_;
    off_t size_;
    bool isValid_ = false;
};

constexpr int FAST_THREAD_NUM = 3;
constexpr int THREAD_NUM = 2;
class ThumbnailManager : NoCopyable {
public:
    virtual ~ThumbnailManager();
    static std::shared_ptr<ThumbnailManager> GetInstance();

    void Init();
    std::string AddPhotoRequest(const RequestPhotoParams &params, napi_env env, napi_ref callback);
    void RemovePhotoRequest(const std::string &requestId);
    static std::unique_ptr<PixelMap> QueryThumbnail(const std::string &uri, const Size &size,
        const std::string &path);
    void DeleteRequestIdFromMap(const std::string &requestId);
private:
    ThumbnailManager() = default;
    void FastImageWorker(int num);
    void DealWithFastRequest(const RequestSharedPtr &request);
    void QualityImageWorker(int num);
    void AddFastPhotoRequest(const RequestSharedPtr &request);
    void AddQualityPhotoRequest(const RequestSharedPtr &request);
    void AddNewQualityPhotoRequest(const RequestSharedPtr &request);
    bool NotifyImage(const RequestSharedPtr &request, bool isFastImage);
    bool RequestFastImage(const RequestSharedPtr &request);

    SafeMap<std::string, RequestSharedPtr> thumbRequest_;
    SafeQueue<RequestSharedPtr> fastQueue_;
    SafeQueue<RequestSharedPtr> qualityQueue_;

    std::mutex fastLock_;
    std::condition_variable fastCv_;
    std::mutex qualityLock_;
    std::condition_variable qualityCv_;
    std::vector<std::thread> fastThreads_;
    std::vector<std::thread> threads_;

    static std::shared_ptr<ThumbnailManager> instance_;
    static std::mutex mutex_;
    static bool init_;
    std::atomic<bool> isThreadRunning_;
};

} // Media
} // OHOS

#endif // INTERFACES_KITS_JS_MEDIALIBRARY_INCLUDE_THUMBNAIL_MANAGER_H

