import type { Argv } from 'yargs';
import { resolveDebuggerUiPort, startDebuggerServer } from '../debugger/server';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { makeCommandHandler } from '../utils/errorUtils';

interface CommandParameters {
  host: string;
  port: number;
  strictPort: boolean;
  json: boolean;
}

async function waitForShutdown(closeServer: () => Promise<void>): Promise<void> {
  return new Promise((resolve, reject) => {
    let shuttingDown = false;
    const shutdown = () => {
      if (shuttingDown) return;
      shuttingDown = true;
      process.off('SIGINT', shutdown);
      process.off('SIGTERM', shutdown);
      void closeServer().then(resolve, reject);
    };

    process.once('SIGINT', shutdown);
    process.once('SIGTERM', shutdown);
  });
}

async function valdiDebugger(argv: ArgumentsResolver<CommandParameters>): Promise<void> {
  const host = argv.getArgument('host');
  const port = argv.getArgument('port');
  const strictPort = argv.getArgument('strictPort');
  const json = argv.getArgument('json');

  const debuggerServer = await startDebuggerServer({
    host,
    port,
    strictPort,
  });

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
      }),
    );
  } else {
    console.log(`Valdi debugger listening on ${debuggerServer.url}`);
    console.log(`VALDI_DEBUGGER_URL=${debuggerServer.url}`);
    if (debuggerServer.portWasAutoSelected) {
      console.log(`Port ${debuggerServer.requestedPort} was busy; using ${debuggerServer.port}.`);
    }
  }

  await waitForShutdown(debuggerServer.close);
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
    });
};

export const handler = makeCommandHandler(valdiDebugger);
