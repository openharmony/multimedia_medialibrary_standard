/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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
#define MLOG_TAG "AlbumOperation"

#include "medialibrary_album_operations.h"

#include "directory_ex.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "medialibrary_data_manager_utils.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "medialibrary_notify.h"
#include "medialibrary_object_utils.h"
#include "medialibrary_rdb_transaction.h"
#include "medialibrary_rdb_utils.h"
#include "medialibrary_rdbstore.h"
#include "medialibrary_tracer.h"
#include "medialibrary_unistore_manager.h"
#include "photo_album_column.h"
#include "photo_map_column.h"

#include "result_set_utils.h"
#include "values_bucket.h"

using namespace std;
using namespace OHOS::NativeRdb;
using namespace OHOS::DataShare;
using namespace OHOS::RdbDataShareAdapter;

namespace OHOS::Media {
using ChangeType = AAFwk::ChangeInfo::ChangeType;
constexpr int32_t AFTER_AGR_SIZE = 2;
constexpr int32_t THAN_AGR_SIZE = 1;

int32_t MediaLibraryAlbumOperations::CreateAlbumOperation(MediaLibraryCommand &cmd)
{
    int64_t outRow = -1;
    int32_t errCode = MediaLibraryObjectUtils::CreateDirObj(cmd, outRow);
    if (errCode == E_SUCCESS) {
        return outRow;
    }
    return errCode;
}

// only support modify in the same parent folder, like: a/b/c --> a/b/d
int32_t MediaLibraryAlbumOperations::ModifyAlbumOperation(MediaLibraryCommand &cmd)
{
    string strId = cmd.GetOprnFileId();
    string srcDirPath = MediaLibraryObjectUtils::GetPathByIdFromDb(strId);
    if (srcDirPath.empty()) {
        MEDIA_ERR_LOG("Get path of id %{private}s from database file!", strId.c_str());
        return E_INVALID_PATH;
    }

    auto values = cmd.GetValueBucket();
    string dstDirName;
    ValueObject valueObject;
    if (values.GetObject(MEDIA_DATA_DB_NAME, valueObject)) {
        valueObject.GetString(dstDirName);
    }
    int ret;
    if (dstDirName.empty() && !values.IsEmpty()) {
        ret = MediaLibraryObjectUtils::ModifyInfoByIdInDb(cmd);
    } else {
        string dstDirPath = MediaFileUtils::GetParentPath(srcDirPath) + "/" + dstDirName;
        ret = MediaLibraryObjectUtils::RenameDirObj(cmd, srcDirPath, dstDirPath);
    }
    return ret;
}

string MediaLibraryAlbumOperations::GetDistributedAlbumSql(const string &strQueryCondition, const string &tableName)
{
    string distributedAlbumSql = "SELECT * FROM ( " + DISTRIBUTED_ALBUM_COLUMNS + " FROM " + tableName + " " +
        FILE_TABLE + ", " + tableName + " " + ALBUM_TABLE +
        DISTRIBUTED_ALBUM_WHERE_AND_GROUPBY + " )";
    if (!strQueryCondition.empty()) {
        distributedAlbumSql += " WHERE " + strQueryCondition;
    }
    MEDIA_DEBUG_LOG("GetDistributedAlbumSql distributedAlbumSql = %{private}s", distributedAlbumSql.c_str());
    return distributedAlbumSql;
}

#ifdef MEDIALIBRARY_COMPATIBILITY
static void ReplaceRelativePath(string &selection, vector<string> &selectionArgs)
{
    for (size_t pos = 0; pos != string::npos;) {
        pos = selection.find(MEDIA_DATA_DB_RELATIVE_PATH, pos);
        if (pos == string::npos) {
            break;
        }
        size_t argPos = selection.find('?', pos);
        if (argPos == string::npos) {
            break;
        }
        size_t argIndex = 0;
        for (size_t i = 0; i < argPos; i++) {
            if (selection[i] == '?') {
                argIndex++;
            }
        }
        if (argIndex > selectionArgs.size() - 1) {
            MEDIA_WARN_LOG("SelectionArgs size is not valid, selection format maybe incorrect: %{private}s",
                selection.c_str());
            break;
        }
        const string &arg = selectionArgs[argIndex];
        if (!arg.empty()) {
            MEDIA_WARN_LOG("No empty args in ReplaceRelativePath");
            return;
        }
        selection.replace(argPos, 1, "? OR 1=1)");
        selection.replace(pos, MEDIA_DATA_DB_RELATIVE_PATH.length(), "(" + PhotoAlbumColumns::ALBUM_ID);

        selectionArgs[argIndex] = "1";
        pos = argPos + 1;
    }
}

static void ReplaceMediaType(string &selection, vector<string> &selectionArgs)
{
    for (size_t pos = 0; pos != string::npos;) {
        pos = selection.find(MEDIA_DATA_DB_MEDIA_TYPE, pos);
        if (pos == string::npos) {
            break;
        }
        size_t argPos = selection.find('?', pos);
        if (argPos == string::npos) {
            break;
        }
        size_t argIndex = 0;
        for (size_t i = 0; i < argPos; i++) {
            if (selection[i] == '?') {
                argIndex++;
            }
        }
        if (argIndex > selectionArgs.size() - 1) {
            MEDIA_WARN_LOG("SelectionArgs size is not valid, selection format maybe incorrect: %{private}s",
                selection.c_str());
            break;
        }
        selection.replace(argPos, 1, "? OR 1=1)");
        selection.replace(pos, MEDIA_DATA_DB_MEDIA_TYPE.length(), "(" + PhotoAlbumColumns::ALBUM_ID);

        selectionArgs[argIndex] = "1";
        pos = argPos + 1;
    }
}

static void GetSqlArgs(MediaLibraryCommand &cmd, string &sql, vector<string> &selectionArgs,
    const vector<string> &columns)
{
    string clause = cmd.GetAbsRdbPredicates()->GetWhereClause();
    selectionArgs = cmd.GetAbsRdbPredicates()->GetWhereArgs();
    sql = "SELECT ";
    for (size_t i = 0; i < columns.size(); i++) {
        if (i != columns.size() - 1) {
            sql += columns[i] + ",";
        } else {
            sql += columns[i];
        }
    }
    sql += " FROM " + cmd.GetAbsRdbPredicates()->GetTableName();
    sql += " WHERE ";
    ReplaceRelativePath(clause, selectionArgs);
    ReplaceMediaType(clause, selectionArgs);
    sql += clause;
}

static void QueryAlbumDebug(MediaLibraryCommand &cmd, const vector<string> &columns,
    const shared_ptr<MediaLibraryUnistore> &store)
{
    MEDIA_DEBUG_LOG("Querying album, table: %{private}s selections: %{private}s",
        cmd.GetAbsRdbPredicates()->GetTableName().c_str(), cmd.GetAbsRdbPredicates()->GetWhereClause().c_str());
    for (const auto &arg : cmd.GetAbsRdbPredicates()->GetWhereArgs()) {
        MEDIA_DEBUG_LOG("Querying album, arg: %{private}s", arg.c_str());
    }
    for (const auto &col : columns) {
        MEDIA_DEBUG_LOG("Querying album, col: %{private}s", col.c_str());
    }

    auto resultSet = store->Query(cmd, columns);
    if (resultSet == nullptr) {
        MEDIA_ERR_LOG("Failed to query file!");
        return;
    }
    int32_t count = -1;
    int32_t err = resultSet->GetRowCount(count);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to get count, err: %{public}d", err);
        return;
    }
    MEDIA_DEBUG_LOG("Querying album, count: %{public}d", count);
}

static void QuerySqlDebug(const string &sql, const vector<string> &selectionArgs, const vector<string> &columns,
    const shared_ptr<MediaLibraryUnistore> &store)
{
    constexpr int32_t printMax = 512;
    for (size_t pos = 0; pos < sql.size(); pos += printMax) {
        MEDIA_DEBUG_LOG("Quering album sql: %{private}s", sql.substr(pos, printMax).c_str());
    }
    for (const auto &arg : selectionArgs) {
        MEDIA_DEBUG_LOG("Quering album, arg: %{private}s", arg.c_str());
    }
    for (const auto &col : columns) {
        MEDIA_DEBUG_LOG("Quering album, col: %{private}s", col.c_str());
    }
    auto resultSet = store->QuerySql(sql, selectionArgs);
    if (resultSet == nullptr) {
        MEDIA_ERR_LOG("Failed to query album!");
        return;
    }
    int32_t count = -1;
    int32_t err = resultSet->GetRowCount(count);
    if (err != E_OK) {
        MEDIA_ERR_LOG("Failed to get count, err: %{public}d", err);
        return;
    }
    MEDIA_DEBUG_LOG("Quering album, count: %{public}d", count);
}
#endif

shared_ptr<ResultSet> MediaLibraryAlbumOperations::QueryAlbumOperation(
    MediaLibraryCommand &cmd, const vector<string> &columns)
{
    auto uniStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStore();
    if (uniStore == nullptr) {
        MEDIA_ERR_LOG("uniStore is nullptr!");
        return nullptr;
    }

    if (cmd.GetOprnObject() == OperationObject::MEDIA_VOLUME) {
        MEDIA_DEBUG_LOG("QUERY_MEDIA_VOLUME = %{private}s", QUERY_MEDIA_VOLUME.c_str());
        return uniStore->QuerySql(QUERY_MEDIA_VOLUME + " UNION " + PhotoColumn::QUERY_MEDIA_VOLUME + " UNION " +
            AudioColumn::QUERY_MEDIA_VOLUME);
    }

#ifdef MEDIALIBRARY_COMPATIBILITY
    string whereClause = cmd.GetAbsRdbPredicates()->GetWhereClause();
    if (whereClause.find(MEDIA_DATA_DB_RELATIVE_PATH) != string::npos ||
        whereClause.find(MEDIA_DATA_DB_MEDIA_TYPE) != string::npos) {
        string sql;
        vector<string> selectionArgs;
        GetSqlArgs(cmd, sql, selectionArgs, columns);
        QuerySqlDebug(sql, selectionArgs, columns, uniStore);
        return uniStore->QuerySql(sql, selectionArgs);
    }

    QueryAlbumDebug(cmd, columns, uniStore);
    return uniStore->Query(cmd, columns);
#else
    string strQueryCondition = cmd.GetAbsRdbPredicates()->GetWhereClause();
    strQueryCondition += " GROUP BY " + MEDIA_DATA_DB_BUCKET_ID;
    cmd.GetAbsRdbPredicates()->SetWhereClause(strQueryCondition);
    string networkId = cmd.GetOprnDevice();
    if (!networkId.empty()) {
        string tableName = cmd.GetTableName();
        MEDIA_INFO_LOG("tableName is %{private}s", tableName.c_str());
        if (!strQueryCondition.empty()) {
            strQueryCondition = MediaLibraryDataManagerUtils::ObtionCondition(strQueryCondition,
                cmd.GetAbsRdbPredicates()->GetWhereArgs());
        }
        string distributedAlbumSql = GetDistributedAlbumSql(strQueryCondition, tableName);
        return uniStore->QuerySql(distributedAlbumSql);
    }

    if (!strQueryCondition.empty()) {
        return uniStore->Query(cmd, columns);
    }
    string querySql = "SELECT * FROM " + cmd.GetTableName();
    return uniStore->QuerySql(querySql);
#endif
}

inline int32_t GetStringObject(const ValuesBucket &values, const string &key, string &value)
{
    value = "";
    ValueObject valueObject;
    if (values.GetObject(key, valueObject)) {
        valueObject.GetString(value);
    } else {
        return -EINVAL;
    }
    return E_OK;
}

inline void PrepareUserAlbum(const string &albumName, const string &relativePath, ValuesBucket &values)
{
    values.PutString(PhotoAlbumColumns::ALBUM_NAME, albumName);
    values.PutInt(PhotoAlbumColumns::ALBUM_TYPE, PhotoAlbumType::USER);
    values.PutInt(PhotoAlbumColumns::ALBUM_SUBTYPE, PhotoAlbumSubType::USER_GENERIC);
    values.PutLong(PhotoAlbumColumns::ALBUM_DATE_MODIFIED, MediaFileUtils::UTCTimeSeconds());

    if (!relativePath.empty()) {
        values.PutString(PhotoAlbumColumns::ALBUM_RELATIVE_PATH, relativePath);
    }
}

inline void PrepareWhere(const string &albumName, const string &relativePath, RdbPredicates &predicates)
{
    predicates.EqualTo(PhotoAlbumColumns::ALBUM_NAME, albumName);
    predicates.EqualTo(PhotoAlbumColumns::ALBUM_TYPE, to_string(PhotoAlbumType::USER));
    predicates.EqualTo(PhotoAlbumColumns::ALBUM_SUBTYPE, to_string(PhotoAlbumSubType::USER_GENERIC));
    if (relativePath.empty()) {
        predicates.IsNull(PhotoAlbumColumns::ALBUM_RELATIVE_PATH);
    } else {
        predicates.EqualTo(PhotoAlbumColumns::ALBUM_RELATIVE_PATH, relativePath);
    }
}

// Caller is responsible for checking @albumName AND @relativePath
int DoCreatePhotoAlbum(const string &albumName, const string &relativePath)
{
    // Build insert sql
    string sql;
    vector<ValueObject> bindArgs;
    sql.append("INSERT").append(" OR ROLLBACK").append(" INTO ").append(PhotoAlbumColumns::TABLE).append(" ");

    ValuesBucket albumValues;
    PrepareUserAlbum(albumName, relativePath, albumValues);
    MediaLibraryRdbStore::BuildValuesSql(albumValues, bindArgs, sql);

    RdbPredicates wherePredicates(PhotoAlbumColumns::TABLE);
    PrepareWhere(albumName, relativePath, wherePredicates);
    sql.append(" WHERE NOT EXISTS (");
    MediaLibraryRdbStore::BuildQuerySql(wherePredicates, { PhotoAlbumColumns::ALBUM_ID }, bindArgs, sql);
    sql.append(");");
    MEDIA_DEBUG_LOG("DoCreatePhotoAlbum InsertSql: %{private}s", sql.c_str());

    return MediaLibraryRdbStore::ExecuteForLastInsertedRowId(sql, bindArgs);
}

inline int CreatePhotoAlbum(const string &albumName)
{
    int32_t err = MediaFileUtils::CheckAlbumName(albumName);
    if (err < 0) {
        return err;
    }

    return DoCreatePhotoAlbum(albumName, "");
}

int CreatePhotoAlbum(MediaLibraryCommand &cmd)
{
    string albumName;
    int err = GetStringObject(cmd.GetValueBucket(), PhotoAlbumColumns::ALBUM_NAME, albumName);
    if (err < 0) {
        return err;
    }
    int rowId = CreatePhotoAlbum(albumName);
    auto watch = MediaLibraryNotify::GetInstance();
    if (rowId > 0) {
        watch->Notify(MediaFileUtils::GetUriByExtrConditions(PhotoAlbumColumns::ALBUM_URI_PREFIX, to_string(rowId)),
            NotifyType::NOTIFY_ADD);
    }
    return rowId;
}

int32_t MediaLibraryAlbumOperations::DeletePhotoAlbum(RdbPredicates &predicates)
{
    // Only user generic albums can be deleted
    predicates.And()->BeginWrap()->EqualTo(PhotoAlbumColumns::ALBUM_TYPE, to_string(PhotoAlbumType::USER));
    predicates.EqualTo(PhotoAlbumColumns::ALBUM_SUBTYPE, to_string(PhotoAlbumSubType::USER_GENERIC));
    predicates.EndWrap();

    int deleteRow = MediaLibraryRdbStore::Delete(predicates);
    auto watch = MediaLibraryNotify::GetInstance();
    for (size_t i = 0; i < predicates.GetWhereArgs().size() - AFTER_AGR_SIZE; i++) {
        if (deleteRow > 0) {
            watch->Notify(MediaFileUtils::GetUriByExtrConditions(PhotoAlbumColumns::ALBUM_URI_PREFIX,
                predicates.GetWhereArgs()[i]), NotifyType::NOTIFY_REMOVE);
        }
    }
    return deleteRow;
}

shared_ptr<ResultSet> MediaLibraryAlbumOperations::QueryPhotoAlbum(MediaLibraryCommand &cmd,
    const vector<string> &columns)
{
    return MediaLibraryRdbStore::Query(*(cmd.GetAbsRdbPredicates()), columns);
}

int32_t PrepareUpdateValues(const ValuesBucket &values, ValuesBucket &updateValues)
{
    // Collect albumName if exists and check
    string albumName;
    if (GetStringObject(values, PhotoAlbumColumns::ALBUM_NAME, albumName) == E_OK) {
        int32_t err = MediaFileUtils::CheckAlbumName(albumName);
        if (err < 0) {
            return err;
        }
        updateValues.PutString(PhotoAlbumColumns::ALBUM_NAME, albumName);
    }

    // Collect coverUri if exists
    string coverUri;
    if (GetStringObject(values, PhotoAlbumColumns::ALBUM_COVER_URI, coverUri) == E_OK) {
        updateValues.PutString(PhotoAlbumColumns::ALBUM_COVER_URI, coverUri);
    }

    if (updateValues.IsEmpty()) {
        return -EINVAL;
    }
    updateValues.PutLong(PhotoAlbumColumns::ALBUM_DATE_MODIFIED, MediaFileUtils::UTCTimeSeconds());
    return E_OK;
}

int32_t UpdatePhotoAlbum(const ValuesBucket &values, const DataSharePredicates &predicates)
{
    ValuesBucket rdbValues;
    int32_t err = PrepareUpdateValues(values, rdbValues);
    if (err < 0) {
        return err;
    }

    RdbPredicates rdbPredicates = RdbUtils::ToPredicates(predicates, PhotoAlbumColumns::TABLE);
    // Only user generic albums can be updated
    rdbPredicates.And()->BeginWrap()->EqualTo(PhotoAlbumColumns::ALBUM_TYPE, to_string(PhotoAlbumType::USER));
    rdbPredicates.EqualTo(PhotoAlbumColumns::ALBUM_SUBTYPE, to_string(PhotoAlbumSubType::USER_GENERIC));
    rdbPredicates.EndWrap();

    int32_t changedRows = MediaLibraryRdbStore::Update(rdbValues, rdbPredicates);
    auto watch = MediaLibraryNotify::GetInstance();
    if (changedRows > 0) {
        for (size_t i = 0; i < rdbPredicates.GetWhereArgs().size() - AFTER_AGR_SIZE; i++) {
            watch->Notify(MediaFileUtils::GetUriByExtrConditions(PhotoAlbumColumns::ALBUM_URI_PREFIX,
                rdbPredicates.GetWhereArgs()[i]), NotifyType::NOTIFY_UPDATE);
        }
    }
    return changedRows;
}

int32_t RecoverPhotoAssets(const DataSharePredicates &predicates)
{
    RdbPredicates rdbPredicates = RdbUtils::ToPredicates(predicates, PhotoColumn::PHOTOS_TABLE);
    rdbPredicates.GreaterThan(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    vector<string> whereArgs = rdbPredicates.GetWhereArgs();
    MediaLibraryRdbStore::ReplacePredicatesUriToId(rdbPredicates);

    ValuesBucket rdbValues;
    rdbValues.PutInt(MediaColumn::MEDIA_DATE_TRASHED, 0);

    int32_t changedRows = MediaLibraryRdbStore::Update(rdbValues, rdbPredicates);
    if (changedRows < 0) {
        return changedRows;
    }
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw();
    MediaLibraryRdbUtils::UpdateUserAlbumInternal(rdbStore);
    MediaLibraryRdbUtils::UpdateSystemAlbumInternal(rdbStore);

    auto watch = MediaLibraryNotify::GetInstance();
    size_t count = whereArgs.size() - THAN_AGR_SIZE;
    for (size_t i = 0; i < count; i++) {
        string notifyUri = MediaFileUtils::Encode(whereArgs[i]);
        watch->Notify(notifyUri, NotifyType::NOTIFY_ADD);
        watch->Notify(notifyUri, NotifyType::NOTIFY_ALBUM_ADD_ASSERT);
    }
    int trashAlbumId = watch->GetAlbumIdBySubType(PhotoAlbumSubType::TRASH);
    if (trashAlbumId > 0) {
        for (size_t i = 0; i < count; i++) {
            watch->Notify(MediaFileUtils::Encode(whereArgs[i]), NotifyType::NOTIFY_ALBUM_REMOVE_ASSET, trashAlbumId);
        }
    }
    return changedRows;
}

int32_t DoDeletePhotoAssets(RdbPredicates &rdbPredicates, bool isAging, const bool compatible)
{
    vector<string> whereArgs = rdbPredicates.GetWhereArgs();
    MediaLibraryRdbStore::ReplacePredicatesUriToId(rdbPredicates);
    vector<string> agingNotifyUris;

    // Query asset uris for notify before delete.
    if (isAging) {
        MediaLibraryNotify::GetNotifyUris(rdbPredicates, agingNotifyUris);
    }
    int32_t deletedRows = MediaLibraryRdbStore::DeleteFromDisk(rdbPredicates, compatible);
    MediaLibraryRdbUtils::UpdateSystemAlbumInternal(
        MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw(),
        { to_string(PhotoAlbumSubType::TRASH) });

    auto watch = MediaLibraryNotify::GetInstance();
    int trashAlbumId = watch->GetAlbumIdBySubType(PhotoAlbumSubType::TRASH);
    if (trashAlbumId <= 0) {
        return deletedRows;
    }

    // Send notify of trash album in aging case.
    if (isAging) {
        for (const auto &notifyUri : agingNotifyUris) {
            watch->Notify(MediaFileUtils::Encode(notifyUri), NotifyType::NOTIFY_ALBUM_REMOVE_ASSET, trashAlbumId);
        }
        return deletedRows;
    }

    size_t count = whereArgs.size() - THAN_AGR_SIZE;
    for (size_t i = 0; i < count; i++) {
        watch->Notify(MediaFileUtils::Encode(whereArgs[i]), NotifyType::NOTIFY_ALBUM_REMOVE_ASSET, trashAlbumId);
    }
    return deletedRows;
}

static inline int32_t CompatDeletePhotoAssets(const DataSharePredicates &predicates, bool isAging)
{
    RdbPredicates rdbPredicates = RdbUtils::ToPredicates(predicates, PhotoColumn::PHOTOS_TABLE);
    return DoDeletePhotoAssets(rdbPredicates, isAging, true);
}

static inline int32_t DeletePhotoAssets(const DataSharePredicates &predicates, bool isAging)
{
    RdbPredicates rdbPredicates = RdbUtils::ToPredicates(predicates, PhotoColumn::PHOTOS_TABLE);
    rdbPredicates.GreaterThan(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    return DoDeletePhotoAssets(rdbPredicates, isAging, false);
}

int32_t AgingPhotoAssets(shared_ptr<int> countPtr)
{
    auto time = MediaFileUtils::UTCTimeSeconds();
    DataSharePredicates predicates;
    predicates.GreaterThan(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.And()->LessThanOrEqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(time - AGING_TIME));
    int32_t ret = DeletePhotoAssets(predicates, true);
    if (ret < 0) {
        return ret;
    }
    if (countPtr != nullptr) {
        *countPtr = ret;
    }
    return E_OK;
}

int32_t MediaLibraryAlbumOperations::HandlePhotoAlbum(const OperationType &opType, const ValuesBucket &values,
    const DataSharePredicates &predicates, shared_ptr<int> countPtr)
{
    switch (opType) {
        case OperationType::UPDATE:
            return UpdatePhotoAlbum(values, predicates);
        case OperationType::ALBUM_RECOVER_ASSETS:
            return RecoverPhotoAssets(predicates);
        case OperationType::ALBUM_DELETE_ASSETS:
            return DeletePhotoAssets(predicates, false);
        case OperationType::COMPAT_ALBUM_DELETE_ASSETS:
            return CompatDeletePhotoAssets(predicates, false);
        case OperationType::AGING:
            return AgingPhotoAssets(countPtr);
        default:
            MEDIA_ERR_LOG("Unknown operation type: %{public}d", opType);
            return E_ERR;
    }
}

int MediaLibraryAlbumOperations::HandlePhotoAlbumOperations(MediaLibraryCommand &cmd)
{
    switch (cmd.GetOprnType()) {
        case OperationType::CREATE:
            return CreatePhotoAlbum(cmd);
        default:
            MEDIA_ERR_LOG("Unknown operation type: %{public}d", cmd.GetOprnType());
            return E_ERR;
    }
}
} // namespace OHOS::Media
