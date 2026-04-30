import type { Argv } from 'yargs';
import ora from 'ora';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { fetchRegistry, fetchSkillContent, getSkillResourceDir } from '../../utils/skillsRegistry';
import { detectAdapters, getAdapterByName } from '../../utils/skillsAdapters';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  name: string | undefined;
  for: string | undefined;
  category: string | undefined;
}

async function skillsInstall(argv: ArgumentsResolver<CommandParameters>) {
  const skillName = argv.getArgument('name');
  const forAgent = argv.getArgument('for');
  const categoryFilter = argv.getArgument('category') as string | undefined;

  let registry;
  try {
    registry = await fetchRegistry();
  } catch (error) {
    console.log(wrapInColor(
      `Could not load skills: ${error instanceof Error ? error.message : String(error)}`,
      ANSI_COLORS.RED_COLOR,
    ));
    return;
  }

  // Determine which skills to install
  let skillsToInstall = skillName
    ? registry.skills.filter((s) => s.name === skillName)
    : registry.skills;

  if (skillName && skillsToInstall.length === 0) {
    console.log(wrapInColor(`Skill "${skillName}" not found in registry.`, ANSI_COLORS.RED_COLOR));
    console.log(`Run ${wrapInColor('valdi skills list', ANSI_COLORS.BLUE_COLOR)} to see available skills.`);
    return;
  }

  if (categoryFilter) {
    skillsToInstall = skillsToInstall.filter((s) => s.category.includes(categoryFilter as 'framework' | 'client'));
    if (skillsToInstall.length === 0) {
      console.log(wrapInColor(`No skills found for category "${categoryFilter}".`, ANSI_COLORS.YELLOW_COLOR));
      return;
    }
  }

  // Determine which adapters to use
  let adapters = detectAdapters().filter((a) => a.name !== 'generic');
  if (forAgent) {
    if (forAgent === 'all') {
      // keep all detected adapters
    } else {
      const specific = getAdapterByName(forAgent);
      if (!specific) {
        console.log(wrapInColor(`Unknown agent "${forAgent}". Valid options: claude, cursor, copilot, all`, ANSI_COLORS.RED_COLOR));
        return;
      }
      adapters = [specific];
    }
  }

  if (adapters.length === 0) {
    console.log(wrapInColor('No supported AI agents detected.', ANSI_COLORS.YELLOW_COLOR));
    console.log('Use --for=claude, --for=cursor, or --for=copilot to install for a specific agent.');
    return;
  }

  console.log(
    `Installing ${wrapInColor(String(skillsToInstall.length), ANSI_COLORS.GREEN_COLOR)} skill(s) for: ${wrapInColor(adapters.map((a) => a.name).join(', '), ANSI_COLORS.BLUE_COLOR)}\n`,
  );

  for (const skill of skillsToInstall) {
    const installSpinner = ora(`Installing ${skill.name}…`).start();
    try {
      const content = await fetchSkillContent(skill.path);
      const resourceDir = getSkillResourceDir(skill.path) ?? undefined;
      for (const adapter of adapters) {
        adapter.install(skill.name, content, skill, resourceDir);
      }
      installSpinner.succeed(`Installed ${wrapInColor(skill.name, ANSI_COLORS.GREEN_COLOR)}`);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      installSpinner.fail(`Failed to install ${skill.name}: ${message}`);
    }
  }

  const claudeInstalled = adapters.some((a) => a.name === 'claude');
  console.log(`\nDone. Run ${wrapInColor('valdi skills list', ANSI_COLORS.BLUE_COLOR)} to see installed skills.`);
  if (claudeInstalled) {
    console.log(wrapInColor('Restart Claude Code to activate the new skills.', ANSI_COLORS.YELLOW_COLOR));
  }
  if (!categoryFilter) {
    console.log(
      `Tip: install only module-development skills with ${wrapInColor('valdi skills install --category=client', ANSI_COLORS.BLUE_COLOR)}`,
    );
  }
}

export const command = 'install [name]';
export const describe = 'Install skills for detected AI agents (--category=framework|client to filter)';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .positional('name', {
      describe: 'Skill name to install (omit to install all)',
      type: 'string',
    })
    .option('for', {
      describe: 'Target agent to install for',
      type: 'string',
      choices: ['claude', 'cursor', 'copilot', 'all'],
    })
    .option('category', {
      describe: 'Only install skills in this category',
      type: 'string',
      choices: ['framework', 'client'],
    });
};
export const handler = makeCommandHandler(skillsInstall);
