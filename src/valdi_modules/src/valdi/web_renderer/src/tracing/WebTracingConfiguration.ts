import { hasSingleWebLocationQueryParameter } from '../utils/LocationQuery';
import { createBrowserChromeDevToolsTracing } from './ChromeDevToolsTracing';
import { isValdiWebTracingEnabled, setValdiWebTracing } from './ValdiWebTracing';

const VALDI_TRACE_QUERY_KEY = 'valdiTrace';
const CHROMIUM_TRACE_QUERY_VALUE = 'chrome';
const DEVTOOLS_QUERY_KEY = 'valdiDevTools';
const DEVTOOLS_QUERY_VALUE = '1';

/** Enable Chromium tracing only when the development host explicitly requests it. */
export function configureValdiWebTracingFromLocation(locationSearch: string | undefined): boolean {
  if (locationSearch === undefined || isValdiWebTracingEnabled()) {
    return false;
  }

  if (
    !hasSingleWebLocationQueryParameter(locationSearch, DEVTOOLS_QUERY_KEY, DEVTOOLS_QUERY_VALUE) ||
    !hasSingleWebLocationQueryParameter(locationSearch, VALDI_TRACE_QUERY_KEY, CHROMIUM_TRACE_QUERY_VALUE)
  ) {
    return false;
  }

  setValdiWebTracing(createBrowserChromeDevToolsTracing());
  return true;
}
