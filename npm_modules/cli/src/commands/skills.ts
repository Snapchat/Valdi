import type { Argv } from 'yargs';

export const command = 'skills <command>';
export const describe = 'Manage AI assistant skills for Valdi development';
export const builder = (yargs: Argv) => {
  return yargs
    .commandDir('skills_commands', { extensions: ['js', 'ts'] })
    .demandCommand(1, 'Use list, install, update, remove, or add')
    .recommendCommands()
    .wrap(yargs.terminalWidth())
    .help();
};
export const handler = () => {};
