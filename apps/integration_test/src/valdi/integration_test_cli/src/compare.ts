import { Base64 } from 'coreutils/src/Base64';
import { wrapBitmap } from 'drawing/src/BitmapFactory';
import { fs } from 'file_system/src/FileSystem';
import { Path } from 'valdi_cli/src/Path';
import type { IBitmap } from 'drawing/src/IBitmap';

import { writeHtmlReport } from './htmlReport';
import * as ImageDiffNative from './ImageDiffNative';
import { encodePng } from './png';
import type { IntegrationTestCaseResult, IntegrationTestRenderedNode, IntegrationTestResult } from './types';

type MissingFrom = 'before' | 'after';
type ComparisonStatus = 'compared' | 'missing' | 'missing-snapshot';

export interface CompareOptions {
  before: string;
  after: string;
  outputDir: string;
  pixelThreshold?: number | string;
  failAbove?: number | string;
}

export interface ImageDiff {
  changedPixels: number;
  totalPixels: number;
  diffPercent: number;
  dimensionMismatch: boolean;
  image: IBitmap;
}

export interface CaseComparison {
  id: string;
  name?: string;
  element?: string;
  status: ComparisonStatus;
  missingFrom?: MissingFrom;
  diffPercent: number;
  changedPixels?: number;
  totalPixels?: number;
  dimensionMismatch?: boolean;
  beforeImagePath?: string;
  afterImagePath?: string;
  diffImagePath?: string;
  observationsChanged: boolean;
  beforeStatus?: string;
  afterStatus?: string;
  description?: string;
  beforeNodeOutput?: IntegrationTestRenderedNode;
  afterNodeOutput?: IntegrationTestRenderedNode;
  beforeObservations?: string;
  afterObservations?: string;
  beforeError?: string;
  afterError?: string;
}

export interface ComparisonSummary {
  before: string;
  after: string;
  beforePlatform: string;
  afterPlatform: string;
  pixelThreshold: number;
  caseCount: number;
  changedCaseCount: number;
  maxDiffPercent: number;
  cases: CaseComparison[];
}

function diffEncodedImages(beforeData: Uint8Array, afterData: Uint8Array, pixelThreshold: number): ImageDiff {
  const diff = ImageDiffNative.diffEncodedImages(beforeData, afterData, pixelThreshold);
  return {
    changedPixels: diff.changedPixels,
    totalPixels: diff.totalPixels,
    diffPercent: diff.totalPixels === 0 ? 0 : (diff.changedPixels / diff.totalPixels) * 100,
    dimensionMismatch: diff.dimensionMismatch,
    image: wrapBitmap(diff.image),
  };
}

function safeFileName(id: string): string {
  return id.replace(/[^a-zA-Z0-9_.-]/g, '_');
}

function asBytes(value: Uint8Array | ArrayBuffer): Uint8Array {
  return value instanceof Uint8Array ? value : new Uint8Array(value);
}

function pngBytesFromBase64(value: string): Uint8Array {
  const normalized = value.replace(/^data:image\/png;base64,/, '');
  return asBytes(Base64.toByteArray(normalized));
}

function pngDimensions(data: Uint8Array): { width: number; height: number } {
  if (data.length < 24) {
    throw new Error('Invalid PNG data');
  }
  return {
    width: ((data[16]! << 24) | (data[17]! << 16) | (data[18]! << 8) | data[19]!) >>> 0,
    height: ((data[20]! << 24) | (data[21]! << 16) | (data[22]! << 8) | data[23]!) >>> 0,
  };
}

function recreateDirectory(dir: Path): void {
  const path = dir.toString();
  if (fs.existsSync(path)) {
    fs.removeSync(path);
  }
  dir.ensureDirectory();
}

function writeSnapshotImage(testCase: IntegrationTestCaseResult | undefined, dir: Path): string | undefined {
  if (!testCase?.snapshotBase64) {
    return undefined;
  }
  const imagePath = dir.appending(`${safeFileName(testCase.id)}.png`);
  fs.writeFileSync(imagePath.toString(), pngBytesFromBase64(testCase.snapshotBase64));
  return imagePath.toString();
}

function loadResult(file: Path): IntegrationTestResult {
  return JSON.parse(fs.readFileSync(file.toString(), { encoding: 'utf8' }) as string) as IntegrationTestResult;
}

function caseMap(cases: IntegrationTestCaseResult[]): Map<string, IntegrationTestCaseResult> {
  return new Map(cases.map(testCase => [testCase.id, testCase] as const));
}

export function compareResults(options: CompareOptions): ComparisonSummary {
  const beforePath = Path.resolve(options.before);
  const afterPath = Path.resolve(options.after);
  const outputDir = Path.resolve(options.outputDir);
  const beforeImageDir = outputDir.appending('before');
  const afterImageDir = outputDir.appending('after');
  const diffDir = outputDir.appending('diffs');
  const before = loadResult(beforePath);
  const after = loadResult(afterPath);
  const pixelThreshold = Number(options.pixelThreshold ?? 3);
  outputDir.ensureDirectory();
  recreateDirectory(beforeImageDir);
  recreateDirectory(afterImageDir);
  recreateDirectory(diffDir);

  const beforeById = caseMap(before.cases);
  const afterById = caseMap(after.cases);
  const allIds = [...new Set([...beforeById.keys(), ...afterById.keys()])].sort();
  const cases: CaseComparison[] = [];

  for (const id of allIds) {
    const beforeCase = beforeById.get(id);
    const afterCase = afterById.get(id);
    const beforeImagePath = writeSnapshotImage(beforeCase, beforeImageDir);
    const afterImagePath = writeSnapshotImage(afterCase, afterImageDir);
    if (!beforeCase || !afterCase) {
      const existingCase = beforeCase ?? afterCase;
      cases.push({
        id,
        name: existingCase?.name,
        description: existingCase?.description,
        element: existingCase?.element,
        status: 'missing',
        missingFrom: beforeCase ? 'after' : 'before',
        diffPercent: 100,
        beforeImagePath,
        afterImagePath,
        observationsChanged: true,
        beforeStatus: beforeCase?.status,
        afterStatus: afterCase?.status,
        beforeNodeOutput: beforeCase?.nodeOutput,
        afterNodeOutput: afterCase?.nodeOutput,
        beforeObservations: beforeCase?.observations,
        afterObservations: afterCase?.observations,
        beforeError: beforeCase?.error,
        afterError: afterCase?.error,
      });
      continue;
    }

    let diffPercent = 100;
    let changedPixels = 0;
    let totalPixels = 0;
    let dimensionMismatch = false;
    let diffImagePath: string | undefined;
    let status: ComparisonStatus = 'compared';

    if (beforeCase.snapshotBase64 && afterCase.snapshotBase64) {
      diffImagePath = diffDir.appending(`${safeFileName(id)}.png`).toString();
      if (beforeCase.snapshotBase64 === afterCase.snapshotBase64) {
        const pngBytes = pngBytesFromBase64(afterCase.snapshotBase64);
        const dimensions = pngDimensions(pngBytes);
        totalPixels = dimensions.width * dimensions.height;
        diffPercent = 0;
        fs.writeFileSync(diffImagePath, pngBytes);
      } else {
        const beforePngBytes = pngBytesFromBase64(beforeCase.snapshotBase64);
        const afterPngBytes = pngBytesFromBase64(afterCase.snapshotBase64);
        const diff = diffEncodedImages(beforePngBytes, afterPngBytes, pixelThreshold);
        try {
          changedPixels = diff.changedPixels;
          totalPixels = diff.totalPixels;
          diffPercent = diff.diffPercent;
          dimensionMismatch = diff.dimensionMismatch;
          fs.writeFileSync(diffImagePath!, encodePng(diff.image));
        } finally {
          diff.image.dispose();
        }
      }
    } else if (!beforeCase.snapshotBase64 && !afterCase.snapshotBase64) {
      diffPercent = 0;
      status = 'missing-snapshot';
    } else {
      status = 'missing-snapshot';
    }

    cases.push({
      id,
      name: afterCase.name,
      description: afterCase.description ?? beforeCase.description,
      element: afterCase.element,
      status,
      diffPercent,
      changedPixels,
      totalPixels,
      dimensionMismatch,
      beforeImagePath,
      afterImagePath,
      diffImagePath,
      observationsChanged: (beforeCase.observations ?? '') !== (afterCase.observations ?? ''),
      beforeStatus: beforeCase.status,
      afterStatus: afterCase.status,
      beforeNodeOutput: beforeCase.nodeOutput,
      afterNodeOutput: afterCase.nodeOutput,
      beforeObservations: beforeCase.observations,
      afterObservations: afterCase.observations,
      beforeError: beforeCase.error,
      afterError: afterCase.error,
    });
  }

  const summary: ComparisonSummary = {
    before: beforePath.toString(),
    after: afterPath.toString(),
    beforePlatform: before.platform,
    afterPlatform: after.platform,
    pixelThreshold,
    caseCount: cases.length,
    changedCaseCount: cases.filter(testCase => testCase.diffPercent > 0 || testCase.observationsChanged).length,
    maxDiffPercent: cases.reduce((max, testCase) => Math.max(max, testCase.diffPercent), 0),
    cases,
  };

  fs.writeFileSync(outputDir.appending('summary.json').toString(), JSON.stringify(summary, null, 2));
  fs.writeFileSync(
    outputDir.appending('summary.md').toString(),
    [
      '# Valdi Integration Diff',
      '',
      `Before: ${summary.before}`,
      `After: ${summary.after}`,
      `Cases: ${summary.caseCount}`,
      `Changed cases: ${summary.changedCaseCount}`,
      `Max diff: ${summary.maxDiffPercent.toFixed(4)}%`,
      '',
      '| Case | Element | Diff | Observations |',
      '| --- | --- | ---: | --- |',
      ...cases.map(
        testCase =>
          `| ${testCase.id} | ${testCase.element ?? ''} | ${testCase.diffPercent.toFixed(4)}% | ${testCase.observationsChanged ? 'changed' : 'same'} |`,
      ),
      '',
    ].join('\n'),
  );
  writeHtmlReport(summary, outputDir);

  console.log(`Compared ${summary.caseCount} cases. Max diff: ${summary.maxDiffPercent.toFixed(4)}%`);
  if (options.failAbove !== undefined && summary.maxDiffPercent > Number(options.failAbove)) {
    throw new Error(`Max diff ${summary.maxDiffPercent.toFixed(4)}% is above ${options.failAbove}%`);
  }
  return summary;
}

export function compareUsage(): string {
  return `compare --before before.json --after after.json --output-dir out [--pixel-threshold 3] [--fail-above 0.1]\nHTML report, before images, after images, and diff images are written under <output-dir>.`;
}

export function printComparedFileHint(outputDir: string): void {
  const resolved = Path.resolve(outputDir);
  console.log(`HTML: ${resolved.appending('index.html')}`);
  console.log(`Summary: ${resolved.appending('summary.json')}`);
  console.log(`Markdown: ${resolved.appending('summary.md')}`);
  console.log(`Before images: ${resolved.appending('before')}`);
  console.log(`After images: ${resolved.appending('after')}`);
  console.log(`Diff images: ${resolved.appending('diffs')}`);
  console.log(`Compared output directory: ${resolved.basename()}`);
}
