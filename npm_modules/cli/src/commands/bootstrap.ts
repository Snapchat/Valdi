import path from 'path';
import inquirer from 'inquirer';
import type { Argv } from 'yargs';
import { ANSI_COLORS, BOOTSTRAP_DIR_PATH, TEMPLATE_BASE_PATHS, VALDI_CONFIG_PATHS } from '../core/constants';
import { CliError } from '../core/errors';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import type { CliChoice } from '../utils/cliUtils';
import { getUserChoice, getUserConfirmation } from '../utils/cliUtils';
import { copyBootstrapFiles } from '../utils/copyBootstrapFiles';
import { makeCommandHandler } from '../utils/errorUtils';
import type { Replacements } from '../utils/fileUtils';
import {
  TemplateFile,
  deleteAll,
  fileExists,
  isDirectoryEmpty,
  processReplacements,
  resolveFilePath,
} from '../utils/fileUtils';
import { wrapInColor } from '../utils/logUtils';
import { toPascalCase } from '../utils/stringUtils';
import { getAllProjectSyncTargets, runProjectSync } from './projectsync';
import { resolveLatestReleaseRef } from '../utils/githubUtils';

interface CommandParameters {
  confirmBootstrap: boolean;
  projectName: string;
  applicationType: string;
  valdiImport: string;
  valdiWidgetsImport: string;
  skipProjectsync: boolean;
  withCleanup: boolean;
}

interface ApplicationTemplate {
  name: string;
  path: string;
  description: string;
}

const ALL_APPLICATION_TEMPLATES: readonly ApplicationTemplate[] = [
  {
    name: 'UI Application',
    path: 'ui_application',
    description: `An application rendering UI in a window on iOS, Android and macOS.`,
  },
  {
    name: 'CLI Application',
    path: 'cli_application',
    description: `A command line application that runs in a terminal`,
  },
];

const VALDI_GIT_URL = 'https://github.com/Snapchat/Valdi';
const VALDI_WIDGETS_GIT_URL = 'https://github.com/Snapchat/Valdi_Widgets';

const DEFAULT_VALDI_IMPORT = `
http_archive(
    name = "valdi",
    strip_prefix = "Valdi-{{VALDI_RELEASE_TAG}}",
    url = "${VALDI_GIT_URL}/archive/{{VALDI_RELEASE_REF}}.tar.gz",
)`;

const DEFAULT_VALDI_WIDGETS_IMPORT = `
http_archive(
    name = "valdi_widgets",
    strip_prefix = "Valdi_Widgets-{{VALDI_WIDGETS_RELEASE_TAG}}",
    url = "${VALDI_WIDGETS_GIT_URL}/archive/{{VALDI_WIDGETS_RELEASE_REF}}.tar.gz",
)`;

const LOCAL_VALDI_IMPORT_TEMPLATE = `
local_repository(
    name = "valdi",
    path = "{{PATH}}",
)
`;

const LOCAL_VALDI_WIDGETS_IMPORT_TEMPLATE = `
local_repository(
    name = "valdi_widgets",
    path = "{{PATH}}",
)
`;

function isAlreadyInitialized(): boolean {
  return fileExists(path.join(process.cwd(), 'WORKSPACE'));
}

async function getShouldBootstrap(argv: ArgumentsResolver<CommandParameters>): Promise<boolean> {
  // Get confirmation on directory path
  return argv.getArgumentOrResolve('confirmBootstrap', () => {
    return getUserConfirmation('Do you want to create a new project in the current directory?', true);
  });
}

const defaultProjectName = path.basename(process.cwd());

async function getApplicationType(argv: ArgumentsResolver<CommandParameters>): Promise<ApplicationTemplate> {
  const applicationType = argv.getArgument('applicationType');

  if (applicationType) {
    const target = ALL_APPLICATION_TEMPLATES.find(target => target.path === applicationType);

    if (target) {
      return target;
    } else {
      throw new CliError(`Unknown application type: ${applicationType}`);
    }
  }

  const choices: CliChoice<ApplicationTemplate>[] = ALL_APPLICATION_TEMPLATES.map(target => ({
    name: `${wrapInColor(target.name, ANSI_COLORS.GREEN_COLOR)} . ${target.description}`,
    value: target,
  }));

  return await getUserChoice(choices, `Please choose the application type:`);
}

async function getProjectName(argv: ArgumentsResolver<CommandParameters>): Promise<string> {
  return argv.getArgumentOrResolve('projectName', async () => {
    const result = await inquirer.prompt<{ projectName: string }>([
      {
        type: 'input',
        name: 'projectName',
        message: 'Please provide a name for this project:',
        default: defaultProjectName,
      },
    ]);

    return result.projectName;
  });
}

function getValdiImport(argv: ArgumentsResolver<CommandParameters>, valdiReleaseRef: string): string {
  const valdiImport = argv.getArgument('valdiImport');
  if (valdiImport) {
    return processReplacements(LOCAL_VALDI_IMPORT_TEMPLATE, { PATH: valdiImport });
  } else {
    const valdiReleaseTag = valdiReleaseRef.split('/').pop()!;
    return processReplacements(DEFAULT_VALDI_IMPORT, {
      VALDI_RELEASE_REF: valdiReleaseRef,
      VALDI_RELEASE_TAG: valdiReleaseTag,
    });
  }
}

function getValdiWidgetsImport(argv: ArgumentsResolver<CommandParameters>, valdiWidgetsReleaseRef: string): string {
  const valdiWidgetsImport = argv.getArgument('valdiWidgetsImport');
  if (valdiWidgetsImport) {
    return processReplacements(LOCAL_VALDI_WIDGETS_IMPORT_TEMPLATE, { PATH: valdiWidgetsImport });
  } else {
    const valdiWidgetsReleaseTag = valdiWidgetsReleaseRef.split('/').pop()!;
    return processReplacements(DEFAULT_VALDI_WIDGETS_IMPORT, {
      VALDI_WIDGETS_RELEASE_REF: valdiWidgetsReleaseRef,
      VALDI_WIDGETS_RELEASE_TAG: valdiWidgetsReleaseTag,
    });
  }
}

// Create files from templates
function initializeConfigFiles(
  projectName: string,
  template: ApplicationTemplate,
  valdiImport: string,
  valdiWidgetsImport: string,
) {

  const TEMPLATE_FILES = [
    TemplateFile.init(TEMPLATE_BASE_PATHS.USER_CONFIG).withOutputPath(resolveFilePath(VALDI_CONFIG_PATHS[0] ?? '')),
    TemplateFile.init(TEMPLATE_BASE_PATHS.BAZEL_VERSION),
    TemplateFile.init(TEMPLATE_BASE_PATHS.WORKSPACE).withReplacements({
      WORKSPACE_NAME: projectName,
      VALDI_IMPORT: valdiImport,
      VALDI_WIDGETS_IMPORT: valdiWidgetsImport,
    }),
    TemplateFile.init(TEMPLATE_BASE_PATHS.BAZEL_RC),
    TemplateFile.init(TEMPLATE_BASE_PATHS.README),
    TemplateFile.init(TEMPLATE_BASE_PATHS.GIT_IGNORE),
    TemplateFile.init(TEMPLATE_BASE_PATHS.WATCHMAN_CONFIG),
  ];

  TEMPLATE_FILES.forEach(templateFile => {
    console.log(
      wrapInColor(`Creating ${templateFile.baseName} in ${templateFile.outputPath}...`, ANSI_COLORS.YELLOW_COLOR),
    );
    templateFile.expandTemplate();
  });

  // Setup hello world application
  console.log(wrapInColor(`Initializing ${template.name} application...`, ANSI_COLORS.YELLOW_COLOR));
  const sourcePath = path.join(BOOTSTRAP_DIR_PATH, 'apps', template.path);
  const destPath = process.cwd();

  const replacements: Replacements = {
    MODULE_NAME: projectName,
    MODULE_NAME_PASCAL_CASED: toPascalCase(projectName),
  };

  copyBootstrapFiles(sourcePath, destPath, replacements);
}

async function valdiBootstrap(argv: ArgumentsResolver<CommandParameters>) {
  // Set up a Valdi project in the current directory. Will go through a flow to ask some questions to the user like the name of the application. This will create the following files:
  // user_config.yaml: User configuration file
  // application_names.bzl: Bazel macro file with application names
  const curDir = process.cwd();

  console.log(`\nCurrent directory: ${wrapInColor(curDir, ANSI_COLORS.GREEN_COLOR)}`);

  if (isAlreadyInitialized()) {
    // Check for existing project files
    // If the directory already contains a project, prompt the user to re-run command with explicit --with-cleanup flag
    if (argv.getArgument('withCleanup')) {
      console.log(wrapInColor('Deleting all files in current directory...', ANSI_COLORS.YELLOW_COLOR));
      deleteAll(curDir);
    } else {
      throw new CliError(
        `Detected existing project files. Please remove them before re-running 'valdi bootstrap'.\nYou can run '${wrapInColor('valdi bootstrap --with-cleanup', ANSI_COLORS.YELLOW_COLOR, ANSI_COLORS.RED_COLOR)}' to remove ALL FILES in the current directory.`,
      );
    }
  } else if (!isDirectoryEmpty(curDir)) {
    // Folder contains non project files
    // Prompt for manual clean up to prevent accidental deletion of important files
    throw new CliError(
      "Current directory is not empty. Please run 'valdi bootstrap' in an empty directory or manually clean up existing files.",
    );
  }

  if (!(await getShouldBootstrap(argv))) {
    throw new CliError('Bootstrap process aborted.');
  }

  const applicationType = await getApplicationType(argv);

  // Prompt user for input
  // - Application Name
  const projectName = await getProjectName(argv);
  if (!projectName) {
    throw new CliError('Project name cannot be empty.');
  }

  const valdiCommitHash = await resolveLatestReleaseRef(`${VALDI_GIT_URL}.git`);
  const valdiWidgetsCommitHash = await resolveLatestReleaseRef(`${VALDI_WIDGETS_GIT_URL}.git`);

  const valdiImport = getValdiImport(argv, valdiCommitHash);
  const valdiWidgetsImport = getValdiWidgetsImport(argv, valdiWidgetsCommitHash);

  // Creating basic config files and Hello World application
  console.log(wrapInColor('Initializing config files...', ANSI_COLORS.BLUE_COLOR));
  initializeConfigFiles(projectName, applicationType, valdiImport, valdiWidgetsImport);

  // Check bazel version matches .bazelversion
  console.log(wrapInColor('Verifying Bazel installation...', ANSI_COLORS.BLUE_COLOR));
  const bazel = new BazelClient();
  const [returnCode, versionInfo, errors] = await bazel.getVersion();

  if (returnCode !== 0) {
    throw new CliError(`Bazel installation failed to verify with errors: ${errors.toString()}`);
  }

  console.log(versionInfo);

  if (argv.getArgument('skipProjectsync')) {
    console.log(wrapInColor('Skipping projectsync', ANSI_COLORS.YELLOW_COLOR));
  } else {
    console.log(wrapInColor('Running projectsync...', ANSI_COLORS.BLUE_COLOR));
    // eslint-disable-next-line unicorn/no-useless-undefined
    await runProjectSync(bazel, await getAllProjectSyncTargets(bazel), undefined, true);
  }

  // Finalize message
  console.log(wrapInColor('Bootstrap complete!', ANSI_COLORS.GREEN_COLOR));
}

export const command = 'bootstrap';
export const describe = 'Prepares and initializes a project to build and run in Valdi';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('confirmBootstrap', {
      describe: 'Skip confirmation prompt and start bootstrap process',
      type: 'boolean',
      alias: 'y',
    })
    .option('applicationType', {
      describe: 'Type of application to create',
      alias: 't',
    })
    .option('projectName', {
      describe: 'Name of the project',
      type: 'string',
      alias: 'n',
    })
    .option('valdiImport', {
      describe:
        'Path to a local checkout of the Valdi repo. If not specified, uses the latest release from GitHub.',
      type: 'string',
      alias: 'l',
    })
    .option('valdiWidgetsImport', {
      describe:
        'Path to a local checkout of the Valdi_Widgets repo. If not specified, uses the latest release from GitHub.',
      type: 'string',
      alias: 'w',
    })
    .option('skipProjectsync', {
      describe: 'Skip projectsync for testing purposes',
      type: 'boolean',
      alias: 'p',
    })
    .option('withCleanup', {
      describe: 'Deletes all existing files in the current directory before initiating bootstrap',
      type: 'boolean',
      alias: 'c',
    });
};
export const handler = makeCommandHandler(valdiBootstrap);
