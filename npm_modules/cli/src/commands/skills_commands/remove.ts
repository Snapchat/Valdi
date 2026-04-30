import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { detectAdapters } from '../../utils/skillsAdapters';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  name: string;
}

async function skillsRemove(argv: ArgumentsResolver<CommandParameters>) {
  const skillName = argv.getArgument('name');
  const adapters = detectAdapters();

  let removedFromAny = false;

  for (const adapter of adapters) {
    const installed = adapter.listInstalled();
    if (installed.includes(skillName)) {
      try {
        adapter.remove(skillName);
        console.log(
          `Removed ${wrapInColor(skillName, ANSI_COLORS.GREEN_COLOR)} from ${wrapInColor(adapter.name, ANSI_COLORS.BLUE_COLOR)}`,
        );
        removedFromAny = true;
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        console.log(
          wrapInColor(`Failed to remove ${skillName} from ${adapter.name}: ${message}`, ANSI_COLORS.RED_COLOR),
        );
      }
    }
  }

  if (!removedFromAny) {
    console.log(
      wrapInColor(`Skill "${skillName}" is not installed for any detected agent.`, ANSI_COLORS.YELLOW_COLOR),
    );
    console.log(`Run ${wrapInColor('valdi skills list', ANSI_COLORS.BLUE_COLOR)} to see installed skills.`);
  }
}

export const command = 'remove <name>';
export const describe = 'Remove a skill from all detected AI agents';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs.positional('name', {
    describe: 'Name of the skill to remove',
    type: 'string',
    demandOption: true,
  });
};
export const handler = makeCommandHandler(skillsRemove);
