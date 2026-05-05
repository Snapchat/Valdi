import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, DEFAULT_PORT, loadInspectConfig, saveInspectConfig } from '../../utils/daemonClient';
import { getUserChoice } from '../../utils/cliUtils';
import { CliError } from '../../core/errors';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  port: number;
}

async function inspectSelect(argv: ArgumentsResolver<CommandParameters>) {
  const port = argv.getArgument('port') as number;

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clients = await conn.listConnectedClients();

    if (clients.length === 0) {
      throw new CliError('No devices connected to the Valdi daemon.');
    }

    const clientId = await getUserChoice(
      clients.map((c) => ({
        name: `${c.application_id} (${c.platform})  [${c.client_id}]`,
        value: c.client_id,
      })),
      'Select a device to use for inspect commands:',
    );

    const config = loadInspectConfig();
    saveInspectConfig({ ...config, selectedClientId: clientId });

    const chosen = clients.find((c) => c.client_id === clientId)!;
    console.log(
      `Selected: ${wrapInColor(chosen.application_id, ANSI_COLORS.GREEN_COLOR)} (${chosen.platform})`,
    );
    console.log(wrapInColor(`Saved to ~/.valdi-inspect.json`, ANSI_COLORS.GRAY_COLOR));
  } finally {
    conn.close();
  }
}

export const command = 'select';
export const describe = 'Interactively select a device and save it for subsequent inspect commands';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs.option('port', {
    describe: 'Daemon TCP port',
    type: 'number',
    default: DEFAULT_PORT,
  });
};
export const handler = makeCommandHandler(inspectSelect);
