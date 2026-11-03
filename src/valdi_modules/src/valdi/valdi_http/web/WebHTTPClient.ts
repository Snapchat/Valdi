import { StringMap } from '../../coreutils/src/StringMap';
import { HTTPRequest, HTTPResponse } from '../src/HTTPTypes';

export function performRequest(
  request: HTTPRequest,
  completion: (response: HTTPResponse | undefined, error: unknown | undefined) => void,
): () => void {
  let done = false;
  const completeOnce = (resp?: HTTPResponse, err?: unknown): void => {
    if (done) return;
    done = true;
    try {
      completion(resp, err);
    } catch {
      /* swallow in stub */
    }
  };

  // If fetch exists, use it; otherwise stub a success response.
  let abortCtrl: AbortController | undefined;

  if (typeof fetch === 'function') {
    try {
      if (typeof AbortController !== 'undefined') abortCtrl = new AbortController();
    } catch {
      /* older envs */
    }

    // Prepare headers as a plain object (works across environments)
    const headersObj: Record<string, string> = {};
    if (request.headers) {
      for (const k in request.headers) {
        if (Object.prototype.hasOwnProperty.call(request.headers, k)) {
          const v = request.headers[k];
          if (typeof v === 'string') headersObj[k] = v;
        }
      }
    }

    // Prepare body
    let body: BodyInit | undefined;
    if (request.body !== undefined && request.body !== null) {
      if (request.body instanceof Uint8Array) body = request.body;
      else if (typeof ArrayBuffer !== 'undefined' && request.body instanceof ArrayBuffer) {
        body = new Uint8Array(request.body);
      }
    }

    // priority is ignored in this stub

    fetch(request.url, {
      method: request.method, // string literal from const enum
      headers: headersObj,
      body,
      signal: abortCtrl?.signal,
    })
      .then(res => {
        // Build response headers map
        const respHeaders: StringMap<string> = {};
        if (res.headers && typeof res.headers.forEach === 'function') {
          res.headers.forEach((value, key) => {
            respHeaders[key] = value;
          });
        }

        // Get body as bytes (optional)
        return res
          .arrayBuffer()
          .then(ab => {
            completeOnce({ headers: respHeaders, statusCode: res.status, body: new Uint8Array(ab) }, undefined);
          })
          .catch(() => {
            // Some responses may not have a body
            completeOnce({ headers: respHeaders, statusCode: res.status }, undefined);
          });
      })
      .catch(err => {
        completeOnce(undefined, err);
      });
  } else {
    // No fetch: deliver a tiny successful response on the next microtask
    void Promise.resolve().then(() =>
      completeOnce({ headers: {}, statusCode: 200, body: new Uint8Array(0) }, undefined),
    );
  }

  // Return cancel function
  return () => {
    done = true;
    if (abortCtrl) {
      try {
        abortCtrl.abort();
      } catch {
        /* ignore */
      }
    }
  };
}
