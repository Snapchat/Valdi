import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { fetchRegistry } from '../../utils/skillsRegistry';
import { detectAdapters } from '../../utils/skillsAdapters';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  category: string | undefined;
}

async function skillsList(argv: ArgumentsResolver<CommandParameters>) {
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

  const adapters = detectAdapters();
  const adapterNames = adapters.map((a) => a.name);

  // Build installed sets per adapter
  const installedByAdapter = new Map<string, Set<string>>();
  for (const adapter of adapters) {
    installedByAdapter.set(adapter.name, new Set(adapter.listInstalled()));
  }

  const skills = categoryFilter
    ? registry.skills.filter((s) => s.category.includes(categoryFilter as 'framework' | 'client'))
    : registry.skills;

  if (categoryFilter && skills.length === 0) {
    console.log(wrapInColor(`No skills found for category "${categoryFilter}".`, ANSI_COLORS.YELLOW_COLOR));
    return;
  }

  // Header
  const nameWidth = 28;
  const catWidth = 18;
  const descWidth = 44;
  const colWidth = 8;

  const headerParts = [
    'NAME'.padEnd(nameWidth),
    'CATEGORY'.padEnd(catWidth),
    'DESCRIPTION'.padEnd(descWidth),
    ...adapterNames.map((n) => n.toUpperCase().padEnd(colWidth)),
  ];
  console.log(wrapInColor(headerParts.join('  '), ANSI_COLORS.GRAY_COLOR));
  console.log(wrapInColor('-'.repeat(nameWidth + catWidth + descWidth + adapterNames.length * (colWidth + 2) + 4), ANSI_COLORS.GRAY_COLOR));

  // Group by category for cleaner display (skills in multiple categories appear in each group)
  const categories = [...new Set(skills.flatMap((s) => s.category))];
  for (const cat of categories) {
    const group = skills.filter((s) => s.category.includes(cat as 'framework' | 'client'));
    if (categories.length > 1) {
      console.log(wrapInColor(`\n[${cat}]`, ANSI_COLORS.BLUE_COLOR));
    }
    for (const skill of group) {
      const name = skill.name.padEnd(nameWidth);
      const category = skill.category.join(', ').padEnd(catWidth);
      const desc = skill.description.length > descWidth - 1
        ? skill.description.slice(0, descWidth - 1) + '…'
        : skill.description.padEnd(descWidth);

      const statusCols = adapterNames.map((adapterName) => {
        const installed = installedByAdapter.get(adapterName)?.has(skill.name) ?? false;
        const mark = installed
          ? wrapInColor('✓', ANSI_COLORS.GREEN_COLOR)
          : wrapInColor('-', ANSI_COLORS.GRAY_COLOR);
        return mark.padEnd(colWidth);
      });

      console.log([name, category, desc, ...statusCols].join('  '));
    }
  }

  console.log(
    `\n${wrapInColor(`${skills.length} skill(s) available`, ANSI_COLORS.GRAY_COLOR)}` +
    `  Detected agents: ${wrapInColor(adapterNames.join(', '), ANSI_COLORS.GREEN_COLOR)}`,
  );
  console.log(
    `\nInstall a skill: ${wrapInColor('valdi skills install <name>', ANSI_COLORS.BLUE_COLOR)}`,
  );
  if (!categoryFilter) {
    console.log(
      `Filter by category: ${wrapInColor('valdi skills list --category=framework', ANSI_COLORS.BLUE_COLOR)}`,
    );
  }
}

export const command = 'list';
export const describe = 'List available skills and their installation status';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs.option('category', {
    describe: 'Filter by category (framework, client)',
    type: 'string',
    choices: ['framework', 'client'],
  });
};
export const handler = makeCommandHandler(skillsList);
