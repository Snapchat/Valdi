import fs from 'fs';
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
  resolveFilePath,
} from '../utils/fileUtils';
import { wrapInColor } from '../utils/logUtils';
import { toPascalCase, sanitizeProjectName, validateProjectName } from '../utils/stringUtils';
import { getAllProjectSyncTargets, runProjectSync } from './projectsync';
import { getLatestReleaseTag } from '../utils/githubUtils';

interface CommandParameters {
  confirmBootstrap: boolean;
  projectName: string;
  applicationType: string;
  valdiVersion: string;
  valdiWidgetsVersion: string;
  localValdiPath: string;
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

/** Pinned Valdi release used by default for reproducible bootstraps. Bump when cutting a new Valdi release. */
const DEFAULT_VALDI_RELEASE_TAG = 'beta-0.0.3';
/** Pinned Valdi_Widgets release used by default. Should match the Valdi release cycle. */
const DEFAULT_VALDI_WIDGETS_RELEASE_TAG = 'beta-0.0.3';


function isAlreadyInitialized(): boolean {
  return fileExists(path.join(process.cwd(), 'MODULE.bazel')) || fileExists(path.join(process.cwd(), 'WORKSPACE'));
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
    let projectName = '';
    let isValid = false;

    while (!isValid) {
      const result = await inquirer.prompt<{ projectName: string }>([
        {
          type: 'input',
          name: 'projectName',
          message: 'Please provide a name for this project:',
          default: defaultProjectName,
        },
      ]);

      projectName = result.projectName;
      const validationError = validateProjectName(projectName);

      if (validationError) {
        console.log(wrapInColor(`\n❌ ${validationError}\n`, ANSI_COLORS.RED_COLOR));
        continue;
      }

      const sanitized = sanitizeProjectName(projectName);
      if (sanitized !== projectName) {
        console.log(
          wrapInColor(
            `\n⚠️  Project name will be sanitized from "${projectName}" to "${sanitized}"`,
            ANSI_COLORS.YELLOW_COLOR,
          ),
        );
        const confirm = await getUserConfirmation('Do you want to continue with this name?', true);
        if (!confirm) {
          continue;
        }
      }

      isValid = true;
    }

    return sanitizeProjectName(projectName);
  });
}

function resolveValdiReleaseTag(
  argv: ArgumentsResolver<CommandParameters>,
  gitUrl: string,
  defaultTag: string,
  versionOption: 'valdiVersion' | 'valdiWidgetsVersion' = 'valdiVersion',
): Promise<string> {
  const override = argv.getArgument(versionOption);
  if (!override) {
    return Promise.resolve(defaultTag);
  }
  if (override === 'latest') {
    return getLatestReleaseTag(gitUrl);
  }
  return Promise.resolve(override);
}


// Create files from templates
function initializeConfigFiles(
  projectName: string,
  template: ApplicationTemplate,
  valdiReleaseTag: string,
  valdiWidgetsReleaseTag: string,
) {

  const TEMPLATE_FILES = [
    TemplateFile.init(TEMPLATE_BASE_PATHS.USER_CONFIG).withOutputPath(resolveFilePath(VALDI_CONFIG_PATHS[0] ?? '')),
    TemplateFile.init(TEMPLATE_BASE_PATHS.BAZEL_VERSION),
    TemplateFile.init(TEMPLATE_BASE_PATHS.MODULE_BAZEL).withReplacements({
      WORKSPACE_NAME: projectName,
      VALDI_RELEASE_TAG: valdiReleaseTag,
      VALDI_WIDGETS_RELEASE_TAG: valdiWidgetsReleaseTag,
    }),
    TemplateFile.init(TEMPLATE_BASE_PATHS.BAZEL_RC).withReplacements({
      VALDI_RELEASE_TAG: valdiReleaseTag,
    }),
    TemplateFile.init(TEMPLATE_BASE_PATHS.README),
    TemplateFile.init(TEMPLATE_BASE_PATHS.GIT_IGNORE),
    TemplateFile.init(TEMPLATE_BASE_PATHS.WATCHMAN_CONFIG),
    TemplateFile.init(TEMPLATE_BASE_PATHS.EDITOR_CONFIG),
    TemplateFile.init(TEMPLATE_BASE_PATHS.AGENTS).withReplacements({ MODULE_NAME: projectName }),
  ];

  TEMPLATE_FILES.forEach(templateFile => {
    console.log(
      wrapInColor(`Creating ${templateFile.baseName} in ${templateFile.outputPath}...`, ANSI_COLORS.YELLOW_COLOR),
    );
    templateFile.expandTemplate();
  });

  // Copy shared bootstrap directories
  const replacements: Replacements = {
    MODULE_NAME: projectName,
    MODULE_NAME_PASCAL_CASED: toPascalCase(projectName),
  };
  const githubSourcePath = path.join(BOOTSTRAP_DIR_PATH, '.github');
  if (fileExists(githubSourcePath)) {
    console.log(wrapInColor('Creating GitHub templates...', ANSI_COLORS.YELLOW_COLOR));
    const githubDestPath = path.join(process.cwd(), '.github');
    copyBootstrapFiles(githubSourcePath, githubDestPath, replacements);
  }

  // Setup hello world application
  console.log(wrapInColor(`Initializing ${template.name} application...`, ANSI_COLORS.YELLOW_COLOR));
  const sourcePath = path.join(BOOTSTRAP_DIR_PATH, 'apps', template.path);
  const destPath = process.cwd();

  copyBootstrapFiles(sourcePath, destPath, replacements);
}

const VALDI_SUB_MODULES: readonly { name: string; subPath: string }[] = [
  { name: 'android_macros', subPath: 'bzl/macros' },
  { name: 'snap_macros', subPath: 'bzl/valdi/snap_macros' },
  { name: 'snap_client_toolchains', subPath: 'bzl/toolchains' },
  { name: 'snap_platforms', subPath: 'bzl/platforms' },
  { name: 'skia_user_config', subPath: 'third-party/skia_user_config' },
  { name: 'rules_hdrs', subPath: 'third-party/rules_hdrs' },
  { name: 'valdi_toolchain', subPath: 'bin' },
  { name: 'resvg_libs', subPath: 'third-party/resvg/resvg_libs' },
];

function applyLocalOverrides(localValdiPath: string) {
  const resolvedPath = path.resolve(localValdiPath);
  const registryPath = path.join(resolvedPath, 'registry');

  const moduleBazelPath = path.join(process.cwd(), 'MODULE.bazel');
  let content = fs.readFileSync(moduleBazelPath, 'utf-8');

  // Replace Valdi archive variables + override with local_path_override
  content = content.replace(
    /VALDI_TAG = .*\nVALDI_URL = .*\nVALDI_PREFIX = .*\n\nbazel_dep\(name = "valdi".*\)\narchive_override\(\n\s+module_name = "valdi",\n\s+urls = \[VALDI_URL\],\n\s+strip_prefix = VALDI_PREFIX,\n\)/,
    `bazel_dep(name = "valdi", version = "0.1")\nlocal_path_override(module_name = "valdi", path = "${resolvedPath}")`,
  );

  // Replace sub-module archive_overrides with local_path_override
  for (const sub of VALDI_SUB_MODULES) {
    content = content.replace(
      new RegExp(`bazel_dep\\(name = "${sub.name}"\\)\\narchive_override\\(module_name = "${sub.name}".*\\)`),
      `bazel_dep(name = "${sub.name}")\nlocal_path_override(module_name = "${sub.name}", path = "${resolvedPath}/${sub.subPath}")`,
    );
  }

  // Remove Widgets section entirely in local mode (not available locally)
  content = content.replace(
    /# -- Valdi Widgets.*\nWIDGETS_TAG = .*\n\nbazel_dep\(name = "valdi_widgets"\)\narchive_override\(\n\s+module_name = "valdi_widgets",\n\s+urls = \[.*\],\n\s+strip_prefix = .*,\n\)/,
    '',
  );

  fs.writeFileSync(moduleBazelPath, content);

  // Update .bazelrc to use local registry
  const bazelrcPath = path.join(process.cwd(), '.bazelrc');
  let bazelrc = fs.readFileSync(bazelrcPath, 'utf-8');
  bazelrc = bazelrc.replace(
    /common --registry=https:\/\/raw\.githubusercontent\.com\/Snapchat\/Valdi\/.*\/registry/,
    `common --registry=file://${registryPath}`,
  );
  fs.writeFileSync(bazelrcPath, bazelrc);
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
  let projectName = await getProjectName(argv);
  
  // Validate project name if provided via command line argument
  if (argv.getArgument('projectName')) {
    const validationError = validateProjectName(projectName);
    if (validationError) {
      throw new CliError(validationError);
    }
    projectName = sanitizeProjectName(projectName);
  }
  
  if (!projectName) {
    throw new CliError('Project name cannot be empty.');
  }

  const valdiReleaseTag = await resolveValdiReleaseTag(argv, VALDI_GIT_URL, DEFAULT_VALDI_RELEASE_TAG);
  const valdiWidgetsReleaseTag = await resolveValdiReleaseTag(
    argv,
    VALDI_WIDGETS_GIT_URL,
    DEFAULT_VALDI_WIDGETS_RELEASE_TAG,
    'valdiWidgetsVersion',
  );

  // Creating basic config files and Hello World application
  console.log(wrapInColor('Initializing config files...', ANSI_COLORS.BLUE_COLOR));
  initializeConfigFiles(projectName, applicationType, valdiReleaseTag, valdiWidgetsReleaseTag);

  const localValdiPath = argv.getArgument('localValdiPath');
  if (localValdiPath) {
    console.log(wrapInColor(`Applying local overrides for ${localValdiPath}...`, ANSI_COLORS.YELLOW_COLOR));
    applyLocalOverrides(localValdiPath);
  }

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
    .option('valdiVersion', {
      describe: `Valdi release tag or branch to use (e.g. beta-0.1.0, main). Use "latest" for the GitHub latest release, or "main" for bleeding edge. Default: ${DEFAULT_VALDI_RELEASE_TAG}`,
      type: 'string',
    })
    .option('valdiWidgetsVersion', {
      describe: `Valdi_Widgets release tag or branch to use. Use "latest" for the GitHub latest release, or "main" for bleeding edge. Default: ${DEFAULT_VALDI_WIDGETS_RELEASE_TAG}`,
      type: 'string',
    })
    .option('localValdiPath', {
      describe: 'Path to a local Valdi checkout. Generates local_path_override entries instead of archive_override.',
      type: 'string',
      alias: 'l',
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
