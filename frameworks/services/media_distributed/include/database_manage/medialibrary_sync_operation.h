/*
 * Copyright (C) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef OHOS_MEDIALIBRARY_SYNC_TABLE_H
#define OHOS_MEDIALIBRARY_SYNC_TABLE_H

#include <string>
#include "abs_rdb_predicates.h"
#include "distributed_kv_data_manager.h"
#include "medialibrary_db_const.h"
#include "rdb_errno.h"
#include "rdb_helper.h"
#include "rdb_store.h"
#include "rdb_store_config.h"
#include "rdb_types.h"
#include "single_kvstore.h"

namespace OHOS {
namespace Media {
using namespace OHOS::NativeRdb;
#ifdef DISTRIBUTED
struct MediaLibrarySyncOpts {
    std::shared_ptr<NativeRdb::RdbStore> rdbStore;
    std::shared_ptr<DistributedKv::SingleKvStore> kvStore;
    std::string table;
    std::string bundleName;
    std::string row;
};
#endif
class SyncStatus {
public:
    std::condition_variable cond_;
    std::mutex mtx_;
    bool isSyncComplete_{false};
};

class MediaLibrarySyncCallback : public DistributedKv::KvStoreSyncCallback {
public:
    MediaLibrarySyncCallback() = default;
    ~MediaLibrarySyncCallback() override {}
    void SyncCompleted(const std::map<std::string, DistributedKv::Status> &results) override;
    bool WaitFor();
private:
    SyncStatus status_;
};
#ifdef DISTRIBUTED
class MediaLibrarySyncOperation {
public:
    MediaLibrarySyncOperation() = delete;
    ~MediaLibrarySyncOperation() = delete;

    static bool SyncPullAllTableByNetworkId(MediaLibrarySyncOpts &syncOpts, std::vector<std::string> &devices);
    static bool SyncPullTable(MediaLibrarySyncOpts &syncOpts, std::vector<std::string> &devices);
    static bool SyncPushTable(MediaLibrarySyncOpts &syncOpts, std::vector<std::string> &devices, bool isBlock = false);
    static DistributedKv::Status SyncPushKvstore(const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore,
        const std::vector<std::string> &key, const std::string &networkId);
    static DistributedKv::Status SyncPullKvstore(const std::shared_ptr<DistributedKv::SingleKvStore> &kvStore,
        const std::vector<std::string> &key, const std::string &networkId);

private:
    static void GetOnlineDevices(const std::string &bundleName, const std::vector<std::string> &originalDevices,
        std::vector<std::string> &onlineDevices);
};
#endif
} // namespace Media
} // namespace OHOS
#endif // OHOS_MEDIALIBRARY_SYNC_TABLE_H
