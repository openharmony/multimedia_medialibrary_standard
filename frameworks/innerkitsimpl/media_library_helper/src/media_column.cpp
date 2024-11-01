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

#include "media_column.h"

#include <string>

#include "base_column.h"
#include "userfile_manager_types.h"

namespace OHOS {
namespace Media {
const std::string MediaColumn::MEDIA_ID = "file_id";
const std::string MediaColumn::MEDIA_FILE_PATH = "data";
const std::string MediaColumn::MEDIA_SIZE = "size";
const std::string MediaColumn::MEDIA_TITLE = "title";
const std::string MediaColumn::MEDIA_NAME = "display_name";
const std::string MediaColumn::MEDIA_TYPE = "media_type";
const std::string MediaColumn::MEDIA_MIME_TYPE = "mime_type";
const std::string MediaColumn::MEDIA_OWNER_PACKAGE = "owner_package";
const std::string MediaColumn::MEDIA_PACKAGE_NAME = "package_name";
const std::string MediaColumn::MEDIA_DEVICE_NAME = "device_name";
const std::string MediaColumn::MEDIA_DATE_MODIFIED = "date_modified";
const std::string MediaColumn::MEDIA_DATE_ADDED = "date_added";
const std::string MediaColumn::MEDIA_DATE_TAKEN = "date_taken";
const std::string MediaColumn::MEDIA_TIME_VISIT = "time_visit";
const std::string MediaColumn::MEDIA_DURATION = "duration";
const std::string MediaColumn::MEDIA_TIME_PENDING = "time_pending";
const std::string MediaColumn::MEDIA_IS_FAV = "is_favorite";
const std::string MediaColumn::MEDIA_DATE_TRASHED = "date_trashed";
const std::string MediaColumn::MEDIA_DATE_DELETED = "date_deleted";
const std::string MediaColumn::MEDIA_HIDDEN = "hidden";
const std::string MediaColumn::MEDIA_PARENT_ID = "parent";
const std::string MediaColumn::MEDIA_RELATIVE_PATH = "relative_path";
const std::string MediaColumn::MEDIA_VIRTURL_PATH = "virtual_path";
const std::set<std::string> MediaColumn::MEDIA_COLUMNS = {
    MEDIA_ID, MEDIA_FILE_PATH, MEDIA_SIZE, MEDIA_TITLE, MEDIA_NAME, MEDIA_TYPE, MEDIA_MIME_TYPE,
    MEDIA_OWNER_PACKAGE, MEDIA_PACKAGE_NAME, MEDIA_DEVICE_NAME, MEDIA_DATE_MODIFIED, MEDIA_DATE_ADDED,
    MEDIA_DATE_TAKEN, MEDIA_TIME_VISIT, MEDIA_DURATION, MEDIA_TIME_PENDING, MEDIA_IS_FAV, MEDIA_DATE_TRASHED,
    MEDIA_DATE_DELETED, MEDIA_HIDDEN, MEDIA_PARENT_ID, MEDIA_RELATIVE_PATH, MEDIA_VIRTURL_PATH
};
const std::set<std::string> MediaColumn::DEFAULT_FETCH_COLUMNS = {
    MEDIA_ID, MEDIA_FILE_PATH, MEDIA_NAME, MEDIA_TYPE
};

const std::string PhotoColumn::PHOTO_DIRTY = "dirty";
const std::string PhotoColumn::PHOTO_CLOUD_ID = "cloud_id";
const std::string PhotoColumn::PHOTO_META_DATE_MODIFIED = "meta_date_modified";
const std::string PhotoColumn::PHOTO_SYNC_STATUS = "sync_status";
const std::string PhotoColumn::PHOTO_CLOUD_VERSION = "cloud_version";
const std::string PhotoColumn::PHOTO_ORIENTATION = "orientation";
const std::string PhotoColumn::PHOTO_LATITUDE = "latitude";
const std::string PhotoColumn::PHOTO_LONGITUDE = "longitude";
const std::string PhotoColumn::PHOTO_HEIGHT = "height";
const std::string PhotoColumn::PHOTO_WIDTH = "width";
const std::string PhotoColumn::PHOTO_LCD_VISIT_TIME = "lcd_visit_time";
const std::string PhotoColumn::PHOTO_POSITION = "position";
const std::string PhotoColumn::PHOTO_SUBTYPE = "subtype";
const std::string PhotoColumn::CAMERA_SHOT_KEY = "camera_shot_key";
const std::string PhotoColumn::PHOTO_USER_COMMENT = "user_comment";
const std::string PhotoColumn::PHOTO_ALL_EXIF = "all_exif";
const std::string PhotoColumn::PHOTO_DATE_YEAR = "date_year";
const std::string PhotoColumn::PHOTO_DATE_MONTH = "date_month";
const std::string PhotoColumn::PHOTO_DATE_DAY = "date_day";

const std::string PhotoColumn::PHOTO_DATE_YEAR_INDEX = "date_year_index";
const std::string PhotoColumn::PHOTO_DATE_MONTH_INDEX = "date_month_index";
const std::string PhotoColumn::PHOTO_DATE_DAY_INDEX = "date_day_index";

const std::string PhotoColumn::PHOTO_DATE_YEAR_FORMAT = "%Y";
const std::string PhotoColumn::PHOTO_DATE_MONTH_FORMAT = "%Y%m";
const std::string PhotoColumn::PHOTO_DATE_DAY_FORMAT = "%Y%m%d";


const std::string PhotoColumn::PHOTOS_TABLE = "Photos";

const std::string PhotoColumn::PHOTO_URI_PREFIX = "file://media/Photo/";
const std::string PhotoColumn::DEFAULT_PHOTO_URI = "file://media/Photo";
const std::string PhotoColumn::PHOTO_TYPE_URI = "/Photo";

const std::string PhotoColumn::CREATE_PHOTO_TABLE = "CREATE TABLE IF NOT EXISTS " +
    PHOTOS_TABLE + " (" +
    MEDIA_ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
    MEDIA_FILE_PATH + " TEXT, " +
    MEDIA_SIZE + " BIGINT, " +
    MEDIA_TITLE + " TEXT, " +
    MEDIA_NAME + " TEXT, " +
    MEDIA_TYPE + " INT, " +
    MEDIA_MIME_TYPE + " TEXT, " +
    MEDIA_OWNER_PACKAGE + " TEXT, " +
    MEDIA_PACKAGE_NAME + " TEXT, " +
    MEDIA_DEVICE_NAME + " TEXT, " +
    MEDIA_DATE_ADDED + " BIGINT, " +
    MEDIA_DATE_MODIFIED + " BIGINT, " +
    MEDIA_DATE_TAKEN + " BIGINT DEFAULT 0, " +
    MEDIA_TIME_VISIT + " BIGINT DEFAULT 0, " +
    MEDIA_DURATION + " INT, " +
    MEDIA_TIME_PENDING + " BIGINT DEFAULT 0, " +
    MEDIA_IS_FAV + " INT DEFAULT 0, " +
    MEDIA_DATE_TRASHED + " BIGINT DEFAULT 0, " +
    MEDIA_DATE_DELETED + " BIGINT DEFAULT 0, " +
    MEDIA_HIDDEN + " INT DEFAULT 0, " +
    MEDIA_PARENT_ID + " INT DEFAULT 0, " +
    MEDIA_RELATIVE_PATH + " TEXT, " +
    MEDIA_VIRTURL_PATH + " TEXT UNIQUE, " +
    PHOTO_DIRTY + " INT DEFAULT 1, " +
    PHOTO_CLOUD_ID + " TEXT, " +
    PHOTO_META_DATE_MODIFIED + "  BIGINT DEFAULT 0, " +
    PHOTO_SYNC_STATUS + "  INT DEFAULT 0, " +
    PHOTO_CLOUD_VERSION + " BIGINT DEFAULT 0, " +
    PHOTO_ORIENTATION + " INT DEFAULT 0, " +
    PHOTO_LATITUDE + " DOUBLE DEFAULT 0, " +
    PHOTO_LONGITUDE + " DOUBLE DEFAULT 0, " +
    PHOTO_HEIGHT + " INT, " +
    PHOTO_WIDTH + " INT, " +
    PHOTO_LCD_VISIT_TIME + " BIGINT DEFAULT 0, " +
    PHOTO_POSITION + " INT DEFAULT 1, " +
    PHOTO_SUBTYPE + " INT DEFAULT 0, " +
    CAMERA_SHOT_KEY + " TEXT, " +
    PHOTO_USER_COMMENT + " TEXT, " +
    PHOTO_ALL_EXIF  + " TEXT, " +
    PHOTO_DATE_YEAR + " TEXT, " +
    PHOTO_DATE_MONTH + " TEXT, " +
    PHOTO_DATE_DAY + " TEXT)";


const std::string PhotoColumn::CREATE_YEAR_INDEX = BaseColumn::CreateIndex() +
    PHOTO_DATE_YEAR_INDEX + " ON " + PHOTOS_TABLE + " (" + PHOTO_DATE_YEAR + " DESC)";

const std::string PhotoColumn::CREATE_MONTH_INDEX = BaseColumn::CreateIndex() +
    PHOTO_DATE_MONTH_INDEX + " ON " + PHOTOS_TABLE + " (" + PHOTO_DATE_MONTH + " DESC)";

const std::string PhotoColumn::CREATE_DAY_INDEX = BaseColumn::CreateIndex() +
    PHOTO_DATE_DAY_INDEX + " ON " + PHOTOS_TABLE + " (" + PHOTO_DATE_DAY + " DESC)";

const std::string PhotoColumn::QUERY_MEDIA_VOLUME = "SELECT sum(" + MediaColumn::MEDIA_SIZE + ") AS " +
    MediaColumn::MEDIA_SIZE + "," +
    MediaColumn::MEDIA_TYPE + " FROM " +
    PhotoColumn::PHOTOS_TABLE + " WHERE " +
    MediaColumn::MEDIA_TYPE + " = " + std::to_string(MEDIA_TYPE_IMAGE) + " OR " +
    MediaColumn::MEDIA_TYPE + " = " + std::to_string(MEDIA_TYPE_VIDEO) + " GROUP BY " +
    MediaColumn::MEDIA_TYPE;

// Create indexes
const std::string PhotoColumn::INDEX_STHP_ADDTIME =
    BaseColumn::CreateIndex() + "idx_sthp_dateadded" + " ON " + PHOTOS_TABLE +
    " (" + PHOTO_SYNC_STATUS + "," + MEDIA_DATE_TRASHED + "," + MEDIA_HIDDEN +
    "," + MEDIA_TIME_PENDING + "," + MEDIA_DATE_ADDED + ");";

const std::string PhotoColumn::INDEX_CAMERA_SHOT_KEY =
    BaseColumn::CreateIndex() + "idx_camera_shot_key" + " ON " + PHOTOS_TABLE +
    " (" + CAMERA_SHOT_KEY + ");";

const std::string PhotoColumn::CREATE_PHOTOS_DELETE_TRIGGER =
                        "CREATE TRIGGER photos_delete_trigger AFTER UPDATE ON " +
                        PhotoColumn::PHOTOS_TABLE + " FOR EACH ROW WHEN new." + PhotoColumn::PHOTO_DIRTY +
                        " = " + std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_DELETED)) +
                        " AND OLD." + PhotoColumn::PHOTO_CLOUD_ID + " is NULL AND is_caller_self_func() = 'true'" +
                        " BEGIN DELETE FROM " + PhotoColumn::PHOTOS_TABLE +
                        " WHERE " + PhotoColumn::MEDIA_ID + " = old." + PhotoColumn::MEDIA_ID + "; END;";

const std::string PhotoColumn::CREATE_PHOTOS_FDIRTY_TRIGGER =
                        "CREATE TRIGGER photos_fdirty_trigger AFTER UPDATE ON " +
                        PhotoColumn::PHOTOS_TABLE + " FOR EACH ROW WHEN OLD.cloud_id IS NOT NULL AND" +
                        " new.date_modified <> old.date_modified " +
                        " AND new.dirty = old.dirty AND is_caller_self_func() = 'true'" +
                        " BEGIN " +
                        " UPDATE " + PhotoColumn::PHOTOS_TABLE + " SET dirty = " +
                        std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_FDIRTY)) +
                        " WHERE file_id = old.file_id;" +
                        " SELECT cloud_sync_func(); " +
                        " END;";

const std::string PhotoColumn::CREATE_PHOTOS_MDIRTY_TRIGGER =
                        "CREATE TRIGGER photos_mdirty_trigger AFTER UPDATE ON " +
                        PhotoColumn::PHOTOS_TABLE + " FOR EACH ROW WHEN OLD.cloud_id IS NOT NULL" +
                        " AND new.date_modified = old.date_modified AND old.dirty = " +
                        std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_SYNCED)) +
                        " AND new.time_visit = old.time_visit " +
                        " AND new.dirty = old.dirty AND is_caller_self_func() = 'true'" +
                        " BEGIN " +
                        " UPDATE " + PhotoColumn::PHOTOS_TABLE + " SET dirty = " +
                        std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_MDIRTY)) +
                        " WHERE file_id = old.file_id;" +
                        " SELECT cloud_sync_func(); " +
                        " END;";

const std::string  PhotoColumn::CREATE_PHOTOS_INSERT_CLOUD_SYNC =
                        " CREATE TRIGGER photo_insert_cloud_sync_trigger AFTER INSERT ON " + PhotoColumn::PHOTOS_TABLE +
                        " BEGIN SELECT cloud_sync_func(); END;";

const std::string PhotoColumn::CREATE_PHOTOS_UPDATE_CLOUD_SYNC =
                        " CREATE TRIGGER photo_update_cloud_sync_trigger AFTER UPDATE ON " + PhotoColumn::PHOTOS_TABLE +
                        " FOR EACH ROW WHEN OLD.dirty IN (1,2,3,5) AND new.dirty != " +
                        std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_SYNCED)) +
                        " BEGIN SELECT cloud_sync_func(); END;";

const std::set<std::string> PhotoColumn::PHOTO_COLUMNS = {
    PhotoColumn::PHOTO_ORIENTATION, PhotoColumn::PHOTO_LATITUDE, PhotoColumn::PHOTO_LONGITUDE,
    PhotoColumn::PHOTO_HEIGHT, PhotoColumn::PHOTO_WIDTH, PhotoColumn::PHOTO_LCD_VISIT_TIME, PhotoColumn::PHOTO_POSITION,
    PhotoColumn::PHOTO_DIRTY, PhotoColumn::PHOTO_CLOUD_ID, PhotoColumn::CAMERA_SHOT_KEY, PhotoColumn::PHOTO_ALL_EXIF,
    PhotoColumn::PHOTO_USER_COMMENT, PhotoColumn::PHOTO_DATE_YEAR, PhotoColumn::PHOTO_DATE_MONTH,
    PhotoColumn::PHOTO_DATE_DAY
};

bool PhotoColumn::IsPhotoColumn(const std::string &columnName)
{
    if (columnName == "count(*)") {
        return true;
    }
    return (PHOTO_COLUMNS.find(columnName) != PHOTO_COLUMNS.end()) ||
        (MEDIA_COLUMNS.find(columnName) != MEDIA_COLUMNS.end());
}

const std::string AudioColumn::AUDIO_ALBUM = "audio_album";
const std::string AudioColumn::AUDIO_ARTIST = "artist";

const std::string AudioColumn::AUDIOS_TABLE = "Audios";

const std::string AudioColumn::AUDIO_URI_PREFIX = "file://media/Audio/";
const std::string AudioColumn::DEFAULT_AUDIO_URI = "file://media/Audio";
const std::string AudioColumn::AUDIO_TYPE_URI = "/Audio";

const std::string AudioColumn::CREATE_AUDIO_TABLE = "CREATE TABLE IF NOT EXISTS " +
    AUDIOS_TABLE + " (" +
    MEDIA_ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
    MEDIA_FILE_PATH + " TEXT, " +
    MEDIA_SIZE + " BIGINT, " +
    MEDIA_TITLE + " TEXT, " +
    MEDIA_NAME + " TEXT, " +
    MEDIA_TYPE + " INT, " +
    MEDIA_MIME_TYPE + " TEXT, " +
    MEDIA_OWNER_PACKAGE + " TEXT, " +
    MEDIA_PACKAGE_NAME + " TEXT, " +
    MEDIA_DEVICE_NAME + " TEXT, " +
    AUDIO_ARTIST + " TEXT, " +
    MEDIA_DATE_ADDED + " BIGINT, " +
    MEDIA_DATE_MODIFIED + " BIGINT, " +
    MEDIA_DATE_TAKEN + " BIGINT DEFAULT 0, " +
    MEDIA_TIME_VISIT + " BIGINT DEFAULT 0, " +
    MEDIA_DURATION + " INT, " +
    MEDIA_TIME_PENDING + " BIGINT DEFAULT 0, " +
    MEDIA_IS_FAV + " INT DEFAULT 0, " +
    MEDIA_DATE_TRASHED + " BIGINT DEFAULT 0, " +
    MEDIA_DATE_DELETED + " BIGINT DEFAULT 0, " +
    MEDIA_PARENT_ID + " INT DEFAULT 0, " +
    MEDIA_RELATIVE_PATH + " TEXT, " +
    MEDIA_VIRTURL_PATH + " TEXT UNIQUE, " +
    AUDIO_ALBUM + " TEXT)";

const std::string AudioColumn::QUERY_MEDIA_VOLUME = "SELECT sum(" + MediaColumn::MEDIA_SIZE + ") AS " +
    MediaColumn::MEDIA_SIZE + "," +
    MediaColumn::MEDIA_TYPE + " FROM " +
    AudioColumn::AUDIOS_TABLE + " WHERE " +
    MediaColumn::MEDIA_TYPE + " = " + std::to_string(MEDIA_TYPE_AUDIO) + " GROUP BY " +
    MediaColumn::MEDIA_TYPE;

const std::set<std::string> AudioColumn::AUDIO_COLUMNS = {
    AudioColumn::AUDIO_ALBUM, AudioColumn::AUDIO_ARTIST
};

bool AudioColumn::IsAudioColumn(const std::string &columnName)
{
    return (AUDIO_COLUMNS.find(columnName) != AUDIO_COLUMNS.end()) ||
        (MEDIA_COLUMNS.find(columnName) != MEDIA_COLUMNS.end());
}

const std::string MediaColumn::ASSETS_QUERY_FILTER =
    PhotoColumn::PHOTO_SYNC_STATUS + " = 0" + " AND " +
    MediaColumn::MEDIA_DATE_TRASHED + " = 0" + " AND " +
    MediaColumn::MEDIA_HIDDEN + " = 0" + " AND " +
    MediaColumn::MEDIA_TIME_PENDING + " = 0";
}  // namespace Media
}  // namespace OHOS
