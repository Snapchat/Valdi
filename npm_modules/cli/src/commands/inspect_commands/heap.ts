import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, resolveClientId, DEFAULT_PORT } from '../../utils/daemonClient';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  gc: boolean;
  port: number;
  client: string | undefined;
  pretty: boolean;
}

async function inspectHeap(argv: ArgumentsResolver<CommandParameters>) {
  const gc = argv.getArgument('gc') as boolean;
  const port = argv.getArgument('port') as number;
  const clientOverride = argv.getArgument('client') as string | undefined;
  const pretty = argv.getArgument('pretty') as boolean;

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clientId = await resolveClientId(conn, clientOverride);
    const result = await conn.dumpHeap(clientId, gc);

    if (pretty) {
      const heap = result as { memoryUsageBytes?: number; heapDumpJSON?: string } | null;
      if (heap?.memoryUsageBytes !== undefined) {
        const mb = (heap.memoryUsageBytes / 1024 / 1024).toFixed(2);
        console.log(`Memory usage: ${wrapInColor(`${mb} MB`, ANSI_COLORS.YELLOW_COLOR)} (${heap.memoryUsageBytes} bytes)`);
      }
      console.log(JSON.stringify(result, null, 2));
    } else {
      console.log(JSON.stringify(result));
    }
  } finally {
    conn.close();
  }
}

export const command = 'heap';
export const describe = 'Dump JS heap stats for the connected device';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('gc', {
      describe: 'Run garbage collection before dumping heap',
      type: 'boolean',
      default: false,
    })
    .option('port', {
      describe: 'Daemon TCP port',
      type: 'number',
      default: DEFAULT_PORT,
    })
    .option('client', {
      describe: 'Client ID to target (from "valdi inspect devices")',
      type: 'string',
    })
    .option('pretty', {
      describe: 'Human-readable output instead of JSON',
      type: 'boolean',
      default: false,
    });
};
export const handler = makeCommandHandler(inspectHeap);
