import { Base64 } from 'coreutils/src/Base64';
import { fs } from 'file_system/src/FileSystem';
import { Argument, ArgumentsParser } from 'valdi_cli/src/ArgumentsParser';
import { Path } from 'valdi_cli/src/Path';
import type { IBitmap } from 'drawing/src/IBitmap';

import { accessBitmapPixels, createRgbaBitmap, decodePng, encodePng, LockedBitmapPixels } from './png';
import type { Pixel } from './png';
import type { IntegrationTestResult } from './types';

const sheetBackgroundPixel = LockedBitmapPixels.rgba(241, 245, 249, 255);
const sheetBorderPixel = LockedBitmapPixels.rgba(203, 213, 225, 255);
const sheetCellPixel = LockedBitmapPixels.rgba(255, 255, 255, 255);
const missingBackgroundPixel = LockedBitmapPixels.rgba(255, 245, 245, 255);
const missingStrokePixel = LockedBitmapPixels.rgba(220, 38, 38, 255);

interface ContactSheetOptions {
  columns?: number | string;
  cellWidth?: number | string;
  cellHeight?: number | string;
  padding?: number | string;
}

export interface ExportSnapshotsOptions {
  result: string;
  outputDir: string;
  columns?: number | string;
}

interface SnapshotEntry {
  index: number;
  id: string;
  name: string;
  description?: string;
  element: string;
  status: string;
  error?: string;
  fileName: string | null;
  width: number;
  height: number;
  image: IBitmap | null;
}

type SnapshotManifestEntry = Omit<SnapshotEntry, 'image'>;

function safeFileName(value: string): string {
  return value.replace(/[^a-zA-Z0-9_.-]/g, '_');
}

function asBytes(value: Uint8Array | ArrayBuffer): Uint8Array {
  return value instanceof Uint8Array ? value : new Uint8Array(value);
}

function copyPixel(source: LockedBitmapPixels, target: LockedBitmapPixels, sourceX: number, sourceY: number, targetX: number, targetY: number): void {
  target.setPixel(targetX, targetY, source.getPixel(sourceX, sourceY));
}

function fillRect(image: LockedBitmapPixels, left: number, top: number, width: number, height: number, pixel: Pixel): void {
  for (let y = Math.max(0, top); y < Math.min(image.info.height, top + height); y++) {
    for (let x = Math.max(0, left); x < Math.min(image.info.width, left + width); x++) {
      image.setPixel(x, y, pixel);
    }
  }
}

function pasteScaled(source: IBitmap, target: LockedBitmapPixels, left: number, top: number, width: number, height: number): void {
  accessBitmapPixels(source, sourcePixels => {
    for (let y = 0; y < height; y++) {
      const sourceY = Math.min(sourcePixels.info.height - 1, Math.floor((y / height) * sourcePixels.info.height));
      for (let x = 0; x < width; x++) {
        const sourceX = Math.min(sourcePixels.info.width - 1, Math.floor((x / width) * sourcePixels.info.width));
        copyPixel(sourcePixels, target, sourceX, sourceY, left + x, top + y);
      }
    }
  });
}

function drawMissing(target: LockedBitmapPixels, left: number, top: number, width: number, height: number): void {
  fillRect(target, left, top, width, height, missingBackgroundPixel);
  const steps = Math.min(width, height);
  for (let i = 0; i < steps; i++) {
    for (let thickness = -2; thickness <= 2; thickness++) {
      const x1 = left + Math.floor((i / steps) * width);
      const y1 = top + Math.floor((i / steps) * height) + thickness;
      const x2 = left + width - 1 - Math.floor((i / steps) * width);
      if (x1 >= left && x1 < left + width && y1 >= top && y1 < top + height) {
        fillRect(target, x1, y1, 1, 1, missingStrokePixel);
      }
      if (x2 >= left && x2 < left + width && y1 >= top && y1 < top + height) {
        fillRect(target, x2, y1, 1, 1, missingStrokePixel);
      }
    }
  }
}

function buildContactSheet(entries: SnapshotEntry[], outputPath: string, options: ContactSheetOptions = {}): void {
  const columns = Number(options.columns ?? 4);
  const cellWidth = Number(options.cellWidth ?? 220);
  const cellHeight = Number(options.cellHeight ?? 360);
  const padding = Number(options.padding ?? 10);
  const rows = Math.ceil(entries.length / columns);
  const sheet = createRgbaBitmap(columns * cellWidth, rows * cellHeight);
  accessBitmapPixels(sheet, sheetPixels => {
    fillRect(sheetPixels, 0, 0, sheetPixels.info.width, sheetPixels.info.height, sheetBackgroundPixel);

    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      if (!entry) {
        continue;
      }

      const col = i % columns;
      const row = Math.floor(i / columns);
      const left = col * cellWidth + padding;
      const top = row * cellHeight + padding;
      const boxWidth = cellWidth - padding * 2;
      const boxHeight = cellHeight - padding * 2;
      fillRect(sheetPixels, left - 1, top - 1, boxWidth + 2, boxHeight + 2, sheetBorderPixel);
      fillRect(sheetPixels, left, top, boxWidth, boxHeight, sheetCellPixel);

      if (!entry.image) {
        drawMissing(sheetPixels, left, top, boxWidth, boxHeight);
        continue;
      }

      const image = entry.image;
      const imageInfo = image.getInfo();
      const scale = Math.min(boxWidth / imageInfo.width, boxHeight / imageInfo.height);
      const scaledWidth = Math.max(1, Math.round(imageInfo.width * scale));
      const scaledHeight = Math.max(1, Math.round(imageInfo.height * scale));
      pasteScaled(
        image,
        sheetPixels,
        left + Math.floor((boxWidth - scaledWidth) / 2),
        top + Math.floor((boxHeight - scaledHeight) / 2),
        scaledWidth,
        scaledHeight,
      );
    }
  });

  try {
    fs.writeFileSync(outputPath, encodePng(sheet));
  } finally {
    sheet.dispose();
  }
}

export function exportSnapshots(options: ExportSnapshotsOptions): SnapshotEntry[] {
  const result = JSON.parse(fs.readFileSync(Path.resolve(options.result).toString(), { encoding: 'utf8' }) as string) as IntegrationTestResult;
  const outputDir = Path.resolve(options.outputDir);
  outputDir.ensureDirectory();

  const entries = result.cases.map((testCase, index): SnapshotEntry => {
    const number = String(index + 1).padStart(2, '0');
    const fileName = `${number}-${safeFileName(testCase.id)}.png`;
    const filePath = outputDir.appending(fileName).toString();
    let image: IBitmap | null = null;
    if (testCase.snapshotBase64) {
      const normalized = testCase.snapshotBase64.replace(/^data:image\/png;base64,/, '');
      const pngData = asBytes(Base64.toByteArray(normalized));
      fs.writeFileSync(filePath, pngData);
      image = decodePng(pngData);
    }
    const imageInfo = image?.getInfo();
    return {
      index: index + 1,
      id: testCase.id,
      name: testCase.name,
      description: testCase.description,
      element: testCase.element,
      status: testCase.status,
      error: testCase.error,
      fileName: image ? fileName : null,
      width: imageInfo?.width ?? 0,
      height: imageInfo?.height ?? 0,
      image,
    };
  });

  try {
    buildContactSheet(entries, outputDir.appending('contact-sheet.png').toString(), {
      columns: options.columns,
    });
    fs.writeFileSync(
      outputDir.appending('manifest.json').toString(),
      JSON.stringify({
        platform: result.platform,
        caseCount: result.cases.length,
        cases: entries.map(({ image: _image, ...entry }): SnapshotManifestEntry => entry),
      }, null, 2),
    );
    fs.writeFileSync(
      outputDir.appending('index.md').toString(),
      [
        `# ${result.platform} Integration Snapshots`,
        '',
        '![contact sheet](contact-sheet.png)',
        '',
        '| # | Case | Element | Status | Description | Image |',
        '| ---: | --- | --- | --- | --- | --- |',
        ...entries.map(entry =>
          `| ${entry.index} | ${entry.id} | ${entry.element} | ${entry.status}${entry.error ? ` (${entry.error})` : ''} | ${entry.description ?? ''} | ${entry.fileName ?? 'missing'} |`,
        ),
        '',
      ].join('\n'),
    );
  } finally {
    for (const entry of entries) {
      entry.image?.dispose();
    }
  }

  console.log(`Exported ${entries.length} snapshots to ${outputDir}`);
  return entries;
}

export function exportSnapshotsFromArgs(args: string[]): SnapshotEntry[] {
  const parser = new ArgumentsParser('export-snapshots', ['export-snapshots', ...args]);
  const result: Argument<string> = parser.addString('--result', 'Result JSON path', true);
  const outputDir: Argument<string> = parser.addString('--output-dir', 'Directory for exported PNGs and manifest', true);
  const columns: Argument<string> = parser.addString('--columns', 'Contact sheet column count', false);
  parser.parse();
  return exportSnapshots({
    result: result.value!,
    outputDir: outputDir.value!,
    columns: columns.value,
  });
}

export function exportSnapshotsUsage(): string {
  return 'export-snapshots --result result.json --output-dir out [--columns 4]';
}
