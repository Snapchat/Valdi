import { beginKeepAlive, endKeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';
import { HTTPClient } from 'valdi_http/src/HTTPClient';
import { ArgumentsParser } from 'valdi_standalone/src/ArgumentsParser';
import { getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';

const DEFAULT_URL = 'https://example.com';

const standalone = getStandaloneRuntime();

const parser = new ArgumentsParser('cli_http_example', standalone.arguments);
const urlArgument = parser.addString('--url', `URL to fetch (default ${DEFAULT_URL})`, false);
parser.parse();

const url = urlArgument.value ?? DEFAULT_URL;

const keepAlive = beginKeepAlive();

console.info(`GET ${url}`);

new HTTPClient().get(url).then(
  response => {
    const length = response.body ? response.body.byteLength : 0;
    console.info(`OK: status ${response.statusCode}, ${length} bytes`);
    endKeepAlive(keepAlive);
    standalone.exit(0);
  },
  error => {
    console.error(`FAIL: request rejected: ${error}`);
    endKeepAlive(keepAlive);
    standalone.exit(1);
  },
);
