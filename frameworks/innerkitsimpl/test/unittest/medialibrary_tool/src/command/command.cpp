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
#include "command/command.h"

#include "command/list_command_v10.h"
#include "command/recv_command_v10.h"
#include "command/send_command_v10.h"

namespace OHOS {
namespace Media {
namespace MediaTool {
std::unique_ptr<Command> Command::Create(const ExecEnv &env)
{
    if (env.api == MediaLibraryApi::API_10) {
        if (env.optArgs.cmdType == OptCmdType::TYPE_LIST) {
            return std::make_unique<ListCommandV10>();
        }
        if (env.optArgs.cmdType == OptCmdType::TYPE_RECV) {
            return std::make_unique<RecvCommandV10>();
        }
        if (env.optArgs.cmdType == OptCmdType::TYPE_SEND) {
            return std::make_unique<SendCommandV10>();
        }
    }
    return nullptr;
}

const std::vector<MediaType> &Command::GetSupportTypes()
{
    static const std::vector<MediaType> SUPPORT_TYPES = {
        MediaType::MEDIA_TYPE_AUDIO,
        MediaType::MEDIA_TYPE_FILE,
        MediaType::MEDIA_TYPE_IMAGE,
        MediaType::MEDIA_TYPE_VIDEO
    };
    return SUPPORT_TYPES;
}
} // namespace MediaTool
} // namespace Media
} // namespace OHOS