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
#define MLOG_TAG "UserFileClientEx"
#include "userfile_client_ex.h"

#include <map>
#include <string>
#include <unordered_map>
#include <unistd.h>

#include "datashare_abs_result_set.h"
#include "datashare_predicates.h"
#include "media_column.h"
#include "media_file_uri.h"
#include "media_file_utils.h"
#include "media_log.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "mimetype_utils.h"
#include "scanner_utils.h"
#include "string_ex.h"
#include "thumbnail_const.h"
#include "uri.h"
#include "userfile_client.h"
#include "userfile_manager_types.h"

using namespace OHOS::DataShare;
namespace OHOS {
namespace Media {
namespace MediaTool {
const std::string URI_DELIMITER = std::string(1, SLASH_CHAR);
const std::string URI_ARG_FIRST_DELIMITER = "?";
const std::string URI_API_VERSION_STR = std::to_string(static_cast<uint32_t>(MediaLibraryApi::API_10));
const std::string URI_API_VERSION = URI_PARAM_API_VERSION + "=" + URI_API_VERSION_STR;

enum MediaToolOperation {
    INSERT,
    QUERY,
    CLOSE,
    DELETE,
    UPDATE
};

const std::map<MediaToolOperation, std::string> PHOTOOPRN_URI_MAP = {
    { MediaToolOperation::INSERT, TOOL_CREATE_PHOTO },
    { MediaToolOperation::QUERY, TOOL_QUERY_PHOTO },
    { MediaToolOperation::CLOSE, TOOL_CLOSE_PHOTO },
    { MediaToolOperation::DELETE, TOOL_DELETE_PHOTO },
    { MediaToolOperation::UPDATE, TOOL_UPDATE_PHOTO }
};

const std::map<MediaToolOperation, std::string> AUDIOOPRN_URI_MAP = {
    { MediaToolOperation::INSERT, TOOL_CREATE_AUDIO },
    { MediaToolOperation::QUERY, TOOL_QUERY_AUDIO },
    { MediaToolOperation::CLOSE, TOOL_CLOSE_AUDIO },
    { MediaToolOperation::DELETE, TOOL_DELETE_AUDIO },
    { MediaToolOperation::UPDATE, TOOL_UPDATE_AUDIO }
};

static std::string GetOperation(const std::string &tableName, MediaToolOperation oprn)
{
    if (tableName == PhotoColumn::PHOTOS_TABLE) {
        auto item = PHOTOOPRN_URI_MAP.find(oprn);
        if (item != PHOTOOPRN_URI_MAP.end()) {
            return item->second;
        }
    } else if (tableName == AudioColumn::AUDIOS_TABLE) {
        auto item = AUDIOOPRN_URI_MAP.find(oprn);
        if (item != AUDIOOPRN_URI_MAP.end()) {
            return item->second;
        }
    }
    MEDIA_ERR_LOG("get operation failed. tableName:%{public}s", tableName.c_str());
    return "";
}

static bool CheckTableName(const std::string &tableName)
{
    static const std::set<std::string> VALID_TABLENAME_WHITELIST = {
        PhotoColumn::PHOTOS_TABLE,
        AudioColumn::AUDIOS_TABLE
    };
    if (tableName.empty()) {
        return false;
    }
    if (VALID_TABLENAME_WHITELIST.find(tableName) == VALID_TABLENAME_WHITELIST.end()) {
        return false;
    }
    return true;
}

static inline bool GetUriInfo(const std::string &uri, std::string &uriId)
{
    MediaFileUri fileUri(uri);
    if (!fileUri.IsValid()) {
        MEDIA_ERR_LOG("uri %{private}s is invalid", uri.c_str());
        return false;
    }
    uriId = fileUri.GetFileId();
    return true;
}

static inline std::string GetInsertUri(const std::string &tableName)
{
    std::string uri = GetOperation(tableName, MediaToolOperation::INSERT);
    if (uri.empty()) {
        MEDIA_ERR_LOG("get insert uri failed. tableName:%{public}s", tableName.c_str());
        return uri;
    }
    uri.append(URI_ARG_FIRST_DELIMITER + URI_API_VERSION);
    return uri;
}

static inline std::string GetQueryUri(const std::string &tableName)
{
    std::string uri = GetOperation(tableName, MediaToolOperation::QUERY);
    if (uri.empty()) {
        MEDIA_ERR_LOG("get query uri failed. tableName:%{public}s", tableName.c_str());
        return uri;
    }
    uri.append(URI_ARG_FIRST_DELIMITER + URI_API_VERSION);
    return uri;
}

static inline std::string GetCloseUri(const std::string &tableName)
{
    std::string uri = GetOperation(tableName, MediaToolOperation::CLOSE);
    if (uri.empty()) {
        MEDIA_ERR_LOG("get close uri failed. tableName:%{public}s", tableName.c_str());
        return uri;
    }
    uri.append(URI_ARG_FIRST_DELIMITER + URI_API_VERSION);
    return uri;
}

static inline std::string GetUpdateUri(const std::string &tableName)
{
    std::string uri = GetOperation(tableName, MediaToolOperation::UPDATE);
    if (uri.empty()) {
        MEDIA_ERR_LOG("get update uri failed. tableName:%{public}s", tableName.c_str());
        return uri;
    }
    uri.append(URI_ARG_FIRST_DELIMITER + URI_API_VERSION);
    return uri;
}

static inline std::string GetDeleteUri(const std::string &tableName)
{
    std::string uri = GetOperation(tableName, MediaToolOperation::DELETE);
    if (uri.empty()) {
        MEDIA_ERR_LOG("get delete uri failed. tableName:%{public}s", tableName.c_str());
        return uri;
    }
    uri.append(URI_ARG_FIRST_DELIMITER + URI_API_VERSION);
    return uri;
}

bool UserFileClientEx::Init(const sptr<IRemoteObject> &token)
{
    UserFileClient::Init(token);
    return UserFileClient::IsValid();
}

void UserFileClientEx::Clear()
{
    UserFileClient::Clear();
}

int32_t UserFileClientEx::InsertExt(const std::string &tableName, const std::string &name,
    std::string &outString)
{
    std::string insertUriStr = GetInsertUri(tableName);
    if (insertUriStr.empty()) {
        MEDIA_ERR_LOG("insert failed. tableName:%{public}s, name:%{private}s", tableName.c_str(),
            name.c_str());
        return Media::E_ERR;
    }
    Uri insertUri(insertUriStr);
    DataShare::DataShareValuesBucket values;
    values.Put(MediaColumn::MEDIA_NAME, name);
    string mimeType = MimeTypeUtils::GetMimeTypeFromExtension(ScannerUtils::GetFileExtension(name));
    values.Put(MediaColumn::MEDIA_TYPE, MimeTypeUtils::GetMediaTypeFromMimeType(mimeType));
    MEDIA_INFO_LOG("insertext. insertUri:%{private}s, name:%{private}s", insertUri.ToString().c_str(), name.c_str());
    auto ret = UserFileClient::InsertExt(insertUri, values, outString);
    if (ret <= 0) {
        MEDIA_ERR_LOG("insertext failed. ret:%{public}d", ret);
    }
    return ret;
}

int32_t UserFileClientEx::Query(const std::string &tableName, const std::string &uri,
    std::shared_ptr<DataShare::DataShareResultSet> &resultSet)
{
    if (!CheckTableName(tableName)) {
        MEDIA_ERR_LOG("tableName %{public}s is Invalid", tableName.c_str());
        return Media::E_ERR;
    }
    resultSet = nullptr;
    std::string id;
    if ((!uri.empty()) && (!GetUriInfo(uri, id))) {
        MEDIA_ERR_LOG("query failed, uri:%{private}s", uri.c_str());
        return Media::E_ERR;
    }
    std::string queryUriStr = GetQueryUri(tableName);
    if (queryUriStr.empty()) {
        MEDIA_ERR_LOG("query failed. queryUriStr:empty, tableName:%{public}s", tableName.c_str());
        return Media::E_ERR;
    }
    Uri queryUri(queryUriStr);
    DataShare::DataSharePredicates predicates;
    // Id is empty, meaning get all object from table
    if (!id.empty()) {
        predicates.And()->EqualTo(MediaColumn::MEDIA_ID, id);
    }
    std::vector<std::string> columns;
    int errCode = 0;
    MEDIA_INFO_LOG("query. queryUri:%{private}s, tableName:%{private}s, uri:%{private}s, "
        "id:%{private}s", queryUri.ToString().c_str(), tableName.c_str(), uri.c_str(), id.c_str());
    resultSet = UserFileClient::Query(queryUri, predicates, columns, errCode);
    if (resultSet == nullptr) {
        MEDIA_ERR_LOG("query failed. resultSet:null, errCode:%{public}d.", errCode);
        return ((errCode == Media::E_OK) ? Media::E_OK : Media::E_ERR);
    }
    if (errCode != Media::E_OK) {
        MEDIA_ERR_LOG("query failed. errCode:%{public}d.", errCode);
        resultSet->Close();
        return Media::E_ERR;
    }
    return Media::E_OK;
}

int UserFileClientEx::Open(const std::string &uri, const std::string &mode)
{
    if (uri.empty()) {
        return Media::E_FAIL;
    }
    Uri openUri(uri);
    MEDIA_INFO_LOG("open. uri:%{private}s, mode:%{private}s", uri.c_str(), mode.c_str());
    return UserFileClient::OpenFile(openUri, mode);
}

int UserFileClientEx::Close(const std::string &uri, const int fileFd, const std::string &mode,
    bool isCreateThumbSync)
{
    if (!UserFileClient::IsValid()) {
        MEDIA_ERR_LOG("close failed. helper:null. uri:%{private}s, fileFd:%{public}d, mode:%{private}s",
            uri.c_str(), fileFd, mode.c_str());
        return Media::E_FAIL;
    }
    if (mode == Media::MEDIA_FILEMODE_READONLY) {
        if (close(fileFd) != E_SUCCESS) {
            MEDIA_ERR_LOG("close failed. uri:%{private}s, fileFd:%{public}d, mode:%{private}s",
                uri.c_str(), fileFd, mode.c_str());
            return Media::E_FAIL;
        }
        return Media::E_OK;
    }
    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(MEDIA_DATA_DB_URI, uri);
    if (isCreateThumbSync) {
        valuesBucket.Put(CLOSE_CREATE_THUMB_STATUS, CREATE_THUMB_SYNC_STATUS);
    }
    std::string closeUriStr = GetCloseUri(GetTableNameByUri(uri));
    if (closeUriStr.empty()) {
        MEDIA_ERR_LOG("get close uri failed. uri:%{public}s", uri.c_str());
        return Media::E_FAIL;
    }
    Uri closeUri(closeUriStr);
    MEDIA_INFO_LOG("close. closeUri:%{private}s, uri:%{private}s", closeUri.ToString().c_str(), uri.c_str());
    auto ret = UserFileClient::Insert(closeUri, valuesBucket);
    if (ret != Media::E_OK) {
        MEDIA_ERR_LOG("close the file failed. ret:%{public}d, closeUri:%{private}s, uri:%{private}s",
            ret, closeUri.ToString().c_str(), uri.c_str());
    }
    if (close(fileFd) != E_SUCCESS) {
        MEDIA_ERR_LOG("close failed. uri:%{private}s, fileFd:%{public}d, mode:%{private}s",
            uri.c_str(), fileFd, mode.c_str());
        return Media::E_FAIL;
    }
    return ret;
}

int32_t UserFileClientEx::Trash(const std::string &uri)
{
    if (!UserFileClient::IsValid()) {
        MEDIA_ERR_LOG("close failed. helper:null.");
        return Media::E_FAIL;
    }
    MediaFileUri fileUri(uri);
    if (!fileUri.IsValid()) {
        MEDIA_ERR_LOG("FileUri %{public}s is not Valid", uri.c_str());
        return Media::E_FAIL;
    }
    string tableName = GetTableNameByUri(uri);
    std::string trashUriStr = GetUpdateUri(tableName);
    if (trashUriStr.empty()) {
        MEDIA_ERR_LOG("get trash uri failed. uri:%{public}s", uri.c_str());
        return Media::E_FAIL;
    }

    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(MediaColumn::MEDIA_DATE_TRASHED, MediaFileUtils::UTCTimeSeconds());
    DataShare::DataSharePredicates predicates;
    predicates.SetWhereClause(MediaColumn::MEDIA_ID + " = ? ");
    predicates.SetWhereArgs({ fileUri.GetFileId() });
    Uri trashUri(trashUriStr);
    MEDIA_INFO_LOG("trash. trashUri:%{public}s, uri:%{public}s", trashUri.ToString().c_str(), uri.c_str());
    auto ret = UserFileClient::Update(trashUri, predicates, valuesBucket);
    if (ret < 0) {
        MEDIA_ERR_LOG("trash the file failed. ret:%{public}d, trashUri:%{public}s, uri:%{public}s",
            ret, trashUri.ToString().c_str(), uri.c_str());
    }
    return ret;
}

int32_t UserFileClientEx::Delete(const std::string &uri)
{
    if (!UserFileClient::IsValid()) {
        MEDIA_ERR_LOG("close failed. helper:null.");
        return Media::E_FAIL;
    }
    MediaFileUri fileUri(uri);
    if (!fileUri.IsValid()) {
        MEDIA_ERR_LOG("FileUri %{public}s is not Valid", uri.c_str());
        return Media::E_FAIL;
    }
    string tableName = GetTableNameByUri(uri);
    std::string deleteUriStr = GetDeleteUri(tableName);
    if (deleteUriStr.empty()) {
        MEDIA_ERR_LOG("get delete uri failed. uri:%{public}s", uri.c_str());
        return Media::E_FAIL;
    }

    DataShare::DataSharePredicates predicates;
    predicates.EqualTo(MediaColumn::MEDIA_ID, fileUri.GetFileId());
    Uri deleteUri(deleteUriStr);
    MEDIA_INFO_LOG("delete. deleteUri:%{public}s, uri:%{public}s", deleteUri.ToString().c_str(), uri.c_str());
    auto ret = UserFileClient::Delete(deleteUri, predicates);
    if (ret < 0) {
        MEDIA_ERR_LOG("delete the file failed. ret:%{public}d, deleteUri:%{public}s, uri:%{public}s",
            ret, deleteUri.ToString().c_str(), uri.c_str());
    }
    return ret;
}

int32_t UserFileClientEx::Delete(bool isOnlyDeleteDb)
{
    if (!UserFileClient::IsValid()) {
        MEDIA_ERR_LOG("close failed. helper:null.");
        return Media::E_FAIL;
    }
    DataShare::DataShareValuesBucket valuesBucket;
    valuesBucket.Put(DELETE_TOOL_ONLY_DATABASE, isOnlyDeleteDb);
    std::string uri = URI_DELETE_TOOL;
    Uri deleteUri(uri);
    auto ret = UserFileClient::Insert(deleteUri, valuesBucket);
    if (ret != Media::E_OK) {
        MEDIA_ERR_LOG("Delete all Files in MediaLibrary failed, ret=%{public}d", ret);
    }
    return ret;
}

std::string UserFileClientEx::GetTableNameByMediaType(const MediaType mediaType)
{
    static const std::map<MediaType, std::string> TYPE_TABLE_MAP = {
        { MediaType::MEDIA_TYPE_AUDIO, AudioColumn::AUDIOS_TABLE },
        { MediaType::MEDIA_TYPE_IMAGE, PhotoColumn::PHOTOS_TABLE },
        { MediaType::MEDIA_TYPE_VIDEO, PhotoColumn::PHOTOS_TABLE },
        { MediaType::MEDIA_TYPE_PHOTO, PhotoColumn::PHOTOS_TABLE }
    };
    std::string tableName;
    auto item = TYPE_TABLE_MAP.find(mediaType);
    if (item != TYPE_TABLE_MAP.end()) {
        tableName = item->second;
    } else {
        MEDIA_ERR_LOG("get table name failed. mediaType:%{public}d", mediaType);
    }
    return tableName;
}

std::string UserFileClientEx::GetTableNameByUri(const std::string &uri)
{
    MediaFileUri fileUri(uri);
    if (!fileUri.IsValid()) {
        MEDIA_ERR_LOG("uri %{private}s is invalid", uri.c_str());
        return "";
    }
    return fileUri.GetTableName();
}

const std::vector<MediaType> &UserFileClientEx::GetSupportTypes()
{
    static const std::vector<MediaType> SUPPORT_TYPES = {
        MediaType::MEDIA_TYPE_AUDIO,
        MediaType::MEDIA_TYPE_IMAGE,
        MediaType::MEDIA_TYPE_VIDEO
    };
    return SUPPORT_TYPES;
}

const std::vector<std::string> &UserFileClientEx::GetSupportTables()
{
    static const std::vector<std::string> SUPPORT_TABLES = {
        PhotoColumn::PHOTOS_TABLE,
        AudioColumn::AUDIOS_TABLE
    };
    return SUPPORT_TABLES;
}
} // namespace MediaTool
} // namespace Media
} // namespace OHOS
