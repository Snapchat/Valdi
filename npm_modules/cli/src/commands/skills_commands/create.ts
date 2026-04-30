import type { Argv } from 'yargs';
import * as fs from 'fs';
import * as path from 'path';
import inquirer from 'inquirer';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import type { SkillRegistry, SkillMeta } from '../../utils/skillsRegistry';
import { ANSI_COLORS, META_DIR_PATH } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

// ─── Template loading ────────────────────────────────────────────────────────

function loadTemplate(filename: string, replacements: Record<string, string>): string {
  const raw = fs.readFileSync(path.join(META_DIR_PATH, filename), 'utf8');
  return Object.entries(replacements).reduce(
    (content, [key, value]) => content.replaceAll(key, value),
    raw,
  );
}

function toIosModuleName(skillName: string): string {
  const pascal = skillName
    .split('-')
    .map((w: string) => w.charAt(0).toUpperCase() + w.slice(1))
    .join('');
  return `SCCValdi${pascal}Ref`;
}

// ─── Valdi root detection ────────────────────────────────────────────────────

function findValdiRoot(dir: string): string | null {
  let current = path.resolve(dir);
  while (true) {
    if (fs.existsSync(path.join(current, 'ai-skills', 'registry.json'))) {
      return current;
    }
    const parent = path.dirname(current);
    if (parent === current) return null; // filesystem root
    current = parent;
  }
}

// eslint-disable-next-line @typescript-eslint/no-empty-interface
interface CommandParameters {}

async function skillsCreate(_argv: ArgumentsResolver<CommandParameters>) {
  const valdiRoot = findValdiRoot(process.cwd());
  if (valdiRoot == null) {
    console.log(
      wrapInColor(
        'Not inside a Valdi framework checkout. Run this command from within the Valdi repo.',
        ANSI_COLORS.RED_COLOR,
      ),
    );
    console.log('Expected to find ai-skills/registry.json in the current directory or a parent.');
    return;
  }

  const skillsDir = path.join(valdiRoot, 'ai-skills', 'skills');
  const registryPath = path.join(valdiRoot, 'ai-skills', 'registry.json');

  console.log(wrapInColor(`Valdi root: ${valdiRoot}\n`, ANSI_COLORS.GRAY_COLOR));
  console.log(wrapInColor('Create a new Valdi skill\n', ANSI_COLORS.BLUE_COLOR));

  const answers = await inquirer.prompt<{
    name: string;
    description: string;
    category: ('framework' | 'client')[];
    tags: string;
  }>([
    {
      type: 'input',
      name: 'name',
      message: 'Skill name (e.g. valdi-animations):',
      validate: (input: string) => {
        if (!input.trim()) return 'Name is required';
        if (!/^[\w-]+$/u.test(input.trim())) return 'Name must be alphanumeric with hyphens only';
        const existing = fs.existsSync(path.join(skillsDir, input.trim()));
        if (existing) return `Skill "${input.trim()}" already exists`;
        return true;
      },
      filter: (input: string) => input.trim(),
    },
    {
      type: 'input',
      name: 'description',
      message: 'Short description (one sentence):',
      validate: (input: string) => (input.trim() ? true : 'Description is required'),
      filter: (input: string) => input.trim(),
    },
    {
      type: 'checkbox',
      name: 'category',
      message: 'Category (select all that apply):',
      choices: [
        { name: 'client  — creating / updating / testing Valdi modules', value: 'client', checked: true },
        { name: 'framework — working on the Valdi repo itself', value: 'framework' },
      ],
      validate: (input: string[]) => (input.length > 0 ? true : 'Select at least one category'),
    },
    {
      type: 'input',
      name: 'tags',
      message: 'Tags (comma-separated, e.g. tsx,components):',
      filter: (input: string) =>
        input
          .split(',')
          .map((t) => t.trim())
          .filter(Boolean)
          .join(','),
    },
  ]);

  const { name, description, category, tags } = answers;
  const tagList = tags ? tags.split(',') : [];

  const skillDir = path.join(skillsDir, name);
  const skillFile = path.join(skillDir, 'skill.md');
  const skillPath = `skills/${name}/skill.md`;
  const testsDir = path.join(skillDir, 'tests');
  const testsSrcDir = path.join(testsDir, 'src');

  // Create skill.md from template
  fs.mkdirSync(skillDir, { recursive: true });
  fs.writeFileSync(
    skillFile,
    loadTemplate('skill.md.template', { SKILL_NAME: name, SKILL_DESCRIPTION: description }),
    'utf8',
  );

  // Scaffold tests/ directory from templates
  fs.mkdirSync(testsSrcDir, { recursive: true });
  fs.writeFileSync(
    path.join(testsSrcDir, 'reference.tsx'),
    loadTemplate('skill-reference.tsx.template', { SKILL_NAME: name }),
    'utf8',
  );
  fs.writeFileSync(
    path.join(testsDir, 'BUILD.bazel'),
    loadTemplate('skill-tests-BUILD.bazel.template', {
      SKILL_NAME: name,
      SKILL_IOS_MODULE_NAME: toIosModuleName(name),
    }),
    'utf8',
  );
  fs.writeFileSync(
    path.join(testsDir, 'tsconfig.json'),
    '{"extends": "../../../../src/valdi_modules/src/valdi/_configs/base.tsconfig.json"}\n',
    'utf8',
  );
  fs.writeFileSync(
    path.join(testsDir, 'README.md'),
    loadTemplate('skill-tests-README.md.template', { SKILL_NAME: name }),
    'utf8',
  );

  // Register in registry.json
  const registry = JSON.parse(fs.readFileSync(registryPath, 'utf8')) as SkillRegistry;
  const entry: SkillMeta = { name, description, tags: tagList, path: skillPath, category };
  registry.skills.push(entry);
  fs.writeFileSync(registryPath, JSON.stringify(registry, null, 2) + '\n', 'utf8');

  console.log(`\n${wrapInColor('Created:', ANSI_COLORS.GREEN_COLOR)} ${skillFile}`);
  console.log(`${wrapInColor('Created:', ANSI_COLORS.GREEN_COLOR)} ${testsDir}/`);
  console.log(`${wrapInColor('Registered:', ANSI_COLORS.GREEN_COLOR)} ${registryPath}`);
  console.log(`\nNext steps:`);
  console.log(`  1. Edit ${wrapInColor(skillFile, ANSI_COLORS.BLUE_COLOR)} — fill in the skill content`);
  console.log(`  2. Edit ${wrapInColor(path.join(testsDir, 'src', 'reference.tsx'), ANSI_COLORS.BLUE_COLOR)} — add compile-check examples`);
  console.log(`  3. Run ${wrapInColor(`bazel build //ai-skills/skills/${name}/tests:tests`, ANSI_COLORS.BLUE_COLOR)} to verify the reference compiles`);
  console.log(`  4. Run ${wrapInColor('valdi skills install', ANSI_COLORS.BLUE_COLOR)} to test the skill locally`);
  console.log(`  5. Open a PR against the Valdi repo to share it with the community`);
}

export const command = 'create';
export const describe = 'Scaffold a new skill in the Valdi framework checkout';
export const builder = (_yargs: Argv<CommandParameters>) => {};
export const handler = makeCommandHandler(skillsCreate);
