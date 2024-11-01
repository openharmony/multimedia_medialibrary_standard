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
#include "media_log.h"
#include "medialibrary_type_const.h"
#include "photo_map_column.h"
#include <string>

namespace OHOS::Media {
using namespace std;
using namespace NativeRdb;

// PhotoAlbum table
const string PhotoAlbumColumns::TABLE = "PhotoAlbum";
const string PhotoAlbumColumns::ALBUM_ID = "album_id";
const string PhotoAlbumColumns::ALBUM_TYPE = "album_type";
const string PhotoAlbumColumns::ALBUM_SUBTYPE = "album_subtype";
const string PhotoAlbumColumns::ALBUM_NAME = "album_name";
const string PhotoAlbumColumns::ALBUM_COVER_URI = "cover_uri";
const string PhotoAlbumColumns::ALBUM_COUNT = "count";
const string PhotoAlbumColumns::ALBUM_DATE_MODIFIED = "date_modified";
const string PhotoAlbumColumns::ALBUM_DIRTY = "dirty";
const string PhotoAlbumColumns::ALBUM_CLOUD_ID = "cloud_id";
// For api9 compatibility
const string PhotoAlbumColumns::ALBUM_RELATIVE_PATH = "relative_path";
// default fetch columns
const set<string> PhotoAlbumColumns::DEFAULT_FETCH_COLUMNS = {
    ALBUM_ID, ALBUM_TYPE, ALBUM_SUBTYPE, ALBUM_NAME, ALBUM_COVER_URI, ALBUM_COUNT, ALBUM_DATE_MODIFIED
};

const string PhotoAlbumColumns::ALBUM_URI_PREFIX = "file://media/PhotoAlbum/";
const string PhotoAlbumColumns::DEFAULT_PHOTO_ALBUM_URI = "file://media/PhotoAlbum";

// Create tables
const string PhotoAlbumColumns::CREATE_TABLE = CreateTable() +
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
const string PhotoAlbumColumns::INDEX_ALBUM_TYPES = CreateIndex() + "photo_album_types" + " ON " + TABLE +
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

bool PhotoAlbumColumns::IsPhotoAlbumColumn(const string &columnName)
{
    static const set<string> PHOTO_ALBUM_COLUMNS = {
        PhotoAlbumColumns::ALBUM_ID, PhotoAlbumColumns::ALBUM_TYPE, PhotoAlbumColumns::ALBUM_SUBTYPE,
        PhotoAlbumColumns::ALBUM_NAME, PhotoAlbumColumns::ALBUM_COVER_URI, PhotoAlbumColumns::ALBUM_COUNT,
        PhotoAlbumColumns::ALBUM_RELATIVE_PATH
    };
    return PHOTO_ALBUM_COLUMNS.find(columnName) != PHOTO_ALBUM_COLUMNS.end();
}

void PhotoAlbumColumns::GetUserAlbumPredicates(const int32_t albumId, RdbPredicates &predicates)
{
    string onClause = MediaColumn::MEDIA_ID + " = " + PhotoMap::ASSET_ID;
    predicates.InnerJoin(PhotoMap::TABLE)->On({ onClause });
    predicates.EqualTo(PhotoMap::ALBUM_ID, to_string(albumId));
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
}

static void GetFavoritePredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    constexpr int32_t isFavorite = 1;
    predicates.EqualTo(MediaColumn::MEDIA_IS_FAV, to_string(isFavorite));
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.And()->EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.And()->EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
    predicates.EndWrap();
}

static void GetVideoPredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    predicates.EqualTo(MediaColumn::MEDIA_TYPE, to_string(MEDIA_TYPE_VIDEO));
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.And()->EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.And()->EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
    predicates.EndWrap();
}

static void GetHiddenPredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    constexpr int32_t isHidden = 1;
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.And()->EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(isHidden));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
    predicates.EndWrap();
}

static void GetTrashPredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.GreaterThan(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.EndWrap();
}

static void GetScreenshotPredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    predicates.EqualTo(PhotoColumn::PHOTO_SUBTYPE, to_string(static_cast<int32_t>(PhotoSubType::SCREENSHOT)));
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.And()->EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.And()->EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
    predicates.EndWrap();
}

static void GetCameraPredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    predicates.EqualTo(PhotoColumn::PHOTO_SUBTYPE, to_string(static_cast<int32_t>(PhotoSubType::CAMERA)));
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.And()->EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.And()->EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
    predicates.EndWrap();
}

static void GetAllImagesPredicates(RdbPredicates &predicates)
{
    predicates.BeginWrap();
    predicates.EqualTo(MediaColumn::MEDIA_TYPE, to_string(MEDIA_TYPE_IMAGE));
    predicates.EqualTo(PhotoColumn::PHOTO_SYNC_STATUS, to_string(static_cast<int32_t>(SyncStatusType::TYPE_VISIBLE)));
    predicates.And()->EqualTo(MediaColumn::MEDIA_DATE_TRASHED, to_string(0));
    predicates.And()->EqualTo(MediaColumn::MEDIA_HIDDEN, to_string(0));
    predicates.EqualTo(MediaColumn::MEDIA_TIME_PENDING, to_string(0));
    predicates.EndWrap();
}

void PhotoAlbumColumns::GetSystemAlbumPredicates(const PhotoAlbumSubType subtype, RdbPredicates &predicates)
{
    switch (subtype) {
        case PhotoAlbumSubType::FAVORITE: {
            return GetFavoritePredicates(predicates);
        }
        case PhotoAlbumSubType::VIDEO: {
            return GetVideoPredicates(predicates);
        }
        case PhotoAlbumSubType::HIDDEN: {
            return GetHiddenPredicates(predicates);
        }
        case PhotoAlbumSubType::TRASH: {
            return GetTrashPredicates(predicates);
        }
        case PhotoAlbumSubType::SCREENSHOT: {
            return GetScreenshotPredicates(predicates);
        }
        case PhotoAlbumSubType::CAMERA: {
            return GetCameraPredicates(predicates);
        }
        case PhotoAlbumSubType::IMAGES: {
            return GetAllImagesPredicates(predicates);
        }
        default: {
            predicates.EqualTo(PhotoColumn::MEDIA_ID, to_string(0));
            MEDIA_WARN_LOG("Unsupported system album subtype: %{public}d", subtype);
            return;
        }
    }
}
} // namespace OHOS::Media
