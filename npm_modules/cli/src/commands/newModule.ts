import fs from 'fs';
import inquirer from 'inquirer';
import path from 'path';
import type { Argv } from 'yargs';
import { ANSI_COLORS, BOOTSTRAP_DIR_PATH } from '../core/constants';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import type { CliChoice } from '../utils/cliUtils';
import { getUserChoice } from '../utils/cliUtils';
import { copyBootstrapFiles } from '../utils/copyBootstrapFiles';
import { makeCommandHandler } from '../utils/errorUtils';
import type { Replacements } from '../utils/fileUtils';
import { fileExists } from '../utils/fileUtils';
import { wrapInColor } from '../utils/logUtils';
import { toPascalCase, toSnakeCase, sanitizeProjectName, validateProjectName } from '../utils/stringUtils';

const referenceString = `
******************************************
        Valdi Module Creation Guide
******************************************

Requirements for Valdi module names:
- May contain: A-Z, a-z, 0-9, '-', '_', '.'
- Must start with a letter.

Recommended Directory Structure:
my_application/          # Root directory of your application
├── WORKSPACE            # Bazel Workspace
├── BUILD.bazel          # Bazel build
└── modules/
    ├── module_a/
    │   ├── BUILD.bazel
    │   ├── android/     # Native Android sources
    │   ├── ios/         # Native iOS sources
    │   ├── cpp/         # Native C++ sources
    │   └── src/         # Valdi sources
    │       └── ModuleAComponent.tsx
    ├── module_b/
        ├── BUILD.bazel
    │   ├── res/         # Image and font resources
    │   ├── strings/     # Localizable strings
        └── src/
            └── ModuleBComponent.tsx

For more comprehensive details, refer to the core-module documentation:
https://github.com/Snapchat/Valdi/blob/main/docs/docs/core-module.md

******************************************
`;

const ALL_MODULE_TEMPLATES: readonly ModuleTemplate[] = [
  {
    name: 'UI Component',
    path: 'ui_component',
    description: `Exports a Valdi component, which can render UI elements`,
  },
  {
    name: 'iOS/Android Bridge module',
    path: 'ios_android_bridge_module',
    description: `Bridge module with a TypeScript API backed by Objective-C and Kotlin implementation`,
  },
  {
    name: 'C++ Bridge module',
    path: 'cpp_bridge_module',
    description: `Bridge module with a TypeScript API backed by a cross-platform C++ implementation`,
  },
  {
    name: 'iOS/Android View module',
    path: 'ios_android_view_module',
    description: `iOS and Android view exported to TypeScript, rendered through a Valdi component`,
  },
];

interface CommandParameters {
  debug: boolean;
  skipChecks: boolean;
  moduleName: string;
  template?: string
}

interface Checks {
  confirmBazelWorkspace?: boolean;
  confirmValdiPath?: boolean;
  valdiModulePath?: string;
}

interface ModuleTemplate {
  name: string;
  path: string;
  description: string;
}

async function promptChecks(): Promise<Checks> {
  let answers: Checks = {};

  /**
   * Verify Bazel Workspace
   */
  const bazelWorkspacePath = await getBazelWorkspaceRoot();

  answers = await inquirer.prompt<Checks>(
    [
      {
        type: 'confirm',
        name: 'confirmBazelWorkspace',
        message: 'Bazel WORKSPACE not found (did you run `valdi bootstrap` first?). Continue anyway?',
        default: true,
        when() {
          return !bazelWorkspacePath;
        },
      },
    ],
    answers,
  );

  if (!bazelWorkspacePath && !answers.confirmBazelWorkspace) {
    throw new CliError('Aborting.');
  }

  /**
   * Verify directory structure
   */
  answers = await inquirer.prompt<Checks>(
    {
      type: 'confirm',
      name: 'confirmValdiPath',
      message:
        '/modules directory not found (did you run `valdi bootstrap` first?). Create module in current directory?',
      default: true,
      when(answers) {
        const curDir = process.cwd();
        const parsedPath = path.parse(curDir);
        if (parsedPath.base === 'modules') {
          // Create the new module in the current directory
          answers.valdiModulePath = curDir;
        } else if (curDir === bazelWorkspacePath) {
          // Create the new module in ./modules
          const valdiPath = path.join(curDir, 'modules');
          if (fs.existsSync(valdiPath)) {
            answers.valdiModulePath = valdiPath;
          }
        }

        return !answers.valdiModulePath;
      },
    },
    answers,
  );

  if (!answers.valdiModulePath && !answers.confirmValdiPath) {
    throw new CliError('Aborting.');
  }

  return answers;
}

async function getModuleName(argv: ArgumentsResolver<CommandParameters>): Promise<string> {
  return argv.getArgumentOrResolve('moduleName', async () => {
    const result = await inquirer.prompt<{ moduleName?: string }>(
      [
        {
          type: 'input',
          name: 'moduleName',
          message: 'Please provide a name for this module:',
          validate: (input: string) => {
            const validationError = validateProjectName(input);
            if (validationError) {
              return validationError;
            }

            const sanitized = sanitizeProjectName(input);
            const destPath = path.join(process.cwd(), sanitized);
            if (fileExists(destPath)) {
              return `Path ${destPath} already exists. Choose a different name or delete the folder and try again.`;
            }

            return true;
          },
        },
      ],
      {},
    );

    return sanitizeProjectName(result.moduleName ?? '');
  });
}

async function finalConfirmation(destPath: string, argv: ArgumentsResolver<CommandParameters>): Promise<boolean> {
  return argv.getArgumentOrResolve('skipChecks', async () => {
    const confirmMessage = `
Create module?
  Path:               ${wrapInColor(destPath, ANSI_COLORS.GREEN_COLOR)}
`;
    const answers = await inquirer.prompt<{ confirm?: boolean }>(
      [
        {
          type: 'confirm',
          name: 'confirm',
          message: confirmMessage,
          default: true,
        },
      ],
      {},
    );

    return answers.confirm ?? false;
  });
}

function getBazelWorkspaceRoot() {
  return new BazelClient().getWorkspaceRoot();
}

async function valdiNewModule(argv: ArgumentsResolver<CommandParameters>) {
  console.log(referenceString);
  const skipChecks = argv.getArgument('skipChecks');

  const checks: Checks = skipChecks ? {} : await promptChecks();
  const valdiModulePath = checks.valdiModulePath ?? path.join(await getBazelWorkspaceRoot(), 'modules');

  let moduleName = await getModuleName(argv);
  
  // Validate module name if provided via command line argument
  if (argv.getArgument('moduleName')) {
    const validationError = validateProjectName(moduleName);
    if (validationError) {
      throw new CliError(validationError);
    }
    moduleName = sanitizeProjectName(moduleName);
  }

  const destPath = path.join(valdiModulePath, moduleName);
  const didConfirm = skipChecks || (await finalConfirmation(destPath, argv));
  if (!didConfirm) {
    throw new CliError('Cancelled new module creation.');
  }

  /**
   * Do not overwrite existing files
   */
  if (fs.existsSync(destPath)) {
    throw new CliError(`Path already exists ${destPath}`);
  }

  const choices: CliChoice<ModuleTemplate>[] = ALL_MODULE_TEMPLATES.map(target => ({
    name: `${wrapInColor(target.name, ANSI_COLORS.GREEN_COLOR)} . ${target.description}`,
    value: target,
  }));

  const templateOption = argv.getArgument('template');
  const template = templateOption ? ALL_MODULE_TEMPLATES.find(template => template.path === templateOption) : undefined;
  const moduleTemplate = template ?? await getUserChoice(choices, `Please choose the module type:`);

  /**
   * Copy bootstrap files
   */
  const sourcePath = path.join(BOOTSTRAP_DIR_PATH, 'modules', moduleTemplate.path);
  const replacements: Replacements = {
    MODULE_NAME: moduleName,
    MODULE_NAME_PASCAL_CASED: toPascalCase(moduleName),
  };

  copyBootstrapFiles(sourcePath, destPath, replacements);

  // Finalize message
  console.log(`Success! New module directory: ${wrapInColor(destPath, ANSI_COLORS.GREEN_COLOR)}`);
}

export const command = `new_module [module-name]`;
export const describe = referenceString;
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .positional('module-name', {
      describe: 'Name of the Valdi module.',
    })
    .option('template', {
      describe: 'module template to use, will skip the prompt',
      choices: ALL_MODULE_TEMPLATES.map(template => template.path),
    })
    .option('skip-checks', {
      describe: 'Skips confirmation prompts.',
      type: 'boolean',
    });
};
export const handler = makeCommandHandler(valdiNewModule);
