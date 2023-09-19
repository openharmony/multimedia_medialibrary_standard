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
#define MLOG_TAG "RdbStore"

#include "medialibrary_rdbstore.h"

#include <mutex>

#include "cloud_sync_helper.h"
#include "media_file_uri.h"
#include "media_file_utils.h"
#include "media_log.h"
#ifdef DISTRIBUTED
#include "medialibrary_device.h"
#endif
#include "medialibrary_errno.h"
#include "medialibrary_object_utils.h"
#include "medialibrary_tracer.h"
#include "media_scanner.h"
#include "media_scanner_manager.h"
#include "medialibrary_unistore_manager.h"
#include "photo_album_column.h"
#include "photo_map_column.h"
#include "rdb_sql_utils.h"
#include "result_set_utils.h"

using namespace std;
using namespace OHOS::NativeRdb;

namespace OHOS::Media {
shared_ptr<NativeRdb::RdbStore> MediaLibraryRdbStore::rdbStore_;
struct UniqueMemberValuesBucket {
    std::string assetMediaType;
    int32_t startNumber;
};

const std::string MediaLibraryRdbStore::CloudSyncTriggerFunc(const std::vector<std::string> &args)
{
    CloudSyncHelper::GetInstance()->StartSync();
    return "";
}

const std::string MediaLibraryRdbStore::IsCallerSelfFunc(const std::vector<std::string> &args)
{
    return "true";
}

MediaLibraryRdbStore::MediaLibraryRdbStore(const shared_ptr<OHOS::AbilityRuntime::Context> &context)
{
    if (context == nullptr) {
        MEDIA_ERR_LOG("Failed to get context");
        return;
    }
    string databaseDir = context->GetDatabaseDir();
    string name = MEDIA_DATA_ABILITY_DB_NAME;
    int32_t errCode = 0;
    string realPath = RdbSqlUtils::GetDefaultDatabasePath(databaseDir, name, errCode);
    config_.SetName(move(name));
    config_.SetPath(move(realPath));
    config_.SetBundleName(context->GetBundleName());
    config_.SetArea(context->GetArea());
    config_.SetSecurityLevel(SecurityLevel::S3);
    config_.SetScalarFunction("cloud_sync_func", 0, CloudSyncTriggerFunc);
    config_.SetScalarFunction("is_caller_self_func", 0, IsCallerSelfFunc);
    isInTransaction_.store(false);
}

int32_t MediaLibraryRdbStore::Init()
{
    MEDIA_INFO_LOG("Init rdb store");
    if (rdbStore_ != nullptr) {
        return E_OK;
    }

    int32_t errCode = 0;
    MediaLibraryDataCallBack rdbDataCallBack;
    rdbStore_ = RdbHelper::GetRdbStore(config_, MEDIA_RDB_VERSION, rdbDataCallBack, errCode);
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("GetRdbStore is failed ");
        return E_ERR;
    }
    MEDIA_INFO_LOG("SUCCESS");
    return E_OK;
}

MediaLibraryRdbStore::~MediaLibraryRdbStore() = default;

void MediaLibraryRdbStore::Stop()
{
    if (rdbStore_ == nullptr) {
        return;
    }

    rdbStore_ = nullptr;
}

static int32_t ExecSqls(const vector<string> &sqls, RdbStore &store)
{
    int32_t err = NativeRdb::E_OK;
    for (const auto &sql : sqls) {
        err = store.ExecuteSql(sql);
        if (err != NativeRdb::E_OK) {
            MEDIA_ERR_LOG("Failed to exec: %{public}s", sql.c_str());
            /* try update as much as possible */
            continue;
        }
    }
    return NativeRdb::E_OK;
}

#ifdef DISTRIBUTED
void GetAllNetworkId(vector<string> &networkIds)
{
    vector<OHOS::DistributedHardware::DmDeviceInfo> deviceList;
    MediaLibraryDevice::GetInstance()->GetAllNetworkId(deviceList);
    for (auto& deviceInfo : deviceList) {
        networkIds.push_back(deviceInfo.networkId);
    }
}
#endif

int32_t MediaLibraryRdbStore::Insert(MediaLibraryCommand &cmd, int64_t &rowId)
{
    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryRdbStore::Insert");
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }

    int32_t ret = rdbStore_->Insert(rowId, cmd.GetTableName(), cmd.GetValueBucket());
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("rdbStore_->Insert failed, ret = %{public}d", ret);
        return E_HAS_DB_ERROR;
    }

    MEDIA_DEBUG_LOG("rdbStore_->Insert end, rowId = %d, ret = %{public}d", (int)rowId, ret);
    return ret;
}

static int32_t DoDeleteFromPredicates(NativeRdb::RdbStore &rdb, const AbsRdbPredicates &predicates,
    int32_t &deletedRows)
{
    int32_t ret = NativeRdb::E_ERROR;
    string tableName = predicates.GetTableName();
    ValuesBucket valuesBucket;
    if (tableName == MEDIALIBRARY_TABLE || tableName == PhotoColumn::PHOTOS_TABLE) {
        valuesBucket.PutInt(MEDIA_DATA_DB_DIRTY, static_cast<int32_t>(DirtyType::TYPE_DELETED));
        valuesBucket.PutInt(MEDIA_DATA_DB_SYNC_STATUS, static_cast<int32_t>(SyncStatusType::TYPE_UPLOAD));
        ret = rdb.Update(deletedRows, tableName, valuesBucket, predicates.GetWhereClause(),
            predicates.GetWhereArgs());
    } else if (tableName == PhotoAlbumColumns::TABLE) {
        valuesBucket.PutInt(PhotoAlbumColumns::ALBUM_DIRTY, static_cast<int32_t>(DirtyType::TYPE_DELETED));
        ret = rdb.Update(deletedRows, tableName, valuesBucket, predicates.GetWhereClause(),
            predicates.GetWhereArgs());
    } else if (tableName == PhotoMap::TABLE) {
        valuesBucket.PutInt(PhotoMap::DIRTY, static_cast<int32_t>(DirtyType::TYPE_DELETED));
        ret = rdb.Update(deletedRows, tableName, valuesBucket, predicates.GetWhereClause(),
            predicates.GetWhereArgs());
    } else {
        ret = rdb.Delete(deletedRows, tableName, predicates.GetWhereClause(), predicates.GetWhereArgs());
    }
    return ret;
}

int32_t MediaLibraryRdbStore::Delete(MediaLibraryCommand &cmd, int32_t &deletedRows)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }
    MediaLibraryTracer tracer;
    tracer.Start("RdbStore->DeleteByCmd");
    /* local delete */
    int32_t ret = DoDeleteFromPredicates(*rdbStore_, *(cmd.GetAbsRdbPredicates()), deletedRows);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("rdbStore_->Delete failed, ret = %{public}d", ret);
        return E_HAS_DB_ERROR;
    }
    CloudSyncHelper::GetInstance()->StartSync();
    return ret;
}

int32_t MediaLibraryRdbStore::Update(MediaLibraryCommand &cmd, int32_t &changedRows)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("rdbStore_ is nullptr");
        return E_HAS_DB_ERROR;
    }

    MediaLibraryTracer tracer;
    tracer.Start("RdbStore->UpdateByCmd");
    int32_t ret = rdbStore_->Update(changedRows, cmd.GetTableName(), cmd.GetValueBucket(),
        cmd.GetAbsRdbPredicates()->GetWhereClause(), cmd.GetAbsRdbPredicates()->GetWhereArgs());
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("rdbStore_->Update failed, ret = %{public}d", ret);
        return E_HAS_DB_ERROR;
    }
    return ret;
}

static inline string GetQueryFilter(const string &tableName)
{
    if (tableName == MEDIALIBRARY_TABLE) {
        return MEDIALIBRARY_TABLE + "." + MEDIA_DATA_DB_SYNC_STATUS + " = " +
            to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE));
    } else if (tableName == PhotoColumn::PHOTOS_TABLE) {
        return PhotoColumn::PHOTOS_TABLE + "." + PhotoColumn::PHOTO_SYNC_STATUS + " = " +
            to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE));
    } else if (tableName == PhotoAlbumColumns::TABLE) {
        return PhotoAlbumColumns::TABLE + "." + PhotoAlbumColumns::ALBUM_DIRTY + " != " +
            to_string(static_cast<int32_t>(DirtyTypes::TYPE_DELETED));
    } else if (tableName == PhotoMap::TABLE) {
        return PhotoMap::TABLE + "." + PhotoMap::DIRTY + " != " + to_string(static_cast<int32_t>(
            DirtyTypes::TYPE_DELETED));
    }
    return "";
}

shared_ptr<NativeRdb::ResultSet> MediaLibraryRdbStore::GetIndexOfUri(const AbsRdbPredicates &predicates,
    const vector<string> &columns, const string &id)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("rdbStore_ is nullptr");
        return nullptr;
    }
    MediaLibraryTracer tracer;
    tracer.Start("GetIndexOfUri");
    string sql;
    sql.append("SELECT ").append(PHOTO_INDEX).append(" From (");
    sql.append(RdbSqlUtils::BuildQueryString(predicates, columns));
    sql.append(") where "+ MediaColumn::MEDIA_ID + " = ").append(id);
    MEDIA_DEBUG_LOG("sql = %{public}s", sql.c_str());
    for (auto &arg : predicates.GetWhereArgs()) {
        MEDIA_DEBUG_LOG("arg = %{public}s", arg.c_str());
    }
    return rdbStore_->QuerySql(sql, predicates.GetWhereArgs());
}

shared_ptr<NativeRdb::ResultSet> MediaLibraryRdbStore::Query(MediaLibraryCommand &cmd,
    const vector<string> &columns)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("rdbStore_ is nullptr");
        return nullptr;
    }

    MediaLibraryTracer tracer;
    tracer.Start("RdbStore->QueryByCmd");
#ifdef MEDIALIBRARY_COMPATIBILITY
    auto predicates = cmd.GetAbsRdbPredicates();
    MEDIA_DEBUG_LOG("tablename = %{public}s", predicates->GetTableName().c_str());
    for (const auto &col : columns) {
        MEDIA_DEBUG_LOG("col = %{public}s", col.c_str());
    }
    MEDIA_DEBUG_LOG("whereClause = %{public}s", predicates->GetWhereClause().c_str());
    for (auto &arg : predicates->GetWhereArgs()) {
        MEDIA_DEBUG_LOG("whereArgs = %{public}s", arg.c_str());
    }
    MEDIA_DEBUG_LOG("limit = %{public}d", predicates->GetLimit());
#endif

    /*
     * adapter pattern:
     * Reuse predicates-based query so that no need to modify both func
     * if later logic changes take place
     */
    return Query(*cmd.GetAbsRdbPredicates(), columns);
}

static void AddQueryFilter(AbsRdbPredicates &predicates)
{
    /* build all-table vector */
    string tableName = predicates.GetTableName();
    vector<string> joinTables = predicates.GetJoinTableNames();
    joinTables.push_back(tableName);
    /* add filters */
    string filters;
    for (auto &t : joinTables) {
        string filter = GetQueryFilter(t);
        if (filters.empty()) {
            filters += filter;
        } else {
            filters += " AND " + filter;
        }
    }
    if (filters.empty()) {
        return;
    }

    /* rebuild */
    string queryCondition = predicates.GetWhereClause();
    queryCondition = queryCondition.empty() ? filters : filters + " AND " + queryCondition;
    predicates.SetWhereClause(queryCondition);
}

shared_ptr<NativeRdb::ResultSet> MediaLibraryRdbStore::Query(const AbsRdbPredicates &predicates,
    const vector<string> &columns)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("rdbStore_ is nullptr");
        return nullptr;
    }

    /* add filter */
    AddQueryFilter(const_cast<AbsRdbPredicates &>(predicates));

    MediaLibraryTracer tracer;
    tracer.Start("RdbStore->QueryByPredicates");
    return rdbStore_->Query(predicates, columns);
}

int32_t MediaLibraryRdbStore::ExecuteSql(const string &sql)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }

    MediaLibraryTracer tracer;
    tracer.Start("RdbStore->ExecuteSql");
    int32_t ret = rdbStore_->ExecuteSql(sql);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("rdbStore_->ExecuteSql failed, ret = %{public}d", ret);
        return E_HAS_DB_ERROR;
    }
    return ret;
}

void MediaLibraryRdbStore::BuildValuesSql(const NativeRdb::ValuesBucket &values, vector<ValueObject> &bindArgs,
    string &sql)
{
    map<string, ValueObject> valuesMap;
    values.GetAll(valuesMap);
    sql.append("(");
    for (auto iter = valuesMap.begin(); iter != valuesMap.end(); iter++) {
        sql.append(((iter == valuesMap.begin()) ? "" : ", "));
        sql.append(iter->first);               // columnName
        bindArgs.push_back(iter->second); // columnValue
    }

    sql.append(") select ");
    for (size_t i = 0; i < valuesMap.size(); i++) {
        sql.append(((i == 0) ? "?" : ", ?"));
    }
    sql.append(" ");
}

void MediaLibraryRdbStore::BuildQuerySql(const AbsRdbPredicates &predicates, const vector<string> &columns,
    vector<ValueObject> &bindArgs, string &sql)
{
    sql.append(RdbSqlUtils::BuildQueryString(predicates, columns));
    for (auto &arg : predicates.GetWhereArgs()) {
        bindArgs.emplace_back(arg);
    }
}

/**
 * Returns last insert row id. If insert succeed but no new rows inserted, then return -1.
 * Return E_HAS_DB_ERROR on error cases.
 */
int32_t MediaLibraryRdbStore::ExecuteForLastInsertedRowId(const string &sql, const vector<ValueObject> &bindArgs)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }

    int64_t lastInsertRowId = 0;
    int32_t err = rdbStore_->ExecuteForLastInsertedRowId(lastInsertRowId, sql, bindArgs);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to execute insert, err: %{public}d", err);
        return E_HAS_DB_ERROR;
    }
    return lastInsertRowId;
}

int32_t MediaLibraryRdbStore::Delete(const AbsRdbPredicates &predicates)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }
    int err = E_ERR;
    int32_t deletedRows = 0;
    err = DoDeleteFromPredicates(*rdbStore_, predicates, deletedRows);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to execute delete, err: %{public}d", err);
        return E_HAS_DB_ERROR;
    }
    CloudSyncHelper::GetInstance()->StartSync();
    return deletedRows;
}

/**
 * Return changed rows on success, or negative values on error cases.
 */
int32_t MediaLibraryRdbStore::Update(const ValuesBucket &values,
    const AbsRdbPredicates &predicates)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }

    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryRdbStore::Update by predicates");
    int32_t changedRows = -1;
    int err = rdbStore_->Update(changedRows, values, predicates);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to execute update, err: %{public}d", err);
        return E_HAS_DB_ERROR;
    }
    return changedRows;
}

shared_ptr<NativeRdb::ResultSet> MediaLibraryRdbStore::QuerySql(const string &sql, const vector<string> &selectionArgs)
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return nullptr;
    }

    MediaLibraryTracer tracer;
    tracer.Start("RdbStore->QuerySql");
    return rdbStore_->QuerySql(sql, selectionArgs);
}

int32_t MediaLibraryRdbStore::BeginTransaction()
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }

    unique_lock<mutex> cvLock(transactionMutex_);
    if (isInTransaction_.load()) {
        transactionCV_.wait_for(cvLock, chrono::milliseconds(RDB_TRANSACTION_WAIT_MS),
            [this] () { return !(isInTransaction_.load()); });
    }

    if (rdbStore_->IsInTransaction()) {
        MEDIA_ERR_LOG("RdbStore is still in transaction");
        return E_HAS_DB_ERROR;
    }

    isInTransaction_.store(true);
    int32_t errCode = rdbStore_->BeginTransaction();
    if (errCode != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Start Transaction failed, errCode=%{public}d", errCode);
        isInTransaction_.store(false);
        transactionCV_.notify_one();
        return E_HAS_DB_ERROR;
    }

    return E_OK;
}

int32_t MediaLibraryRdbStore::Commit()
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }

    if (!(isInTransaction_.load()) || !(rdbStore_->IsInTransaction())) {
        MEDIA_ERR_LOG("no transaction now");
        return E_HAS_DB_ERROR;
    }

    int32_t errCode = rdbStore_->Commit();
    isInTransaction_.store(false);
    transactionCV_.notify_all();
    if (errCode != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("commit failed, errCode=%{public}d", errCode);
        return E_HAS_DB_ERROR;
    }

    return E_OK;
}

int32_t MediaLibraryRdbStore::RollBack()
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
        return E_HAS_DB_ERROR;
    }
    if (!(isInTransaction_.load()) || !(rdbStore_->IsInTransaction())) {
        MEDIA_ERR_LOG("no transaction now");
        return E_HAS_DB_ERROR;
    }

    int32_t errCode = rdbStore_->RollBack();
    isInTransaction_.store(false);
    transactionCV_.notify_all();
    if (errCode != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("rollback failed, errCode=%{public}d", errCode);
        return E_HAS_DB_ERROR;
    }

    return E_OK;
}

shared_ptr<NativeRdb::RdbStore> MediaLibraryRdbStore::GetRaw() const
{
    return rdbStore_;
}

static inline int32_t DeleteDbByFileId(const string &table, int32_t fileId)
{
    AbsRdbPredicates predicates(table);
    predicates.EqualTo(MediaColumn::MEDIA_ID, to_string(fileId));
    predicates.GreaterThan(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    return MediaLibraryRdbStore::Delete(predicates);
}

int32_t MediaLibraryRdbStore::DeleteFromDisk(const AbsRdbPredicates &predicates)
{
    vector<string> columns = {
        MediaColumn::MEDIA_ID,
        MediaColumn::MEDIA_FILE_PATH
    };
    auto resultSet = Query(predicates, columns);
    if (resultSet == nullptr) {
        return E_HAS_DB_ERROR;
    }
    int32_t count = 0;
    int32_t err = resultSet->GetRowCount(count);
    if (err != E_OK) {
        return E_HAS_DB_ERROR;
    }
    int32_t deletedRows = 0;
    for (int32_t i = 0; i < count; i++) {
        err = resultSet->GoToNextRow();
        if (err != E_OK) {
            return E_HAS_DB_ERROR;
        }

        // Delete file from db.
        int32_t fileId = get<int32_t>(ResultSetUtils::GetValFromColumn(MediaColumn::MEDIA_ID, resultSet, TYPE_INT32));
        if (fileId <= 0) {
            return E_HAS_DB_ERROR;
        }
        int32_t deletedRow = DeleteDbByFileId(predicates.GetTableName(), fileId);
        if (deletedRow < 0) {
            return E_HAS_DB_ERROR;
        }
        // If deletedRow is 0, the file may be deleted already somewhere else, so just continue.
        if (deletedRow == 0) {
            continue;
        }

        // Delete file from file system.
        string filePath = get<string>(ResultSetUtils::GetValFromColumn(MediaColumn::MEDIA_FILE_PATH, resultSet,
                                                                       TYPE_STRING));
        if (filePath.empty()) {
            return E_HAS_DB_ERROR;
        }
        if (!MediaFileUtils::DeleteFile(filePath) && (errno != -ENOENT)) {
            MEDIA_ERR_LOG("Failed to delete file, errno: %{public}d, path: %{private}s", errno, filePath.c_str());
            return E_HAS_FS_ERROR;
        }
        MediaLibraryObjectUtils::InvalidateThumbnail(to_string(fileId), predicates.GetTableName(), filePath);
        deletedRows += deletedRow;
    }
    return deletedRows;
}

void MediaLibraryRdbStore::ReplacePredicatesUriToId(AbsRdbPredicates &predicates)
{
    const vector<string> &whereUriArgs = predicates.GetWhereArgs();
    vector<string> whereIdArgs;
    whereIdArgs.reserve(whereUriArgs.size());
    for (const auto &arg : whereUriArgs) {
        if (!MediaFileUtils::StartsWith(arg, PhotoColumn::PHOTO_URI_PREFIX)) {
            whereIdArgs.push_back(arg);
            continue;
        }
        whereIdArgs.push_back(MediaFileUri::GetPhotoId(arg));
    }

    predicates.SetWhereArgs(whereIdArgs);
}

int32_t MediaLibraryRdbStore::GetInt(const shared_ptr<ResultSet> &resultSet, const string &column)
{
    return get<int32_t>(ResultSetUtils::GetValFromColumn(column, resultSet, TYPE_INT32));
}

string MediaLibraryRdbStore::GetString(const shared_ptr<ResultSet> &resultSet, const string &column)
{
    return get<string>(ResultSetUtils::GetValFromColumn(column, resultSet, TYPE_STRING));
}

inline void BuildInsertSystemAlbumSql(const ValuesBucket &values, const AbsRdbPredicates &predicates,
    string &sql, vector<ValueObject> &bindArgs)
{
    // Build insert sql
    sql.append("INSERT").append(" OR ROLLBACK ").append(" INTO ").append(PhotoAlbumColumns::TABLE).append(" ");
    MediaLibraryRdbStore::BuildValuesSql(values, bindArgs, sql);
    sql.append(" WHERE NOT EXISTS (");
    MediaLibraryRdbStore::BuildQuerySql(predicates, { PhotoAlbumColumns::ALBUM_ID }, bindArgs, sql);
    sql.append(");");
}

int32_t PrepareSystemAlbums(RdbStore &store)
{
    ValuesBucket values;
    int32_t err = E_FAIL;
    store.BeginTransaction();
    for (int32_t i = PhotoAlbumSubType::SYSTEM_START; i <= PhotoAlbumSubType::SYSTEM_END; i++) {
        values.PutInt(PhotoAlbumColumns::ALBUM_TYPE, PhotoAlbumType::SYSTEM);
        values.PutInt(PhotoAlbumColumns::ALBUM_SUBTYPE, i);

        AbsRdbPredicates predicates(PhotoAlbumColumns::TABLE);
        predicates.EqualTo(PhotoAlbumColumns::ALBUM_TYPE, to_string(PhotoAlbumType::SYSTEM));
        predicates.EqualTo(PhotoAlbumColumns::ALBUM_SUBTYPE, to_string(i));

        string sql;
        vector<ValueObject> bindArgs;
        BuildInsertSystemAlbumSql(values, predicates, sql, bindArgs);
        err = store.ExecuteSql(sql, bindArgs);
        if (err != E_OK) {
            store.RollBack();
            return err;
        }
        values.Clear();
    }
    store.Commit();
    return E_OK;
}

int32_t MediaLibraryDataCallBack::PrepareDir(RdbStore &store)
{
    DirValuesBucket cameraDir = {
        CAMERA_DIRECTORY_TYPE_VALUES, CAMERA_DIR_VALUES, CAMERA_TYPE_VALUES, CAMERA_EXTENSION_VALUES
    };
    DirValuesBucket videoDir = {
        VIDEO_DIRECTORY_TYPE_VALUES, VIDEO_DIR_VALUES, VIDEO_TYPE_VALUES, VIDEO_EXTENSION_VALUES
    };
    DirValuesBucket pictureDir = {
        PIC_DIRECTORY_TYPE_VALUES, PIC_DIR_VALUES, PIC_TYPE_VALUES, PIC_EXTENSION_VALUES
    };
    DirValuesBucket audioDir = {
        AUDIO_DIRECTORY_TYPE_VALUES, AUDIO_DIR_VALUES, AUDIO_TYPE_VALUES, AUDIO_EXTENSION_VALUES
    };
    DirValuesBucket documentDir = {
        DOC_DIRECTORY_TYPE_VALUES, DOC_DIR_VALUES, DOC_TYPE_VALUES, DOC_EXTENSION_VALUES
    };
    DirValuesBucket downloadDir = {
        DOWNLOAD_DIRECTORY_TYPE_VALUES, DOWNLOAD_DIR_VALUES, DOWNLOAD_TYPE_VALUES, DOWNLOAD_EXTENSION_VALUES
    };

    vector<DirValuesBucket> dirValuesBuckets = {
        cameraDir, videoDir, pictureDir, audioDir, documentDir, downloadDir
    };

    for (const auto& dirValuesBucket : dirValuesBuckets) {
        if (InsertDirValues(dirValuesBucket, store) != NativeRdb::E_OK) {
            MEDIA_ERR_LOG("PrepareDir failed");
            return NativeRdb::E_ERROR;
        }
    }
    return NativeRdb::E_OK;
}

int32_t MediaLibraryDataCallBack::InsertDirValues(const DirValuesBucket &dirValuesBucket, RdbStore &store)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutInt(DIRECTORY_DB_DIRECTORY_TYPE, dirValuesBucket.directoryType);
    valuesBucket.PutString(DIRECTORY_DB_DIRECTORY, dirValuesBucket.dirValues);
    valuesBucket.PutString(DIRECTORY_DB_MEDIA_TYPE, dirValuesBucket.typeValues);
    valuesBucket.PutString(DIRECTORY_DB_EXTENSION, dirValuesBucket.extensionValues);
    int64_t outRowId = -1;
    int32_t insertResult = store.Insert(outRowId, MEDIATYPE_DIRECTORY_TABLE, valuesBucket);
    MEDIA_DEBUG_LOG("insert dir outRowId: %{public}ld insertResult: %{public}d", (long)outRowId, insertResult);
    return insertResult;
}

int32_t MediaLibraryDataCallBack::PrepareSmartAlbum(RdbStore &store)
{
    SmartAlbumValuesBucket trashAlbum = {
        TRASH_ALBUM_ID_VALUES, TRASH_ALBUM_NAME_VALUES, TRASH_ALBUM_TYPE_VALUES
    };

    SmartAlbumValuesBucket favAlbum = {
        FAVOURITE_ALBUM_ID_VALUES, FAVOURTIE_ALBUM_NAME_VALUES, FAVOURITE_ALBUM_TYPE_VALUES
    };

    vector<SmartAlbumValuesBucket> smartAlbumValuesBuckets = {
        trashAlbum, favAlbum
    };

    for (const auto& smartAlbum : smartAlbumValuesBuckets) {
        if (InsertSmartAlbumValues(smartAlbum, store) != NativeRdb::E_OK) {
            MEDIA_ERR_LOG("Prepare smartAlbum failed");
            return NativeRdb::E_ERROR;
        }
    }
    return NativeRdb::E_OK;
}

int32_t MediaLibraryDataCallBack::InsertSmartAlbumValues(const SmartAlbumValuesBucket &smartAlbum, RdbStore &store)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutInt(SMARTALBUM_DB_ID, smartAlbum.albumId);
    valuesBucket.PutString(SMARTALBUM_DB_NAME, smartAlbum.albumName);
    valuesBucket.PutInt(SMARTALBUM_DB_ALBUM_TYPE, smartAlbum.albumType);
    int64_t outRowId = -1;
    int32_t insertResult = store.Insert(outRowId, SMARTALBUM_TABLE, valuesBucket);
    return insertResult;
}

static int32_t InsertUniqueMemberTableValues(const UniqueMemberValuesBucket &uniqueMemberValues,
    RdbStore &store)
{
    ValuesBucket valuesBucket;
    valuesBucket.PutString(ASSET_MEDIA_TYPE, uniqueMemberValues.assetMediaType);
    valuesBucket.PutInt(UNIQUE_NUMBER, uniqueMemberValues.startNumber);
    int64_t outRowId = -1;
    int32_t insertResult = store.Insert(outRowId, ASSET_UNIQUE_NUMBER_TABLE, valuesBucket);
    return insertResult;
}

static int32_t PrepareUniqueMemberTable(RdbStore &store)
{
    string queryRowSql = "SELECT COUNT(*) as count FROM " + ASSET_UNIQUE_NUMBER_TABLE;
    auto resultSet = store.QuerySql(queryRowSql);
    if (resultSet == nullptr || resultSet->GoToFirstRow() != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Can not get AssetUniqueNumberTable count");
        return NativeRdb::E_ERROR;
    }
    if (GetInt32Val("count", resultSet) != 0) {
        MEDIA_DEBUG_LOG("AssetUniqueNumberTable is already inited");
        return E_OK;
    }

    UniqueMemberValuesBucket imageBucket = { IMAGE_ASSET_TYPE, 0 };
    UniqueMemberValuesBucket videoBucket = { VIDEO_ASSET_TYPE, 0 };
    UniqueMemberValuesBucket audioBucket = { AUDIO_ASSET_TYPE, 0 };

    vector<UniqueMemberValuesBucket> uniqueNumberValueBuckets = {
        imageBucket, videoBucket, audioBucket
    };

    for (const auto& uniqueNumberValueBucket : uniqueNumberValueBuckets) {
        if (InsertUniqueMemberTableValues(uniqueNumberValueBucket, store) != NativeRdb::E_OK) {
            MEDIA_ERR_LOG("Prepare smartAlbum failed");
            return NativeRdb::E_ERROR;
        }
    }
    return NativeRdb::E_OK;
}

static const string &TriggerDeleteAlbumClearMap()
{
    static const string TRIGGER_CLEAR_MAP = BaseColumn::CreateTrigger() + "photo_album_clear_map" +
    " AFTER DELETE ON " + PhotoAlbumColumns::TABLE +
    " BEGIN " +
        "DELETE FROM " + PhotoMap::TABLE +
        " WHERE " + PhotoMap::ALBUM_ID + "=" + "OLD." + PhotoAlbumColumns::ALBUM_ID + ";" +
    " END;";
    return TRIGGER_CLEAR_MAP;
}

static const string &TriggerAddAssets()
{
    static const string TRIGGER_ADD_ASSETS = BaseColumn::CreateTrigger() + "photo_album_insert_asset" +
    " AFTER INSERT ON " + PhotoMap::TABLE +
    " BEGIN " +
        "UPDATE " + PhotoAlbumColumns::TABLE + " SET " +
            PhotoAlbumColumns::ALBUM_COUNT + " = " + PhotoAlbumColumns::ALBUM_COUNT + " + 1 " +
        "WHERE " + PhotoAlbumColumns::ALBUM_ID + " = " + "NEW." + PhotoMap::ALBUM_ID + ";" +
    " END;";
    return TRIGGER_ADD_ASSETS;
}

static const string &TriggerRemoveAssets()
{
    static const string TRIGGER_REMOVE_ASSETS = BaseColumn::CreateTrigger() + "photo_album_delete_asset" +
    " AFTER DELETE ON " + PhotoMap::TABLE +
    " BEGIN " +
        "UPDATE " + PhotoAlbumColumns::TABLE + " SET " +
            PhotoAlbumColumns::ALBUM_COUNT + " = " + PhotoAlbumColumns::ALBUM_COUNT + " - 1 " +
        "WHERE " + PhotoAlbumColumns::ALBUM_ID + " = " + "OLD." + PhotoMap::ALBUM_ID + ";" +
    " END;";
    return TRIGGER_REMOVE_ASSETS;
}

static const string &TriggerDeletePhotoClearMap()
{
    static const string TRIGGER_DELETE_ASSETS = BaseColumn::CreateTrigger() + "delete_photo_clear_map" +
    " AFTER DELETE ON " + PhotoColumn::PHOTOS_TABLE +
    " BEGIN " +
        "DELETE FROM " + PhotoMap::TABLE +
        " WHERE " + PhotoMap::ASSET_ID + "=" + "OLD." + MediaColumn::MEDIA_ID + ";" +
    " END;";
    return TRIGGER_DELETE_ASSETS;
}

static const string &QueryAlbumJoinMap()
{
    static const string QUERY_ALBUM_JOIN_MAP = " SELECT " + PhotoAlbumColumns::ALBUM_ID +
        " FROM " + PhotoAlbumColumns::TABLE + " INNER JOIN " + PhotoMap::TABLE + " ON " +
            PhotoAlbumColumns::ALBUM_ID + " = " + PhotoMap::ALBUM_ID + " AND " +
            PhotoMap::ASSET_ID + " = " + "NEW." + MediaColumn::MEDIA_ID;
    return QUERY_ALBUM_JOIN_MAP;
}

static const string &SetHiddenUpdateCount()
{
    // Photos.hidden 1 -> 0
    static const string SET_HIDDEN_UPDATE_COUNT = " UPDATE " + PhotoAlbumColumns::TABLE +
        " SET " + PhotoAlbumColumns::ALBUM_COUNT + " = " + PhotoAlbumColumns::ALBUM_COUNT + " + 1" +
        " WHERE " + PhotoAlbumColumns::ALBUM_ID + " IN (" +
            QueryAlbumJoinMap() + " WHERE " +
                "NEW." + MediaColumn::MEDIA_HIDDEN + " = 0" + " AND " +
                "(OLD." + MediaColumn::MEDIA_HIDDEN + " - NEW." + MediaColumn::MEDIA_HIDDEN + " > 0)" +
        ");";
    return SET_HIDDEN_UPDATE_COUNT;
}

static const string &SetTrashUpdateCount()
{
    // Photos.date_trashed timestamp -> 0
    static const string SET_TRASH_UPDATE_COUNT = " UPDATE " + PhotoAlbumColumns::TABLE +
        " SET " + PhotoAlbumColumns::ALBUM_COUNT + " = " + PhotoAlbumColumns::ALBUM_COUNT + " + 1" +
        " WHERE " + PhotoAlbumColumns::ALBUM_ID + " IN (" +
            QueryAlbumJoinMap() + " WHERE " +
                "SIGN(NEW." + MediaColumn::MEDIA_DATE_TRASHED + ") = 0" + " AND " +
                "NEW." + MediaColumn::MEDIA_HIDDEN + " = 0" + " AND " +
                "(" +
                    "SIGN(OLD." + MediaColumn::MEDIA_DATE_TRASHED + ") - " +
                    "SIGN(NEW." + MediaColumn::MEDIA_DATE_TRASHED + ") > 0" +
                ")" +
        ");";
    return SET_TRASH_UPDATE_COUNT;
}

static const string &UnSetHiddenUpdateCount()
{
    // Photos.hidden 0 -> 1
    static const string UNSET_HIDDEN_UPDATE_COUNT = " UPDATE " + PhotoAlbumColumns::TABLE +
        " SET " + PhotoAlbumColumns::ALBUM_COUNT + " = " + PhotoAlbumColumns::ALBUM_COUNT + " - 1" +
        " WHERE " + PhotoAlbumColumns::ALBUM_ID + " IN (" +
            QueryAlbumJoinMap() + " WHERE " +
                "NEW." + MediaColumn::MEDIA_HIDDEN + " = 1" + " AND " +
                "(NEW." + MediaColumn::MEDIA_HIDDEN + " - OLD." + MediaColumn::MEDIA_HIDDEN + " > 0)" +
        ");";
    return UNSET_HIDDEN_UPDATE_COUNT;
}

static const string &UnSetTrashUpdateCount()
{
    // Photos.date_trashed 0 -> timestamp
    static const string UNSET_TRASH_UPDATE_COUNT = " UPDATE " + PhotoAlbumColumns::TABLE +
        " SET " + PhotoAlbumColumns::ALBUM_COUNT + " = " + PhotoAlbumColumns::ALBUM_COUNT + " - 1" +
        " WHERE " + PhotoAlbumColumns::ALBUM_ID + " IN (" +
            QueryAlbumJoinMap() + " WHERE " +
                "SIGN(NEW." + MediaColumn::MEDIA_DATE_TRASHED + ") = 1" + " AND " +
                "NEW." + MediaColumn::MEDIA_HIDDEN + " = 0" + " AND " +
                "(" +
                    "SIGN(NEW." + MediaColumn::MEDIA_DATE_TRASHED + ") - "
                    "SIGN(OLD." + MediaColumn::MEDIA_DATE_TRASHED + ") > 0" +
                ")" +
        ");";
    return UNSET_TRASH_UPDATE_COUNT;
}

static const string &TriggerUpdateUserAlbumCount()
{
    static const string TRIGGER_UPDATE_USER_ALBUM_COUNT = BaseColumn::CreateTrigger() + "update_user_album_count" +
        " AFTER UPDATE ON " + PhotoColumn::PHOTOS_TABLE +
        " BEGIN " +
            SetHiddenUpdateCount() +
            SetTrashUpdateCount() +
            UnSetHiddenUpdateCount() +
            UnSetTrashUpdateCount() +
        " END;";
    return TRIGGER_UPDATE_USER_ALBUM_COUNT;
}

static int32_t ExecuteSql(RdbStore &store)
{
    static const vector<string> executeSqlStrs = {
        CREATE_MEDIA_TABLE,
        PhotoColumn::CREATE_PHOTO_TABLE,
        PhotoColumn::INDEX_STHP_ADDTIME,
        PhotoColumn::INDEX_CAMERA_SHOT_KEY,
        PhotoColumn::CREATE_PHOTOS_DELETE_TRIGGER,
        PhotoColumn::CREATE_PHOTOS_FDIRTY_TRIGGER,
        PhotoColumn::CREATE_PHOTOS_MDIRTY_TRIGGER,
        PhotoColumn::CREATE_PHOTOS_INSERT_CLOUD_SYNC,
        AudioColumn::CREATE_AUDIO_TABLE,
        CREATE_SMARTALBUM_TABLE,
        CREATE_SMARTALBUMMAP_TABLE,
        CREATE_DEVICE_TABLE,
        CREATE_CATEGORY_SMARTALBUMMAP_TABLE,
        CREATE_ASSET_UNIQUE_NUMBER_TABLE,
        CREATE_IMAGE_VIEW,
        CREATE_VIDEO_VIEW,
        CREATE_AUDIO_VIEW,
        CREATE_ALBUM_VIEW,
        CREATE_SMARTALBUMASSETS_VIEW,
        CREATE_ASSETMAP_VIEW,
        CREATE_MEDIATYPE_DIRECTORY_TABLE,
        CREATE_BUNDLE_PREMISSION_TABLE,
        CREATE_MEDIALIBRARY_ERROR_TABLE,
        CREATE_REMOTE_THUMBNAIL_TABLE,
        CREATE_FILES_DELETE_TRIGGER,
        CREATE_FILES_MDIRTY_TRIGGER,
        CREATE_FILES_FDIRTY_TRIGGER,
        CREATE_INSERT_CLOUD_SYNC_TRIGGER,
        PhotoAlbumColumns::CREATE_TABLE,
        PhotoAlbumColumns::INDEX_ALBUM_TYPES,
        PhotoAlbumColumns::CREATE_ALBUM_INSERT_TRIGGER,
        PhotoAlbumColumns::CREATE_ALBUM_MDIRTY_TRIGGER,
        PhotoAlbumColumns::CREATE_ALBUM_DELETE_TRIGGER,
        PhotoMap::CREATE_TABLE,
        PhotoMap::CREATE_NEW_TRIGGER,
        PhotoMap::CREATE_DELETE_TRIGGER,
        TriggerDeleteAlbumClearMap(),
        TriggerDeletePhotoClearMap(),
    };

    for (const string& sqlStr : executeSqlStrs) {
        if (store.ExecuteSql(sqlStr) != NativeRdb::E_OK) {
            return NativeRdb::E_ERROR;
        }
    }
    return NativeRdb::E_OK;
}

int32_t MediaLibraryDataCallBack::OnCreate(RdbStore &store)
{
    if (ExecuteSql(store) != NativeRdb::E_OK) {
        return NativeRdb::E_ERROR;
    }

    if (PrepareSystemAlbums(store) != NativeRdb::E_OK) {
        return NativeRdb::E_ERROR;
    }

    if (PrepareDir(store) != NativeRdb::E_OK) {
        return NativeRdb::E_ERROR;
    }

    if (PrepareSmartAlbum(store) != NativeRdb::E_OK) {
        return NativeRdb::E_ERROR;
    }

    if (PrepareUniqueMemberTable(store) != NativeRdb::E_OK) {
        return NativeRdb::E_ERROR;
    }
    return NativeRdb::E_OK;
}

void VersionAddCloud(RdbStore &store)
{
    const std::string alterCloudId = "ALTER TABLE " + MEDIALIBRARY_TABLE +
        " ADD COLUMN " + MEDIA_DATA_DB_CLOUD_ID +" TEXT";
    int32_t result = store.ExecuteSql(alterCloudId);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb cloud_id error %{private}d", result);
    }
    const std::string alterDirty = "ALTER TABLE " + MEDIALIBRARY_TABLE +
        " ADD COLUMN " + MEDIA_DATA_DB_DIRTY +" INT DEFAULT 0";
    result = store.ExecuteSql(alterDirty);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb dirty error %{private}d", result);
    }
    const std::string alterSyncStatus = "ALTER TABLE " + MEDIALIBRARY_TABLE +
        " ADD COLUMN " + MEDIA_DATA_DB_SYNC_STATUS +" INT DEFAULT 0";
    result = store.ExecuteSql(alterSyncStatus);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb syncStatus error %{private}d", result);
    }
    const std::string alterPosition = "ALTER TABLE " + MEDIALIBRARY_TABLE +
        " ADD COLUMN " + MEDIA_DATA_DB_POSITION +" INT DEFAULT 1";
    result = store.ExecuteSql(alterPosition);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb position error %{private}d", result);
    }
}

void AddMetaModifiedColumn(RdbStore &store)
{
    const std::string alterMetaModified =
        "ALTER TABLE " + MEDIALIBRARY_TABLE + " ADD COLUMN " +
        MEDIA_DATA_DB_META_DATE_MODIFIED + " BIGINT DEFAULT 0";
    int32_t result = store.ExecuteSql(alterMetaModified);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb meta_date_modified error %{private}d", result);
    }
    const std::string alterSyncStatus = "ALTER TABLE " + MEDIALIBRARY_TABLE +
        " ADD COLUMN " + MEDIA_DATA_DB_SYNC_STATUS + " INT DEFAULT 0";
    result = store.ExecuteSql(alterSyncStatus);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb syncStatus error %{private}d", result);
    }
}

void AddTableType(RdbStore &store)
{
    const std::string alterTableName =
        "ALTER TABLE " + BUNDLE_PERMISSION_TABLE + " ADD COLUMN " +
        PERMISSION_TABLE_TYPE + " INT";
    int32_t result = store.ExecuteSql(alterTableName);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb table_name error %{private}d", result);
    }
}

void API10TableCreate(RdbStore &store)
{
    static const vector<string> executeSqlStrs = {
        PhotoColumn::CREATE_PHOTO_TABLE,
        PhotoColumn::INDEX_STHP_ADDTIME,
        PhotoColumn::INDEX_CAMERA_SHOT_KEY,
        PhotoColumn::CREATE_PHOTOS_DELETE_TRIGGER,
        PhotoColumn::CREATE_PHOTOS_FDIRTY_TRIGGER,
        PhotoColumn::CREATE_PHOTOS_MDIRTY_TRIGGER,
        PhotoColumn::CREATE_PHOTOS_INSERT_CLOUD_SYNC,
        AudioColumn::CREATE_AUDIO_TABLE,
        CREATE_ASSET_UNIQUE_NUMBER_TABLE,
        CREATE_FILES_DELETE_TRIGGER,
        CREATE_FILES_MDIRTY_TRIGGER,
        CREATE_FILES_FDIRTY_TRIGGER,
        CREATE_INSERT_CLOUD_SYNC_TRIGGER,
        PhotoAlbumColumns::CREATE_TABLE,
        PhotoAlbumColumns::INDEX_ALBUM_TYPES,
        PhotoMap::CREATE_TABLE,
        TriggerDeleteAlbumClearMap(),
        TriggerAddAssets(),
        TriggerRemoveAssets(),
        TriggerDeletePhotoClearMap(),
        TriggerUpdateUserAlbumCount(),
    };

    for (size_t i = 0; i < executeSqlStrs.size(); i++) {
        if (store.ExecuteSql(executeSqlStrs[i]) != NativeRdb::E_OK) {
            MEDIA_ERR_LOG("upgrade fail idx:%{public}zu", i);
        }
    }
}

void ModifySyncStatus(RdbStore &store)
{
    const std::string dropSyncStatus = "ALTER TABLE " + MEDIALIBRARY_TABLE + " DROP column syncing";
    auto result = store.ExecuteSql(dropSyncStatus);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb syncing error %{private}d", result);
    }

    const std::string addSyncStatus = "ALTER TABLE " + MEDIALIBRARY_TABLE + " ADD COLUMN " +
        MEDIA_DATA_DB_SYNC_STATUS +" INT DEFAULT 0";
    result = store.ExecuteSql(addSyncStatus);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb syncStatus error %{private}d", result);
    }
}

void ModifyDeleteTrigger(RdbStore &store)
{
    /* drop old delete trigger */
    const std::string dropDeleteTrigger = "DROP TRIGGER IF EXISTS photos_delete_trigger";
    if (store.ExecuteSql(dropDeleteTrigger) != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail: drop old delete trigger");
    }

    /* create new delete trigger */
    if (store.ExecuteSql(PhotoColumn::CREATE_PHOTOS_DELETE_TRIGGER) != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail: create new delete trigger");
    }
}

void AddCloudVersion(RdbStore &store)
{
    const std::string addSyncStatus = "ALTER TABLE " + PhotoColumn::PHOTOS_TABLE + " ADD COLUMN " +
        PhotoColumn::PHOTO_CLOUD_VERSION +" BIGINT DEFAULT 0";
    auto result = store.ExecuteSql(addSyncStatus);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Upgrade rdb cloudVersion error %{private}d", result);
    }
}

static string UpdateCloudPathSql(const string &table, const string &column)
{
    static const string LOCAL_PATH = "/storage/media/local/";
    static const string CLOUD_PATH = "/storage/cloud/";
    /*
     * replace only once:
     * UPDATE photos
     * SET data = ([replace](substring(data, 1, len(local_path)), local_path, cloud_path) ||
     * substring(data, len(local_path) + 1));
     */
    return "UPDATE " + table + " SET " + column + " = (REPLACE(SUBSTRING(" +
        column + ", 1, " + to_string(LOCAL_PATH.length()) + "), '" +
        LOCAL_PATH + "', '" + CLOUD_PATH + "') || SUBSTRING(" + column + ", " +
        to_string(LOCAL_PATH.length() + 1) + "))" +
        " WHERE " + column + " LIKE '" + LOCAL_PATH + "%';";
}

static int32_t UpdateCloudPath(RdbStore &store)
{
    const vector<string> updateCloudPath = {
        UpdateCloudPathSql(MEDIALIBRARY_TABLE, MEDIA_DATA_DB_FILE_PATH),
        UpdateCloudPathSql(MEDIALIBRARY_TABLE, MEDIA_DATA_DB_RECYCLE_PATH),
        UpdateCloudPathSql(MEDIALIBRARY_ERROR_TABLE, MEDIA_DATA_ERROR),
        UpdateCloudPathSql(PhotoColumn::PHOTOS_TABLE, MediaColumn::MEDIA_FILE_PATH),
    };
    return ExecSqls(updateCloudPath, store);
}

void UpdateAPI10Table(RdbStore &store)
{
    store.ExecuteSql("DROP INDEX IF EXISTS idx_sthp_dateadded");
    store.ExecuteSql("DROP INDEX IF EXISTS photo_album_types");

    store.ExecuteSql("DROP TRIGGER IF EXISTS photos_delete_trigger");
    store.ExecuteSql("DROP TRIGGER IF EXISTS photos_fdirty_trigger");
    store.ExecuteSql("DROP TRIGGER IF EXISTS photos_mdirty_trigger");
    store.ExecuteSql("DROP TRIGGER IF EXISTS photo_insert_cloud_sync_trigger");

    store.ExecuteSql("DROP TRIGGER IF EXISTS delete_trigger");
    store.ExecuteSql("DROP TRIGGER IF EXISTS mdirty_trigger");
    store.ExecuteSql("DROP TRIGGER IF EXISTS fdirty_trigger");
    store.ExecuteSql("DROP TRIGGER IF EXISTS insert_cloud_sync_trigger");

    store.ExecuteSql("DROP TRIGGER IF EXISTS photo_album_clear_map");
    store.ExecuteSql("DROP TRIGGER IF EXISTS photo_album_insert_asset");
    store.ExecuteSql("DROP TRIGGER IF EXISTS photo_album_delete_asset");
    store.ExecuteSql("DROP TRIGGER IF EXISTS delete_photo_clear_map");
    store.ExecuteSql("DROP TRIGGER IF EXISTS update_user_album_count");

    store.ExecuteSql("DROP TABLE IF EXISTS Photos");
    store.ExecuteSql("DROP TABLE IF EXISTS Audios");
    store.ExecuteSql("DROP TABLE IF EXISTS UniqueNumber");
    store.ExecuteSql("DROP TABLE IF EXISTS PhotoAlbum");
    store.ExecuteSql("DROP TABLE IF EXISTS PhotoMap");

    API10TableCreate(store);
    PrepareSystemAlbums(store);
    PrepareUniqueMemberTable(store);

    // set scan error
    MediaScannerManager::GetInstance()->ErrorRecord();
}

void MediaLibraryRdbStore::UpdateAPI10Tables()
{
    if (rdbStore_ == nullptr) {
        MEDIA_ERR_LOG("Pointer rdbStore_ is nullptr. Maybe it didn't init successfully.");
    }

    UpdateAPI10Table(*rdbStore_);
}

static void AddPackageNameColumnOnTables(RdbStore &store)
{
    static const string ADD_PACKAGE_NAME_ON_PHOTOS = "ALTER TABLE " + PhotoColumn::PHOTOS_TABLE +
        " ADD COLUMN " + PhotoColumn::MEDIA_PACKAGE_NAME + " TEXT";
    static const string ADD_PACKAGE_NAME_ON_AUDIOS = "ALTER TABLE " + AudioColumn::AUDIOS_TABLE +
        " ADD COLUMN " + AudioColumn::MEDIA_PACKAGE_NAME + " TEXT";
    static const string ADD_PACKAGE_NAME_ON_FILES = "ALTER TABLE " + MEDIALIBRARY_TABLE +
        " ADD COLUMN " + MEDIA_DATA_DB_PACKAGE_NAME + " TEXT";

    int32_t result = store.ExecuteSql(ADD_PACKAGE_NAME_ON_PHOTOS);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to update PHOTOS");
    }
    result = store.ExecuteSql(ADD_PACKAGE_NAME_ON_AUDIOS);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to update AUDIOS");
    }
    result = store.ExecuteSql(ADD_PACKAGE_NAME_ON_FILES);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to update FILES");
    }
}

void UpdateCloudAlbum(RdbStore &store)
{
    /* album - add columns */
    const std::string addAlbumDirty = "ALTER TABLE " + PhotoAlbumColumns::TABLE +
        " ADD COLUMN " + PhotoAlbumColumns::ALBUM_DIRTY + " INT DEFAULT " +
        to_string(static_cast<int32_t>(DirtyTypes::TYPE_NEW)) + ";";
    int32_t ret = store.ExecuteSql(addAlbumDirty);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: add ablum dirty", ret);
    }
    const std::string addAlbumCloudId = "ALTER TABLE " + PhotoAlbumColumns::TABLE +
        " ADD COLUMN " + PhotoAlbumColumns::ALBUM_CLOUD_ID + " TEXT;";
    ret = store.ExecuteSql(addAlbumCloudId);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: add ablum cloud id", ret);
    }
    /* album - add triggers */
    ret = store.ExecuteSql(PhotoAlbumColumns::CREATE_ALBUM_INSERT_TRIGGER);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: create album insert trigger", ret);
    }
    ret = store.ExecuteSql(PhotoAlbumColumns::CREATE_ALBUM_MDIRTY_TRIGGER);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: create album modify trigger", ret);
    }
    ret = store.ExecuteSql(PhotoAlbumColumns::CREATE_ALBUM_DELETE_TRIGGER);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: create album delete trigger", ret);
    }
    /* album map - add columns */
    const std::string addAlbumMapColumns = "ALTER TABLE " + PhotoMap::TABLE +
        " ADD COLUMN " + PhotoMap::DIRTY +" INT DEFAULT " +
        to_string(static_cast<int32_t>(DirtyTypes::TYPE_NEW)) + ";";
    ret = store.ExecuteSql(addAlbumMapColumns);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: add ablum columns", ret);
    }
    /* album map - add triggers */
    ret = store.ExecuteSql(PhotoMap::CREATE_NEW_TRIGGER);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: create album map insert trigger", ret);
    }
    ret = store.ExecuteSql(PhotoMap::CREATE_DELETE_TRIGGER);
    if (ret != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("upgrade fail %{public}d: create album map delete trigger", ret);
    }
}

static void AddCameraShotKey(RdbStore &store)
{
    static const string ADD_CAMERA_SHOT_KEY_ON_PHOTOS = "ALTER TABLE " + PhotoColumn::PHOTOS_TABLE +
        " ADD COLUMN " + PhotoColumn::CAMERA_SHOT_KEY + " TEXT";
    int32_t result = store.ExecuteSql(ADD_CAMERA_SHOT_KEY_ON_PHOTOS);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to update PHOTOS");
    }
    result = store.ExecuteSql(PhotoColumn::INDEX_CAMERA_SHOT_KEY);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Failed to create CAMERA_SHOT_KEY index");
    }
}

void RemoveAlbumCountTrigger(RdbStore &store)
{
    const vector<string> removeAlbumCountTriggers = {
        BaseColumn::DropTrigger() + "update_user_album_count",
        BaseColumn::DropTrigger() + "photo_album_insert_asset",
        BaseColumn::DropTrigger() + "photo_album_delete_asset",
    };
    ExecSqls(removeAlbumCountTriggers, store);
}

void AddExifAndUserComment(RdbStore &store)
{
    const string addUserCommentOnPhotos = "ALTER TABLE " + PhotoColumn::PHOTOS_TABLE +
        " ADD COLUMN " + PhotoColumn::PHOTO_USER_COMMENT + " TEXT";

    const string addAllExifOnPhotos = "ALTER TABLE " + PhotoColumn::PHOTOS_TABLE +
        " ADD COLUMN " + PhotoColumn::PHOTO_ALL_EXIF + " TEXT";

    const vector<string> addExifColumns = { addUserCommentOnPhotos, addAllExifOnPhotos };
    ExecSqls(addExifColumns, store);
}

int32_t MediaLibraryDataCallBack::OnUpgrade(RdbStore &store, int32_t oldVersion, int32_t newVersion)
{
    MEDIA_DEBUG_LOG("OnUpgrade old:%d, new:%d", oldVersion, newVersion);
    if (oldVersion < VERSION_ADD_CLOUD) {
        VersionAddCloud(store);
    }

    if (oldVersion < VERSION_ADD_META_MODIFED) {
        AddMetaModifiedColumn(store);
    }

    if (oldVersion < VERSION_MODIFY_SYNC_STATUS) {
        ModifySyncStatus(store);
    }

    if (oldVersion < VERSION_ADD_API10_TABLE) {
        API10TableCreate(store);
    }

    if (oldVersion < VERSION_MODIFY_DELETE_TRIGGER) {
        ModifyDeleteTrigger(store);
    }

    if (oldVersion < VERSION_ADD_CLOUD_VERSION) {
        AddCloudVersion(store);
    }

    if (oldVersion < VERSION_UPDATE_CLOUD_PATH) {
        UpdateCloudPath(store);
    }

    if (oldVersion < VERSION_UPDATE_API10_TABLE) {
        UpdateAPI10Table(store);
    }

    if (oldVersion < VERSION_ADD_TABLE_TYPE) {
        AddTableType(store);
    }

    if (oldVersion < VERSION_ADD_PACKAGE_NAME) {
        AddPackageNameColumnOnTables(store);
    }

    if (oldVersion < VERSION_ADD_CLOUD_ALBUM) {
        UpdateCloudAlbum(store);
    }

    if (oldVersion < VERSION_ADD_CAMERA_SHOT_KEY) {
        AddCameraShotKey(store);
    }
    if (oldVersion < VERSION_REMOVE_ALBUM_COUNT_TRIGGER) {
        RemoveAlbumCountTrigger(store);
    }

    if (oldVersion < VERSION_ADD_ALL_EXIF) {
        AddExifAndUserComment(store);
    }

    return NativeRdb::E_OK;
}

#ifdef DISTRIBUTED
MediaLibraryRdbStoreObserver::MediaLibraryRdbStoreObserver(const string &bundleName)
{
    bundleName_ = bundleName;
    isNotifyDeviceChange_ = false;

    if (timer_ == nullptr) {
        timer_ = make_unique<OHOS::Utils::Timer>(bundleName_);
        timerId_ = timer_->Register(bind(&MediaLibraryRdbStoreObserver::NotifyDeviceChange, this),
            NOTIFY_TIME_INTERVAL);
        timer_->Setup();
    }
}

MediaLibraryRdbStoreObserver::~MediaLibraryRdbStoreObserver()
{
    if (timer_ != nullptr) {
        timer_->Shutdown();
        timer_->Unregister(timerId_);
        timer_ = nullptr;
    }
}

void MediaLibraryRdbStoreObserver::OnChange(const vector<string> &devices)
{
    MEDIA_INFO_LOG("MediaLibraryRdbStoreObserver OnChange call");
    if (devices.empty() || bundleName_.empty()) {
        return;
    }
    MediaLibraryDevice::GetInstance()->NotifyRemoteFileChange();
}

void MediaLibraryRdbStoreObserver::NotifyDeviceChange()
{
    if (isNotifyDeviceChange_) {
        MediaLibraryDevice::GetInstance()->NotifyDeviceChange();
        isNotifyDeviceChange_ = false;
    }
}
#endif

TransactionOperations::TransactionOperations()
{
    rdbStore_ = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
}

TransactionOperations::~TransactionOperations()
{
    if (isStart && !isFinish) {
        TransactionRollback();
    }
}

int32_t TransactionOperations::Start()
{
    if (isStart || isFinish) {
        return E_OK;
    }
    int32_t errCode = BeginTransaction();
    if (errCode == NativeRdb::E_OK) {
        isStart = true;
    }
    return errCode;
}

void TransactionOperations::Finish()
{
    if (!isStart) {
        return;
    }
    if (!isFinish) {
        int32_t ret = TransactionCommit();
        if (ret == E_OK) {
            isFinish = true;
        } else {
            MEDIA_ERR_LOG("Failed to commit transaction, errCode=%{public}d", ret);
        }
    }
}

int32_t TransactionOperations::BeginTransaction()
{
    if (rdbStore_ == nullptr) {
        return E_HAS_DB_ERROR;
    }
    return rdbStore_->BeginTransaction();
}

int32_t TransactionOperations::TransactionCommit()
{
    if (rdbStore_ == nullptr) {
        return E_HAS_DB_ERROR;
    }
    return rdbStore_->Commit();
}

int32_t TransactionOperations::TransactionRollback()
{
    if (rdbStore_ == nullptr) {
        return E_HAS_DB_ERROR;
    }
    return rdbStore_->RollBack();
}
} // namespace OHOS::Media
