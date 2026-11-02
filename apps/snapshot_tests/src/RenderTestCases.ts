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

  for (const tc of testCases) {
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

  console.info(`Rendered ${testCases.length} test cases to ${outputDir}`);
}
