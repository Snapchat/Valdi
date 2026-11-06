import { ImageEncoding } from 'drawing/src/IBitmap';
import { fs } from 'file_system/src/FileSystem';
import { renderImage } from 'valdi_image_generator/src/ImageGenerator';
import { testCases } from './cases/index';

export async function renderAllTestCases(outputDir: string): Promise<void> {
  try {
    fs.createDirectorySync(outputDir, true);
  } catch {
    /* already exists */
  }

  const realRandom = Math.random;
  try {
    for (const tc of testCases) {
      // Reset randomness before every case so a component that samples Math.random at
      // construction renders identically run-to-run, independent of case order. A constant
      // keeps it deterministic without a stateful sequence to reason about.
      Math.random = () => 0.5;

      console.info(`Rendering ${tc.name} (${tc.width}x${tc.height})...`);

      const image = await renderImage(
        {
          width: tc.width,
          height: tc.height,
          imageEncoding: ImageEncoding.PNG,
          encodeQuality: 1.0,
        },
        tc.render,
      );

      const path = `${outputDir}/${tc.name}.png`;
      fs.writeFileSync(path, image);
      console.info(`  -> ${path}`);
    }
  } finally {
    Math.random = realRandom;
  }

  console.info(`Rendered ${testCases.length} test cases to ${outputDir}`);
}
