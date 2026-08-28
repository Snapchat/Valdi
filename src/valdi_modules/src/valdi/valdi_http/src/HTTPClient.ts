import { CancelablePromise, promiseToCancelablePromise } from 'valdi_core/src/CancelablePromise';
import { StringMap } from 'coreutils/src/StringMap';
import { HTTPMethod, HTTPRequest, HTTPResponse } from './HTTPTypes';
import { IHTTPClient } from './IHTTPClient';
import { performRequest } from './NativeHTTPClient';

function makeURL(baseUrl: string | undefined, pathOrUrl: string): string {
  if (pathOrUrl.indexOf('://') >= 0) {
    return pathOrUrl;
  }
  if (!baseUrl) {
    throw new Error('baseUrl should be specified when providing a relative path');
  }

  if (!baseUrl.endsWith('/')) {
    baseUrl += '/';
  }
  if (pathOrUrl.startsWith('/')) {
    pathOrUrl = pathOrUrl.substring(1);
  }
  return baseUrl + pathOrUrl;
}

export class HTTPClient implements IHTTPClient {
  constructor(readonly baseUrl?: string | undefined) {}

  perform(
    pathOrUrl: string,
    method: HTTPMethod,
    headers: StringMap<string> | undefined,
    body: ArrayBuffer | Uint8Array | undefined,
  ): CancelablePromise<HTTPResponse> {
    let cancelFn: (() => void) | undefined;
    let rejectFn: ((reason: unknown) => void) | undefined;
    const promise = new Promise<HTTPResponse>((resolve, reject) => {
      rejectFn = reject;
      try {
        const request: HTTPRequest = {
          url: makeURL(this.baseUrl, pathOrUrl),
          method,
          headers,
          body,
        };

        cancelFn = performRequest(request, (response, error) => {
          if (response) {
            resolve(response);
          } else {
            // A request manager reports its failure as text, so wrap it: callers should not have to
            // ask whether the rejection they caught is a string or an Error.
            reject(error instanceof Error ? error : new Error(String(error)));
          }
        });
      } catch (err: unknown) {
        reject(err);
      }
    });

    // No request manager reports a cancelled request, so without this the promise never settles.
    // Rejecting an already settled one is a no-op, so a late cancel changes nothing.
    return promiseToCancelablePromise(promise, () => {
      cancelFn?.();
      rejectFn?.(new Error('Request was cancelled'));
    });
  }

  get(pathOrUrl: string, headers?: StringMap<string> | undefined): CancelablePromise<HTTPResponse> {
    return this.perform(pathOrUrl, HTTPMethod.GET, headers, undefined);
  }

  put(
    pathOrUrl: string,
    body?: ArrayBuffer | Uint8Array | undefined,
    headers?: StringMap<string> | undefined,
  ): CancelablePromise<HTTPResponse> {
    return this.perform(pathOrUrl, HTTPMethod.PUT, headers, body);
  }

  post(
    pathOrUrl: string,
    body?: ArrayBuffer | Uint8Array | undefined,
    headers?: StringMap<string> | undefined,
  ): CancelablePromise<HTTPResponse> {
    return this.perform(pathOrUrl, HTTPMethod.POST, headers, body);
  }

  delete(pathOrUrl: string, headers: StringMap<string> | undefined): CancelablePromise<HTTPResponse> {
    return this.perform(pathOrUrl, HTTPMethod.DELETE, headers, undefined);
  }

  head(pathOrUrl: string, headers: StringMap<string> | undefined): CancelablePromise<HTTPResponse> {
    return this.perform(pathOrUrl, HTTPMethod.HEAD, headers, undefined);
  }
}
