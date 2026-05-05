import type { Argv } from 'yargs';
import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import ora from 'ora';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { HermesConnection, listHermesDevices, HERMES_PORT } from '../../utils/hermesClient';
import { CliError } from '../../core/errors';
import { getUserChoice } from '../../utils/cliUtils';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  duration: number | undefined;
  output: string | undefined;
  port: number;
  context: string | undefined;
}

async function profileCapture(argv: ArgumentsResolver<CommandParameters>) {
  const duration = argv.getArgument('duration') as number | undefined;
  const outputOverride = argv.getArgument('output') as string | undefined;
  const port = argv.getArgument('port') as number;
  const contextOverride = argv.getArgument('context') as string | undefined;

  // ── Context selection ─────────────────────────────────────────────────────

  const devices = await listHermesDevices(port);

  if (devices.length === 0) {
    throw new CliError('No debuggable JS contexts found. Make sure the Valdi app is running.');
  }

  let targetId: string;
  if (contextOverride) {
    if (!devices.some((d) => d.id === contextOverride)) {
      throw new CliError(
        `Context "${contextOverride}" not found.\n` +
        `Available: ${devices.map((d) => d.id).join(', ')}`,
      );
    }
    targetId = contextOverride;
  } else if (devices.length === 1) {
    targetId = devices[0]!.id;
  } else {
    targetId = await getUserChoice(
      devices.map((d) => ({ name: `${d.id}  ${d.title}`, value: d.id })),
      'Multiple JS contexts found. Select one:',
    );
  }

  // ── Profiling ─────────────────────────────────────────────────────────────

  const conn = await HermesConnection.connect(port, targetId);
  try {
    await conn.startProfiling();

    const startMs = Date.now();
    const spinner = ora({ stream: process.stderr });

    await new Promise<void>((resolve) => {
      let timer: ReturnType<typeof setTimeout> | undefined;

      const stop = () => {
        if (timer) clearTimeout(timer);
        process.off('SIGINT', stop);
        process.exitCode = 0;
        resolve();
      };

      if (duration !== undefined) {
        spinner.start(`Profiling context "${targetId}"… (${duration}s)`);
        timer = setTimeout(stop, duration * 1000);
      } else {
        spinner.start(`Profiling context "${targetId}"… (Ctrl+C to stop)`);
      }

      process.once('SIGINT', stop);
    });

    spinner.text = 'Stopping profiler…';

    // ── Collect + write ──────────────────────────────────────────────────────

    const profile = await conn.stopProfiling();
    spinner.stop();

    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    const outPath = outputOverride
      ? path.resolve(outputOverride)
      : path.join(os.tmpdir(), `valdi-profile-${timestamp}.cpuprofile`);

    fs.writeFileSync(outPath, JSON.stringify(profile), 'utf8');

    const elapsedS = ((Date.now() - startMs) / 1000).toFixed(1);
    const sampleCount = profile.samples?.length ?? 0;
    const sizeKb = Math.round(fs.statSync(outPath).size / 1024);

    console.log(JSON.stringify({ path: outPath }));
    console.error(
      wrapInColor(`Profile saved: ${outPath}`, ANSI_COLORS.GREEN_COLOR) + '\n' +
      `  Duration: ${elapsedS}s  |  Samples: ${sampleCount}  |  Size: ${sizeKb} KB\n` +
      `  Open in: Chrome DevTools → Performance → Load  |  speedscope.app`,
    );
  } finally {
    conn.close();
  }
}

export const command = 'capture';
export const describe = 'Capture a CPU profile from the Hermes JS runtime';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .option('duration', {
      describe: 'Profile for N seconds then stop automatically (omit to stop with Ctrl+C)',
      type: 'number',
      alias: 'd',
    })
    .option('output', {
      describe: 'Output .cpuprofile file path (default: /tmp/valdi-profile-<timestamp>.cpuprofile)',
      type: 'string',
      alias: 'o',
    })
    .option('port', {
      describe: 'Hermes debug socket port',
      type: 'number',
      default: HERMES_PORT,
    })
    .option('context', {
      describe: 'JS context ID to profile (from /json/list); auto-selected if only one',
      type: 'string',
    });
};
export const handler = makeCommandHandler(profileCapture);
