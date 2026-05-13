import type { Argv } from 'yargs';

export const command = 'analyze <command>';
export const describe = 'Analyze Valdi modules — component duplication, patterns, health';
export const builder = (yargs: Argv) => {
  return yargs
    .commandDir('analyze_commands', { extensions: ['js', 'ts'] })
    .demandCommand(1, 'Use: duplicates')
    .recommendCommands()
    .wrap(yargs.terminalWidth())
    .help();
};
export const handler = () => {};
