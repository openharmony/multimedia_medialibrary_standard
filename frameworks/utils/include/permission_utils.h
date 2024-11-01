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
#ifndef MEDIALIBRARY_PERMISSION_UTILS_H
#define MEDIALIBRARY_PERMISSION_UTILS_H

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "bundle_mgr_interface.h"
#include "userfile_manager_types.h"

namespace OHOS {
namespace Media {
const std::string PERMISSION_NAME_READ_MEDIA = "ohos.permission.READ_MEDIA";
const std::string PERMISSION_NAME_WRITE_MEDIA = "ohos.permission.WRITE_MEDIA";
const std::string PERMISSION_NAME_MEDIA_LOCATION = "ohos.permission.MEDIA_LOCATION";
const std::string PERM_READ_IMAGEVIDEO = "ohos.permission.READ_IMAGEVIDEO";
const std::string PERM_READ_AUDIO = "ohos.permission.READ_AUDIO";
const std::string PERM_READ_DOCUMENT = "ohos.permission.READ_DOCUMENT";
const std::string PERM_WRITE_IMAGEVIDEO = "ohos.permission.WRITE_IMAGEVIDEO";
const std::string PERM_WRITE_AUDIO = "ohos.permission.WRITE_AUDIO";
const std::string PERM_WRITE_DOCUMENT = "ohos.permission.WRITE_DOCUMENT";

constexpr int PERM_GRP_SIZE = 3;
const std::array<std::string, PERM_GRP_SIZE> READ_PERMS = {
    PERM_READ_IMAGEVIDEO,
    PERM_READ_AUDIO,
    PERM_READ_DOCUMENT
};

const std::vector<std::string> WRITE_PERMS_V10 = {
    PERM_WRITE_IMAGEVIDEO,
    PERM_WRITE_AUDIO,
    PERM_WRITE_DOCUMENT
};

class PermissionUtils {
public:
    static bool CheckCallerPermission(const std::string &permission);
    static bool CheckNapiCallerPermission(const std::string &permission);
    static bool CheckCallerPermission(const std::vector<std::string> &perms);
    static bool CheckHasPermission(const std::vector<std::string> &perms);
    static void GetClientBundle(const int uid, std::string &bundleName);
    static uint32_t GetTokenId();
    static bool IsSystemApp();
    static bool IsNativeSAApp();
    static bool CheckIsSystemAppByUid();
    static std::string GetPackageNameByBundleName(const std::string &bundleName);

private:
    static sptr<AppExecFwk::IBundleMgr> GetSysBundleManager();
    static sptr<AppExecFwk::IBundleMgr> bundleMgr_;
    static std::mutex bundleMgrMutex_;
};
}  // namespace Media
}  // namespace OHOS
#endif // MEDIALIBRARY_PERMISSION_UTILS_H
