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

#ifndef INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_COLUMN_H_
#define INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_COLUMN_H_

#include <set>
#include <string>

namespace OHOS::Media {

enum class DirtyTypes : int32_t {
    TYPE_SYNCED,
    TYPE_NEW,
    TYPE_MDIRTY,
    TYPE_FDIRTY,
    TYPE_DELETED,
    TYPE_RETRY
};

class MediaColumn {
public:
    // Asset Base Parameter
    static const std::string MEDIA_ID;
    static const std::string MEDIA_FILE_PATH;
    static const std::string MEDIA_SIZE;
    static const std::string MEDIA_TITLE;
    static const std::string MEDIA_NAME;
    static const std::string MEDIA_TYPE;
    static const std::string MEDIA_MIME_TYPE;
    static const std::string MEDIA_OWNER_PACKAGE;
    static const std::string MEDIA_PACKAGE_NAME;
    static const std::string MEDIA_DEVICE_NAME;

    // As set Parameter about time
    static const std::string MEDIA_DATE_MODIFIED;
    static const std::string MEDIA_DATE_ADDED;
    static const std::string MEDIA_DATE_TAKEN;
    static const std::string MEDIA_TIME_VISIT;
    static const std::string MEDIA_DURATION;
    static const std::string MEDIA_TIME_PENDING;
    static const std::string MEDIA_IS_FAV;
    static const std::string MEDIA_DATE_TRASHED;
    static const std::string MEDIA_DATE_DELETED;
    static const std::string MEDIA_HIDDEN;

    // Asset Parameter deperated
    static const std::string MEDIA_PARENT_ID;
    static const std::string MEDIA_RELATIVE_PATH;
    static const std::string MEDIA_VIRTURL_PATH;

    // All Columns
    static const std::set<std::string> MEDIA_COLUMNS;
    // Default fetch columns
    static const std::set<std::string> DEFAULT_FETCH_COLUMNS;

    // Util consts
    static const std::string ASSETS_QUERY_FILTER;
};

class PhotoColumn : public MediaColumn {
public:
    // column only in PhotoTable
    static const std::string PHOTO_ORIENTATION;
    static const std::string PHOTO_LATITUDE;
    static const std::string PHOTO_LONGITUDE;
    static const std::string PHOTO_HEIGHT;
    static const std::string PHOTO_WIDTH;
    static const std::string PHOTO_LCD_VISIT_TIME;
    static const std::string PHOTO_POSITION;
    static const std::string PHOTO_DIRTY;
    static const std::string PHOTO_CLOUD_ID;
    static const std::string PHOTO_SUBTYPE;
    static const std::string PHOTO_META_DATE_MODIFIED;
    static const std::string PHOTO_SYNC_STATUS;
    static const std::string PHOTO_CLOUD_VERSION;
    static const std::string CAMERA_SHOT_KEY;
    static const std::string PHOTO_USER_COMMENT;
    static const std::string PHOTO_ALL_EXIF;

    static const std::string PHOTO_SYNCING;
    static const std::string PHOTO_DATE_YEAR;
    static const std::string PHOTO_DATE_MONTH;
    static const std::string PHOTO_DATE_DAY;
    // index in PhotoTable
    static const std::string PHOTO_DATE_YEAR_INDEX;
    static const std::string PHOTO_DATE_MONTH_INDEX;
    static const std::string PHOTO_DATE_DAY_INDEX;
    // format in PhotoTable year month day
    static const std::string PHOTO_DATE_YEAR_FORMAT;
    static const std::string PHOTO_DATE_MONTH_FORMAT;
    static const std::string PHOTO_DATE_DAY_FORMAT;
    // table name
    static const std::string PHOTOS_TABLE;

    // create PhotoTable sql
    static const std::string CREATE_PHOTO_TABLE;
    static const std::string CREATE_YEAR_INDEX;
    static const std::string CREATE_MONTH_INDEX;
    static const std::string CREATE_DAY_INDEX;

    // create indexes for Photo
    static const std::string INDEX_STHP_ADDTIME;
    static const std::string INDEX_CAMERA_SHOT_KEY;

    // create Photo cloud sync trigger
    static const std::string CREATE_PHOTOS_DELETE_TRIGGER;
    static const std::string CREATE_PHOTOS_FDIRTY_TRIGGER;
    static const std::string CREATE_PHOTOS_MDIRTY_TRIGGER;
    static const std::string CREATE_PHOTOS_INSERT_CLOUD_SYNC;
    static const std::string CREATE_PHOTOS_UPDATE_CLOUD_SYNC;

    // photo uri
    static const std::string PHOTO_URI_PREFIX;
    static const std::string PHOTO_TYPE_URI;
    static const std::string DEFAULT_PHOTO_URI;

    // all columns
    static const std::set<std::string> PHOTO_COLUMNS;

    static const std::string QUERY_MEDIA_VOLUME;

    static bool IsPhotoColumn(const std::string &columnName);
};

class AudioColumn : public MediaColumn {
public:
    // column only in AudioTable
    static const std::string AUDIO_ALBUM;
    static const std::string AUDIO_ARTIST;

    // table name
    static const std::string AUDIOS_TABLE;

    // create AudioTable sql
    static const std::string CREATE_AUDIO_TABLE;

    // audio uri
    static const std::string AUDIO_URI_PREFIX;
    static const std::string AUDIO_TYPE_URI;
    static const std::string DEFAULT_AUDIO_URI;

    // all columns
    static const std::set<std::string> AUDIO_COLUMNS;

    static const std::string QUERY_MEDIA_VOLUME;

    static bool IsAudioColumn(const std::string &columnName);
};
} // namespace OHOS::Media
#endif // INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_COLUMN_H_
