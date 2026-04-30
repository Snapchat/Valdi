import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, resolveClientId, DEFAULT_PORT } from '../../utils/daemonClient';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  port: number;
  client: string | undefined;
  pretty: boolean;
}

async function inspectContexts(argv: ArgumentsResolver<CommandParameters>) {
  const port = argv.getArgument('port') as number;
  const clientOverride = argv.getArgument('client') as string | undefined;
  const pretty = argv.getArgument('pretty') as boolean;

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clientId = await resolveClientId(conn, clientOverride);
    const contexts = await conn.listContexts(clientId);

    if (pretty) {
      if (contexts.length === 0) {
        console.log(wrapInColor('No active contexts found.', ANSI_COLORS.YELLOW_COLOR));
        return;
      }
      console.log(wrapInColor(`${contexts.length} context(s) on device ${clientId}:\n`, ANSI_COLORS.GRAY_COLOR));
      for (const ctx of contexts) {
        console.log(`  ${wrapInColor(ctx.id, ANSI_COLORS.BLUE_COLOR)}  ${ctx.rootComponentName}`);
      }
    } else {
      console.log(JSON.stringify(contexts));
    }
  } finally {
    conn.close();
  }
}

export const command = 'contexts';
export const describe = 'List active Valdi root contexts (components) on the device';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
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
export const handler = makeCommandHandler(inspectContexts);
