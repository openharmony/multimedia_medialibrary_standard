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

#include "thumbnail_manager.h"

#include <memory>
#include <mutex>
#include <sys/mman.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#include "ashmem.h"
#include "image_source.h"
#include "image_type.h"
#include "js_native_api.h"
#include "media_file_uri.h"
#include "medialibrary_errno.h"
#include "medialibrary_napi_log.h"
#include "medialibrary_napi_utils.h"
#include "medialibrary_tracer.h"
#include "pixel_map.h"
#include "pixel_map_napi.h"
#include "post_proc.h"
#include "string_ex.h"
#include "thumbnail_const.h"
#include "unique_fd.h"
#include "userfile_manager_types.h"
#include "uv.h"
#include "userfile_client.h"

#ifdef IMAGE_PURGEABLE_PIXELMAP
#include "purgeable_pixelmap_builder.h"
#endif

using namespace std;
#define UUID_STR_LENGTH 37

namespace OHOS {
namespace Media {
shared_ptr<ThumbnailManager> ThumbnailManager::instance_ = nullptr;
mutex ThumbnailManager::mutex_;
bool ThumbnailManager::init_ = false;

ThumbnailRequest::ThumbnailRequest(const RequestPhotoParams &params, napi_env env,
    napi_ref callback) : callback_(env, callback), requestPhotoType(params.type), uri_(params.uri),
    path_(params.path), requestSize_(params.size)
{
}

ThumbnailRequest::~ThumbnailRequest()
{
    napi_delete_reference(callback_.env_, callback_.callBackRef_);
}

bool ThumbnailRequest::UpdateStatus(ThumbnailStatus status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (status <= status_) {
        return false;
    }
    status_ = status;
    return true;
}

ThumbnailStatus ThumbnailRequest::GetStatus()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

bool ThumbnailRequest::NeedContinue()
{
    return GetStatus() < ThumbnailStatus::THUMB_REMOVE;
}

static bool IsPhotoSizeThumb(const Size &size)
{
    return (size.width >= DEFAULT_THUMB_SIZE || size.height >= DEFAULT_THUMB_SIZE);
}

static bool NeedFastThumb(const Size &size, RequestPhotoType type)
{
    return IsPhotoSizeThumb(size) && (type != RequestPhotoType::REQUEST_QUALITY_THUMB);
}

static bool NeedQualityThumb(const Size &size, RequestPhotoType type)
{
    return IsPhotoSizeThumb(size) && (type != RequestPhotoType::REQUEST_FAST_THUMB);
}

MMapFdPtr::MMapFdPtr(int32_t fd)
{
    if (fd < 0) {
        NAPI_ERR_LOG("Fd is invalid: %{public}d", fd);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        NAPI_ERR_LOG("fstat error, errno:%{public}d", errno);
        return;
    }
    size_ = st.st_size;

    // mmap ptr from fd
    fdPtr_ = mmap(nullptr, size_, PROT_READ, MAP_SHARED, fd, 0);
    if (fdPtr_ == MAP_FAILED || fdPtr_ == nullptr) {
        NAPI_ERR_LOG("mmap uniqueFd failed, errno = %{public}d", errno);
        return;
    }

    isValid_ = true;
}

MMapFdPtr::~MMapFdPtr()
{
    // munmap ptr from fd
    munmap(fdPtr_, size_);
}

void* MMapFdPtr::GetFdPtr()
{
    return fdPtr_;
}

off_t MMapFdPtr::GetFdSize()
{
    return size_;
}

bool MMapFdPtr::IsValid()
{
    return isValid_;
}

static string GenerateRequestId()
{
    uuid_t uuid;
    uuid_generate(uuid);
    char str[UUID_STR_LENGTH] = {};
    uuid_unparse(uuid, str);
    return str;
}

shared_ptr<ThumbnailManager> ThumbnailManager::GetInstance()
{
    if (instance_ == nullptr) {
        lock_guard<mutex> lock(mutex_);
        if (instance_ == nullptr) {
            instance_ = shared_ptr<ThumbnailManager>(new ThumbnailManager());
        }
    }

    return instance_;
}

void ThumbnailManager::Init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (init_) {
        return;
    }
    init_ = true;
    isThreadRunning_ = true;
    for (auto i = 0; i < FAST_THREAD_NUM; i++) {
        fastThreads_.emplace_back(bind(&ThumbnailManager::FastImageWorker, this, i));
        fastThreads_[i].detach();
    }
    for (auto i = 0; i < THREAD_NUM; i++) {
        threads_.emplace_back(bind(&ThumbnailManager::QualityImageWorker, this, i));
        threads_[i].detach();
    }
    return;
}

string ThumbnailManager::AddPhotoRequest(const RequestPhotoParams &params, napi_env env, napi_ref callback)
{
    shared_ptr<ThumbnailRequest> request = make_shared<ThumbnailRequest>(params, env, callback);
    string requestId = GenerateRequestId();
    request->SetUUID(requestId);
    if (!thumbRequest_.Insert(requestId, request)) {
        return "";
    }
    // judge from request option
    if (NeedFastThumb(params.size, params.type)) {
        AddFastPhotoRequest(request);
    } else {
        AddQualityPhotoRequest(request);
    }
    return requestId;
}

void ThumbnailManager::RemovePhotoRequest(const string &requestId)
{
    RequestSharedPtr ptr;
    if (thumbRequest_.Find(requestId, ptr)) {
        if (ptr == nullptr) {
            return;
        }
        // do not need delete from queue, just update remove status.
        ptr->UpdateStatus(ThumbnailStatus::THUMB_REMOVE);
    }
    thumbRequest_.Erase(requestId);
}

ThumbnailManager::~ThumbnailManager()
{
    isThreadRunning_ = false;
    fastCv_.notify_all();
    qualityCv_.notify_all();
    for (auto &fastThread_ : fastThreads_) {
        if (fastThread_.joinable()) {
            fastThread_.join();
        }
    }
    for (auto &thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void SetThreadName(const string &threadName, int num)
{
    string name = threadName;
    name.append(to_string(num));
    pthread_setname_np(pthread_self(), name.c_str());
}

void ThumbnailManager::AddFastPhotoRequest(const RequestSharedPtr &request)
{
    request->UpdateStatus(ThumbnailStatus::THUMB_FAST);
    fastQueue_.Push(request);
    fastCv_.notify_one();
}

void ThumbnailManager::AddQualityPhotoRequest(const RequestSharedPtr &request)
{
    request->UpdateStatus(ThumbnailStatus::THUMB_QUALITY);
    qualityQueue_.Push(request);
    qualityCv_.notify_one();
}

static inline void GetFastThumbNewSize(const Size &size, Size &newSize)
{
    if (size.width > DEFAULT_THUMB_SIZE || size.height > DEFAULT_THUMB_SIZE) {
        newSize.height = DEFAULT_THUMB_SIZE;
        newSize.width = DEFAULT_THUMB_SIZE;
    } else if (size.width > DEFAULT_MTH_SIZE || size.height > DEFAULT_MTH_SIZE) {
        newSize.height = DEFAULT_MTH_SIZE;
        newSize.width = DEFAULT_MTH_SIZE;
    } else if (size.width > DEFAULT_YEAR_SIZE || size.height > DEFAULT_YEAR_SIZE) {
        newSize.height = DEFAULT_YEAR_SIZE;
        newSize.width = DEFAULT_YEAR_SIZE;
    } else {
        // Size is small enough, do not need to smaller
        return;
    }
}

static int OpenThumbnail(const string &path, ThumbnailType type)
{
    if (!path.empty()) {
        string sandboxPath = GetSandboxPath(path, type);
        int fd = -1;
        if (!sandboxPath.empty()) {
            fd = open(sandboxPath.c_str(), O_RDONLY);
        }
        if (fd > 0) {
            return fd;
        }
    }
    return E_ERR;
}

static bool IfSizeEqualsRatio(const Size &imageSize, const Size &targetSize)
{
    if (imageSize.height <= 0 || targetSize.height <= 0) {
        return false;
    }

    return imageSize.width / imageSize.height == targetSize.width / targetSize.height;
}

static bool GetAshmemPtr(int32_t memSize, void **sharedPtr, int32_t &fd)
{
    fd = AshmemCreate("MediaLibrary Create Ashmem Data", memSize);
    if (fd < 0) {
        NAPI_ERR_LOG("Can not create ashmem, fd:%{public}d", fd);
        return false;
    }
    int32_t result = AshmemSetProt(fd, PROT_READ | PROT_WRITE);
    if (result < 0) {
        NAPI_ERR_LOG("Can not set ashmem prot, result:%{public}d", result);
        close(fd);
        return false;
    }
    *sharedPtr = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (*sharedPtr == MAP_FAILED || *sharedPtr == nullptr) {
        NAPI_ERR_LOG("mmap ashmem ptr failed, errno = %{public}d", errno);
        close(fd);
        return false;
    }

    return true;
}

static PixelMapPtr CreateThumbnailByAshmem(UniqueFd &uniqueFd, const Size &size)
{
    MediaLibraryTracer tracer;
    tracer.Start("CreateThumbnailByAshmem");

    Media::InitializationOptions option = {
        .size = size,
        .pixelFormat = PixelFormat::RGBA_8888
    };
    PixelMapPtr pixel = Media::PixelMap::Create(option);
    if (pixel == nullptr) {
        NAPI_ERR_LOG("Can not create pixel");
        return nullptr;
    }

    MMapFdPtr mmapFd(uniqueFd.Get());
    if (!mmapFd.IsValid()) {
        NAPI_ERR_LOG("Can not mmap by fd");
        return nullptr;
    }
    auto memSize = static_cast<int32_t>(mmapFd.GetFdSize());

    // create ashmem and mmap
    void *sharedPtr = nullptr;
    int32_t fd = 0;
    if (!GetAshmemPtr(memSize, &sharedPtr, fd)) {
        return nullptr;
    }
    int32_t err = memcpy_s(sharedPtr, memSize + 1, mmapFd.GetFdPtr(), memSize);
    if (err != EOK) {
        NAPI_ERR_LOG("memcpy failed, errno:%{public}d", err);
        return nullptr;
    }
    auto data = static_cast<uint8_t*>(sharedPtr);
    void* fdPtr = new int32_t();
    *static_cast<int32_t*>(fdPtr) = fd;
    pixel->SetPixelsAddr(data, fdPtr, memSize, Media::AllocatorType::SHARE_MEM_ALLOC, nullptr);
    return pixel;
}

static PixelMapPtr DecodeThumbnail(UniqueFd &uniqueFd, const Size &size)
{
    MediaLibraryTracer tracer;
    tracer.Start("ImageSource::CreateImageSource");
    SourceOptions opts;
    uint32_t err = 0;
    unique_ptr<ImageSource> imageSource = ImageSource::CreateImageSource(uniqueFd.Get(), opts, err);
    if (imageSource  == nullptr) {
        NAPI_ERR_LOG("CreateImageSource err %{public}d", err);
        return nullptr;
    }

    ImageInfo imageInfo;
    err = imageSource->GetImageInfo(0, imageInfo);
    if (err != E_OK) {
        NAPI_ERR_LOG("GetImageInfo err %{public}d", err);
        return nullptr;
    }

    bool isEqualsRatio = IfSizeEqualsRatio(imageInfo.size, size);
    DecodeOptions decodeOpts;
    decodeOpts.desiredSize = isEqualsRatio ? size : imageInfo.size;
    decodeOpts.allocatorType = AllocatorType::SHARE_MEM_ALLOC;
    unique_ptr<PixelMap> pixelMap = imageSource->CreatePixelMap(decodeOpts, err);
    if (pixelMap == nullptr) {
        NAPI_ERR_LOG("CreatePixelMap err %{public}d", err);
        return nullptr;
    }
#ifdef IMAGE_PURGEABLE_PIXELMAP
    PurgeableBuilder::MakePixelMapToBePurgeable(pixelMap, uniqueFd.Get(), opts, decodeOpts);
#endif
    PostProc postProc;
    if (size.width != DEFAULT_ORIGINAL && !isEqualsRatio && !postProc.CenterScale(size, *pixelMap)) {
        return nullptr;
    }
    return pixelMap;
}

unique_ptr<PixelMap> ThumbnailManager::QueryThumbnail(const string &uriStr, const Size &size, const string &path)
{
    MediaLibraryTracer tracer;
    tracer.Start("QueryThumbnail uri:" + uriStr);
    tracer.Start("DataShare::OpenFile");
    ThumbnailType thumbType = GetThumbType(size.width, size.height);
    if (MediaFileUri::GetMediaTypeFromUri(uriStr) == MediaType::MEDIA_TYPE_AUDIO &&
        (thumbType == ThumbnailType::MTH || thumbType == ThumbnailType::YEAR)) {
        thumbType = ThumbnailType::THUMB;
    }
    UniqueFd uniqueFd(OpenThumbnail(path, thumbType));
    if (uniqueFd.Get() == E_ERR) {
        string openUriStr = uriStr + "?" + MEDIA_OPERN_KEYWORD + "=" + MEDIA_DATA_DB_THUMBNAIL + "&" +
            MEDIA_DATA_DB_WIDTH + "=" + to_string(size.width) + "&" + MEDIA_DATA_DB_HEIGHT + "=" +
            to_string(size.height);
        if (IsAsciiString(path)) {
            openUriStr += "&" + THUMBNAIL_PATH + "=" + path;
        }
        Uri openUri(openUriStr);
        uniqueFd = UniqueFd(UserFileClient::OpenFile(openUri, "R"));
    }
    if (uniqueFd.Get() < 0) {
        NAPI_ERR_LOG("queryThumb is null, errCode is %{public}d", uniqueFd.Get());
        return nullptr;
    }
    tracer.Finish();
    if (thumbType == ThumbnailType::MTH || thumbType == ThumbnailType::YEAR) {
        return CreateThumbnailByAshmem(uniqueFd, size);
    } else {
        return DecodeThumbnail(uniqueFd, size);
    }
}

void ThumbnailManager::DeleteRequestIdFromMap(const string &requestId)
{
    thumbRequest_.Erase(requestId);
}

bool ThumbnailManager::RequestFastImage(const RequestSharedPtr &request)
{
    MediaLibraryTracer tracer;
    tracer.Start("ThumbnailManager::RequestFastImage");
    Size fastSize;
    GetFastThumbNewSize(request->GetRequestSize(), fastSize);
    UniqueFd uniqueFd(OpenThumbnail(request->GetPath(), GetThumbType(fastSize.width, fastSize.height)));
    if (uniqueFd.Get() < 0) {
        return false;
    }
    
    PixelMapPtr pixelMap = CreateThumbnailByAshmem(uniqueFd, fastSize);
    request->SetFastPixelMap(move(pixelMap));
    return true;
}

void ThumbnailManager::DealWithFastRequest(const RequestSharedPtr &request)
{
    MediaLibraryTracer tracer;
    tracer.Start("ThumbnailManager::DealWithFastRequest");

    if (request == nullptr) {
        return;
    }
    if (!RequestFastImage(request)) {
        // when local pixelmap not exit, must add QualityThread
        AddQualityPhotoRequest(request);
        return;
    }
    // callback
    if (!NotifyImage(request, true) || !request->NeedContinue()) {
        return;
    }

    if (NeedQualityThumb(request->GetRequestSize(), request->requestPhotoType)) {
        AddQualityPhotoRequest(request);
    } else {
        DeleteRequestIdFromMap(request->GetUUID());
    }
}

void ThumbnailManager::FastImageWorker(int num)
{
    SetThreadName("FastImageWorker", num);
    while (true) {
        if (!isThreadRunning_) {
            return;
        }
        if (fastQueue_.Empty()) {
            std::unique_lock<std::mutex> lock(fastLock_);
            fastCv_.wait(lock, [this]() {
                return !isThreadRunning_ || !fastQueue_.Empty();
            });
        } else {
            RequestSharedPtr request;
            if (fastQueue_.Pop(request) && request->NeedContinue()) {
                DealWithFastRequest(request);
            }
        }
    }
}

void ThumbnailManager::QualityImageWorker(int num)
{
    SetThreadName("QualityImageWorker", num);
    while (true) {
        if (!isThreadRunning_) {
            return;
        }
        if (qualityQueue_.Empty()) {
            std::unique_lock<std::mutex> lock(qualityLock_);
            qualityCv_.wait(lock, [this]() {
                return !isThreadRunning_ || !qualityQueue_.Empty();
            });
        } else {
            RequestSharedPtr request;
            if (qualityQueue_.Pop(request) && request->NeedContinue()) {
                // request quality image
                request->SetPixelMap(QueryThumbnail(request->GetUri(),
                    request->GetRequestSize(), request->GetPath()));
                // callback
                NotifyImage(request, false);
            }
        }
    }
}

static void HandlePixelCallback(const RequestSharedPtr &request, bool isFastImage)
{
    napi_env env = request->callback_.env_;
    napi_value jsCallback = nullptr;
    napi_status status = napi_get_reference_value(env, request->callback_.callBackRef_, &jsCallback);
    if (status != napi_ok) {
        NAPI_ERR_LOG("Create reference fail, status: %{public}d", status);
        return;
    }

    napi_value retVal = nullptr;
    napi_value result[ARGS_ONE];
    if (request->GetStatus() == ThumbnailStatus::THUMB_REMOVE) {
        return;
    }
        
    if (isFastImage) {
        result[PARAM0] = Media::PixelMapNapi::CreatePixelMap(env,
            shared_ptr<PixelMap>(request->GetFastPixelMap()));
    } else {
        result[PARAM0] = Media::PixelMapNapi::CreatePixelMap(env,
            shared_ptr<PixelMap>(request->GetPixelMap()));
    }

    napi_call_function(env, nullptr, jsCallback, ARGS_ONE, result, &retVal);
    if (status != napi_ok) {
        NAPI_ERR_LOG("CallJs napi_call_function fail, status: %{public}d", status);
        return;
    }
}

static void UvJsExecute(uv_work_t *work)
{
    // js thread
    if (work == nullptr) {
        return;
    }

    ThumnailUv *uvMsg = reinterpret_cast<ThumnailUv *>(work->data);
    do {
        if (uvMsg == nullptr || uvMsg->request_ == nullptr) {
            break;
        }
        napi_env env = uvMsg->request_->callback_.env_;
        if (!uvMsg->request_->NeedContinue()) {
            break;
        }
        NapiScopeHandler scopeHandler(env);
        if (!scopeHandler.IsValid()) {
            break;
        }
        HandlePixelCallback(uvMsg->request_, uvMsg->isFastImage_);
    } while (0);
    if ((uvMsg->request_->GetStatus() == ThumbnailStatus::THUMB_QUALITY && !uvMsg->isFastImage_) ||
        (uvMsg->request_->GetStatus() == ThumbnailStatus::THUMB_REMOVE)) {
        if (uvMsg->manager_ != nullptr) {
            uvMsg->manager_->DeleteRequestIdFromMap(uvMsg->request_->GetUUID());
        }
    }

    delete uvMsg;
    delete work;
}

bool ThumbnailManager::NotifyImage(const RequestSharedPtr &request, bool isFastImage)
{
    MediaLibraryTracer tracer;
    tracer.Start("ThumbnailManager::NotifyImage");

    if (!request->NeedContinue()) {
        DeleteRequestIdFromMap(request->GetUUID());
        return false;
    }

    uv_loop_s *loop = nullptr;
    napi_get_uv_event_loop(request->callback_.env_, &loop);
    if (loop == nullptr) {
        DeleteRequestIdFromMap(request->GetUUID());
        return false;
    }

    uv_work_t *work = new (nothrow) uv_work_t;
    if (work == nullptr) {
        DeleteRequestIdFromMap(request->GetUUID());
        return false;
    }

    ThumnailUv *msg = new (nothrow) ThumnailUv(request, this, isFastImage);
    if (msg == nullptr) {
        delete work;
        DeleteRequestIdFromMap(request->GetUUID());
        return false;
    }

    work->data = reinterpret_cast<void *>(msg);
    int ret = uv_queue_work(loop, work, [](uv_work_t *w) {}, [](uv_work_t *w, int s) {
        UvJsExecute(w);
    });
    if (ret != 0) {
        NAPI_ERR_LOG("Failed to execute libuv work queue, ret: %{public}d", ret);
        delete msg;
        delete work;
        return false;
    }
    return true;
}
}
}
