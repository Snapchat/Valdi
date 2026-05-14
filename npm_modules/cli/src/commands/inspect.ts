import type { Argv } from 'yargs';

export const command = 'inspect <command>';
export const describe = 'Inspect a running Valdi app — component trees, contexts, screenshots, heap';
export const builder = (yargs: Argv) => {
  return yargs
    .commandDir('inspect_commands', { extensions: ['js', 'ts'] })
    .demandCommand(1, 'Use devices, select, status, contexts, tree, snapshot, or heap')
    .recommendCommands()
    .wrap(yargs.terminalWidth())
    .help();
};
export const handler = () => {};
