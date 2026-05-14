import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, DEFAULT_PORT } from '../../utils/daemonClient';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  port: number;
  pretty: boolean;
}

async function inspectDevices(argv: ArgumentsResolver<CommandParameters>) {
  const port = argv.getArgument('port') as number;
  const pretty = argv.getArgument('pretty') as boolean;

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clients = await conn.listConnectedClients();

    if (pretty) {
      if (clients.length === 0) {
        console.log(wrapInColor('No devices connected to the Valdi daemon.', ANSI_COLORS.YELLOW_COLOR));
        return;
      }
      const nameWidth = 30;
      const platWidth = 12;
      console.log(
        wrapInColor(
          ['CLIENT ID'.padEnd(36), 'PLATFORM'.padEnd(platWidth), 'APPLICATION ID'.padEnd(nameWidth)].join('  '),
          ANSI_COLORS.GRAY_COLOR,
        ),
      );
      console.log(wrapInColor('-'.repeat(36 + platWidth + nameWidth + 4), ANSI_COLORS.GRAY_COLOR));
      for (const c of clients) {
        console.log([c.client_id.padEnd(36), c.platform.padEnd(platWidth), c.application_id.padEnd(nameWidth)].join('  '));
      }
      console.log(wrapInColor(`\n${clients.length} device(s) connected`, ANSI_COLORS.GRAY_COLOR));
    } else {
      console.log(JSON.stringify(clients));
    }
  } finally {
    conn.close();
  }
}

export const command = 'devices';
export const describe = 'List devices connected to the Valdi daemon';
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
export const handler = makeCommandHandler(inspectDevices);
