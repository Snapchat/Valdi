import { Argument, ArgumentsParser } from 'valdi_cli/src/ArgumentsParser';
import { Path } from 'valdi_cli/src/Path';

import { compareResults } from './compare';
import { isPlatform, runIntegrationTests } from './run';

export async function runFullComparison(args: string[]): Promise<void> {
  const parser = new ArgumentsParser('full-comparison', ['full-comparison', ...args]);
  const beforeRepo: Argument<string> = parser.addString('--before-repo', 'Before Valdi repo root', true);
  const afterRepo: Argument<string> = parser.addString('--after-repo', 'After Valdi repo root', true);
  const beforePlatform: Argument<string> = parser.addString('--before-platform', 'Before platform', true);
  const afterPlatform: Argument<string> = parser.addString('--after-platform', 'After platform', true);
  const outputDirArg: Argument<string> = parser.addString('--output-dir', 'Output directory', true);
  const sharedDeviceId: Argument<string> = parser.addString('--device-id', 'Shared device id', false);
  const beforeDeviceId: Argument<string> = parser.addString('--before-device-id', 'Before device id', false);
  const afterDeviceId: Argument<string> = parser.addString('--after-device-id', 'After device id', false);
  const timeoutMs: Argument<string> = parser.addString('--timeout-ms', 'Timeout in milliseconds', false);
  const beforeBazelArgs: Argument<string> = parser.addString('--before-bazel-args', 'Before Bazel args', false);
  const afterBazelArgs: Argument<string> = parser.addString('--after-bazel-args', 'After Bazel args', false);
  const valdiBin: Argument<string> = parser.addString('--valdi-bin', 'Valdi binary path/name', false);
  const pixelThreshold: Argument<string> = parser.addString('--pixel-threshold', 'Pixel channel threshold', false);
  const failAbove: Argument<string> = parser.addString('--fail-above', 'Fail when max diff exceeds this percent', false);
  parser.parse();

  if (!isPlatform(beforePlatform.value) || !isPlatform(afterPlatform.value)) {
    throw new Error(fullComparisonUsage());
  }

  const outputDir = Path.resolve(outputDirArg.value!);
  outputDir.ensureDirectory();
  const beforeJson = outputDir.appending('before.json').toString();
  const afterJson = outputDir.appending('after.json').toString();

  await runIntegrationTests({
    repo: Path.resolve(beforeRepo.value!).toString(),
    platform: beforePlatform.value,
    output: beforeJson,
    deviceId: beforeDeviceId.value ?? sharedDeviceId.value,
    timeoutMs: timeoutMs.value,
    bazelArgs: beforeBazelArgs.value,
    valdiBin: valdiBin.value,
  });
  await runIntegrationTests({
    repo: Path.resolve(afterRepo.value!).toString(),
    platform: afterPlatform.value,
    output: afterJson,
    deviceId: afterDeviceId.value ?? sharedDeviceId.value,
    timeoutMs: timeoutMs.value,
    bazelArgs: afterBazelArgs.value,
    valdiBin: valdiBin.value,
  });

  compareResults({
    before: beforeJson,
    after: afterJson,
    outputDir: outputDir.appending('comparison').toString(),
    pixelThreshold: pixelThreshold.value,
    failAbove: failAbove.value,
  });
}

export function fullComparisonUsage(): string {
  return 'full-comparison --before-repo repoA --after-repo repoB --before-platform ios|android|macos|web --after-platform ios|android|macos|web --output-dir out';
}
