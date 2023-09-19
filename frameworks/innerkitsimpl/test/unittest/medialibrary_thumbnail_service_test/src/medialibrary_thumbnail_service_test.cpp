/*
 * Copyright (C) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "foundation/ability/form_fwk/test/mock/include/mock_single_kv_store.h"
#include "kvstore.h"
#include "medialibrary_thumbnail_service_test.h"
#define private public
#include "thumbnail_service.h"
#undef private

using namespace std;
using namespace OHOS;
using namespace testing::ext;
using namespace OHOS::NativeRdb;

namespace OHOS {
namespace Media {

class ConfigTestOpenCall : public NativeRdb::RdbOpenCallback {
public:
    int OnCreate(NativeRdb::RdbStore &rdbStore) override;
    int OnUpgrade(NativeRdb::RdbStore &rdbStore, int oldVersion, int newVersion) override;
    static const string CREATE_TABLE_TEST;
};

const string ConfigTestOpenCall::CREATE_TABLE_TEST = string("CREATE TABLE IF NOT EXISTS test ") +
    "(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, age INTEGER, salary REAL, blobType BLOB)";

int ConfigTestOpenCall::OnCreate(RdbStore &store)
{
    return store.ExecuteSql(CREATE_TABLE_TEST);
}

int ConfigTestOpenCall::OnUpgrade(RdbStore &store, int oldVersion, int newVersion)
{
    return 0;
}
shared_ptr<NativeRdb::RdbStore> storePtr = nullptr;
void MediaLibraryThumbnailServiceTest::SetUpTestCase(void)
{
    const string dbPath = "/data/test/medialibrary_thumbnail_service_test.db";
    NativeRdb::RdbStoreConfig config(dbPath);
    ConfigTestOpenCall helper;
    int errCode = 0;
    shared_ptr<NativeRdb::RdbStore> store = NativeRdb::RdbHelper::GetRdbStore(config, 1, helper, errCode);
    storePtr = store;
}
void MediaLibraryThumbnailServiceTest::TearDownTestCase(void) {}

// SetUp:Execute before each test case
void MediaLibraryThumbnailServiceTest::SetUp() {}

void MediaLibraryThumbnailServiceTest::TearDown(void) {}

HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_GetThumbnail_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    string uri = "";
    auto fd = serverTest->GetThumbnailFd(uri);
    EXPECT_LT(fd, 0);
    uri = "ParseThumbnailInfo?" + THUMBNAIL_OPERN_KEYWORD + "=" + MEDIA_DATA_DB_THUMBNAIL + "&" +
        THUMBNAIL_WIDTH + "=1&" + THUMBNAIL_HEIGHT + "=1";
    fd = serverTest->GetThumbnailFd(uri);
    EXPECT_LT(fd, 0);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(storePtr, kvStorePtr, context);
    fd = serverTest->GetThumbnailFd(uri);
    EXPECT_LT(fd, 0);
    serverTest->ReleaseService();
}

HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_LcdAging_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    int32_t ret = serverTest->LcdAging();
    EXPECT_EQ(ret, 0);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(storePtr, kvStorePtr, context);
    ret = serverTest->LcdAging();
    EXPECT_EQ(ret, 0);
    serverTest->ReleaseService();
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_LcdDistributeAging_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    string udid = "";
    int32_t ret = serverTest->LcdDistributeAging(udid);
    EXPECT_EQ(ret, -1);
    udid = "/storage/cloud/files/";
    ret = serverTest->LcdDistributeAging(udid);
    EXPECT_EQ(ret, -1);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(storePtr, kvStorePtr, context);
    ret = serverTest->LcdDistributeAging(udid);
    EXPECT_EQ(ret, 0);
    serverTest->ReleaseService();
}
#endif

HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_GenerateThumbnails_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    int32_t ret = serverTest->GenerateThumbnails();
    EXPECT_EQ(ret, -1);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(nullptr, kvStorePtr, context);
    ret = serverTest->GenerateThumbnails();
    EXPECT_EQ(ret, -1);
    serverTest->Init(storePtr, kvStorePtr, context);
    ret = serverTest->GenerateThumbnails();
    EXPECT_EQ(ret, E_EMPTY_VALUES_BUCKET);
    serverTest->ReleaseService();
}

HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_InterruptBgworker_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    serverTest->InterruptBgworker();
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(storePtr, kvStorePtr, context);
    serverTest->InterruptBgworker();
    serverTest->ReleaseService();
}

HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_StopAllWorker_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    serverTest->StopAllWorker();
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(storePtr, kvStorePtr, context);
    serverTest->StopAllWorker();
    serverTest->ReleaseService();
}

#ifdef DISTRIBUTED
HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_InvalidateDistributeThumbnail_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string udid = "";
    shared_ptr<ThumbnailService> serverTest = ThumbnailService::GetInstance();
    int32_t ret = serverTest->InvalidateDistributeThumbnail(udid);
    EXPECT_EQ(ret, -1);
    udid = "/storage/cloud/files/";
    ret = serverTest->InvalidateDistributeThumbnail(udid);
    EXPECT_EQ(ret, -1);
    shared_ptr<DistributedKv::SingleKvStore> kvStorePtr = make_shared<MockSingleKvStore>();
    shared_ptr<OHOS::AbilityRuntime::Context> context;
    serverTest->Init(storePtr, kvStorePtr, context);
    ret = serverTest->InvalidateDistributeThumbnail(udid);
    EXPECT_EQ(ret, 0);
    serverTest->ReleaseService();
}
#endif

HWTEST_F(MediaLibraryThumbnailServiceTest, medialib_CreateThumbnailAsync_test_001, TestSize.Level0)
{
    if (storePtr == nullptr) {
        exit(1);
    }
    string url = "";
    ThumbnailService serverTest;
    int32_t ret = serverTest.CreateThumbnail(url, "");
    EXPECT_EQ(ret, E_OK);
    serverTest.ReleaseService();
}

} // namespace Media
} // namespace OHOS