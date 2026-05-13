import type { Argv } from 'yargs';
import ora from 'ora';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { fetchRegistry, fetchSkillContent, getSkillResourceDir } from '../../utils/skillsRegistry';
import { detectAdapters } from '../../utils/skillsAdapters';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

// eslint-disable-next-line @typescript-eslint/no-empty-interface
interface CommandParameters {}

async function skillsUpdate(_argv: ArgumentsResolver<CommandParameters>) {
  const adapters = detectAdapters().filter((a) => a.name !== 'generic');
  if (adapters.length === 0) {
    console.log(
      wrapInColor('No supported AI agents detected. Nothing to update.', ANSI_COLORS.YELLOW_COLOR),
    );
    return;
  }

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

  // Only update skills that are already installed somewhere.
  const installedSkillNames = new Set<string>();
  for (const adapter of adapters) {
    for (const skillName of adapter.listInstalled()) {
      installedSkillNames.add(skillName);
    }
  }

  const skillsToUpdate = registry.skills.filter((s) => installedSkillNames.has(s.name));

  if (skillsToUpdate.length === 0) {
    console.log('No installed skills found to update.');
    console.log(
      `Run ${wrapInColor('valdi skills install', ANSI_COLORS.BLUE_COLOR)} to install skills.`,
    );
    return;
  }

  console.log(
    `\nUpdating ${wrapInColor(String(skillsToUpdate.length), ANSI_COLORS.GREEN_COLOR)} installed skill(s) for: ${wrapInColor(adapters.map((a) => a.name).join(', '), ANSI_COLORS.BLUE_COLOR)}\n`,
  );

  for (const skill of skillsToUpdate) {
    const updateSpinner = ora(`Updating ${skill.name}…`).start();
    try {
      const content = await fetchSkillContent(skill.path);
      const resourceDir = getSkillResourceDir(skill.path) ?? undefined;
      for (const adapter of adapters) {
        if (adapter.listInstalled().includes(skill.name)) {
          adapter.install(skill.name, content, skill, resourceDir);
        }
      }
      updateSpinner.succeed(`Updated ${wrapInColor(skill.name, ANSI_COLORS.GREEN_COLOR)}`);
    } catch (error) {
      updateSpinner.fail(
        `Failed to update ${skill.name}: ${error instanceof Error ? error.message : String(error)}`,
      );
    }
  }

  console.log(`\nAll installed skills updated.`);
}

export const command = 'update';
export const describe = 'Re-install all currently installed skills from the bundled package';
export const builder = (_yargs: Argv<CommandParameters>) => {};
export const handler = makeCommandHandler(skillsUpdate);
