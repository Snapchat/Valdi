import { fs } from 'file_system/src/FileSystem';
import { Path } from 'valdi_cli/src/Path';

import { createRgbaBitmap, encodeBase64Png } from './png';
import { compareResults, type ComparisonSummary } from './compare';
import type { IntegrationTestCaseResult, IntegrationTestResult } from './types';

function assert(value: boolean, message: string): void {
  if (!value) {
    throw new Error(message);
  }
}

function pngBase64(width: number, height: number, pixels: number[]): string {
  const bitmap = createRgbaBitmap(width, height, new Uint8Array(pixels));
  try {
    return encodeBase64Png(bitmap);
  } finally {
    bitmap.dispose();
  }
}

function result(cases: IntegrationTestCaseResult[]): IntegrationTestResult {
  return {
    schemaVersion: 1,
    platform: 'self-test',
    generatedAt: '2026-01-01T00:00:00.000Z',
    cases,
  };
}

function writeJson(dir: Path, name: string, value: IntegrationTestResult): string {
  const file = dir.appending(name).toString();
  fs.writeFileSync(file, JSON.stringify(value, null, 2));
  return file;
}

function comparisonCase(summary: ComparisonSummary, id: string) {
  const item = summary.cases.find(testCase => testCase.id === id);
  assert(item !== undefined, `Expected comparison case ${id}`);
  return item!;
}

export function runSelfTest(): void {
  const dir = Path.fromString('/tmp').appending(`valdi-integration-cli-self-test-${Date.now()}`);
  dir.ensureDirectory();
  const dirString = dir.toString();

  const red = pngBase64(1, 1, [255, 0, 0, 255]);
  const beforeIdentical = writeJson(
    dir,
    'before-identical.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: red, observations: 'same', status: 'passed' },
    ]),
  );
  const afterIdentical = writeJson(
    dir,
    'after-identical.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: red, observations: 'same', status: 'passed' },
    ]),
  );
  const identical = compareResults({
    before: beforeIdentical,
    after: afterIdentical,
    outputDir: dir.appending('identical').toString(),
  });
  assert(identical.maxDiffPercent === 0, 'identical images should have zero diff');
  assert(!comparisonCase(identical, 'case').observationsChanged, 'identical observations should not change');
  const htmlReportPath = dir.appending('identical').appending('index.html').toString();
  assert(fs.existsSync(htmlReportPath), 'comparison should write an HTML report');
  const htmlReport = fs.readFileSync(htmlReportPath, { encoding: 'utf8' }) as string;
  assert(htmlReport.includes('Valdi Integration Diff'), 'HTML report should contain its title');
  assert(htmlReport.includes('Case'), 'HTML report should contain comparison cases');

  const beforePixel = pngBase64(2, 1, [255, 0, 0, 255, 0, 255, 0, 255]);
  const afterPixel = pngBase64(2, 1, [255, 0, 0, 255, 0, 0, 255, 255]);
  const beforePixelFile = writeJson(
    dir,
    'before-pixel.json',
    result([
      {
        id: 'case',
        name: 'Case',
        element: 'view',
        snapshotBase64: beforePixel,
        observations: 'same',
        status: 'passed',
      },
    ]),
  );
  const afterPixelFile = writeJson(
    dir,
    'after-pixel.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: afterPixel, observations: 'same', status: 'passed' },
    ]),
  );
  const pixel = compareResults({
    before: beforePixelFile,
    after: afterPixelFile,
    outputDir: dir.appending('pixel').toString(),
    pixelThreshold: 0,
  });
  const pixelCase = comparisonCase(pixel, 'case');
  assert(pixelCase.changedPixels === 1, 'one pixel should be changed');
  assert(pixelCase.diffPercent === 50, 'one of two pixels should be 50% diff');
  assert(pixelCase.diffImagePath !== undefined && fs.existsSync(pixelCase.diffImagePath), 'diff image should exist');

  const afterSize = pngBase64(2, 2, [255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255]);
  const beforeSizeFile = writeJson(
    dir,
    'before-size.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: red, observations: 'same', status: 'passed' },
    ]),
  );
  const afterSizeFile = writeJson(
    dir,
    'after-size.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: afterSize, observations: 'same', status: 'passed' },
    ]),
  );
  const size = compareResults({
    before: beforeSizeFile,
    after: afterSizeFile,
    outputDir: dir.appending('size').toString(),
  });
  assert(comparisonCase(size, 'case').dimensionMismatch === true, 'dimension mismatch should be marked');
  assert(
    comparisonCase(size, 'case').diffPercent === 0,
    'same-aspect dimension mismatch should normalize before diffing',
  );

  const afterNonProportionalSize = pngBase64(2, 1, [255, 0, 0, 255, 255, 0, 0, 255]);
  const afterNonProportionalSizeFile = writeJson(
    dir,
    'after-non-proportional-size.json',
    result([
      {
        id: 'case',
        name: 'Case',
        element: 'view',
        snapshotBase64: afterNonProportionalSize,
        observations: 'same',
        status: 'passed',
      },
    ]),
  );
  const nonProportionalSize = compareResults({
    before: beforeSizeFile,
    after: afterNonProportionalSizeFile,
    outputDir: dir.appending('non-proportional-size').toString(),
  });
  const nonProportionalSizeCase = comparisonCase(nonProportionalSize, 'case');
  assert(nonProportionalSizeCase.dimensionMismatch === true, 'non-proportional dimension mismatch should be marked');
  assert(nonProportionalSizeCase.changedPixels === 1, 'non-proportional extra pixels should be counted');
  assert(nonProportionalSizeCase.diffPercent === 50, 'non-proportional extra pixels should affect diff percent');

  const missingBeforeFile = writeJson(
    dir,
    'before-missing.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: red, observations: 'before', status: 'passed' },
      { id: 'missing', name: 'Missing', element: 'view', snapshotBase64: red, observations: '', status: 'passed' },
    ]),
  );
  const missingAfterFile = writeJson(
    dir,
    'after-missing.json',
    result([
      { id: 'case', name: 'Case', element: 'view', snapshotBase64: red, observations: 'after', status: 'passed' },
    ]),
  );
  const missing = compareResults({
    before: missingBeforeFile,
    after: missingAfterFile,
    outputDir: dir.appending('missing').toString(),
  });
  assert(comparisonCase(missing, 'case').observationsChanged === true, 'observation changes should be reported');
  assert(comparisonCase(missing, 'missing').status === 'missing', 'missing cases should be reported');

  console.log(`Self-test passed. Fixtures: ${dirString}`);
}
