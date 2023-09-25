/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

const photoAccessHelper = requireInternal('file.photoAccessHelper');
const bundleManager = requireNapi('bundle.bundleManager');

const ARGS_TWO = 2;

const WRITE_PERMISSION = 'ohos.permission.WRITE_IMAGEVIDEO';

const PERMISSION_DENIED = 13900012;
const ERR_CODE_PARAMERTER_INVALID = 13900020;
const ERROR_MSG_WRITE_PERMISSION = 'not have ohos.permission.WRITE_IMAGEVIDEO';
const ERROR_MSG_USER_DENY = 'user deny';
const ERROR_MSG_PARAMERTER_INVALID = 'input parmaeter invalid';

const MAX_DELETE_NUMBER = 3600;
const MIN_DELETE_NUMBER = 1;

let gContext = undefined;

class BusinessError extends Error {
  constructor(msg, code) {
    super(msg);
    this.code = code || PERMISSION_DENIED;
  }
}
function checkParams(uriList, asyncCallback) {
  if (arguments.length > ARGS_TWO) {
    return false;
  }
  if (!Array.isArray(uriList)) {
    return false;
  }
  if (asyncCallback && typeof asyncCallback !== 'function') {
    return false;
  }
  if (uriList.length < MIN_DELETE_NUMBER || uriList.length > MAX_DELETE_NUMBER) {
    return false;
  }
  let tag = 'file://media/Photo/';
  for (let uri of uriList) {
    if (!uri.includes(tag)) {
      console.info(`photoAccessHelper invalid uri: ${uri}`);
      return false;
    }
  }
  return true;
}
function errorResult(rej, asyncCallback) {
  if (asyncCallback) {
    return asyncCallback(rej);
  }
  return new Promise((resolve, reject) => {
    reject(rej);
  });
}

function getAbilityResource(bundleInfo) {
  let labelId = bundleInfo.abilitiesInfo[0].labelId;
  for (let abilityInfo of bundleInfo.abilitiesInfo) {
    if (abilityInfo.name === bundleInfo.mainElementName) {
      labelId = abilityInfo.labelId;
    }
  }

  return labelId;
}

async function getAppName() {
  let appName = '';
  try {
    const flags = bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_ABILITY | bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_HAP_MODULE;
    const bundleInfo = await bundleManager.getBundleInfoForSelf(flags);
    console.info(`photoAccessHelper bundleInfo: ${JSON.stringify(bundleInfo)}`)
    if (bundleInfo === undefined || bundleInfo.hapModulesInfo === undefined || bundleInfo.hapModulesInfo.length === 0) {
      return appName;
    }
    const labelId = getAbilityResource(bundleInfo.hapModulesInfo[0]);
    const resourceMgr = gContext.resourceManager;
    appName = await resourceMgr.getStringValue(labelId);
    console.info(`photoAccessHelper appName: ${appName}`)
  } catch (error) {
    console.info(`photoAccessHelper error: ${JSON.stringify(error)}`)
  }

  return appName;
}

async function createPhotoDeleteRequestParamsOk(uriList, asyncCallback) {
  let flags = bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_REQUESTED_PERMISSION;
  let { reqPermissionDetails } = await bundleManager.getBundleInfoForSelf(flags);
  let isPermission = reqPermissionDetails.findIndex(({ name }) => name === WRITE_PERMISSION) !== -1;
  if (!isPermission) {
    return errorResult(new BusinessError(ERROR_MSG_WRITE_PERMISSION), asyncCallback);
  }
  const appName = await getAppName();
  if (appName.length === 0) {
    console.info(`photoAccessHelper appName not found`)
  }
  const startParameter = {
    action: 'ohos.want.action.deleteDialog',
    type: 'image/*',
    parameters: {
      uris: uriList,
      appName: appName
    },
  };
  try {
    const result = await gContext.requestDialogService(startParameter);
    console.info(`photoAccessHelper result: ${JSON.stringify(result)}`);
    if (result === null || result === undefined) {
      console.log('photoAccessHelper createDeleteRequest return null');
      return errorResult(Error('requestDialog return undefined'), asyncCallback);
    }
    if (asyncCallback) {
      if (result.result === 0) {
        return asyncCallback();
      } else {
        return asyncCallback(new BusinessError(ERROR_MSG_USER_DENY));
      }
    }
    return new Promise((resolve, reject) => {
      if (result.result === 0) {
        resolve();
      } else {
        reject(new BusinessError(ERROR_MSG_USER_DENY));
      }
    });
  } catch (error) {
    return errorResult(new BusinessError(error.message, error.code), asyncCallback);
  }
}

function createDeleteRequest(...params) {
  if (!checkParams(...params)) {
    throw new BusinessError(ERROR_MSG_PARAMERTER_INVALID, ERR_CODE_PARAMERTER_INVALID);
  }
  return createPhotoDeleteRequestParamsOk(...params);
}

function getPhotoAccessHelper(context) {
  if (context === undefined) {
    console.log('photoAccessHelper gContext undefined');
    throw Error('photoAccessHelper gContext undefined');
  }
  gContext = context;
  let helper = photoAccessHelper.getPhotoAccessHelper(gContext);
  if (helper !== undefined) {
    console.log('photoAccessHelper getPhotoAccessHelper inner add createDeleteRequest');
    helper.createDeleteRequest = createDeleteRequest;
  }
  return helper;
}

function getPhotoAccessHelperAsync(context, asyncCallback) {
  if (context === undefined) {
    console.log('photoAccessHelper gContext undefined');
    throw Error('photoAccessHelper gContext undefined');
  }
  gContext = context;
  if (arguments.length === 1) {
    return photoAccessHelper.getPhotoAccessHelperAsync(gContext)
      .then((helper) => {
        if (helper !== undefined) {
          console.log('photoAccessHelper getPhotoAccessHelperAsync inner add createDeleteRequest');
          helper.createDeleteRequest = createDeleteRequest;
        }
        return helper;
      })
      .catch((err) => {
        console.log('photoAccessHelper getPhotoAccessHelperAsync err ' + err);
        throw Error(err);
      });
  } else if (arguments.length === ARGS_TWO && typeof asyncCallback === 'function') {
    photoAccessHelper.getPhotoAccessHelperAsync(gContext, (err, helper) => {
      console.log('photoAccessHelper getPhotoAccessHelperAsync callback ' + err);
      if (err) {
        asyncCallback(err);
      } else {
        if (helper !== undefined) {
          console.log('photoAccessHelper getPhotoAccessHelperAsync callback add createDeleteRequest');
          helper.createDeleteRequest = createDeleteRequest;
        }
        asyncCallback(err, helper);
      }
    });
  } else {
    console.log('photoAccessHelper getPhotoAccessHelperAsync param invalid');
    throw new BusinessError(ERROR_MSG_PARAMERTER_INVALID, ERR_CODE_PARAMERTER_INVALID);
  }
  return undefined;
}

const PhotoViewMIMETypes = {
  IMAGE_TYPE: 'image/*',
  VIDEO_TYPE: 'video/*',
  IMAGE_VIDEO_TYPE: '*/*',
  INVALID_TYPE: ''
}

const ErrCode = {
  INVALID_ARGS: 13900020,
  RESULT_ERROR: 13900042,
}

const ERRCODE_MAP = new Map([
  [ErrCode.INVALID_ARGS, 'Invalid argument'],
  [ErrCode.RESULT_ERROR, 'Unknown error'],
]);

const PHOTO_VIEW_MIME_TYPE_MAP = new Map([
  [PhotoViewMIMETypes.IMAGE_TYPE, 'FILTER_MEDIA_TYPE_IMAGE'],
  [PhotoViewMIMETypes.VIDEO_TYPE, 'FILTER_MEDIA_TYPE_VIDEO'],
  [PhotoViewMIMETypes.IMAGE_VIDEO_TYPE, 'FILTER_MEDIA_TYPE_ALL'],
]);

const ARGS_ZERO = 0;
const ARGS_ONE = 1;

function checkArguments(args) {
  let checkArgumentsResult = undefined;

  if (args.length === ARGS_TWO && typeof args[ARGS_ONE] !== 'function') {
    checkArgumentsResult = getErr(ErrCode.INVALID_ARGS);
  }

  if (args.length > 0 && typeof args[ARGS_ZERO] === 'object') {
    let option = args[ARGS_ZERO];
    if (option.maxSelectNumber !== undefined) {
      if (option.maxSelectNumber.toString().indexOf('.') !== -1) {
        checkArgumentsResult = getErr(ErrCode.INVALID_ARGS);
      }
    }
  }

  return checkArgumentsResult;
}

function getErr(errCode) {
  return { code: errCode, message: ERRCODE_MAP.get(errCode) };
}

function parsePhotoPickerSelectOption(args) {
  let config = {
    action: 'ohos.want.action.photoPicker',
    type: 'multipleselect',
    parameters: {
      uri: 'multipleselect',
    },
  };

  if (args.length > ARGS_ZERO && typeof args[ARGS_ZERO] === 'object') {
    let option = args[ARGS_ZERO];
    if (option.maxSelectNumber && option.maxSelectNumber > 0) {
      let select = (option.maxSelectNumber === 1) ? 'singleselect' : 'multipleselect';
      config.type = select;
      config.parameters.uri = select;
      config.parameters.maxSelectCount = option.maxSelectNumber;
    }
    if (option.MIMEType && PHOTO_VIEW_MIME_TYPE_MAP.has(option.MIMEType)) {
      config.parameters.filterMediaType = PHOTO_VIEW_MIME_TYPE_MAP.get(option.MIMEType);
    }
  }

  return config;
}

function getPhotoPickerSelectResult(args) {
  let selectResult = {
    error: undefined,
    data: undefined,
  };

  if (args.resultCode === 0) {
    if (args.want && args.want.parameters) {
      let uris = args.want.parameters['select-item-list'];
      let isOrigin = args.want.parameters.isOriginal;
      selectResult.data = new PhotoSelectResult(uris, isOrigin);
    }
  } else if (result.resultCode === -1) {
    selectResult.data = new PhotoSelectResult([], undefined);
  } else {
    selectResult.error = getErr(ErrCode.RESULT_ERROR);
  }

  return selectResult;
}

async function photoPickerSelect(...args) {
  let checkArgsResult = checkArguments(args);
  if (checkArgsResult !== undefined) {
    console.log('[picker] Invalid argument');
    throw checkArgsResult;
  }

  const config = parsePhotoPickerSelectOption(args);
  console.log('[picker] config: ' + JSON.stringify(config));

  try {
    let context = getContext(this);
    let result = await context.startAbilityForResult(config, {windowMode: 1});
    console.log('[picker] result: ' + JSON.stringify(result));
    const selectResult = getPhotoPickerSelectResult(result);
    console.log('[picker] selectResult: ' + JSON.stringify(selectResult));
    if (args.length === ARGS_TWO && typeof args[ARGS_ONE] === 'function') {
      return args[ARGS_ONE](selectResult.error, selectResult.data);
    } else if (args.length === ARGS_ONE && typeof args[ARGS_ZERO] === 'function') {
      return args[ARGS_ZERO](selectResult.error, selectResult.data);
    }
    return new Promise((resolve, reject) => {
      if (selectResult.data !== undefined) {
        resolve(selectResult.data);
      } else {
        reject(selectResult.error);
      }
    })
  } catch (error) {
    console.log('[picker] error: ' + error);
  }
  return undefined;
}

function PhotoSelectOptions() {
  this.MIMEType = PhotoViewMIMETypes.INVALID_TYPE;
  this.maxSelectNumber = -1;
}

function PhotoSelectResult(uris, isOriginalPhoto) {
  this.photoUris = uris;
  this.isOriginalPhoto = isOriginalPhoto;
}

function PhotoViewPicker() {
  this.select = photoPickerSelect;
}

export default {
  getPhotoAccessHelper,
  getPhotoAccessHelperAsync,
  PhotoType: photoAccessHelper.PhotoType,
  PhotoKeys: photoAccessHelper.PhotoKeys,
  AlbumKeys: photoAccessHelper.AlbumKeys,
  AlbumType: photoAccessHelper.AlbumType,
  AlbumSubtype: photoAccessHelper.AlbumSubtype,
  PositionType: photoAccessHelper.PositionType,
  PhotoSubtype: photoAccessHelper.PhotoSubtype,
  NotifyType: photoAccessHelper.NotifyType,
  DefaultChangeUri: photoAccessHelper.DefaultChangeUri,
  PhotoViewMIMETypes: PhotoViewMIMETypes,
  PhotoSelectOptions: PhotoSelectOptions,
  PhotoSelectResult: PhotoSelectResult,
  PhotoViewPicker: PhotoViewPicker,
};
