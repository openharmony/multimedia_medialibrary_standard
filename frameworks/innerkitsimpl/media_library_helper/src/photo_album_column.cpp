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

#include "photo_album_column.h"

#include "media_column.h"
#include "photo_map_column.h"

namespace OHOS::Media {
// PhotoAlbum table
const std::string PhotoAlbumColumns::TABLE = "PhotoAlbum";
const std::string PhotoAlbumColumns::ALBUM_ID = "album_id";
const std::string PhotoAlbumColumns::ALBUM_TYPE = "album_type";
const std::string PhotoAlbumColumns::ALBUM_SUBTYPE = "album_subtype";
const std::string PhotoAlbumColumns::ALBUM_NAME = "album_name";
const std::string PhotoAlbumColumns::ALBUM_COVER_URI = "cover_uri";
const std::string PhotoAlbumColumns::ALBUM_COUNT = "count";
const std::string PhotoAlbumColumns::ALBUM_DATE_MODIFIED = "date_modified";
const std::string PhotoAlbumColumns::ALBUM_DIRTY = "dirty";
const std::string PhotoAlbumColumns::ALBUM_CLOUD_ID = "cloud_id";
// For api9 compatibility
const std::string PhotoAlbumColumns::ALBUM_RELATIVE_PATH = "relative_path";
// default fetch columns
const std::set<std::string> PhotoAlbumColumns::DEFAULT_FETCH_COLUMNS = {
    ALBUM_ID, ALBUM_TYPE, ALBUM_SUBTYPE, ALBUM_NAME, ALBUM_COVER_URI, ALBUM_COUNT
};

const std::string PhotoAlbumColumns::ALBUM_URI_PREFIX = "file://media/PhotoAlbum/";
const std::string PhotoAlbumColumns::DEFAULT_PHOTO_ALBUM_URI = "file://media/PhotoAlbum";

// Create tables
const std::string PhotoAlbumColumns::CREATE_TABLE = CreateTable() +
    TABLE + " (" +
    ALBUM_ID + " INTEGER PRIMARY KEY AUTOINCREMENT, " +
    ALBUM_TYPE + " INT, " +
    ALBUM_SUBTYPE + " INT, " +
    ALBUM_NAME + " TEXT COLLATE NOCASE, " +
    ALBUM_COVER_URI + " TEXT, " +
    ALBUM_COUNT + " INT DEFAULT 0, " +
    ALBUM_DATE_MODIFIED + " BIGINT DEFAULT 0, " +
    ALBUM_DIRTY + " INT DEFAULT " + std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_NEW)) + ", " +
    ALBUM_CLOUD_ID + " TEXT, " +
    ALBUM_RELATIVE_PATH + " TEXT)";

// Create indexes
const std::string PhotoAlbumColumns::INDEX_ALBUM_TYPES = CreateIndex() + "photo_album_types" + " ON " + TABLE +
    " (" + ALBUM_TYPE + "," + ALBUM_SUBTYPE + ");";

// Create triggers
const std::string PhotoAlbumColumns::CREATE_ALBUM_INSERT_TRIGGER =
    " CREATE TRIGGER album_insert_cloud_sync_trigger AFTER INSERT ON " + TABLE +
    " BEGIN SELECT cloud_sync_func(); END;";

const std::string PhotoAlbumColumns::CREATE_ALBUM_DELETE_TRIGGER =
    "CREATE TRIGGER album_delete_trigger AFTER UPDATE ON " + TABLE +
    " FOR EACH ROW WHEN new." + ALBUM_DIRTY + " = " +
    std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_DELETED)) +
    " AND old." + ALBUM_DIRTY + " = " + std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_NEW)) +
    " AND is_caller_self_func() = 'true' BEGIN DELETE FROM " + TABLE +
    " WHERE " + ALBUM_ID + " = old." + ALBUM_ID + "; SELECT cloud_sync_func(); END;";

const std::string PhotoAlbumColumns::CREATE_ALBUM_MDIRTY_TRIGGER =
    "CREATE TRIGGER album_modify_trigger AFTER UPDATE ON " + TABLE +
    " FOR EACH ROW WHEN old." + ALBUM_DIRTY + " = " +
    std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_SYNCED)) +
    " AND old." + ALBUM_DIRTY + " = " + "new." + ALBUM_DIRTY +
    " AND is_caller_self_func() = 'true'" +
    " BEGIN UPDATE " + TABLE + " SET dirty = " +
    std::to_string(static_cast<int32_t>(DirtyTypes::TYPE_MDIRTY)) +
    " WHERE " + ALBUM_ID + " = old." + ALBUM_ID + "; SELECT cloud_sync_func(); END;";

bool PhotoAlbumColumns::IsPhotoAlbumColumn(const std::string &columnName)
{
    static const std::set<std::string> PHOTO_ALBUM_COLUMNS = {
        PhotoAlbumColumns::ALBUM_ID, PhotoAlbumColumns::ALBUM_TYPE, PhotoAlbumColumns::ALBUM_SUBTYPE,
        PhotoAlbumColumns::ALBUM_NAME, PhotoAlbumColumns::ALBUM_COVER_URI, PhotoAlbumColumns::ALBUM_COUNT,
        PhotoAlbumColumns::ALBUM_RELATIVE_PATH
    };
    return PHOTO_ALBUM_COLUMNS.find(columnName) != PHOTO_ALBUM_COLUMNS.end();
}
} // namespace OHOS::Media
