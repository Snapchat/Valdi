import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import type { Argv } from 'yargs';
import { resolveDebuggerUiPort, startDebuggerServer } from '../debugger/server';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { makeCommandHandler } from '../utils/errorUtils';
import { writeOwlDevToolsExtension } from '../utils/owlDevToolsExtension';

interface CommandParameters {
  host: string;
  port: number;
  strictPort: boolean;
  json: boolean;
  webPreviewUrl?: string;
  chromiumDebuggingPort: number;
}

async function waitForShutdown(closeServer: () => Promise<void>): Promise<void> {
  return new Promise((resolve, reject) => {
    let shuttingDown = false;
    const signals: NodeJS.Signals[] = ['SIGINT', 'SIGTERM', 'SIGHUP'];
    const shutdown = () => {
      if (shuttingDown) return;
      shuttingDown = true;
      for (const signal of signals) process.off(signal, shutdown);
      void closeServer().then(resolve, reject);
    };

    for (const signal of signals) process.once(signal, shutdown);
  });
}

export async function valdiDebugger(argv: ArgumentsResolver<CommandParameters>): Promise<void> {
  const host = argv.getArgument('host');
  const port = argv.getArgument('port');
  const strictPort = argv.getArgument('strictPort');
  const json = argv.getArgument('json');
  const webPreviewUrlArgument = argv.getArgument('webPreviewUrl');
  const chromiumDebuggingPort = argv.getArgument('chromiumDebuggingPort');
  let applicationUrl: URL | undefined;
  if (webPreviewUrlArgument !== undefined) {
    const normalizedWebPreviewUrl = webPreviewUrlArgument.trim();
    if (!normalizedWebPreviewUrl) {
      throw new Error('The --web-preview-url option must not be blank.');
    }
    try {
      applicationUrl = new URL(normalizedWebPreviewUrl);
    } catch {
      throw new Error(`Invalid web preview URL: ${normalizedWebPreviewUrl}`);
    }
  }
  const webPreviewUrl = applicationUrl?.toString();
  const inspectedUrl = applicationUrl ? new URL(applicationUrl) : undefined;
  inspectedUrl?.searchParams.set('valdiDebugger', '1');
  inspectedUrl?.searchParams.set('valdiDevTools', '1');

  const debuggerServer = await startDebuggerServer({
    host,
    port,
    strictPort,
    chromiumDebuggingPort,
    ...(webPreviewUrl ? { webPreviewUrl } : {}),
  });

  let extensionDirectory: string | undefined;
  try {
    if (webPreviewUrl) {
      extensionDirectory = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-devtools-extension-'));
      writeOwlDevToolsExtension(extensionDirectory, debuggerServer.url);
    }
  } catch (error) {
    try {
      await debuggerServer.close();
    } catch (closeError) {
      console.warn(`Could not close the Valdi debugger after extension setup failed: ${String(closeError)}`);
    } finally {
      if (extensionDirectory) fs.rmSync(extensionDirectory, { force: true, recursive: true });
    }
    throw error;
  }
  let extensionExitCleanup: (() => void) | undefined;
  if (extensionDirectory) {
    extensionExitCleanup = () => fs.rmSync(extensionDirectory, { force: true, recursive: true });
    process.once('exit', extensionExitCleanup);
  }
  const cleanupExtension = (): void => {
    if (extensionExitCleanup) {
      process.off('exit', extensionExitCleanup);
      extensionExitCleanup = undefined;
    }
    if (extensionDirectory) fs.rmSync(extensionDirectory, { force: true, recursive: true });
  };
  const close = async (): Promise<void> => {
    try {
      await debuggerServer.close();
    } finally {
      cleanupExtension();
    }
  };

  if (json) {
    console.log(
      JSON.stringify({
        url: debuggerServer.url,
        apiToken: debuggerServer.apiToken,
        apiTokenHeader: debuggerServer.apiTokenHeader,
        host: debuggerServer.host,
        port: debuggerServer.port,
        requestedPort: debuggerServer.requestedPort,
        portWasAutoSelected: debuggerServer.portWasAutoSelected,
        pid: process.pid,
        ...(extensionDirectory ? { extensionDirectory } : {}),
        ...(inspectedUrl ? { inspectedUrl: inspectedUrl.toString(), chromiumDebuggingPort } : {}),
      }),
    );
  } else {
    console.log(`Valdi debugger listening on ${debuggerServer.url}`);
    console.log(`VALDI_DEBUGGER_URL=${debuggerServer.url}`);
    if (debuggerServer.portWasAutoSelected) {
      console.log(`Port ${debuggerServer.requestedPort} was busy; using ${debuggerServer.port}.`);
    }
    if (extensionDirectory && inspectedUrl) {
      console.log(`Valdi DevTools extension: ${extensionDirectory}`);
      console.log(`Open this exact preview URL: ${inspectedUrl.toString()}`);
      console.log(
        `Start Owl/Chromium with --remote-debugging-port=${chromiumDebuggingPort} --load-extension=${extensionDirectory}`,
      );
    }
  }

  await waitForShutdown(close);
}

export const command = 'debugger';
export const describe = 'Starts the Valdi debugger web interface';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('host', {
      describe: 'Loopback host address to bind the debugger web server to',
      type: 'string',
      default: process.env['VALDI_DEBUGGER_HOST'] || '127.0.0.1',
    })
    .option('port', {
      describe: 'Preferred debugger web server port',
      type: 'number',
      default: resolveDebuggerUiPort(),
    })
    .option('strict-port', {
      describe: 'Fail instead of selecting the next available port when the preferred port is busy',
      type: 'boolean',
      default: false,
    })
    .option('json', {
      describe: 'Print startup information as one JSON object for automation',
      type: 'boolean',
      default: false,
    })
    .option('web-preview-url', {
      describe: 'Exact loopback web/Owl application URL to expose in the integrated Chromium DevTools panel',
      type: 'string',
    })
    .option('chromium-debugging-port', {
      describe: 'Loopback Chromium remote debugging port used with --web-preview-url',
      type: 'number',
      default: Number.parseInt(process.env['VALDI_CHROMIUM_DEBUGGING_PORT'] || '9222', 10),
    });
};

export const handler = makeCommandHandler(valdiDebugger);
