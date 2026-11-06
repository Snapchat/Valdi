import { decodeBitmap } from 'drawing/src/BitmapFactory';
import { fs } from 'file_system/src/FileSystem';
import { generateDiffImage } from './DiffImage';
import { pixelDiff, PixelDiffResult } from './PixelDiff';
import { testCases } from './cases/index';

export interface CompareOptions {
  baselineDir: string;
  actualDir: string;
  outputDir: string;
  tolerance: number;
  // Budget for benign per-pixel noise. A case passes if mismatched pixels are within
  // maxDiffPercent of the total, capped at maxDiffPixels. Both default to strict-zero.
  maxDiffPixels: number;
  maxDiffPercent: number;
}

interface CompareResult {
  name: string;
  status: 'pass' | 'fail' | 'new';
  diff?: PixelDiffResult;
}

export async function compareSnapshots(options: CompareOptions): Promise<boolean> {
  try {
    fs.createDirectorySync(options.outputDir, true);
  } catch {
    /* already exists */
  }

  const results: CompareResult[] = [];
  let hasFailure = false;

  for (const tc of testCases) {
    const baselinePath = `${options.baselineDir}/${tc.name}.png`;
    const actualPath = `${options.actualDir}/${tc.name}.png`;

    let baselineData: ArrayBuffer | undefined;
    try {
      baselineData = fs.readFileSync(baselinePath) as ArrayBuffer;
    } catch {
      // No baseline yet
    }

    if (!baselineData) {
      console.info(`[FAIL] ${tc.name} — no baseline (run rebase to generate)`);
      results.push({ name: tc.name, status: 'new' });
      hasFailure = true;
      continue;
    }

    const actualData = fs.readFileSync(actualPath) as ArrayBuffer;

    const baselineBitmap = decodeBitmap(baselineData);
    const actualBitmap = decodeBitmap(actualData);

    try {
      const diff = pixelDiff(baselineBitmap, actualBitmap, options.tolerance);

      const baseInfo = baselineBitmap.getInfo();
      const actInfo = actualBitmap.getInfo();
      const dimensionsMatch = baseInfo.width === actInfo.width && baseInfo.height === actInfo.height;

      // A dimension change always fails regardless of budget. Otherwise allow up to a
      // percentage of pixels to differ, capped by an absolute count, to absorb benign
      // rasterizer/anti-aliasing noise without hiding a real geometry change.
      const allowedPixels = Math.min(
        options.maxDiffPixels,
        Math.floor((diff.totalPixels * options.maxDiffPercent) / 100),
      );
      const passed = dimensionsMatch && diff.mismatchedPixels <= allowedPixels;

      if (passed) {
        if (diff.mismatchedPixels > 0) {
          console.info(
            `[PASS WITH DIFF] ${tc.name} — ${diff.mismatchedPixels}/${
              diff.totalPixels
            } pixels (${diff.mismatchPercent.toFixed(2)}%) within budget of ${allowedPixels}`,
          );
        } else {
          console.info(`[PASS] ${tc.name}`);
        }
        results.push({ name: tc.name, status: 'pass', diff });
      } else {
        console.info(
          `[FAIL] ${tc.name} — ${diff.mismatchedPixels}/${diff.totalPixels} pixels (${diff.mismatchPercent.toFixed(
            2,
          )}%), max diff: ${diff.maxDifference}, budget: ${allowedPixels}`,
        );

        if (dimensionsMatch) {
          const diffImage = generateDiffImage(baselineBitmap, actualBitmap, options.tolerance);
          const diffPath = `${options.outputDir}/${tc.name}_diff.png`;
          fs.writeFileSync(diffPath, diffImage);
          console.info(`       diff: ${diffPath}`);
        } else {
          console.info(
            `       diff: skipped (dimensions differ: ${baseInfo.width}x${baseInfo.height} vs ${actInfo.width}x${actInfo.height})`,
          );
        }

        results.push({ name: tc.name, status: 'fail', diff });
        hasFailure = true;
      }
    } finally {
      baselineBitmap.dispose();
      actualBitmap.dispose();
    }
  }

  console.info('');
  console.info('=== Results ===');
  const padName = Math.max(...results.map(r => r.name.length));
  for (const r of results) {
    const status = r.status === 'pass' ? 'PASS' : r.status === 'fail' ? 'FAIL' : ' NEW';
    const detail = r.diff
      ? `${r.diff.mismatchedPixels}/${r.diff.totalPixels} mismatched (${r.diff.mismatchPercent.toFixed(2)}%)`
      : '';
    console.info(`  ${status}  ${r.name.padEnd(padName)}  ${detail}`);
  }
  console.info('');

  return !hasFailure;
}
