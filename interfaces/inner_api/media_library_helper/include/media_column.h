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

#include <string>

namespace OHOS::Media {

class MediaColumn {
public:
    // Asset Base Parameter
    static const std::string MEDIA_ID;
    static const std::string MEDIA_URI;
    static const std::string MEDIA_FILE_PATH;
    static const std::string MEDIA_SIZE;
    static const std::string MEDIA_TITLE;
    static const std::string MEDIA_NAME;
    static const std::string MEDIA_TYPE;
    static const std::string MEDIA_MIME_TYPE;
    static const std::string MEDIA_OWNER_PACKAGE;
    static const std::string MEDIA_DEVICE_NAME;
    static const std::string MEDIA_THUMBNAIL;

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
};

class PhotoColumn : public MediaColumn {
public:
    // column only in PhotoTable
    static const std::string PHOTO_ORIENTATION;
    static const std::string PHOTO_LATITUDE;
    static const std::string PHOTO_LONGITUDE;
    static const std::string PHOTO_LCD;
    static const std::string PHOTO_HEIGHT;
    static const std::string PHOTO_WIDTH;
    static const std::string PHOTO_LCD_VISIT_TIME;
    static const std::string PHOTO_POSITION;
    static const std::string PHOTO_DIRTY;
    static const std::string PHOTO_CLOUD_ID;

    // table name
    static const std::string PHOTOS_TABLE;

    // create PhotoTable sql
    static const std::string CREATE_PHOTO_TABLE;
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
};

class DocumentColumn : public MediaColumn {
public:
    // column only in DocumentColumn
    static const std::string DOCUMENT_LCD;
    static const std::string DOCUMENT_LCD_VISIT_TIME;

    // table name
    static const std::string DOCUMENTS_TABLE;

    // create DocumentTable sql
    static const std::string CREATE_DOCUMENT_TABLE;
};
} // namespace OHOS::Media
#endif // INTERFACES_INNERKITS_NATIVE_INCLUDE_MEDIA_COLUMN_H_