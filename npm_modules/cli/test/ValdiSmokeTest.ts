/* eslint-disable @typescript-eslint/no-non-null-assertion */
import assert from 'node:assert';
import { mkdir, rm } from 'node:fs/promises';
import { basename, join } from 'node:path';
import { afterEach, beforeEach, describe, it } from 'node:test';
import { lookForOutput } from './helpers/AsyncHelpers';
import { run } from './helpers/CommandHelpers';
import {
  atBeginning,
  createProject,
  findDefinitionForTokenInFile,
  insertLineAfter,
  insertLineBefore,
  lineMatches,
} from './helpers/ProjectHelpers';
import { type ProjectConfig } from './helpers/ProjectHelpers';
import { type TSServerClient, createClient, lineAndOffset } from './helpers/TypeScriptClient';
import { error } from 'node:console';


const PROJECT_ROOT = process.env['PROJECT_ROOT']!;
const PROJECT_NAME = process.env['PROJECT_NAME'] ?? basename(PROJECT_ROOT);

// eslint-disable-next-line unicorn/prefer-module
const runtimeDir = __dirname;

const OPEN_SOURCE_DIR = process.env['OPEN_SOURCE_DIR']!;

void describe('valdi ui app', () => {
  let client: TSServerClient;
  let project: ProjectConfig;

  beforeEach(async () => {
    await bootstrap(PROJECT_ROOT, PROJECT_NAME, 'ui_application');
    // Create a tsserver client to interact with tsserver
    client = createClient(runtimeDir);
    // Create the project instance which is a wrapper to find and modify files in valdi modules
    project = createProject(PROJECT_NAME, PROJECT_ROOT);
  });

  afterEach(async () => {
    // Shuts down the tsserver instance for this test
    await client.disconnect();
  });

  void describe('cli', () => {
    void it('build ios', async () => {
      const task = run('valdi', ['build', 'ios']);
      await lookForOutput({ stderr: [/Build completed successfully, \d+ total actions?/] }, task);
    });

    void it('build macos', async () => {
      const task = run('valdi', ['build', 'macos']);
      await lookForOutput({ stderr: [/Build completed successfully, \d+ total actions?/] }, task);
    });

    void it('build android', { skip: true }, async () => {
      const task = run('valdi', ['build', 'android']);
      await lookForOutput({ stderr: [/Build completed successfully, \d+ total actions?/] }, task);
    });

    void it('valdi doctor', async () => {
      const task = run('valdi', ['doctor']);
      await lookForOutput({ stdout: [
        'Valdi Doctor Report',
        'Node.js installation',
        'Bazel installation',
        'Android installation',
        'iOS development',
        'Java installation',
        'Development tools'
      ]}, task)
    });

    /**
     * Runs `valdi new_module`
     * Updates `App.tsx` to import the module and use it
     * Checks that the tsserver discovers the reference can be found
     */
    void it('new_module ui_component', async () => {
      const moduleName = 'testNewModule';
      const task = run('valdi', ['new_module', '--skip-checks', '--template', 'ui_component', moduleName]);

      await lookForOutput({ stdout: ['Success! New module directory:'] }, task);

      // Add testNewModule bazel dependency for valdi_app
      await project.updateFile(
        'BUILD.bazel',
        'valdi_app',
        insertLineAfter(lineMatches('deps = ['), `\t\t"//modules/${moduleName}",\n`),
      );

      // Insert new import statement for the new module at the top of the file
      await project.updateFile(
        'App.tsx',
        'valdi_app',
        insertLineBefore(() => true, `import { NewModuleComponent } from '${moduleName}/src/NewModuleComponent';\n`),
      );
      // Try to use the component in onRender
      await project.updateFile(
        'App.tsx',
        'valdi_app',
        insertLineBefore(lineMatches('</view>;'), '<NewModuleComponent />\n'),
      );

      await run('valdi', ['projectsync'], { pipeOutput: true }).wait();

      // We updated the file on the filesystem, the ts client needs to know to refresh the file
      const file = await project.findFile('App.tsx', 'valdi_app');
      await client.command('open', { file });

      // Throws if it cannot find a valid TypeScript code reference
      await findDefinitionForTokenInFile({ project, client }, 'NewModuleComponent />', 'App.tsx', 'valdi_app');

      await client.command('close', { file });
    });
  });

  void describe('typescript', () => {
    void it('finds project references', async () => {
      await client.command('open', { file: await project.findFile('App.tsx', 'valdi_app') });

      await Promise.all(
        ['StatefulComponent', 'Label', 'Device', 'view>', 'style='].map(token =>
          findDefinitionForTokenInFile({ client, project }, token, 'App.tsx', 'valdi_app'),
        ),
      );

      await client.command('close', { file: await project.findFile('App.tsx', 'valdi_app') });
    });

    /**
     * Check for VSCode completions when typing "<l" inside the App `onRender` method.
     *
     * It should offer `<label` and `<layout`.
     */
    void it('finds code completions', async () => {
      const file = await project.findFile('App.tsx', 'valdi_app');
      await client.command('open', { file });
      await client.command('reload', { file });

      const appSource = await project.findFile('App.tsx');

      // Look for the whole definition block of onRender by asking tsserver for the definition
      const view = await lineAndOffset('onRender', appSource);
      const onRenderDef = (await client.command('definition', { file: appSource, ...view })) as {
        body?: { contextEnd: { line: number; offset: number } }[];
      };

      const defInfo = onRenderDef.body![0]!;

      // Insert the '<' character before the ending "}" of onRender
      const insertionLocation = {
        file: appSource,
        line: defInfo.contextEnd.line,
        endLine: defInfo.contextEnd.line,
        offset: defInfo.contextEnd.offset - 2,
        endOffset: defInfo.contextEnd.offset,
      };

      // Tell tsserver to update its representatin of the file
      await client.command('change', { insertString: '<l', ...insertionLocation });

      // Ask for the completion info at the place the '<l' was inserted
      const info = (await client.command('completionInfo', {
        file: appSource,
        ...defInfo.contextEnd,
        offset: defInfo.contextEnd.offset,
        prefix: 'l',
      })) as { body?: { entries: { name: string }[] } };

      // We expect certain items to be available as completions
      const completionsExpected = ['label', 'layout'];
      const missing = completionsExpected.filter(
        item => info.body!.entries.some(({ name }) => name === item) === undefined,
      );

      if (missing.length > 0) {
        throw new Error('missing completions: ' + missing.join(', '));
      }

      await client.command('close', { file });
    });

    void it('has localized strings', async () => {
      await project.updateFile(
        'BUILD.bazel',
        'valdi_app',
        insertLineBefore(lineMatches('visibility = ["//visibility:public"],'), '    strings_dir = "strings",\n'),
      );

      const strings = {
        l10nsvc: 'sourcefile',
        welcome: {
          defaultMessage: 'Hello World',
        },
      };

      await project.createFile('strings/strings-en.json', 'valdi_app', JSON.stringify(strings, null, '  '));
      await run('valdi', ['projectsync'], { pipeOutput: true }).wait();

      await project.updateFile(
        'App.tsx',
        'valdi_app',
        insertLineBefore(atBeginning, "import strings from './Strings'\n"),
      );

      const appFile = await project.findFile('App.tsx', 'valdi_app');
      await client.command('open', { file: appFile });
      const [pathDescriptor] = await findDefinitionForTokenInFile(
        { project, client },
        'strings',
        'App.tsx',
        'valdi_app',
      );

      assert.match(pathDescriptor, /\/generated_ts\/valdi_app\/src\/Strings\.d\.ts:\d+:\d+/);

      await client.command('close', { file: appFile });
    });
  });
});

void describe('valdi cli app', () => {
  const CLI_PROJECT_ROOT = join(PROJECT_ROOT, 'valdi_cli_app');
  const CLI_PROJECT_NAME = basename(CLI_PROJECT_ROOT);

  beforeEach(async () => {
    await bootstrap(CLI_PROJECT_ROOT, CLI_PROJECT_NAME, 'cli_application');
  });

  void it('install cli', async () => {
    const task = run('valdi', ['install', 'cli'], { pipeOutput: true });
    await lookForOutput(
      {
        // means the bazel build was succesful
        stderr: [/Build completed successfully, \d+ total actions?/],
        // The default bootstrapped app logs "Hello world!"
        stdout: ['Hello world!'],
      },
      task,
    );
  });
});

/**
 * Sets up a valdi project for tests
 *
 *  - Delete's the existing directory (if any)
 *  - Re-creates the directory
 *  - Changes the currenty working directory to the new directory
 *  - Runs `valdi bootstrap` linked to the OPEN_SOURCE_DIR for `@@valdi`
 *  - Runs `valdi projectsync`
 */
async function bootstrap(
  projectRootPath: string,
  projectName: string,
  applicationType: 'ui_application' | 'cli_application',
) {
  // Delete and recreate the bootstrapped project
  await rm(projectRootPath, { recursive: true, force: true }).catch((error) => {
    // Logging the failure; if it doesn't exist the force flag will handle it 
    console.error('Failed to remove', projectRootPath, ':', error);
  });
  await mkdir(projectRootPath, { recursive: true });

  // Run all commands from within the projectRootPath
  process.chdir(projectRootPath);

  await run(
    'valdi',
    ['bootstrap', '-y', `-n=${projectName}`, `-t=${applicationType}`, `-l=${OPEN_SOURCE_DIR}`, '-p', '--with-cleanup'],
    {
      pipeOutput: true,
    },
  ).wait();

  await run('valdi', ['projectsync'], { pipeOutput: true }).wait();
}
