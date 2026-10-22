import 'valdi_cli/src/Process';
import { Argument, ArgumentsParser } from 'valdi_cli/src/ArgumentsParser';

import { compareResults, compareUsage, printComparedFileHint } from './compare';
import { exportSnapshotsFromArgs, exportSnapshotsUsage } from './exportSnapshots';
import { fullComparisonUsage, runFullComparison } from './fullComparison';
import { runFromArgs, runUsage } from './run';
import { runSelfTest } from './selfTest';

function usage(): string {
  return [
    'Valdi Integration Test CLI',
    '',
    'Usage:',
    `  ${runUsage()}`,
    `  ${compareUsage()}`,
    `  ${fullComparisonUsage()}`,
    `  ${exportSnapshotsUsage()}`,
    '  self-test',
  ].join('\n');
}

async function main(): Promise<void> {
  const argv = process.argv ?? [];
  const command = argv[2];
  const args = argv.slice(3);

  if (!command || command === '--help' || command === 'help') {
    console.log(usage());
    return;
  }

  if (command === 'run') {
    await runFromArgs(args);
    return;
  }

  if (command === 'compare') {
    const parser = new ArgumentsParser('compare', ['compare', ...args]);
    const before: Argument<string> = parser.addString('--before', 'Before result JSON', true);
    const after: Argument<string> = parser.addString('--after', 'After result JSON', true);
    const outputDir: Argument<string> = parser.addString('--output-dir', 'Output directory', true);
    const pixelThreshold: Argument<string> = parser.addString('--pixel-threshold', 'Pixel channel threshold', false);
    const failAbove: Argument<string> = parser.addString('--fail-above', 'Fail when max diff exceeds this percent', false);
    parser.parse();
    compareResults({
      before: before.value!,
      after: after.value!,
      outputDir: outputDir.value!,
      pixelThreshold: pixelThreshold.value,
      failAbove: failAbove.value,
    });
    printComparedFileHint(outputDir.value!);
    return;
  }

  if (command === 'full-comparison') {
    await runFullComparison(args);
    return;
  }

  if (command === 'export-snapshots') {
    exportSnapshotsFromArgs(args);
    return;
  }

  if (command === 'self-test') {
    runSelfTest();
    return;
  }

  throw new Error(`Unknown command ${command}\n\n${usage()}`);
}

try {
  main().catch(error => {
    console.error(error instanceof Error ? error.message : String(error));
    if (process.exit) {
      process.exit(1);
    }
  });
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  if (process.exit) {
    process.exit(1);
  }
}
