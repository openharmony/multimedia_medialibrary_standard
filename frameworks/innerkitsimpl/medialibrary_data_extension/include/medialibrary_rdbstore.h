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

#ifndef OHOS_MEDIALIBRARY_RDBSTORE_H
#define OHOS_MEDIALIBRARY_RDBSTORE_H

#include "medialibrary_unistore.h"
#include "medialibrary_sync_operation.h"
#include "timer.h"
#include "value_object.h"
#include <memory>

namespace OHOS {
namespace Media {
#ifdef DISTRIBUTED
class MediaLibraryRdbStoreObserver;
#endif

class MediaLibraryDataCallBack;

class MediaLibraryRdbStore final : public MediaLibraryUnistore {
public:
    explicit MediaLibraryRdbStore(const std::shared_ptr<OHOS::AbilityRuntime::Context> &context);
    virtual ~MediaLibraryRdbStore();

    virtual int32_t Init() override;
    virtual void Stop() override;

    virtual int32_t Insert(MediaLibraryCommand &cmd, int64_t &rowId) override;
    virtual int32_t Delete(MediaLibraryCommand &cmd, int32_t &deletedRows) override;
    virtual int32_t Update(MediaLibraryCommand &cmd, int32_t &changedRows) override;
    std::shared_ptr<NativeRdb::ResultSet> Query(MediaLibraryCommand &cmd,
        const std::vector<std::string> &columns) override;

    int32_t ExecuteSql(const std::string &sql) override;
    std::shared_ptr<NativeRdb::ResultSet> QuerySql(const std::string &sql,
        const std::vector<std::string> &selectionArgs = std::vector<std::string>()) override;

    int32_t BeginTransaction();
    int32_t RollBack();
    int32_t Commit();

    std::shared_ptr<NativeRdb::RdbStore> GetRaw() const;

    static void BuildValuesSql(const NativeRdb::ValuesBucket &values, std::vector<NativeRdb::ValueObject> &bindArgs,
        std::string &sql);
    static void BuildQuerySql(const NativeRdb::AbsRdbPredicates &predicates, const std::vector<std::string> &columns,
        std::vector<NativeRdb::ValueObject> &bindArgs, std::string &sql);
    static int32_t ExecuteForLastInsertedRowId(const std::string &sql,
        const std::vector<NativeRdb::ValueObject> &bindArgs);
    static std::shared_ptr<NativeRdb::ResultSet> Query(const NativeRdb::AbsRdbPredicates &predicates,
        const std::vector<std::string> &columns);
    static int32_t Delete(const NativeRdb::AbsRdbPredicates &predicates);
    static int32_t Update(const NativeRdb::ValuesBucket &values, const NativeRdb::AbsRdbPredicates &predicates);
    static int32_t DeleteFromDisk(const NativeRdb::AbsRdbPredicates &predicates);
    static void ReplacePredicatesUriToId(NativeRdb::AbsRdbPredicates &predicates);
    static void UpdateAPI10Tables();
    static std::shared_ptr<NativeRdb::ResultSet> GetIndexOfUri(const NativeRdb::AbsRdbPredicates &predicates,
        const std::vector<std::string> &columns, const std::string &id);
    static int32_t GetInt(const std::shared_ptr<NativeRdb::ResultSet> &resultSet, const std::string &column);
    static std::string GetString(const std::shared_ptr<NativeRdb::ResultSet> &resultSet, const std::string &column);

private:
    static const std::string CloudSyncTriggerFunc(const std::vector<std::string> &args);
    static const std::string IsCallerSelfFunc(const std::vector<std::string> &args);
    static constexpr int RDB_TRANSACTION_WAIT_MS = 1000;
    std::mutex transactionMutex_;
    std::condition_variable transactionCV_;
    std::atomic<bool> isInTransaction_;
    static std::shared_ptr<NativeRdb::RdbStore> rdbStore_;
#ifdef DISTRIBUTED
    std::shared_ptr<MediaLibraryRdbStoreObserver> rdbStoreObs_;
#endif
    std::string bundleName_{BUNDLE_NAME};
    NativeRdb::RdbStoreConfig config_{""};
};

class MediaLibraryDataCallBack : public NativeRdb::RdbOpenCallback {
public:
    struct DirValuesBucket {
        int32_t directoryType;
        std::string dirValues;
        std::string typeValues;
        std::string extensionValues;
    };

    struct SmartAlbumValuesBucket {
        int32_t albumId;
        std::string albumName;
        int32_t albumType;
    };

    int32_t OnCreate(NativeRdb::RdbStore &rdbStore) override;
    int32_t OnUpgrade(NativeRdb::RdbStore &rdbStore, int32_t oldVersion, int32_t newVersion) override;

private:
    int32_t PrepareDir(NativeRdb::RdbStore &store);
    int32_t PrepareSmartAlbum(NativeRdb::RdbStore &store);

    int32_t InsertDirValues(const DirValuesBucket &dirValuesBucket, NativeRdb::RdbStore &store);
    int32_t InsertSmartAlbumValues(const SmartAlbumValuesBucket &smartAlbum, NativeRdb::RdbStore &store);
};

#ifdef DISTRIBUTED
class MediaLibraryRdbStoreObserver : public NativeRdb::RdbStore::RdbStoreObserver {
public:
    explicit MediaLibraryRdbStoreObserver(const std::string &bundleName);
    virtual ~MediaLibraryRdbStoreObserver();
    void OnChange(const std::vector<std::string> &devices) override;

private:
    void NotifyDeviceChange();
    static constexpr int NOTIFY_TIME_INTERVAL = 10000;
    std::unique_ptr<OHOS::Utils::Timer> timer_;
    uint32_t timerId_{0};
    std::string bundleName_;
    bool isNotifyDeviceChange_;
};
#endif

/**
 * This class is used for database transaction creation, commit, and rollback
 * The usage of class is as follows:
 *   1. initialize TransactionOperations object
 *          (for example: TranscationOperations opt)
 *   2. After init opt, you need call Start() function to start transaction
 *          int32_t err = opt.Start();
 *          if err != E_OK, transaction init failed
 *   3. If you need to commit transaction, then use
 *          int32_t err = opt.Finish();
 *          if err != E_OK, transation commit failed and auto rollback
 *   4. If TransactionOperations is destructed without successfully finish, it will be auto rollback
 */
class TransactionOperations {
public:
    TransactionOperations();
    ~TransactionOperations();
    int32_t Start();
    void Finish();
private:
    int32_t BeginTransaction();
    int32_t TransactionCommit();
    int32_t TransactionRollback();

    std::shared_ptr<MediaLibraryRdbStore> rdbStore_;
    bool isStart = false;
    bool isFinish = false;
    std::mutex mutex_;
};
} // namespace Media
} // namespace OHOS

#endif // OHOS_MEDIALIBRARY_RDBSTORE_OPERATIONS_H
