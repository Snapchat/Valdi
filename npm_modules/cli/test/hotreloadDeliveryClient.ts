/**
 * Standalone hot-reload delivery assertion, driven by tools/ci/hotreload_delivery_smoke.sh.
 *
 * The script builds + runs apps/helloworld's reloader (in --usb mode, like
 * hotreload_smoke.sh). The reloader never accepts TCP connections; its simulator
 * auto-connector dials localhost:13591 (Ports.reloaderStandalone) looking for an app.
 * We mock that device: LISTEN on 13591, let the daemon connect to us, configure as a
 * client, edit the app's .tsx, and assert the recompiled module is delivered to us.
 *
 * Runs as a plain script (not node:test) so the shell can orchestrate the reloader
 * lifecycle around it. Exits 0 on delivery, 1 otherwise.
 *
 * Env:
 *   RELOADER_PORT       sim dial-in port to listen on (default 13591)
 *   EDIT_FILE           absolute path to the .tsx to edit (required)
 *   MARKER              unique token to inject + look for (default generated)
 *   DIAL_TIMEOUT_MS     how long to wait for the daemon to dial in (default 20m)
 *   DELIVER_TIMEOUT_MS  how long to wait for the edited module after the edit (default 5m)
 */
import assert from 'node:assert';
import { appendFile } from 'node:fs/promises';
import { listenMockApp, resourceText } from './helpers/MockAppClient';

const PORT = Number(process.env['RELOADER_PORT'] ?? '13591');
const EDIT_FILE = process.env['EDIT_FILE'];
const MARKER = process.env['MARKER'] ?? `valdi_delivery_${Date.now()}`;
const DIAL_TIMEOUT_MS = Number(process.env['DIAL_TIMEOUT_MS'] ?? '1200000');
const DELIVER_TIMEOUT_MS = Number(process.env['DELIVER_TIMEOUT_MS'] ?? '300000');

async function main(): Promise<void> {
  if (!EDIT_FILE) {
    throw new Error('EDIT_FILE env var is required');
  }

  console.log(`[hotreload-delivery] listening on 127.0.0.1:${PORT} for the reloader to dial in...`);
  const mock = await listenMockApp(PORT, '127.0.0.1', DIAL_TIMEOUT_MS);
  try {
    console.log('[hotreload-delivery] daemon connected; configuring as a client');
    await mock.configure({ applicationId: 'valdi-delivery-smoke', platform: 'ios' });

    // Start waiting before the edit so we can't miss the delivery. The marker is a
    // unique string literal that survives compilation, so matching it proves the
    // *edited* module reached us (not the initial connect-time resource dump).
    const delivered = mock.waitForResources(resource => resourceText(resource).includes(MARKER), DELIVER_TIMEOUT_MS);
    delivered.catch(() => undefined);

    console.log(`[hotreload-delivery] editing ${EDIT_FILE} (marker ${MARKER})`);
    await appendFile(EDIT_FILE, `\nexport const ${MARKER} = '${MARKER}';\n`);

    const resources = await delivered;
    assert.ok(
      resources.some(resource => resourceText(resource).includes(MARKER)),
      'delivered resources did not contain the edited module',
    );
    console.log(`[hotreload-delivery] OK: reloader delivered the edited module to the mock client`);
  } finally {
    mock.close();
  }
}

main().then(
  () => process.exit(0),
  (error: unknown) => {
    console.error('[hotreload-delivery] FAIL:', error instanceof Error ? error.message : error);
    process.exit(1);
  },
);
