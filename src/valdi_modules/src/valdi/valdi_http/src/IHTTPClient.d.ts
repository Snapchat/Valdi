import { StringMap } from 'coreutils/src/StringMap';
import { CancelablePromise } from 'valdi_core/src/CancelablePromise';
import { HTTPResponse } from './HTTPTypes';

export interface IHTTPClient {
  get(pathOrUrl: string, headers?: StringMap<string> | undefined): CancelablePromise<HTTPResponse>;

  put(
    pathOrUrl: string,
    body?: ArrayBuffer | Uint8Array | undefined,
    headers?: StringMap<string> | undefined,
  ): CancelablePromise<HTTPResponse>;

  post(
    pathOrUrl: string,
    body?: ArrayBuffer | Uint8Array | undefined,
    headers?: StringMap<string> | undefined,
  ): CancelablePromise<HTTPResponse>;

  delete(pathOrUrl: string, headers: StringMap<string> | undefined): CancelablePromise<HTTPResponse>;

  head(pathOrUrl: string, headers: StringMap<string> | undefined): CancelablePromise<HTTPResponse>;
}
