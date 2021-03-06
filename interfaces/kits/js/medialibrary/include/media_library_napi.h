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

#ifndef MEDIA_LIBRARY_NAPI_H
#define MEDIA_LIBRARY_NAPI_H

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <ftw.h>
#include <securec.h>
#include <sys/stat.h>
#include <unistd.h>
#include <variant>

#include "ability.h"
#include "ability_loader.h"
#include "abs_shared_result_set.h"
#include "album_asset_napi.h"
#include "album_napi.h"
#include "audio_asset_napi.h"
#include "data_ability_helper.h"
#include "data_ability_predicates.h"
#include "fetch_file_result_napi.h"
#include "file_asset_napi.h"
#include "image_asset_napi.h"
#include "IMediaLibraryClient.h"
#include "media_asset_napi.h"
#include "media_data_ability_const.h"
#include "medialibrary_data_ability.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "uri.h"
#include "values_bucket.h"
#include "video_asset_napi.h"
#include "data_ability_observer_stub.h"
#include "media_log.h"

namespace OHOS {
static const std::string MEDIA_LIB_NAPI_CLASS_NAME = "MediaLibrary";
const std::string AUDIO_LISTENER = "audio";
const std::string VIDEO_LISTENER = "video";
const std::string IMAGE_LISTENER = "image";
const std::string FILE_LISTENER = "file";

struct MediaChangeListener {
    Media::MediaType mediaType;
};

class ChangeListenerNapi {
public:
    explicit ChangeListenerNapi(napi_env env) : env_(env) {}

    ~ChangeListenerNapi()
    {
        MEDIA_ERR_LOG("ChangeListener destructor called");
    }

    void OnChange(const MediaChangeListener &listener, const napi_ref cbRef);

    napi_ref cbOnRef_ = nullptr;
    napi_ref cbOffRef_ = nullptr;
    sptr<AAFwk::IDataAbilityObserver> audioDataObserver_ = nullptr;
    sptr<AAFwk::IDataAbilityObserver> videoDataObserver_ = nullptr;
    sptr<AAFwk::IDataAbilityObserver> imageDataObserver_ = nullptr;
    sptr<AAFwk::IDataAbilityObserver> fileDataObserver_ = nullptr;

private:
    napi_env env_ = nullptr;
};

class MediaObserver : public AAFwk::DataAbilityObserverStub {
public:
    MediaObserver() {}
    MediaObserver(const ChangeListenerNapi &listObj, Media::MediaType mediaType)
    {
        listObj_ = const_cast<ChangeListenerNapi *>(&listObj);
        mediaType_ = mediaType;
    }

    ~MediaObserver() = default;

    void OnChange() override
    {
        MEDIA_ERR_LOG("OnChange invoked");
        MediaChangeListener listener;
        MEDIA_ERR_LOG("mediatype = %{public}d", mediaType_);
        listener.mediaType = mediaType_;
        listObj_->OnChange(listener, listObj_->cbOnRef_);
    }

    ChangeListenerNapi *listObj_ = nullptr;
    Media::MediaType mediaType_;
};

class MediaLibraryNapi {
public:
    static napi_value Init(napi_env env, napi_value exports);
    Media::IMediaLibraryClient* GetMediaLibClientInstance();

    MediaLibraryNapi();
    ~MediaLibraryNapi();

    static std::shared_ptr<AppExecFwk::DataAbilityHelper> GetDataAbilityHelper(napi_env env);
    static std::shared_ptr<AppExecFwk::DataAbilityHelper> sAbilityHelper_;

private:
    static void MediaLibraryNapiDestructor(napi_env env, void* nativeObject, void* finalize_hint);
    static napi_value MediaLibraryNapiConstructor(napi_env env, napi_callback_info info);

    static napi_value GetMediaLibraryNewInstance(napi_env env, napi_callback_info info);
    static napi_value GetMediaLibraryOldInstance(napi_env env, napi_callback_info info);
    static napi_value GetMediaAssets(napi_env env, napi_callback_info info);
    static napi_value GetAudioAssets(napi_env env, napi_callback_info info);
    static napi_value GetVideoAssets(napi_env env, napi_callback_info info);
    static napi_value GetImageAssets(napi_env env, napi_callback_info info);
    static napi_value GetVideoAlbums(napi_env env, napi_callback_info info);
    static napi_value GetImageAlbums(napi_env env, napi_callback_info info);
    static napi_value CreateAudioAsset(napi_env env, napi_callback_info info);
    static napi_value CreateVideoAsset(napi_env env, napi_callback_info info);
    static napi_value CreateImageAsset(napi_env env, napi_callback_info info);
    static napi_value CreateAlbum(napi_env env, napi_callback_info info);

    // New APIs For L2
    static napi_value JSGetFileAssets(napi_env env, napi_callback_info info);
    static napi_value JSGetAlbums(napi_env env, napi_callback_info info);

    static napi_value JSCreateAsset(napi_env env, napi_callback_info info);
    static napi_value JSModifyAsset(napi_env env, napi_callback_info info);
    static napi_value JSDeleteAsset(napi_env env, napi_callback_info info);
    static napi_value JSOpenAsset(napi_env env, napi_callback_info info);
    static napi_value JSCloseAsset(napi_env env, napi_callback_info info);

    static napi_value JSCreateAlbum(napi_env env, napi_callback_info info);
    static napi_value JSModifyAlbum(napi_env env, napi_callback_info info);
    static napi_value JSDeleteAlbum(napi_env env, napi_callback_info info);

    static napi_value JSOnCallback(napi_env env, napi_callback_info info);
    static napi_value JSOffCallback(napi_env env, napi_callback_info info);

    static napi_value CreateMediaTypeEnum(napi_env env);
    static napi_value CreateFileKeyEnum(napi_env env);

    void RegisterChange(napi_env env, const ChangeListenerNapi &listObj);
    void RegisterChangeByType(std::string type, const ChangeListenerNapi &listObj);
    void UnregisterChange(napi_env env, const ChangeListenerNapi &listObj);
    void UnregisterChangeByType(std::string type, const ChangeListenerNapi &listObj);

    Media::IMediaLibraryClient *mediaLibrary_;

    std::vector<std::string> subscribeList_;
    std::vector<std::string> unsubscribeList_;

    napi_env env_;
    napi_ref wrapper_;

    static napi_ref sConstructor_;
    static napi_ref sMediaTypeEnumRef_;
    static napi_ref sFileKeyEnumRef_;
};

struct MediaLibraryAsyncContext {
    napi_env env;
    napi_async_work work;
    napi_deferred deferred;
    napi_ref callbackRef;
    bool status;
    AssetType assetType;
    AlbumType albumType;
    MediaLibraryNapi *objectInfo;
    std::string selection;
    std::vector<std::string> selectionArgs;
    std::string order;
    std::vector<std::unique_ptr<Media::MediaAsset>> mediaAssets;
    std::vector<std::unique_ptr<Media::AudioAsset>> audioAssets;
    std::vector<std::unique_ptr<Media::VideoAsset>> videoAssets;
    std::vector<std::unique_ptr<Media::ImageAsset>> imageAssets;
    std::vector<std::unique_ptr<Media::AlbumAsset>> albumAssets;
    std::unique_ptr<Media::FetchResult> fetchFileResult;
    OHOS::NativeRdb::ValuesBucket valuesBucket;
};
} // namespace OHOS
#endif /* MEDIA_LIBRARY_NAPI_H */
