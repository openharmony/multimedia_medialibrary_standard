/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#define MLOG_TAG "Extension"

#include "media_datashare_ext_ability.h"

#include "ability_info.h"
#include "dataobs_mgr_client.h"
#include "media_datashare_stub_impl.h"
#include "hilog_wrapper.h"
#include "ipc_skeleton.h"
#include "sa_mgr_client.h"
#include "datashare_ext_ability_context.h"
#include "runtime.h"
#include "napi/native_api.h"
#include "napi/native_node_api.h"
#include "medialibrary_data_manager.h"
#include "medialibrary_errno.h"
#include "medialibrary_subscriber.h"
#include "media_log.h"
#include "system_ability_definition.h"
#include "permission_utils.h"

using namespace std;
using namespace OHOS::AppExecFwk;
using namespace OHOS::NativeRdb;
using namespace OHOS::DistributedKv;
using namespace OHOS::Media;
using namespace OHOS::DataShare;

namespace OHOS {
namespace AbilityRuntime {
using namespace OHOS::AppExecFwk;
using DataObsMgrClient = OHOS::AAFwk::DataObsMgrClient;
constexpr int INVALID_VALUE = -1;

MediaDataShareExtAbility* MediaDataShareExtAbility::Create(const std::unique_ptr<Runtime>& runtime)
{
    return new MediaDataShareExtAbility(static_cast<Runtime&>(*runtime));
}

MediaDataShareExtAbility::MediaDataShareExtAbility(Runtime& runtime) : DataShareExtAbility(), runtime_(runtime) {}

MediaDataShareExtAbility::~MediaDataShareExtAbility()
{
}

void MediaDataShareExtAbility::Init(const std::shared_ptr<AbilityLocalRecord> &record,
    const std::shared_ptr<OHOSApplication> &application, std::shared_ptr<AbilityHandler> &handler,
    const sptr<IRemoteObject> &token)
{
    DataShareExtAbility::Init(record, application, handler, token);
    auto context = AbilityRuntime::Context::GetApplicationContext();
    if (context == nullptr) {
        MEDIA_ERR_LOG("Failed to get context");
        return;
    }
    MEDIA_INFO_LOG("%{public}s runtime language  %{public}d", __func__, runtime_.GetLanguage());

    MediaLibraryDataManager::GetInstance()->InitMediaLibraryMgr(context);
    MediaLibraryDataManager::GetInstance()->SetOwner(static_pointer_cast<MediaDataShareExtAbility>(shared_from_this()));
}

void MediaDataShareExtAbility::OnStart(const AAFwk::Want &want)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    Extension::OnStart(want);
    Media::MedialibrarySubscriber::Subscribe();
    MEDIA_INFO_LOG("%{public}s end.", __func__);
}

void MediaDataShareExtAbility::OnStop()
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    MediaLibraryDataManager::GetInstance()->ClearMediaLibraryMgr();
    MEDIA_INFO_LOG("%{public}s end.", __func__);
}

sptr<IRemoteObject> MediaDataShareExtAbility::OnConnect(const AAFwk::Want &want)
{
    MEDIA_INFO_LOG("%{public}s begin. ", __func__);
    Extension::OnConnect(want);
    sptr<MediaDataShareStubImpl> remoteObject = new (std::nothrow) MediaDataShareStubImpl(
        std::static_pointer_cast<MediaDataShareExtAbility>(shared_from_this()),
        nullptr);
    if (remoteObject == nullptr) {
        MEDIA_ERR_LOG("%{public}s No memory allocated for DataShareStubImpl", __func__);
        return nullptr;
    }
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return remoteObject->AsObject();
}

std::vector<std::string> MediaDataShareExtAbility::GetFileTypes(const Uri &uri, const std::string &mimeTypeFilter)
{
    std::vector<std::string> ret;
    return ret;
}

int MediaDataShareExtAbility::OpenFile(const Uri &uri, const std::string &mode)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    if (mode == MEDIA_FILEMODE_READONLY) {
        if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_READ_MEDIA)) {
            return E_PERMISSION_DENIED;
        }
    } else if (mode == MEDIA_FILEMODE_WRITEONLY ||
               mode == MEDIA_FILEMODE_WRITETRUNCATE ||
               mode == MEDIA_FILEMODE_WRITEAPPEND) {
        if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_WRITE_MEDIA)) {
            return E_PERMISSION_DENIED;
        }
    } else if (mode == MEDIA_FILEMODE_READWRITETRUNCATE ||
               mode == MEDIA_FILEMODE_READWRITE) {
        if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_READ_MEDIA) ||
            !PermissionUtils::CheckCallerPermission(PERMISSION_NAME_WRITE_MEDIA)) {
            return E_PERMISSION_DENIED;
        }
    }
    return MediaLibraryDataManager::GetInstance()->OpenFile(uri, mode);
}

int MediaDataShareExtAbility::OpenRawFile(const Uri &uri, const std::string &mode)
{
    return 0;
}

int MediaDataShareExtAbility::Insert(const Uri &uri, const DataShareValuesBucket &value)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    string tmpUri = MEDIALIBRARY_DATA_URI + "/" + MEDIA_FILEOPRN + "/" + MEDIA_FILEOPRN_CLOSEASSET;
    if (uri.ToString() == tmpUri) {
        if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_READ_MEDIA)) {
            return E_PERMISSION_DENIED;
        }
    } else if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_WRITE_MEDIA)) {
        return E_PERMISSION_DENIED;
    }
    return MediaLibraryDataManager::GetInstance()->Insert(uri, value);
}

int MediaDataShareExtAbility::Update(const Uri &uri, const DataSharePredicates &predicates,
    const DataShareValuesBucket &value)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_WRITE_MEDIA)) {
        MEDIA_ERR_LOG("%{public}s Check calling permission failed.", __func__);
        return E_PERMISSION_DENIED;
    }

    return MediaLibraryDataManager::GetInstance()->Update(uri, value, predicates);
}

int MediaDataShareExtAbility::Delete(const Uri &uri, const DataSharePredicates &predicates)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_WRITE_MEDIA)) {
        MEDIA_ERR_LOG("%{public}s Check calling permission failed.", __func__);
        return E_PERMISSION_DENIED;
    }

    return MediaLibraryDataManager::GetInstance()->Delete(uri, predicates);
}

std::shared_ptr<DataShareResultSet> MediaDataShareExtAbility::Query(const Uri &uri,
    const DataSharePredicates &predicates, std::vector<std::string> &columns)
{
    if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_READ_MEDIA)) {
        MEDIA_ERR_LOG("%{public}s Check calling permission failed.", __func__);
        return nullptr;
    }
    auto queryResultSet = MediaLibraryDataManager::GetInstance()->Query(uri, columns, predicates);
    if (queryResultSet == nullptr) {
        return nullptr;
    }
    std::shared_ptr<DataShareResultSet> resultSet = std::make_shared<DataShareResultSet>(queryResultSet);
    return resultSet;
}

std::string MediaDataShareExtAbility::GetType(const Uri &uri)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    auto ret = MediaLibraryDataManager::GetInstance()->GetType(uri);
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return ret;
}

int MediaDataShareExtAbility::BatchInsert(const Uri &uri, const std::vector<DataShareValuesBucket> &values)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    int ret = INVALID_VALUE;
    if (!PermissionUtils::CheckCallerPermission(PERMISSION_NAME_WRITE_MEDIA)) {
        MEDIA_ERR_LOG("%{public}s Check calling permission failed.", __func__);
        return ret;
    }

    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return ret;
}

bool MediaDataShareExtAbility::RegisterObserver(const Uri &uri, const sptr<AAFwk::IDataAbilityObserver> &dataObserver)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    auto obsMgrClient = DataObsMgrClient::GetInstance();
    if (obsMgrClient == nullptr) {
        MEDIA_ERR_LOG("%{public}s obsMgrClient is nullptr", __func__);
        return false;
    }

    ErrCode ret = obsMgrClient->RegisterObserver(uri, dataObserver);
    if (ret != ERR_OK) {
        MEDIA_ERR_LOG("%{public}s obsMgrClient->RegisterObserver error return %{public}d", __func__, ret);
        return false;
    }
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return true;
}

bool MediaDataShareExtAbility::UnregisterObserver(const Uri &uri, const sptr<AAFwk::IDataAbilityObserver> &dataObserver)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    auto obsMgrClient = DataObsMgrClient::GetInstance();
    if (obsMgrClient == nullptr) {
        MEDIA_ERR_LOG("%{public}s obsMgrClient is nullptr", __func__);
        return false;
    }

    ErrCode ret = obsMgrClient->UnregisterObserver(uri, dataObserver);
    if (ret != ERR_OK) {
        MEDIA_ERR_LOG("%{public}s obsMgrClient->UnregisterObserver error return %{public}d", __func__, ret);
        return false;
    }
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return true;
}

bool MediaDataShareExtAbility::NotifyChange(const Uri &uri)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    auto obsMgrClient = DataObsMgrClient::GetInstance();
    if (obsMgrClient == nullptr) {
        MEDIA_ERR_LOG("%{public}s obsMgrClient is nullptr", __func__);
        return false;
    }

    ErrCode ret = obsMgrClient->NotifyChange(uri);
    if (ret != ERR_OK) {
        MEDIA_ERR_LOG("%{public}s obsMgrClient->NotifyChange error return %{public}d", __func__, ret);
        return false;
    }
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return true;
}

Uri MediaDataShareExtAbility::NormalizeUri(const Uri &uri)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    auto ret = uri;
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return ret;
}

Uri MediaDataShareExtAbility::DenormalizeUri(const Uri &uri)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    auto ret = uri;
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return ret;
}

std::vector<std::shared_ptr<DataShareResult>> MediaDataShareExtAbility::ExecuteBatch(
    const std::vector<std::shared_ptr<DataShareOperation>> &operations)
{
    MEDIA_INFO_LOG("%{public}s begin.", __func__);
    std::vector<std::shared_ptr<DataShareResult>> ret;
    MEDIA_INFO_LOG("%{public}s end.", __func__);
    return ret;
}
} // namespace AbilityRuntime
} // namespace OHOS
