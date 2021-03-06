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

#include <grp.h>
#include <securec.h>
#include <unistd.h>

#include "medialibrary_album_operations.h"

using namespace std;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {
void MediaLibraryAlbumOperations::ChangeGroupToMedia(const string &path)
{
    uid_t usrId;
    gid_t grpId;
    struct group *grp = nullptr;

    usrId = getuid();

    grp = getgrnam("media_rw");
    if (grp == nullptr) {
        MEDIA_ERR_LOG("Failed to obtain the group information");
        return;
    }
    grpId = grp->gr_gid;

    if (chown(path.c_str(), usrId, grpId) == -1) {
        MEDIA_ERR_LOG("chown failed for the given path");
    }
}

int32_t InsertAlbumInfoUtil(const ValuesBucket &valuesBucket, const string &albumPath, shared_ptr<RdbStore> rdbStore,
    const MediaLibraryAlbumDb &albumDbOprn)
{
    ValuesBucket values = const_cast<ValuesBucket &>(valuesBucket);

    string albumName;
    size_t index = albumPath.rfind('/');
    if (index != string::npos) {
        albumName = albumPath.substr(index + 1);
        values.PutString(Media::MEDIA_DATA_DB_RELATIVE_PATH, albumPath.substr(0, index));
    }

    values.PutString(Media::MEDIA_DATA_DB_ALBUM_NAME, albumName);
    if ((!albumName.empty()) && (albumName.at(0) == '.')) {
        return 0;
    }

    values.PutInt(Media::MEDIA_DATA_DB_MEDIA_TYPE, MediaType::MEDIA_TYPE_ALBUM);

    struct stat statInfo {};
    if (stat(albumPath.c_str(), &statInfo) == 0) {
        values.PutLong(MEDIA_DATA_DB_DATE_MODIFIED, statInfo.st_mtime);
    }

    return const_cast<MediaLibraryAlbumDb &>(albumDbOprn).InsertAlbumInfo(values, rdbStore);
}

int32_t UpdateAlbumInfoUtil(const ValuesBucket &valuesBucket, const string &albumPath,
    const string &albumNewName, shared_ptr<RdbStore> rdbStore, const MediaLibraryAlbumDb &albumDbOprn)
{
    int32_t retVal = DATA_ABILITY_FAIL;
    ValuesBucket values = const_cast<ValuesBucket &>(valuesBucket);
    string newAlbumPath;

    if ((rdbStore == nullptr) || (albumPath.empty()) || (albumNewName.empty())) {
        return retVal;
    }

    if (albumNewName.at(0) == '.') {
        int32_t deletedRows = ALBUM_OPERATION_ERR;
        vector<string> whereArgs = { (albumPath.back() != '/' ? (albumPath + "/%") : (albumPath + "%")), albumPath };

        int32_t deleteResult = rdbStore->Delete(deletedRows, MEDIALIBRARY_TABLE, "path LIKE ? OR path = ?", whereArgs);
        if (deleteResult != E_OK) {
            MEDIA_ERR_LOG("Delete rows failed");
        }
        return DATA_ABILITY_SUCCESS;
    }

    size_t slashIndex = albumPath.rfind("/");
    if (slashIndex != string::npos) {
        newAlbumPath = albumPath.substr(0, slashIndex) + "/" + albumNewName;
        values.PutString(Media::MEDIA_DATA_DB_RELATIVE_PATH, albumPath.substr(0, slashIndex));
    }

    values.PutString(Media::MEDIA_DATA_DB_FILE_PATH, newAlbumPath);

    struct stat statInfo {};
    if (stat(newAlbumPath.c_str(), &statInfo) == 0) {
        values.PutLong(MEDIA_DATA_DB_DATE_MODIFIED, statInfo.st_mtime);
    }

    retVal = const_cast<MediaLibraryAlbumDb &>(albumDbOprn).UpdateAlbumInfo(values, rdbStore);
    if ((retVal == DATA_ABILITY_SUCCESS) && (!newAlbumPath.empty())) {
        // Update the path, relative path and album Name for internal files
        const std::string modifyAlbumInternalsStmt =
            "UPDATE MEDIALIBRARY_DATA SET path = replace(path, '" + albumPath + "/' , '" + newAlbumPath + "/'), "
            + "relative_path = replace(relative_path, '" + albumPath + "', '" + newAlbumPath + "'), "
            + "album_name = replace(album_name, '" + albumPath.substr(slashIndex + 1) + "', '" + albumNewName + "')"
            + "where path LIKE '" + albumPath + "/%'";

        auto ret = rdbStore->ExecuteSql(modifyAlbumInternalsStmt);
        CHECK_AND_PRINT_LOG(ret == 0, "Album update sql failed");
    }

    return retVal;
}

int32_t DeleteAlbumInfoUtil(const ValuesBucket &valuesBucket, int32_t albumId, const string &albumPath,
    shared_ptr<RdbStore> rdbStore, const MediaLibraryAlbumDb &albumDbOprn)
{
    int32_t retVal;
    ValuesBucket values = const_cast<ValuesBucket &>(valuesBucket);

    retVal = const_cast<MediaLibraryAlbumDb &>(albumDbOprn).DeleteAlbumInfo(albumId, rdbStore);
    if ((retVal == DATA_ABILITY_SUCCESS) && (rdbStore != nullptr) && (!albumPath.empty())) {
        int32_t deletedRows = ALBUM_OPERATION_ERR;
        vector<string> whereArgs = { (albumPath.back() != '/' ? (albumPath + "/%") : (albumPath + "%")) };

        int32_t deleteResult = rdbStore->Delete(deletedRows, MEDIALIBRARY_TABLE, "path LIKE ?", whereArgs);
        if (deleteResult != E_OK) {
            MEDIA_ERR_LOG("Delete rows failed");
        }
    }

    return retVal;
}

int32_t MediaLibraryAlbumOperations::HandleAlbumOperations(const string &oprn, const ValuesBucket &valuesBucket,
    const shared_ptr<RdbStore> &rdbStore)
{
    ValuesBucket values = const_cast<ValuesBucket &>(valuesBucket);
    AlbumAsset albumAsset;
    MediaLibraryAlbumDb albumDbOprn;
    int32_t errCode = DATA_ABILITY_FAIL;
    ValueObject valueObject;

    if (oprn == MEDIA_ALBUMOPRN_CREATEALBUM) {
        string albumPath;
        values.GetObject(MEDIA_DATA_DB_FILE_PATH, valueObject);
        valueObject.GetString(albumPath);
        CHECK_AND_RETURN_RET_LOG(!albumPath.empty(), DATA_ABILITY_FAIL, "Path is empty");

        albumAsset.SetAlbumPath(albumPath);

        if (albumAsset.CreateAlbumAsset() == true) {
            ChangeGroupToMedia(albumPath);
            errCode = InsertAlbumInfoUtil(values, albumPath, rdbStore, albumDbOprn);
        }
    } else if (oprn == MEDIA_ALBUMOPRN_MODIFYALBUM) {
        int32_t albumId;
        values.GetObject(MEDIA_DATA_DB_ID, valueObject);
        valueObject.GetInt(albumId);

        string albumNewName;
        values.GetObject(MEDIA_DATA_DB_ALBUM_NAME, valueObject);
        valueObject.GetString(albumNewName);

        albumAsset.SetAlbumName(albumNewName);

        string albumPath = MediaLibraryDataAbilityUtils::GetPathFromDb(to_string(albumId), rdbStore);
        if (albumAsset.ModifyAlbumAsset(albumPath) == true) {
            errCode = UpdateAlbumInfoUtil(values, albumPath, albumNewName, rdbStore, albumDbOprn);
        }
    } else if (oprn == MEDIA_ALBUMOPRN_DELETEALBUM) {
        int32_t albumId = 0;
        values.GetObject(MEDIA_DATA_DB_ID, valueObject);
        valueObject.GetInt(albumId);

        string albumPath = MediaLibraryDataAbilityUtils::GetPathFromDb(to_string(albumId), rdbStore);
        if (albumAsset.DeleteAlbumAsset(albumPath) == true) {
            errCode = DeleteAlbumInfoUtil(values, albumId, albumPath, rdbStore, albumDbOprn);
        }
    }

    return errCode;
}
} // namespace Media
} // namespace OHOS