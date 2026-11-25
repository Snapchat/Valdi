/* eslint-disable @typescript-eslint/no-non-null-assertion */
import assert from 'node:assert';
import { type ChildProcess } from 'node:child_process';
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

// A remote-cache download that goes idle past --remote_timeout fails the build with this
// message even though nothing is wrong with the code under test.
const REMOTE_CACHE_TIMEOUT = /Download of '[^']+' timed out/;

function killProcessGroup(child: ChildProcess): void {
  if (child.pid === undefined || child.exitCode !== null) {
    return;
  }
  try {
    process.kill(-child.pid, 'SIGTERM');
  } catch {
    // Already gone.
  }
}

/**
 * Runs `valdi build <platform>` and retries once if the build failed only because a remote
 * cache download timed out. Bazel keeps everything it already fetched, so the retry resumes
 * close to where the first attempt died instead of repeating the whole cold build. Any other
 * failure is not retried.
 */
async function buildWithCacheRetry(platform: string, attempts = 2): Promise<void> {
  for (let attempt = 1; ; attempt++) {
    const task = run('valdi', ['build', platform]);
    let stderr = '';
    task.process.stderr.on('data', (chunk: Buffer) => {
      stderr += chunk.toString('utf8');
    });
    try {
      await lookForOutput({ stderr: [/Build completed successfully, \d+ total actions?/], idleTimeoutMs: 120_000 }, task);
      return;
    } catch (error_) {
      // run() spawns detached, so an idle-timeout rejection leaves valdi and bazel running
      // (and holding the output base lock) unless the whole group is killed.
      killProcessGroup(task.process);
      if (attempt >= attempts || !REMOTE_CACHE_TIMEOUT.test(stderr)) {
        throw error_;
      }
      console.warn(`build ${platform} hit a remote cache download timeout; retrying (attempt ${attempt + 1} of ${attempts})`);
    }
  }
}

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
      // Match build android's idle budget: a cold/remote-cache bazel build can go quiet
      // for well over the 30s default, which otherwise fails this as a flake.
      await buildWithCacheRetry('ios');
    });

    // Compiling proves nothing about launching: UIKit enforces SDK-specific startup rules
    // (e.g. the UIScene life cycle on the iOS 27 SDK) when the process starts, so a build can
    // pass for an app that exits immediately.
    void it('launch ios', { skip: process.platform !== 'darwin', timeout: 30 * 60_000 }, async () => {
      await run('bash', [join(OPEN_SOURCE_DIR, 'tools/ci/ios_launch_smoke.sh'), PROJECT_ROOT], {
        pipeOutput: true,
      }).wait();
    });

    void it('build macos', async () => {
      await buildWithCacheRetry('macos');
    });

    void it('build android', async () => {
      await buildWithCacheRetry('android');
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
        // Match the build tests' idle budget for pauses while fetching remote-cache results.
        idleTimeoutMs: 120_000,
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
