import type { Argv } from 'yargs';
import { ANSI_COLORS } from '../core/constants';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import { LoadingIndicator } from '../utils/LoadingIndicator';
import { makeCommandHandler } from '../utils/errorUtils';
import { wrapInColor } from '../utils/logUtils';

interface CommandParameters {
  module: string | undefined;
  target: string | undefined;
  json: boolean;
  quick: boolean;
}

interface StepResult {
  name: string;
  status: 'pass' | 'fail' | 'skip';
  durationMs: number;
  message?: string;
}

async function runStep(
  name: string,
  fn: () => Promise<void>,
  json: boolean,
): Promise<StepResult> {
  const start = Date.now();
  if (!json) {
    console.log(wrapInColor(`▸ ${name}...`, ANSI_COLORS.YELLOW_COLOR));
  }
  try {
    await fn();
    const durationMs = Date.now() - start;
    if (!json) {
      console.log(wrapInColor(`  ✓ ${name} (${durationMs}ms)`, ANSI_COLORS.GREEN_COLOR));
    }
    return { name, status: 'pass', durationMs };
  } catch (error) {
    const durationMs = Date.now() - start;
    const message = error instanceof Error ? error.message : String(error);
    if (!json) {
      console.log(wrapInColor(`  ✗ ${name}: ${message}`, ANSI_COLORS.RED_COLOR));
    }
    return { name, status: 'fail', durationMs, message };
  }
}

async function getModuleTargets(client: BazelClient, module: string): Promise<string[]> {
  const allTargets = await client.queryTargetsByKindWithFilter('valdi_module', [module], 'pipe');
  const androidDebug = allTargets.filter((t: string) => t.includes('android.debug'));
  return androidDebug.length > 0 ? androidDebug : allTargets;
}

async function getTestTargets(client: BazelClient, module: string): Promise<string[]> {
  return await client.queryTargetsByKindWithFilter('test', [module], 'pipe');
}

async function valdiAgentCheck(argv: ArgumentsResolver<CommandParameters>) {
  const json = argv.getArgument('json') ?? false;
  const quick = argv.getArgument('quick') ?? false;
  const bazelClient = new BazelClient();

  const moduleName = argv.getArgument('module');
  const targetName = argv.getArgument('target');

  if (!moduleName && !targetName) {
    throw new Error('Either --module or --target is required');
  }

  const results: StepResult[] = [];
  const totalStart = Date.now();

  if (!json) {
    const mode = quick ? 'Quick' : 'Full';
    console.log(wrapInColor(`\n═══ Valdi Agent Check (${mode}) ═══\n`, ANSI_COLORS.GREEN_COLOR));
  }

  let buildTargets: string[] = [];
  let testTargets: string[] = [];

  if (targetName) {
    buildTargets = [targetName];
  } else if (moduleName) {
    const resolveResult = await runStep('Resolve module targets', async () => {
      const task = getModuleTargets(bazelClient, moduleName);
      buildTargets = json
        ? await task
        : await LoadingIndicator.fromTask(task)
            .setText(wrapInColor('Resolving module targets...', ANSI_COLORS.YELLOW_COLOR))
            .setSuccessText(wrapInColor('Resolved module targets.', ANSI_COLORS.GREEN_COLOR))
            .setFailureText(wrapInColor('Failed to resolve targets.', ANSI_COLORS.RED_COLOR))
            .show();
      if (buildTargets.length === 0) {
        throw new Error(`No valdi_module targets found for module "${moduleName}"`);
      }
    }, json);
    results.push(resolveResult);
    if (resolveResult.status === 'fail') {
      return outputResults(results, totalStart, json);
    }

    testTargets = await getTestTargets(bazelClient, moduleName).catch(() => []);
  }

  // Build (skipped in --quick mode because hot reloader handles compilation)
  if (!quick) {
    const buildResult = await runStep('Build', async () => {
      await bazelClient.buildTargets(buildTargets);
    }, json);
    results.push(buildResult);
  } else {
    if (!json) {
      console.log(wrapInColor('  ⊘ Build: skipped (--quick mode, hot reloader handles compilation)', ANSI_COLORS.YELLOW_COLOR));
    }
    results.push({ name: 'Build', status: 'skip', durationMs: 0, message: 'Skipped in quick mode' });
  }

  // Lint and Tests run in parallel after build
  const lintPromise = (buildTargets.length > 0)
    ? runStep('Lint check', async () => {
      const { runCliCommand } = await import('../utils/cliUtils');
      const { globSync } = await import('glob');
      const firstTarget = buildTargets[0] ?? '';
      const packagePath = firstTarget.replace(/^\/\//, '').replace(/:.*$/, '') || '.';
      const globPattern = `${packagePath}/**/*.{ts,tsx}`;
      const tsFiles = globSync(globPattern);

      if (tsFiles.length === 0) {
        return;
      }

      const result = await runCliCommand(
        `npx prettier --check "${globPattern}"`,
      );
      if (result.returnCode !== 0) {
        throw new Error('Lint check failed — files not formatted');
      }
    }, json)
    : Promise.resolve({ name: 'Lint check', status: 'skip' as const, durationMs: 0, message: 'No targets resolved for lint' });

  const testPromise = (testTargets.length > 0)
    ? runStep('Tests', async () => {
      await bazelClient.testTargets(testTargets, '');
    }, json)
    : (() => {
      if (!json) {
        console.log(wrapInColor('  ⊘ Tests: skipped (no test targets found)', ANSI_COLORS.YELLOW_COLOR));
      }
      return Promise.resolve({ name: 'Tests', status: 'skip' as const, durationMs: 0, message: 'No test targets found' });
    })();

  const [lintResult, testResult] = await Promise.all([lintPromise, testPromise]);
  results.push(lintResult);
  results.push(testResult);

  return outputResults(results, totalStart, json);
}

function outputResults(results: StepResult[], totalStart: number, json: boolean) {
  const totalDurationMs = Date.now() - totalStart;
  const passed = results.filter(r => r.status === 'pass').length;
  const failed = results.filter(r => r.status === 'fail').length;
  const skipped = results.filter(r => r.status === 'skip').length;

  if (json) {
    const output = {
      timestamp: new Date().toISOString(),
      totalDurationMs,
      summary: { passed, failed, skipped, total: results.length },
      results,
      success: failed === 0,
    };
    console.log(JSON.stringify(output, null, 2));
  } else {
    console.log(wrapInColor('\n═══ Summary ═══', ANSI_COLORS.GREEN_COLOR));
    console.log(`  Passed:  ${passed}`);
    console.log(`  Failed:  ${failed}`);
    console.log(`  Skipped: ${skipped}`);
    console.log(`  Total:   ${totalDurationMs}ms`);

    if (failed > 0) {
      console.log(wrapInColor('\n✗ Agent check FAILED', ANSI_COLORS.RED_COLOR));
      process.exitCode = 1;
    } else {
      console.log(wrapInColor('\n✓ Agent check PASSED', ANSI_COLORS.GREEN_COLOR));
    }
  }

  if (failed > 0) {
    process.exitCode = 1;
  }
}

export const command = 'agent-check [--module module_name] [--target target_name] [--quick]';
export const describe =
  'Validate a Valdi module: resolve targets, build, lint, and test in one command. '
  + 'Use --quick to skip the build step when the hot reloader is already compiling your changes. '
  + 'Use --json for structured output that AI agents can parse directly.';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('module', {
      describe: 'Name of the Valdi module to validate',
      type: 'string',
      requiresArg: true,
    })
    .option('target', {
      describe: 'Bazel target path to validate',
      type: 'string',
      requiresArg: true,
    })
    .option('quick', {
      describe: 'Skip the build step (use when hot reloader is running). Runs only lint + test.',
      type: 'boolean',
      default: false,
    })
    .option('json', {
      describe: 'Output results as machine-readable JSON',
      type: 'boolean',
      default: false,
    });
};
export const handler = makeCommandHandler(valdiAgentCheck);
