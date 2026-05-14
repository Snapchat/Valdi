import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, DEFAULT_PORT, MOBILE_PORT, STANDALONE_PORT } from '../../utils/daemonClient';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  port: number;
  pretty: boolean;
}

async function inspectStatus(argv: ArgumentsResolver<CommandParameters>) {
  const port = argv.getArgument('port') as number;
  const pretty = argv.getArgument('pretty') as boolean;

  let connected = false;
  let clientCount = 0;
  let error: string | undefined;

  try {
    const conn = await connectToDaemon(port);
    try {
      await conn.configure();
      const clients = await conn.listConnectedClients();
      connected = true;
      clientCount = clients.length;
    } finally {
      conn.close();
    }
  } catch (err) {
    error = err instanceof Error ? err.message : String(err);
  }

  const result = {
    connected,
    port,
    connectedDevices: clientCount,
    portName: port === MOBILE_PORT ? 'mobile' : port === STANDALONE_PORT ? 'standalone' : 'custom',
    error: error ?? null,
  };

  if (pretty) {
    const statusColor = connected ? ANSI_COLORS.GREEN_COLOR : ANSI_COLORS.RED_COLOR;
    const statusLabel = connected ? '● connected' : '○ not connected';
    console.log(`Daemon status: ${wrapInColor(statusLabel, statusColor)}`);
    console.log(`Port: ${wrapInColor(String(port), ANSI_COLORS.BLUE_COLOR)} (${result.portName})`);
    if (connected) {
      console.log(`Connected devices: ${wrapInColor(String(clientCount), ANSI_COLORS.GREEN_COLOR)}`);
    }
    if (error) {
      console.log(wrapInColor(error, ANSI_COLORS.RED_COLOR));
    }
  } else {
    console.log(JSON.stringify(result));
  }
}

export const command = 'status';
export const describe = 'Check Valdi daemon connection status and connected device count';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('port', {
      describe: 'Daemon TCP port',
      type: 'number',
      default: DEFAULT_PORT,
    })
    .option('pretty', {
      describe: 'Human-readable output instead of JSON',
      type: 'boolean',
      default: false,
    });
};
export const handler = makeCommandHandler(inspectStatus);
