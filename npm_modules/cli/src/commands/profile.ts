import type { Argv } from 'yargs';

export const command = 'profile <command>';
export const describe = 'Capture a CPU profile from the Hermes JS runtime';
export const builder = (yargs: Argv) => {
  return yargs
    .commandDir('profile_commands', { extensions: ['js', 'ts'] })
    .demandCommand(1, 'Use: capture')
    .recommendCommands()
    .wrap(yargs.terminalWidth())
    .help();
};
export const handler = () => {};
