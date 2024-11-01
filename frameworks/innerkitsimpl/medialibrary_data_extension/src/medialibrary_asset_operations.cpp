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

#include "medialibrary_asset_operations.h"

#include <algorithm>
#include <dirent.h>
#include <memory>
#include <mutex>

#include "directory_ex.h"
#include "file_asset.h"
#include "media_column.h"
#include "media_exif.h"
#include "media_file_utils.h"
#include "media_file_uri.h"
#include "media_log.h"
#include "media_scanner_manager.h"
#include "medialibrary_album_operations.h"
#include "medialibrary_audio_operations.h"
#include "medialibrary_command.h"
#include "medialibrary_common_utils.h"
#include "medialibrary_data_manager.h"
#include "medialibrary_data_manager_utils.h"
#include "medialibrary_db_const.h"
#include "medialibrary_errno.h"
#include "medialibrary_inotify.h"
#include "medialibrary_notify.h"
#include "medialibrary_photo_operations.h"
#include "medialibrary_rdb_transaction.h"
#include "medialibrary_rdb_utils.h"
#include "medialibrary_rdbstore.h"
#include "medialibrary_tracer.h"
#include "medialibrary_type_const.h"
#include "medialibrary_unistore_manager.h"
#include "media_privacy_manager.h"
#include "mimetype_utils.h"
#include "permission_utils.h"
#include "rdb_errno.h"
#include "rdb_utils.h"
#include "result_set_utils.h"
#include "thumbnail_service.h"
#include "uri_permission_manager_client.h"
#include "userfile_manager_types.h"
#include "value_object.h"
#include "values_bucket.h"

using namespace std;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {

mutex g_uniqueNumberLock;

const string DEFAULT_IMAGE_NAME = "IMG_";
const string DEFAULT_VIDEO_NAME = "VID_";
const string DEFAULT_AUDIO_NAME = "AUD_";

int32_t MediaLibraryAssetOperations::HandleInsertOperation(MediaLibraryCommand &cmd)
{
    int errCode = E_ERR;
    switch (cmd.GetOprnType()) {
        case OperationType::CREATE:
            errCode = CreateOperation(cmd);
            break;
        case OperationType::CLOSE:
            errCode = CloseOperation(cmd);
            break;
        default:
            MEDIA_ERR_LOG("unknown operation type %{public}d", cmd.GetOprnType());
            break;
    }
    return errCode;
}

int32_t MediaLibraryAssetOperations::CreateOperation(MediaLibraryCommand &cmd)
{
    // CreateAsset specify type
    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_PHOTO:
            return MediaLibraryPhotoOperations::Create(cmd);
        case OperationObject::FILESYSTEM_AUDIO:
            return MediaLibraryAudioOperations::Create(cmd);
        case OperationObject::FILESYSTEM_ASSET:
            MEDIA_ERR_LOG("create asset by FileSysetm_Asset is deperated");
            return E_INVALID_VALUES;
        default:
            MEDIA_ERR_LOG("error operation object: %{public}d", cmd.GetOprnObject());
            return E_INVALID_VALUES;
    }
}

int32_t MediaLibraryAssetOperations::DeleteOperation(MediaLibraryCommand &cmd)
{
    // delete Asset specify type
    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_PHOTO:
            return MediaLibraryPhotoOperations::Delete(cmd);
        case OperationObject::FILESYSTEM_AUDIO:
            return MediaLibraryAudioOperations::Delete(cmd);
        case OperationObject::FILESYSTEM_ASSET:
            MEDIA_ERR_LOG("delete asset by FILESYSTEM_ASSET is deperated");
            return E_INVALID_VALUES;
        default:
            MEDIA_ERR_LOG("error operation object: %{public}d", cmd.GetOprnObject());
            return E_INVALID_VALUES;
    }
}

shared_ptr<NativeRdb::ResultSet> MediaLibraryAssetOperations::QueryOperation(
    MediaLibraryCommand &cmd, const vector<string> &columns)
{
    // query asset specify type
    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_PHOTO:
            return MediaLibraryPhotoOperations::Query(cmd, columns);
        case OperationObject::FILESYSTEM_AUDIO:
            return MediaLibraryAudioOperations::Query(cmd, columns);
        case OperationObject::FILESYSTEM_ASSET:
            MEDIA_ERR_LOG("api9 operation is not finished");
            return nullptr;
        default:
            MEDIA_ERR_LOG("error operation objec: %{public}d", cmd.GetOprnObject());
            return nullptr;
    }
}

int32_t MediaLibraryAssetOperations::UpdateOperation(MediaLibraryCommand &cmd)
{
    if (!AssetInputParamVerification::CheckParamForUpdate(cmd)) {
        return E_INVALID_VALUES;
    }

    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_PHOTO:
            return MediaLibraryPhotoOperations::Update(cmd);
        case OperationObject::FILESYSTEM_AUDIO:
            return MediaLibraryAudioOperations::Update(cmd);
        case OperationObject::FILESYSTEM_ASSET:
            MEDIA_ERR_LOG("create asset by FILESYSTEM_ASSET is deperated");
            return E_INVALID_VALUES;
        default:
            MEDIA_ERR_LOG("error operation object: %{public}d", cmd.GetOprnObject());
            return E_INVALID_VALUES;
    }
}

int32_t MediaLibraryAssetOperations::OpenOperation(MediaLibraryCommand &cmd, const string &mode)
{
    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryAssetOperations::OpenOperation");

    // Open specify type
    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_PHOTO:
            return MediaLibraryPhotoOperations::Open(cmd, mode);
        case OperationObject::FILESYSTEM_AUDIO:
            return MediaLibraryAudioOperations::Open(cmd, mode);
        case OperationObject::FILESYSTEM_ASSET:
            MEDIA_ERR_LOG("open by FILESYSTEM_ASSET is deperated");
            return E_INVALID_VALUES;
        default:
            MEDIA_ERR_LOG("error operation object: %{public}d", cmd.GetOprnObject());
            return E_INVALID_VALUES;
    }
}

int32_t MediaLibraryAssetOperations::CloseOperation(MediaLibraryCommand &cmd)
{
    // Close specify type
    switch (cmd.GetOprnObject()) {
        case OperationObject::FILESYSTEM_PHOTO:
            return MediaLibraryPhotoOperations::Close(cmd);
        case OperationObject::FILESYSTEM_AUDIO:
            return MediaLibraryAudioOperations::Close(cmd);
        case OperationObject::FILESYSTEM_ASSET:
            MEDIA_ERR_LOG("close by FILESYSTEM_ASSET is deperated");
            return E_INVALID_VALUES;
        default:
            MEDIA_ERR_LOG("error operation object: %{public}d", cmd.GetOprnObject());
            return E_INVALID_VALUES;
    }
}

int32_t MediaLibraryAssetOperations::DeleteToolOperation(MediaLibraryCommand &cmd)
{
    auto valuesBucket = cmd.GetValueBucket();
    int32_t isOnlyDeleteDb = 0;
    if (!GetInt32FromValuesBucket(valuesBucket, DELETE_TOOL_ONLY_DATABASE, isOnlyDeleteDb)) {
        MEDIA_ERR_LOG("Can not get delete tool value");
        return E_INVALID_VALUES;
    }
    MediaLibraryRdbStore::UpdateAPI10Tables();
    const static vector<string> DELETE_DIR_LIST = {
        ROOT_MEDIA_DIR + PHOTO_BUCKET,
        ROOT_MEDIA_DIR + AUDIO_BUCKET,
        ROOT_MEDIA_DIR + CAMERA_DIR_VALUES,
        ROOT_MEDIA_DIR + VIDEO_DIR_VALUES,
        ROOT_MEDIA_DIR + PIC_DIR_VALUES,
        ROOT_MEDIA_DIR + AUDIO_DIR_VALUES,
        ROOT_MEDIA_DIR + ".thumbs"
    };

    if (!isOnlyDeleteDb) {
        for (const string &dir : DELETE_DIR_LIST) {
            if (!MediaFileUtils::DeleteDir(dir)) {
                MEDIA_ERR_LOG("Delete dir %{private}s failed", dir.c_str());
            }
        }
        for (auto &dir : PRESET_ROOT_DIRS) {
            string ditPath = ROOT_MEDIA_DIR + dir;
            MediaFileUtils::CreateDirectory(ditPath);
        }
    }

    return E_OK;
}

static bool CheckOprnObject(OperationObject object)
{
    const set<OperationObject> validOprnObjectet = {
        OperationObject::FILESYSTEM_PHOTO,
        OperationObject::FILESYSTEM_AUDIO
    };
    if (validOprnObjectet.find(object) == validOprnObjectet.end()) {
        MEDIA_ERR_LOG("input OperationObject %{public}d error!", object);
        return false;
    }
    return true;
}

static OperationObject GetOprnObjectByMediaType(int32_t type)
{
    switch (type) {
        case MediaType::MEDIA_TYPE_IMAGE:
        case MediaType::MEDIA_TYPE_VIDEO: {
            return OperationObject::FILESYSTEM_PHOTO;
        }
        case MediaType::MEDIA_TYPE_AUDIO: {
            return OperationObject::FILESYSTEM_AUDIO;
        }
        case MediaType::MEDIA_TYPE_FILE: {
            return OperationObject::FILESYSTEM_ASSET;
        }
        default: {
            return OperationObject::UNKNOWN_OBJECT;
        }
    }
}

shared_ptr<FileAsset> MediaLibraryAssetOperations::GetAssetFromResultSet(
    const shared_ptr<NativeRdb::ResultSet> &resultSet, const vector<string> &columns)
{
    auto fileAsset = make_shared<FileAsset>();
    CHECK_AND_RETURN_RET_LOG(resultSet != nullptr, nullptr, "resultSet is nullptr");
    int32_t count = 0;
    CHECK_AND_RETURN_RET_LOG(resultSet->GetRowCount(count) == NativeRdb::E_OK, nullptr,
        "can not get resultset row count");
    CHECK_AND_RETURN_RET_LOG(count == 1, nullptr, "ResultSet count is %{public}d, not 1", count);
    CHECK_AND_RETURN_RET_LOG(resultSet->GoToFirstRow() == NativeRdb::E_OK, nullptr, "can not go to first row");
    for (const auto &column : columns) {
        int32_t columnIndex = 0;
        CHECK_AND_RETURN_RET_LOG(resultSet->GetColumnIndex(column, columnIndex) == NativeRdb::E_OK,
            nullptr, "Can not get column %{private}s index", column.c_str());
        CHECK_AND_RETURN_RET_LOG(FILEASSET_MEMBER_MAP.find(column) != FILEASSET_MEMBER_MAP.end(), nullptr,
            "Can not find column %{private}s from member map", column.c_str());
        int32_t memberType = FILEASSET_MEMBER_MAP.at(column);
        switch (memberType) {
            case MEMBER_TYPE_INT32: {
                int32_t value = 0;
                CHECK_AND_RETURN_RET_LOG(resultSet->GetInt(columnIndex, value) == NativeRdb::E_OK, nullptr,
                    "Can not get int value from column %{private}s", column.c_str());
                auto &map = fileAsset->GetMemberMap();
                map[column] = value;
                break;
            }
            case MEMBER_TYPE_INT64: {
                int64_t value = 0;
                CHECK_AND_RETURN_RET_LOG(resultSet->GetLong(columnIndex, value) == NativeRdb::E_OK, nullptr,
                    "Can not get long value from column %{private}s", column.c_str());
                auto &map = fileAsset->GetMemberMap();
                map[column] = value;
                break;
            }
            case MEMBER_TYPE_STRING: {
                string value;
                CHECK_AND_RETURN_RET_LOG(resultSet->GetString(columnIndex, value) == NativeRdb::E_OK, nullptr,
                    "Can not get string value from column %{private}s", column.c_str());
                auto &map = fileAsset->GetMemberMap();
                map[column] = value;
                break;
            }
        }
    }
    return fileAsset;
}

shared_ptr<FileAsset> MediaLibraryAssetOperations::GetFileAssetFromDb(const string &column,
    const string &value, OperationObject oprnObject, const vector<string> &columns, const string &networkId)
{
    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryAssetOperations::GetFileAssetFromDb");

    if (!CheckOprnObject(oprnObject) || column.empty() || value.empty()) {
        return nullptr;
    }

    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return nullptr;
    }

    MediaLibraryCommand cmd(oprnObject, OperationType::QUERY, networkId);
    cmd.GetAbsRdbPredicates()->EqualTo(column, value);

    auto resultSet = rdbStore->Query(cmd, columns);
    if (resultSet == nullptr) {
        return nullptr;
    }
    return GetAssetFromResultSet(resultSet, columns);
}

shared_ptr<FileAsset> MediaLibraryAssetOperations::GetFileAssetFromDb(AbsPredicates &predicates,
    OperationObject oprnObject, const vector<string> &columns, const string &networkId)
{
    if (!CheckOprnObject(oprnObject)) {
        return nullptr;
    }

    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return nullptr;
    }

    MediaLibraryCommand cmd(oprnObject, OperationType::QUERY, networkId);
    cmd.GetAbsRdbPredicates()->SetWhereClause(predicates.GetWhereClause());
    cmd.GetAbsRdbPredicates()->SetWhereArgs(predicates.GetWhereArgs());
    cmd.GetAbsRdbPredicates()->SetOrder(predicates.GetOrder());

    auto resultSet = rdbStore->Query(cmd, columns);
    if (resultSet == nullptr) {
        return nullptr;
    }
    return GetAssetFromResultSet(resultSet, columns);
}

static inline string GetVirtualPath(const string &relativePath, const string &displayName)
{
    if (relativePath[relativePath.size() - 1] != SLASH_CHAR) {
        return relativePath + SLASH_CHAR + displayName;
    } else {
        return relativePath + displayName;
    }
}

static string GetAssetPackageName(const FileAsset &fileAsset, const string &bundleName)
{
    if (fileAsset.GetPhotoSubType() == static_cast<int32_t>(PhotoSubType::SCREENSHOT)) {
        if (fileAsset.GetMediaType() == static_cast<int32_t>(MediaType::MEDIA_TYPE_IMAGE) ||
            fileAsset.GetMediaType() == static_cast<int32_t>(MediaType::MEDIA_TYPE_PHOTO)) {
            return "截图";
        } else if (fileAsset.GetMediaType() == static_cast<int32_t>(MediaType::MEDIA_TYPE_VIDEO)) {
            return "屏幕录制";
        }
    }
    return PermissionUtils::GetPackageNameByBundleName(bundleName);
}

static void FillAssetInfo(MediaLibraryCommand &cmd, const FileAsset &fileAsset)
{
    // Fill basic file information into DB
    const string& displayName = fileAsset.GetDisplayName();
    int64_t nowTime = MediaFileUtils::UTCTimeSeconds();
    ValuesBucket assetInfo;
    assetInfo.PutInt(MediaColumn::MEDIA_TYPE, fileAsset.GetMediaType());
    string extension = ScannerUtils::GetFileExtension(displayName);
    assetInfo.PutString(MediaColumn::MEDIA_MIME_TYPE,
        MimeTypeUtils::GetMimeTypeFromExtension(extension));
    assetInfo.PutString(MediaColumn::MEDIA_FILE_PATH, fileAsset.GetPath());
    if (cmd.GetApi() == MediaLibraryApi::API_OLD) {
        assetInfo.PutString(MediaColumn::MEDIA_RELATIVE_PATH,
            fileAsset.GetRelativePath());
        assetInfo.PutString(MediaColumn::MEDIA_VIRTURL_PATH,
            GetVirtualPath(fileAsset.GetRelativePath(), fileAsset.GetDisplayName()));
    } else {
        assetInfo.PutLong(MediaColumn::MEDIA_TIME_PENDING, fileAsset.GetTimePending());
    }
    assetInfo.PutString(MediaColumn::MEDIA_NAME, displayName);
    assetInfo.PutString(MediaColumn::MEDIA_TITLE,
        MediaFileUtils::GetTitleFromDisplayName(displayName));
    if (cmd.GetOprnObject() == OperationObject::FILESYSTEM_PHOTO) {
        assetInfo.PutInt(PhotoColumn::PHOTO_SUBTYPE, fileAsset.GetPhotoSubType());
        assetInfo.PutString(PhotoColumn::CAMERA_SHOT_KEY, fileAsset.GetCameraShotKey());
        assetInfo.PutString(PhotoColumn::PHOTO_DATE_YEAR,
            MediaFileUtils::StrCreateTime(PhotoColumn::PHOTO_DATE_YEAR_FORMAT, nowTime));
        assetInfo.PutString(PhotoColumn::PHOTO_DATE_MONTH,
            MediaFileUtils::StrCreateTime(PhotoColumn::PHOTO_DATE_MONTH_FORMAT, nowTime));
        assetInfo.PutString(PhotoColumn::PHOTO_DATE_DAY,
            MediaFileUtils::StrCreateTime(PhotoColumn::PHOTO_DATE_DAY_FORMAT, nowTime));
    }
    assetInfo.PutString(MediaColumn::MEDIA_OWNER_PACKAGE, cmd.GetBundleName());
    if (!cmd.GetBundleName().empty()) {
        assetInfo.PutString(MediaColumn::MEDIA_PACKAGE_NAME,
            GetAssetPackageName(fileAsset, cmd.GetBundleName()));
    }

    assetInfo.PutString(MediaColumn::MEDIA_DEVICE_NAME, cmd.GetDeviceName());
    assetInfo.PutLong(MediaColumn::MEDIA_DATE_ADDED, nowTime);
    cmd.SetValueBucket(assetInfo);
}

int32_t MediaLibraryAssetOperations::InsertAssetInDb(MediaLibraryCommand &cmd, const FileAsset &fileAsset)
{
    // All values inserted in this function are the base property for files
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return E_HAS_DB_ERROR;
    }

    if (!fileAsset.GetPath().empty() && MediaFileUtils::IsFileExists(fileAsset.GetPath())) {
        return E_FILE_EXIST;
    }
    FillAssetInfo(cmd, fileAsset);

    int64_t outRowId = -1;
    int32_t errCode = rdbStore->Insert(cmd, outRowId);
    if (errCode != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Insert into db failed, errCode = %{public}d", errCode);
        return E_HAS_DB_ERROR;
    }
    return static_cast<int32_t>(outRowId);
}

static bool CheckTypeFromRootDir(const std::string &rootDirName, int32_t type)
{
    // "Camera/"
    if (!strcmp(rootDirName.c_str(), CAMERA_DIR_VALUES.c_str())) {
        if (type == MEDIA_TYPE_IMAGE || type == MEDIA_TYPE_VIDEO) {
            return true;
        }
    }
    // "Videos/"
    if (!strcmp(rootDirName.c_str(), VIDEO_DIR_VALUES.c_str())) {
        if (type == MEDIA_TYPE_VIDEO) {
            return true;
        }
    }
    // "Pictures/"
    if (!strcmp(rootDirName.c_str(), PIC_DIR_VALUES.c_str())) {
        if (type == MEDIA_TYPE_IMAGE) {
            return true;
        }
    }
    // "Audios/"
    if (!strcmp(rootDirName.c_str(), AUDIO_DIR_VALUES.c_str())) {
        if (type == MEDIA_TYPE_AUDIO) {
            return true;
        }
    }
    // "Documents/"
    if (!strcmp(rootDirName.c_str(), DOC_DIR_VALUES.c_str())) {
        return true;
    }
    // "Download/"
    if (!strcmp(rootDirName.c_str(), DOWNLOAD_DIR_VALUES.c_str())) {
        return true;
    }
    MEDIA_ERR_LOG("Cannot match rootDir %{private}s and mediaType %{public}d",
        rootDirName.c_str(), type);
    return false;
}

int32_t MediaLibraryAssetOperations::CheckWithType(bool isContains, const string &displayName,
    const string &extention, int32_t mediaType)
{
    string name = isContains ? displayName : extention;
    int32_t errCode =  isContains ? CheckDisplayNameWithType(name, mediaType) : CheckExtWithType(name, mediaType);
    CHECK_AND_RETURN_RET_LOG(errCode == E_OK, errCode,
        "Failed to Check Dir and extention, (displayName or extention)=%{private}s, mediaType=%{public}d",
        name.c_str(), mediaType);
    return errCode;
}

int32_t MediaLibraryAssetOperations::CheckDisplayNameWithType(const string &displayName, int32_t mediaType)
{
    int32_t ret = MediaFileUtils::CheckDisplayName(displayName);
    CHECK_AND_RETURN_RET_LOG(ret == E_OK, E_INVALID_DISPLAY_NAME, "Check DisplayName failed, "
        "displayName=%{private}s", displayName.c_str());

    string ext = MediaFileUtils::GetExtensionFromPath(displayName);
    CHECK_AND_RETURN_RET_LOG(!ext.empty(), E_INVALID_DISPLAY_NAME, "invalid extension, displayName=%{private}s",
        displayName.c_str());

    auto typeFromExt = MediaFileUtils::GetMediaType(displayName);
    CHECK_AND_RETURN_RET_LOG(typeFromExt == mediaType, E_CHECK_MEDIATYPE_MATCH_EXTENSION_FAIL,
        "cannot match, mediaType=%{public}d, ext=%{private}s, type from ext=%{public}d",
        mediaType, ext.c_str(), typeFromExt);
    return E_OK;
}

int32_t MediaLibraryAssetOperations::CheckExtWithType(const string &extention, int32_t mediaType)
{
    string mimeType = MimeTypeUtils::GetMimeTypeFromExtension(extention);
    auto typeFromExt = MimeTypeUtils::GetMediaTypeFromMimeType(mimeType);
    CHECK_AND_RETURN_RET_LOG(typeFromExt == mediaType, E_CHECK_MEDIATYPE_MATCH_EXTENSION_FAIL,
        "cannot match, mediaType=%{public}d, ext=%{private}s, type from ext=%{public}d",
        mediaType, extention.c_str(), typeFromExt);
    return E_OK;
}

int32_t MediaLibraryAssetOperations::CheckRelativePathWithType(const string &relativePath, int32_t mediaType)
{
    int32_t ret = MediaFileUtils::CheckRelativePath(relativePath);
    CHECK_AND_RETURN_RET_LOG(ret == E_OK, E_INVALID_PATH, "Check relativePath failed, "
        "relativePath=%{private}s", relativePath.c_str());

    // get rootdir and check if it match mediatype
    string rootDirName;
    MediaFileUtils::GetRootDirFromRelativePath(relativePath, rootDirName);
    CHECK_AND_RETURN_RET_LOG(!rootDirName.empty(), E_INVALID_PATH, "Cannot get rootdirName");

    bool isValid = CheckTypeFromRootDir(rootDirName, mediaType);
    CHECK_AND_RETURN_RET(isValid, E_CHECK_MEDIATYPE_FAIL);
    return E_OK;
}

void MediaLibraryAssetOperations::GetAssetRootDir(int32_t mediaType, string &rootDirPath)
{
    map<int, string> rootDir = {
        { MEDIA_TYPE_FILE, DOCUMENT_BUCKET + SLASH_CHAR },
        { MEDIA_TYPE_VIDEO, PHOTO_BUCKET + SLASH_CHAR },
        { MEDIA_TYPE_IMAGE, PHOTO_BUCKET + SLASH_CHAR },
        { MEDIA_TYPE_AUDIO, AUDIO_BUCKET + SLASH_CHAR },
    };
    if (rootDir.count(mediaType) == 0) {
        rootDirPath = rootDir[MEDIA_TYPE_FILE];
    } else {
        rootDirPath = rootDir[mediaType];
    }
}

int32_t MediaLibraryAssetOperations::SetAssetPathInCreate(FileAsset &fileAsset)
{
    if (!fileAsset.GetPath().empty()) {
        return E_OK;
    }
    string extension = MediaFileUtils::GetExtensionFromPath(fileAsset.GetDisplayName());
    string filePath;
    int32_t uniqueId = CreateAssetUniqueId(fileAsset.GetMediaType());
    int32_t errCode = CreateAssetPathById(uniqueId, fileAsset.GetMediaType(), extension, filePath);
    if (errCode != E_OK) {
        MEDIA_ERR_LOG("Create Asset Path failed, errCode=%{public}d", errCode);
        return errCode;
    }

    // filePath can not be empty
    fileAsset.SetPath(filePath);
    return E_OK;
}

int32_t MediaLibraryAssetOperations::SetAssetPath(FileAsset &fileAsset, const string &extension)
{
    string filePath;
    int32_t uniqueId = CreateAssetUniqueId(fileAsset.GetMediaType());
    int32_t errCode = CreateAssetPathById(uniqueId, fileAsset.GetMediaType(), extension, filePath);
    if (errCode != E_OK) {
        MEDIA_ERR_LOG("Create Asset Path failed, errCode=%{public}d", errCode);
        return errCode;
    }

    // filePath can not be empty
    fileAsset.SetPath(filePath);
    string fileName = MediaFileUtils::GetFileName(filePath);
    string displayName = fileName.substr(0, fileName.find('_')) + '_' + fileName.substr(fileName.rfind('_') + 1);
    fileAsset.SetDisplayName(displayName);
    return E_OK;
}

int32_t MediaLibraryAssetOperations::DeleteAssetInDb(MediaLibraryCommand &cmd)
{
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return E_HAS_DB_ERROR;
    }

    string strDeleteCondition = cmd.GetAbsRdbPredicates()->GetWhereClause();
    if (strDeleteCondition.empty()) {
        string strRow = cmd.GetOprnFileId();
        if (strRow.empty() || !MediaLibraryDataManagerUtils::IsNumber(strRow)) {
            MEDIA_ERR_LOG("MediaLibraryAssetOperations DeleteFile: Index not digit, fileIdStr=%{private}s",
                strRow.c_str());
            return E_INVALID_FILEID;
        }
        cmd.GetAbsRdbPredicates()->EqualTo(MediaColumn::MEDIA_ID, strRow);
    }

    int32_t deletedRows = E_HAS_DB_ERROR;
    int32_t result = rdbStore->Delete(cmd, deletedRows);
    if (result != NativeRdb::E_OK) {
        MEDIA_ERR_LOG("Delete operation failed. Result %{public}d.", result);
    }

    return deletedRows;
}

int32_t MediaLibraryAssetOperations::ModifyAssetInDb(MediaLibraryCommand &cmd)
{
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return E_HAS_DB_ERROR;
    }

    TransactionOperations transactionOprn(rdbStore->GetRaw());
    int32_t errCode = transactionOprn.Start();
    if (errCode != E_OK) {
        return errCode;
    }

    int32_t rowId = 0;
    int32_t ret = rdbStore->Update(cmd, rowId);
    if (ret < 0 || rowId < 0) {
        MEDIA_ERR_LOG("update path failed, ret=%{public}d", ret);
        return ret;
    }
    transactionOprn.Finish();

    return rowId;
}

int32_t MediaLibraryAssetOperations::UpdateFileName(MediaLibraryCommand &cmd,
    const shared_ptr<FileAsset> &fileAsset, bool &isNameChanged)
{
    ValuesBucket &values = cmd.GetValueBucket();
    ValueObject valueObject;
    string newTitle;
    string newDisplayName;
    bool containsTitle = false;
    bool containsDisplayName = false;

    if (values.GetObject(MediaColumn::MEDIA_TITLE, valueObject)) {
        valueObject.GetString(newTitle);
        containsTitle = true;
    }
    if (values.GetObject(MediaColumn::MEDIA_NAME, valueObject)) {
        valueObject.GetString(newDisplayName);
        containsDisplayName = true;
    }
    if ((!containsTitle) && (!containsDisplayName)) {
        // do not need to update
        return E_OK;
    }
    if (containsTitle && containsDisplayName &&
        (MediaFileUtils::GetTitleFromDisplayName(newDisplayName) != newTitle)) {
        MEDIA_ERR_LOG("new displayName [%{private}s] and new title [%{private}s] is not same",
            newDisplayName.c_str(), newTitle.c_str());
        return E_INVALID_DISPLAY_NAME;
    }
    if (!containsTitle) {
        newTitle = MediaFileUtils::GetTitleFromDisplayName(newDisplayName);
    }
    if (!containsDisplayName) {
        newDisplayName = newTitle + "." + MediaFileUtils::SplitByChar(fileAsset->GetDisplayName(), '.');
    }

    int32_t ret = CheckDisplayNameWithType(newDisplayName, fileAsset->GetMediaType());
    CHECK_AND_RETURN_RET_LOG(ret == E_OK, ret, "Input displayName invalid %{private}s", newDisplayName.c_str());
    values.PutString(MediaColumn::MEDIA_TITLE, newTitle);
    values.PutString(MediaColumn::MEDIA_NAME, newDisplayName);
    isNameChanged = true;
    return E_OK;
}

int32_t MediaLibraryAssetOperations::SetUserComment(MediaLibraryCommand &cmd,
    const shared_ptr<FileAsset> &fileAsset)
{
    ValuesBucket &values = cmd.GetValueBucket();
    ValueObject valueObject;
    string newUserComment;

    if (values.GetObject(PhotoColumn::PHOTO_USER_COMMENT, valueObject)) {
        valueObject.GetString(newUserComment);
    } else {
        return E_OK;
    }

    uint32_t err = 0;
    SourceOptions opts;
    string filePath = fileAsset->GetFilePath();
    string extension = MediaFileUtils::GetExtensionFromPath(filePath);
    opts.formatHint = "image/" + extension;
    std::unique_ptr<ImageSource> imageSource = ImageSource::CreateImageSource(filePath, opts, err);
    if (err != 0 || imageSource == nullptr) {
        MEDIA_ERR_LOG("Failed to obtain image source, err = %{public}d", err);
        return E_OK;
    }

    string userComment;
    err = imageSource->GetImagePropertyString(0, PHOTO_DATA_IMAGE_USER_COMMENT, userComment);
    if (err != 0) {
        MEDIA_ERR_LOG("Image does not exit exif, no need to modify");
        return E_OK;
    }
    err = imageSource->ModifyImageProperty(0, PHOTO_DATA_IMAGE_USER_COMMENT, newUserComment, filePath);
    if (err != 0) {
        MEDIA_ERR_LOG("Modify image property user comment failed");
    }

    return E_OK;
}

int32_t MediaLibraryAssetOperations::UpdateRelativePath(MediaLibraryCommand &cmd,
    const shared_ptr<FileAsset> &fileAsset, bool &isNameChanged)
{
    string newRelativePath;
    ValuesBucket &values = cmd.GetValueBucket();
    ValueObject valueObject;
    if (values.GetObject(MediaColumn::MEDIA_RELATIVE_PATH, valueObject)) {
        valueObject.GetString(newRelativePath);
    } else {
        // relativePath is not modified
        return E_OK;
    }
    MediaFileUtils::FormatRelativePath(newRelativePath);

    if (newRelativePath == fileAsset->GetRelativePath()) {
        // relativepath has not been modified
        return E_OK;
    }

    int32_t errCode = CheckRelativePathWithType(newRelativePath, fileAsset->GetMediaType());
    if (errCode != E_SUCCESS) {
        MEDIA_ERR_LOG("Check RelativePath failed");
        return errCode;
    }
    values.Delete(MediaColumn::MEDIA_RELATIVE_PATH);
    values.PutString(MediaColumn::MEDIA_RELATIVE_PATH, newRelativePath);

    isNameChanged = true;
    return E_OK;
}

void MediaLibraryAssetOperations::UpdateVirtualPath(MediaLibraryCommand &cmd,
    const shared_ptr<FileAsset> &fileAsset)
{
    string relativePath;
    string displayName;
    ValuesBucket &values = cmd.GetValueBucket();
    ValueObject valueObject;

    if (values.GetObject(MediaColumn::MEDIA_NAME, valueObject)) {
        valueObject.GetString(displayName);
    } else {
        displayName = fileAsset->GetDisplayName();
    }

    if (values.GetObject(MediaColumn::MEDIA_RELATIVE_PATH, valueObject)) {
        valueObject.GetString(relativePath);
    } else {
        relativePath = fileAsset->GetRelativePath();
    }

    if (relativePath.back() != '/') {
        relativePath += '/';
    }
    string virtualPath = relativePath + displayName;
    values.PutString(MediaColumn::MEDIA_VIRTURL_PATH, virtualPath);
}

int32_t MediaLibraryAssetOperations::UpdateFileInDb(MediaLibraryCommand &cmd)
{
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return E_HAS_DB_ERROR;
    }

    int32_t updateRows = 0;
    int32_t result = rdbStore->Update(cmd, updateRows);
    if (result != NativeRdb::E_OK || updateRows <= 0) {
        MEDIA_ERR_LOG("Update File failed. Result %{public}d.", result);
        return E_HAS_DB_ERROR;
    }

    return updateRows;
}

static int32_t OpenFile(const string &filePath, const string &mode)
{
    std::string absFilePath;
    if (!PathToRealPath(filePath, absFilePath)) {
        MEDIA_ERR_LOG("Failed to get real path: %{private}s", filePath.c_str());
        return E_ERR;
    }

    return MediaPrivacyManager(absFilePath, mode).Open();
}

static int32_t SetPendingTime(const shared_ptr<FileAsset> &fileAsset, int64_t pendingTime)
{
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return E_HAS_DB_ERROR;
    }

    MediaLibraryCommand updatePendingCmd(GetOprnObjectByMediaType(fileAsset->GetMediaType()),
        OperationType::UPDATE);
    updatePendingCmd.GetAbsRdbPredicates()->EqualTo(MediaColumn::MEDIA_ID,
        to_string(fileAsset->GetId()));
    ValuesBucket values;
    values.PutLong(MediaColumn::MEDIA_TIME_PENDING, pendingTime);
    updatePendingCmd.SetValueBucket(values);
    int32_t rowId = 0;
    int32_t result = rdbStore->Update(updatePendingCmd, rowId);
    if (result != NativeRdb::E_OK || rowId <= 0) {
        MEDIA_ERR_LOG("Update File pending failed. Result %{public}d.", result);
        return E_HAS_DB_ERROR;
    }
    return E_OK;
}

static int32_t CreateFileAndSetPending(const shared_ptr<FileAsset> &fileAsset, int64_t pendingTime)
{
    int32_t errCode = MediaFileUtils::CreateAsset(fileAsset->GetPath());
    if (errCode != E_OK) {
        MEDIA_ERR_LOG("Create asset failed, path=%{private}s", fileAsset->GetPath().c_str());
        return errCode;
    }

    return SetPendingTime(fileAsset, pendingTime);
}

static int32_t SolvePendingStatus(const shared_ptr<FileAsset> &fileAsset, const string &mode)
{
    int64_t pendingTime = fileAsset->GetTimePending();
    if (pendingTime != 0) {
        if (mode == MEDIA_FILEMODE_READONLY) {
            MEDIA_ERR_LOG("FileAsset [%{private}s] pending status is %{private}ld and open mode is READ_ONLY",
                fileAsset->GetUri().c_str(), (long) pendingTime);
            return E_IS_PENDING_ERROR;
        }
        string networkId = MediaFileUtils::GetNetworkIdFromUri(fileAsset->GetUri());
        if (!networkId.empty()) {
            MEDIA_ERR_LOG("Can not open remote [%{private}s] pending file", networkId.c_str());
            return E_IS_PENDING_ERROR;
        }
        if (pendingTime == UNCREATE_FILE_TIMEPENDING) {
            int32_t errCode = CreateFileAndSetPending(fileAsset, UNCLOSE_FILE_TIMEPENDING);
            return errCode;
        }
        if (pendingTime == UNOPEN_FILE_COMPONENT_TIMEPENDING) {
            int32_t errCode = SetPendingTime(fileAsset, UNCLOSE_FILE_TIMEPENDING);
            return errCode;
        }
    }
    return E_OK;
}

int32_t MediaLibraryAssetOperations::OpenAsset(const shared_ptr<FileAsset> &fileAsset, const string &mode,
    MediaLibraryApi api)
{
    MediaLibraryTracer tracer;
    tracer.Start("MediaLibraryAssetOperations::OpenAsset");

    if (fileAsset == nullptr) {
        return E_INVALID_VALUES;
    }

    string lowerMode = mode;
    transform(lowerMode.begin(), lowerMode.end(), lowerMode.begin(), ::tolower);
    if (!MediaFileUtils::CheckMode(lowerMode)) {
        return E_INVALID_MODE;
    }

    string path;
    if (api == MediaLibraryApi::API_10) {
        int32_t errCode = SolvePendingStatus(fileAsset, mode);
        if (errCode != E_OK) {
            MEDIA_ERR_LOG("Solve pending status failed, errCode=%{public}d", errCode);
            return errCode;
        }
        path = fileAsset->GetPath();
    } else {
        // If below API10, TIME_PENDING is 0 after asset created, so if file is not exist, create an empty one
        if (!MediaFileUtils::IsFileExists(fileAsset->GetPath())) {
            int32_t errCode = MediaFileUtils::CreateAsset(fileAsset->GetPath());
            if (errCode != E_OK) {
                MEDIA_ERR_LOG("Create asset failed, path=%{private}s", fileAsset->GetPath().c_str());
                return errCode;
            }
        }
        path = MediaFileUtils::UpdatePath(fileAsset->GetPath(), fileAsset->GetUri());
    }

    tracer.Start("OpenFile");
    int32_t fd = OpenFile(path, lowerMode);
    tracer.Finish();
    if (fd < 0) {
        MEDIA_ERR_LOG("open file fd %{private}d, errno %{private}d", fd, errno);
        return E_HAS_FS_ERROR;
    }
    tracer.Start("AddWatchList");
    if (mode.find(MEDIA_FILEMODE_WRITEONLY) != string::npos) {
        auto watch = MediaLibraryInotify::GetInstance();
        if (watch != nullptr) {
            MEDIA_DEBUG_LOG("enter inotify, path = %{private}s", path.c_str());
            watch->AddWatchList(path, fileAsset->GetUri(), MediaLibraryApi::API_10);
        }
    }
    tracer.Finish();
    return fd;
}

int32_t MediaLibraryAssetOperations::CloseAsset(const shared_ptr<FileAsset> &fileAsset, bool isCreateThumbSync)
{
    if (fileAsset == nullptr) {
        return E_INVALID_VALUES;
    }

    // remove inotify event since there is close cmd
    auto watch = MediaLibraryInotify::GetInstance();
    if (watch != nullptr) {
        string uri = fileAsset->GetUri();
        watch->RemoveByFileUri(uri, MediaLibraryApi::API_10);
        MEDIA_DEBUG_LOG("watch RemoveByFileUri, uri:%{private}s", uri.c_str());
    }

    string path = fileAsset->GetPath();
    // if pending == 0, scan
    // if pending == UNCREATE_FILE_TIMEPENDING, not occur under normal conditions
    // if pending == UNCLOSE_FILE_TIMEPENDING, set pending = 0 and scan
    // if pending == UNOPEN_FILE_COMPONENT_TIMEPENDING, not allowed to close
    // if pending is timestamp, do nothing
    if (fileAsset->GetTimePending() == 0 || fileAsset->GetTimePending() == UNCLOSE_FILE_TIMEPENDING) {
        MediaLibraryRdbUtils::UpdateSystemAlbumInternal(
            MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw(), {
            to_string(PhotoAlbumSubType::IMAGES),
            to_string(PhotoAlbumSubType::VIDEO),
            to_string(PhotoAlbumSubType::SCREENSHOT),
            to_string(PhotoAlbumSubType::CAMERA),
            to_string(PhotoAlbumSubType::FAVORITE),
        });
        if (fileAsset->GetTimePending() == UNCLOSE_FILE_TIMEPENDING) {
            ScanFile(path, isCreateThumbSync, false);
        } else {
            ScanFile(path, isCreateThumbSync, true);
        }
        return E_OK;
    } else if (fileAsset->GetTimePending() == UNCREATE_FILE_TIMEPENDING ||
        fileAsset->GetTimePending() == UNOPEN_FILE_COMPONENT_TIMEPENDING) {
        MEDIA_ERR_LOG("This asset [%{private}d] pending status cannot close", fileAsset->GetId());
        return E_IS_PENDING_ERROR;
    } else if (fileAsset->GetTimePending() > 0) {
        MEDIA_WARN_LOG("This asset [%{private}d] is in pending", fileAsset->GetId());
        return E_OK;
    } else {
        MEDIA_ERR_LOG("This asset [%{private}d] pending status is invalid", fileAsset->GetId());
        return E_INVALID_VALUES;
    }
}

void MediaLibraryAssetOperations::InvalidateThumbnail(const string &fileId, int32_t type)
{
    string tableName;
    switch (type) {
        case MediaType::MEDIA_TYPE_IMAGE:
        case MediaType::MEDIA_TYPE_VIDEO: {
            tableName = PhotoColumn::PHOTOS_TABLE;
            break;
        }
        case MediaType::MEDIA_TYPE_AUDIO: {
            tableName = AudioColumn::AUDIOS_TABLE;
            break;
        }
        default: {
            MEDIA_ERR_LOG("Can not match this type %{public}d", type);
            return;
        }
    }
    ThumbnailService::GetInstance()->InvalidateThumbnail(fileId, tableName);
}

void MediaLibraryAssetOperations::ScanFile(const string &path, bool isCreateThumbSync, bool isInvalidateThumb)
{
    shared_ptr<ScanAssetCallback> scanAssetCallback = make_shared<ScanAssetCallback>();
    if (scanAssetCallback == nullptr) {
        MEDIA_ERR_LOG("Failed to create scan file callback object");
        return;
    }
    if (isCreateThumbSync) {
        scanAssetCallback->SetSync(true);
    }
    if (!isInvalidateThumb) {
        scanAssetCallback->SetIsInvalidateThumb(false);
    }

    int ret = MediaScannerManager::GetInstance()->ScanFileSync(path, scanAssetCallback, MediaLibraryApi::API_10);
    if (ret != 0) {
        MEDIA_ERR_LOG("Scan file failed!");
    }
}

int32_t MediaLibraryAssetOperations::SendTrashNotify(MediaLibraryCommand &cmd, int32_t rowId, const string &extraUri)
{
    ValueObject value;
    int64_t trashDate = 0;
    if (!cmd.GetValueBucket().GetObject(PhotoColumn::MEDIA_DATE_TRASHED, value)) {
        return E_DO_NOT_NEDD_SEND_NOTIFY;
    }
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw();
    MediaLibraryRdbUtils::UpdateUserAlbumInternal(rdbStore);
    MediaLibraryRdbUtils::UpdateSystemAlbumInternal(rdbStore);

    value.GetLong(trashDate);

    string prefix;
    if (cmd.GetOprnObject() == OperationObject::FILESYSTEM_PHOTO) {
        prefix = PhotoColumn::PHOTO_URI_PREFIX;
    } else if (cmd.GetOprnObject() == OperationObject::FILESYSTEM_AUDIO) {
        prefix = AudioColumn::AUDIO_URI_PREFIX;
    } else {
        return E_OK;
    }

    string notifyUri = MediaFileUtils::GetUriByExtrConditions(prefix, to_string(rowId), extraUri);
    auto watch = MediaLibraryNotify::GetInstance();
    if (trashDate > 0) {
        watch->Notify(notifyUri, NotifyType::NOTIFY_REMOVE);
        watch->Notify(notifyUri, NotifyType::NOTIFY_ALBUM_REMOVE_ASSET);
    } else {
        watch->Notify(notifyUri, NotifyType::NOTIFY_ADD);
        watch->Notify(notifyUri, NotifyType::NOTIFY_ALBUM_ADD_ASSERT);
    }

    int trashAlbumId = watch->GetAlbumIdBySubType(PhotoAlbumSubType::TRASH);
    if (trashAlbumId <= 0) {
        return E_OK;
    }
    NotifyType type = (trashDate > 0) ? NotifyType::NOTIFY_ALBUM_ADD_ASSERT : NotifyType::NOTIFY_ALBUM_REMOVE_ASSET;
    watch->Notify(notifyUri, type, trashAlbumId);
    return E_OK;
}

void MediaLibraryAssetOperations::SendFavoriteNotify(MediaLibraryCommand &cmd, int32_t rowId, const string &extraUri)
{
    ValueObject value;
    int32_t isFavorite = 0;
    if (!cmd.GetValueBucket().GetObject(PhotoColumn::MEDIA_IS_FAV, value)) {
        return;
    }
    value.GetInt(isFavorite);
    MediaLibraryRdbUtils::UpdateSystemAlbumInternal(
        MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw(),
        { to_string(PhotoAlbumSubType::FAVORITE) });
    auto watch = MediaLibraryNotify::GetInstance();
    if (cmd.GetOprnObject() != OperationObject::FILESYSTEM_PHOTO) {
        return;
    }
    int favAlbumId = watch->GetAlbumIdBySubType(PhotoAlbumSubType::FAVORITE);
    if (favAlbumId <= 0) {
        return;
    }

    NotifyType type = (isFavorite) ? NotifyType::NOTIFY_ALBUM_ADD_ASSERT : NotifyType::NOTIFY_ALBUM_REMOVE_ASSET;
    watch->Notify(MediaFileUtils::GetUriByExtrConditions(PhotoColumn::PHOTO_URI_PREFIX, to_string(rowId), extraUri),
        type, favAlbumId);
}

int32_t MediaLibraryAssetOperations::SendHideNotify(MediaLibraryCommand &cmd, int32_t rowId, const string &extraUri)
{
    ValueObject value;
    int32_t hiddenState = 0;
    if (!cmd.GetValueBucket().GetObject(MediaColumn::MEDIA_HIDDEN, value)) {
        return E_DO_NOT_NEDD_SEND_NOTIFY;
    }
    value.GetInt(hiddenState);
    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw()->GetRaw();
    MediaLibraryRdbUtils::UpdateUserAlbumInternal(rdbStore);
    MediaLibraryRdbUtils::UpdateSystemAlbumInternal(rdbStore);

    string prefix;
    if (cmd.GetOprnObject() == OperationObject::FILESYSTEM_PHOTO) {
        prefix = PhotoColumn::PHOTO_URI_PREFIX;
    } else if (cmd.GetOprnObject() == OperationObject::FILESYSTEM_AUDIO) {
        prefix = AudioColumn::AUDIO_URI_PREFIX;
    } else {
        return E_OK;
    }

    string notifyUri = MediaFileUtils::GetUriByExtrConditions(prefix, to_string(rowId), extraUri);
    auto watch = MediaLibraryNotify::GetInstance();
    if (hiddenState > 0) {
        watch->Notify(notifyUri, NotifyType::NOTIFY_REMOVE);
        watch->Notify(notifyUri, NotifyType::NOTIFY_ALBUM_REMOVE_ASSET);
    } else {
        watch->Notify(notifyUri, NotifyType::NOTIFY_ADD);
        watch->Notify(notifyUri, NotifyType::NOTIFY_ALBUM_ADD_ASSERT);
    }

    int hiddenAlbumId = watch->GetAlbumIdBySubType(PhotoAlbumSubType::HIDDEN);
    if (hiddenAlbumId <= 0) {
        return E_OK;
    }
    NotifyType type = (hiddenState > 0) ? NotifyType::NOTIFY_ALBUM_ADD_ASSERT : NotifyType::NOTIFY_ALBUM_REMOVE_ASSET;
    watch->Notify(notifyUri, type, hiddenAlbumId);
    return E_OK;
}

int32_t MediaLibraryAssetOperations::SendModifyUserCommentNotify(MediaLibraryCommand &cmd, int32_t rowId,
    const string &extraUri)
{
    if (cmd.GetOprnType() != OperationType::SET_USER_COMMENT) {
        return E_DO_NOT_NEDD_SEND_NOTIFY;
    }

    auto watch = MediaLibraryNotify::GetInstance();
    watch->Notify(MediaFileUtils::GetUriByExtrConditions(PhotoColumn::PHOTO_URI_PREFIX, to_string(rowId), extraUri),
        NotifyType::NOTIFY_UPDATE);
    return E_OK;
}

int32_t MediaLibraryAssetOperations::SetPendingTrue(const shared_ptr<FileAsset> &fileAsset)
{
    // time_pending = 0, means file is created, not allowed
    // time_pending = UNCREATE_FILE_TIMEPENDING, means file is not created yet, create an empty one
    // time_pending = UNCLOSE_FILE_TIMEPENDING, means file is not close yet, set pending time
    // time_pending = UNOPEN_FILE_COMPONENT_TIMEPENDING, means file is created but not open, set pending time
    // time_pending is timestamp, update it
    int64_t timestamp = MediaFileUtils::UTCTimeSeconds();
    if (timestamp <= 0) {
        MEDIA_ERR_LOG("Get timestamp failed, timestamp:%{public}ld", (long) timestamp);
        return E_INVALID_TIMESTAMP;
    }
    if (fileAsset->GetTimePending() == 0) {
        MEDIA_ERR_LOG("fileAsset time_pending is 0, not allowed");
        return E_INVALID_VALUES;
    } else if (fileAsset->GetTimePending() == UNCREATE_FILE_TIMEPENDING) {
        int32_t errCode = CreateFileAndSetPending(fileAsset, timestamp);
        if (errCode != E_OK) {
            MEDIA_ERR_LOG("Create asset failed, id=%{public}d", fileAsset->GetId());
            return errCode;
        }
    } else if (fileAsset->GetTimePending() == UNCLOSE_FILE_TIMEPENDING ||
        fileAsset->GetTimePending() == UNOPEN_FILE_COMPONENT_TIMEPENDING ||
        fileAsset->GetTimePending() > 0) {
        int32_t errCode = SetPendingTime(fileAsset, timestamp);
        if (errCode != E_OK) {
            MEDIA_ERR_LOG("Set pending time failed, id=%{public}d", fileAsset->GetId());
            return errCode;
        }
    } else {
        MEDIA_ERR_LOG("fileAsset time_pending is invalid, time_pending:%{public}ld, id=%{public}d",
            (long) fileAsset->GetTimePending(), fileAsset->GetId());
        return E_INVALID_VALUES;
    }

    return E_OK;
}

int32_t MediaLibraryAssetOperations::SetPendingFalse(const shared_ptr<FileAsset> &fileAsset)
{
    // time_pending = 0, only return
    // time_pending = UNCREATE_FILE_TIMEPENDING, means file is not created yet, not allowed
    // time_pending = UNCLOSE_FILE_TIMEPENDING, means file is not close yet, not allowed
    // time_pending = UNOPEN_FILE_COMPONENT_TIMEPENDING, means file is created but not open, not allowed
    // time_pending is timestamp, scan and set pending time = 0
    if (fileAsset->GetTimePending() == 0) {
        return E_OK;
    } else if (fileAsset->GetTimePending() == UNCREATE_FILE_TIMEPENDING) {
        MEDIA_ERR_LOG("file is not created yet, not allowed, id=%{public}d", fileAsset->GetId());
        return E_INVALID_VALUES;
    } else if (fileAsset->GetTimePending() == UNCLOSE_FILE_TIMEPENDING) {
        MEDIA_ERR_LOG("file is not close yet, not allowed, id=%{public}d", fileAsset->GetId());
        return E_INVALID_VALUES;
    } else if (fileAsset->GetTimePending() == UNOPEN_FILE_COMPONENT_TIMEPENDING) {
        MEDIA_ERR_LOG("file is created but not open, not allowed, id=%{public}d", fileAsset->GetId());
        return E_INVALID_VALUES;
    } else if (fileAsset->GetTimePending() > 0) {
        ScanFile(fileAsset->GetPath(), true, true);
    } else {
        MEDIA_ERR_LOG("fileAsset time_pending is invalid, time_pending:%{public}ld, id=%{public}d",
            (long) fileAsset->GetTimePending(), fileAsset->GetId());
        return E_INVALID_VALUES;
    }
    return E_OK;
}

int32_t MediaLibraryAssetOperations::SetPendingStatus(MediaLibraryCommand &cmd)
{
    int32_t pendingStatus = 0;
    if (!GetInt32FromValuesBucket(cmd.GetValueBucket(), MediaColumn::MEDIA_TIME_PENDING, pendingStatus)) {
        return E_INVALID_VALUES;
    }

    vector<string> columns = {
        MediaColumn::MEDIA_ID,
        MediaColumn::MEDIA_FILE_PATH,
        MediaColumn::MEDIA_TYPE,
        MediaColumn::MEDIA_TIME_PENDING
    };
    auto fileAsset = GetFileAssetFromDb(*(cmd.GetAbsRdbPredicates()), cmd.GetOprnObject(), columns);
    if (fileAsset == nullptr) {
        return E_INVALID_VALUES;
    }
    if (pendingStatus == 1) {
        return SetPendingTrue(fileAsset);
    } else if (pendingStatus == 0) {
        return SetPendingFalse(fileAsset);
    } else {
        MEDIA_ERR_LOG("pendingStatus is invalid, pendingStatus:%{public}d", pendingStatus);
        return E_INVALID_VALUES;
    }
}

static string ConvertMediaPathFromCloudPath(const string &path)
{
    // if input path is /storage/cloud/xxx, return /storage/media/local/xxx
    string mediaPath = "/storage/media/local/";
    string cloudPath = "/storage/cloud/";
    string newPath = path;
    if (newPath.find(cloudPath) != string::npos) {
        newPath.replace(newPath.find(cloudPath), cloudPath.length(), mediaPath);
    }
    return newPath;
}

int32_t MediaLibraryAssetOperations::GrantUriPermission(const string &uri, const string &bundleName,
    const string &path)
{
    if (uri.empty() || path.empty()) {
        MEDIA_ERR_LOG("uri or path is empty, uri:%{private}s, path:%{private}s", uri.c_str(), path.c_str());
        return E_INVALID_VALUES;
    }
    if (bundleName.empty()) {
        MEDIA_WARN_LOG("bundleName is empty, bundleName:%{private}s", bundleName.c_str());
        return E_OK;
    }
    auto& uriPermissionClient = AAFwk::UriPermissionManagerClient::GetInstance();
    auto flag = AAFwk::Want::FLAG_AUTH_WRITE_URI_PERMISSION;
    if (!MediaFileUtils::CreateFile(path)) {
        MEDIA_ERR_LOG("Can not create file, path: %{private}s, errno: %{public}d", path.c_str(), errno);
        return E_HAS_FS_ERROR;
    }

    int32_t ret = uriPermissionClient.GrantUriPermission(Uri(uri), flag, bundleName, 1);
    if (ret != 0) {
        MEDIA_ERR_LOG("Can not grant uri permission, uri: %{private}s, bundleName: %{private}s, ret: %{public}d",
            uri.c_str(), bundleName.c_str(), ret);
        return E_GRANT_URI_PERM_FAIL;
    }

    MediaLibraryTracer tracer;
    tracer.Start("AddWatchList");
    auto watch = MediaLibraryInotify::GetInstance();
    if (watch != nullptr) {
        MEDIA_DEBUG_LOG("enter inotify, path = %{private}s", path.c_str());
        watch->AddWatchList(ConvertMediaPathFromCloudPath(path), uri, MediaLibraryApi::API_10);
    }
    tracer.Finish();

    return E_OK;
}

bool MediaLibraryAssetOperations::GetInt32FromValuesBucket(const NativeRdb::ValuesBucket &values,
    const std::string &column, int32_t &value)
{
    ValueObject valueObject;
    if (values.GetObject(column, valueObject)) {
        valueObject.GetInt(value);
    } else {
        return false;
    }
    return true;
}

std::string MediaLibraryAssetOperations::CreateExtUriForV10Asset(FileAsset &fileAsset)
{
    const std::string &filePath = fileAsset.GetPath();
    const std::string &displayName = fileAsset.GetDisplayName();
    auto mediaType = fileAsset.GetMediaType();
    if (filePath.empty() || displayName.empty() || mediaType < 0) {
        MEDIA_ERR_LOG("param invalid, filePath %{private}s or displayName %{private}s invalid failed.",
            filePath.c_str(), displayName.c_str());
        return "";
    }

    string extrUri = MediaFileUtils::GetExtraUri(displayName, filePath);
    return MediaFileUtils::GetUriByExtrConditions(ML_FILE_URI_PREFIX + MediaFileUri::GetMediaTypeUri(mediaType,
        MEDIA_API_VERSION_V10) + "/", to_string(fileAsset.GetId()), extrUri);
}

bool MediaLibraryAssetOperations::GetStringFromValuesBucket(const NativeRdb::ValuesBucket &values,
    const std::string &column, string &value)
{
    ValueObject valueObject;
    if (values.GetObject(column, valueObject)) {
        valueObject.GetString(value);
    } else {
        return false;
    }
    return true;
}

int32_t MediaLibraryAssetOperations::CreateAssetUniqueId(int32_t type)
{
    string typeString;
    switch (type) {
        case MediaType::MEDIA_TYPE_IMAGE:
            typeString += IMAGE_ASSET_TYPE;
            break;
        case MediaType::MEDIA_TYPE_VIDEO:
            typeString += VIDEO_ASSET_TYPE;
            break;
        case MediaType::MEDIA_TYPE_AUDIO:
            typeString += AUDIO_ASSET_TYPE;
            break;
        default:
            MEDIA_ERR_LOG("This type %{public}d can not get unique id", type);
            return E_INVALID_VALUES;
    }

    const string updateSql = "UPDATE " + ASSET_UNIQUE_NUMBER_TABLE + " SET " + UNIQUE_NUMBER +
        "=" + UNIQUE_NUMBER + "+1" " WHERE " + ASSET_MEDIA_TYPE + "='" + typeString + "';";
    const string querySql = "SELECT " + UNIQUE_NUMBER + " FROM " + ASSET_UNIQUE_NUMBER_TABLE +
        " WHERE " + ASSET_MEDIA_TYPE + "='" + typeString + "';";

    auto rdbStore = MediaLibraryUnistoreManager::GetInstance().GetRdbStoreRaw();
    if (rdbStore == nullptr) {
        return E_HAS_DB_ERROR;
    }
    lock_guard<mutex> lock(g_uniqueNumberLock);
    int32_t errCode = rdbStore->ExecuteSql(updateSql);
    if (errCode < 0) {
        MEDIA_ERR_LOG("execute update unique number failed, ret=%{public}d", errCode);
        return errCode;
    }

    auto resultSet = rdbStore->QuerySql(querySql);
    if (resultSet == nullptr || resultSet->GoToFirstRow() != NativeRdb::E_OK) {
        return E_HAS_DB_ERROR;
    }
    return GetInt32Val(UNIQUE_NUMBER, resultSet);
}

int32_t MediaLibraryAssetOperations::CreateAssetBucket(int32_t fileId, int32_t &bucketNum)
{
    if (fileId < 0) {
        MEDIA_ERR_LOG("input fileId [%{private}d] is invalid", fileId);
        return E_INVALID_FILEID;
    }
    int start = ASSET_DIR_START_NUM;
    int divider = ASSET_DIR_START_NUM;
    while (fileId > start * ASSET_IN_BUCKET_NUM_MAX) {
        divider = start;
        start <<= 1;
    }

    int fileIdRemainder = fileId % divider;
    if (fileIdRemainder == 0) {
        bucketNum = start + fileIdRemainder;
    } else {
        bucketNum = (start - divider) + fileIdRemainder;
    }
    return E_OK;
}

int32_t MediaLibraryAssetOperations::CreateAssetRealName(int32_t fileId, int32_t mediaType,
    const string &extension, string &name)
{
    string fileNumStr = to_string(fileId);
    if (fileId <= ASSET_MAX_COMPLEMENT_ID) {
        size_t fileIdLen = fileNumStr.length();
        fileNumStr = ("00" + fileNumStr).substr(fileIdLen - 1);
    }

    string mediaTypeStr;
    switch (mediaType) {
        case MediaType::MEDIA_TYPE_IMAGE:
            mediaTypeStr = DEFAULT_IMAGE_NAME;
            break;
        case MediaType::MEDIA_TYPE_VIDEO:
            mediaTypeStr = DEFAULT_VIDEO_NAME;
            break;
        case MediaType::MEDIA_TYPE_AUDIO:
            mediaTypeStr = DEFAULT_AUDIO_NAME;
            break;
        default:
            MEDIA_ERR_LOG("This mediatype %{public}d can not get real name", mediaType);
            return E_INVALID_VALUES;
    }

    name = mediaTypeStr + to_string(MediaFileUtils::UTCTimeSeconds()) + "_" + fileNumStr + "." + extension;
    return E_OK;
}

static inline int32_t PrepareAssetDir(const string &dirPath)
{
    CHECK_AND_RETURN_RET(!dirPath.empty(), E_INVALID_PATH);
    if (!MediaFileUtils::IsFileExists(dirPath)) {
        bool ret = MediaFileUtils::CreateDirectory(dirPath);
        CHECK_AND_RETURN_RET_LOG(ret, E_CHECK_DIR_FAIL, "Create Dir Failed! dirPath=%{private}s",
            dirPath.c_str());
    }
    return E_OK;
}

int32_t MediaLibraryAssetOperations::CreateAssetPathById(int32_t fileId, int32_t mediaType,
    const string &extension, string &filePath)
{
    string mediaDirPath;
    GetAssetRootDir(mediaType, mediaDirPath);
    if (mediaDirPath.empty()) {
        return E_INVALID_VALUES;
    }

    int32_t bucketNum = 0;
    int32_t errCode = CreateAssetBucket(fileId, bucketNum);
    if (errCode != E_OK) {
        return errCode;
    }

    string realName;
    errCode = CreateAssetRealName(fileId, mediaType, extension, realName);
    if (errCode != E_OK) {
        return errCode;
    }

    string dirPath = ROOT_MEDIA_DIR + mediaDirPath + to_string(bucketNum);
    errCode = PrepareAssetDir(dirPath);
    if (errCode != E_OK) {
        return errCode;
    }

    filePath = dirPath + "/" + realName;
    return E_OK;
}

const std::unordered_map<std::string, std::vector<VerifyFunction>>
    AssetInputParamVerification::UPDATE_VERIFY_PARAM_MAP = {
    { MediaColumn::MEDIA_ID, { Forbidden } },
    { MediaColumn::MEDIA_FILE_PATH, { Forbidden } },
    { MediaColumn::MEDIA_SIZE, { Forbidden } },
    { MediaColumn::MEDIA_TITLE, { IsStringNotNull } },
    { MediaColumn::MEDIA_NAME, { IsStringNotNull } },
    { MediaColumn::MEDIA_TYPE, { Forbidden } },
    { MediaColumn::MEDIA_MIME_TYPE, { Forbidden } },
    { MediaColumn::MEDIA_OWNER_PACKAGE, { Forbidden } },
    { MediaColumn::MEDIA_PACKAGE_NAME, { Forbidden } },
    { MediaColumn::MEDIA_DEVICE_NAME, { Forbidden } },
    { MediaColumn::MEDIA_DATE_MODIFIED, { Forbidden } },
    { MediaColumn::MEDIA_DATE_ADDED, { Forbidden } },
    { MediaColumn::MEDIA_DATE_TAKEN, { Forbidden } },
    { MediaColumn::MEDIA_TIME_VISIT, { IsInt64 } },
    { MediaColumn::MEDIA_DURATION, { Forbidden } },
    { MediaColumn::MEDIA_TIME_PENDING, { IsInt64, IsUniqueValue } },
    { MediaColumn::MEDIA_IS_FAV, { IsBool, IsUniqueValue } },
    { MediaColumn::MEDIA_DATE_TRASHED, { IsInt64, IsUniqueValue } },
    { MediaColumn::MEDIA_DATE_DELETED, { IsInt64, IsUniqueValue } },
    { MediaColumn::MEDIA_HIDDEN, { IsBool, IsUniqueValue } },
    { MediaColumn::MEDIA_PARENT_ID, { IsInt64, IsBelowApi9 } },
    { MediaColumn::MEDIA_RELATIVE_PATH, { IsString, IsBelowApi9 } },
    { MediaColumn::MEDIA_VIRTURL_PATH, { Forbidden } },
    { PhotoColumn::PHOTO_ORIENTATION, { IsInt64, IsBelowApi9 } },
    { PhotoColumn::PHOTO_LATITUDE, { Forbidden } },
    { PhotoColumn::PHOTO_LONGITUDE, { Forbidden } },
    { PhotoColumn::PHOTO_HEIGHT, { Forbidden } },
    { PhotoColumn::PHOTO_WIDTH, { Forbidden } },
    { PhotoColumn::PHOTO_LCD_VISIT_TIME, { IsInt64 } },
    { AudioColumn::AUDIO_ALBUM, { Forbidden } },
    { AudioColumn::AUDIO_ARTIST, { Forbidden } },
    { PhotoColumn::CAMERA_SHOT_KEY, { Forbidden } },
    { PhotoColumn::PHOTO_USER_COMMENT, { IsString } },
};

bool AssetInputParamVerification::CheckParamForUpdate(MediaLibraryCommand &cmd)
{
    ValuesBucket &values = cmd.GetValueBucket();
    map<string, ValueObject> valuesMap;
    values.GetAll(valuesMap);
    for (auto &iter : valuesMap) {
        if (UPDATE_VERIFY_PARAM_MAP.find(iter.first) == UPDATE_VERIFY_PARAM_MAP.end()) {
            MEDIA_ERR_LOG("param [%{private}s] is not allowed", iter.first.c_str());
            return false;
        }
        for (auto &verifyFunc : UPDATE_VERIFY_PARAM_MAP.at(iter.first)) {
            if (!verifyFunc(iter.second, cmd)) {
                MEDIA_ERR_LOG("verify param [%{private}s] failed", iter.first.c_str());
                return false;
            }
        }
    }
    return true;
}

bool AssetInputParamVerification::Forbidden(ValueObject &value, MediaLibraryCommand &cmd)
{
    return false;
}

bool AssetInputParamVerification::IsInt32(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (value.GetType() == ValueObjectType::TYPE_INT) {
        return true;
    }
    return false;
}

bool AssetInputParamVerification::IsInt64(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (value.GetType() == ValueObjectType::TYPE_INT) {
        return true;
    }
    return false;
}

bool AssetInputParamVerification::IsBool(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (value.GetType() == ValueObjectType::TYPE_BOOL) {
        return true;
    }
    if (value.GetType() == ValueObjectType::TYPE_INT) {
        int32_t ret;
        value.GetInt(ret);
        if (ret == 0 || ret == 1) {
            return true;
        }
    }
    return false;
}

bool AssetInputParamVerification::IsString(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (value.GetType() == ValueObjectType::TYPE_STRING) {
        return true;
    }
    return false;
}

bool AssetInputParamVerification::IsDouble(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (value.GetType() == ValueObjectType::TYPE_DOUBLE) {
        return true;
    }
    return false;
}

bool AssetInputParamVerification::IsBelowApi9(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (cmd.GetApi() == MediaLibraryApi::API_OLD) {
        return true;
    }
    return false;
}

bool AssetInputParamVerification::IsStringNotNull(ValueObject &value, MediaLibraryCommand &cmd)
{
    if (value.GetType() != ValueObjectType::TYPE_STRING) {
        return false;
    }
    string str;
    value.GetString(str);
    if (str.empty()) {
        return false;
    }
    return true;
}

bool AssetInputParamVerification::IsUniqueValue(ValueObject &value, MediaLibraryCommand &cmd)
{
    // whether this is the unique value in ValuesBucket
    map<string, ValueObject> valuesMap;
    cmd.GetValueBucket().GetAll(valuesMap);
    if (valuesMap.size() != 1) {
        return false;
    }
    return true;
}

static void CreateThumbnail(const string &uri, const string &path, bool isSync)
{
    if (ThumbnailService::GetInstance() == nullptr) {
        return;
    }
    if (!uri.empty()) {
        int32_t err = ThumbnailService::GetInstance()->CreateThumbnail(uri, path, isSync);
        if (err != E_SUCCESS) {
            MEDIA_ERR_LOG("ThumbnailService CreateThumbnail failed : %{public}d", err);
        }
    }
}

int32_t MediaLibraryAssetOperations::ScanAssetCallback::OnScanFinished(const int32_t status,
    const string &uri, const string &path)
{
    if (status == E_SCANNED) {
        MEDIA_DEBUG_LOG("Asset is scannned");
        return E_OK;
    } else if (status != E_OK) {
        MEDIA_ERR_LOG("Scan is failed, status = %{public}d, skip create thumbnail", status);
        return status;
    }

    string fileId = MediaLibraryDataManagerUtils::GetIdFromUri(uri);
    int32_t type = MediaFileUtils::GetMediaType(path);
    if (this->isInvalidateThumb) {
        InvalidateThumbnail(fileId, type);
    }
    CreateThumbnail(uri, path, this->isCreateThumbSync);
    return E_OK;
}
} // namespace Media
} // namespace OHOS
