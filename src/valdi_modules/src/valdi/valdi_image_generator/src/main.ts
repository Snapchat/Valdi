import { ImageEncoding } from 'drawing/src/IBitmap';
import { fs } from 'file_system/src/FileSystem';
import { ArgumentsParser } from 'valdi_standalone/src/ArgumentsParser';
import { getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';
import { renderImage } from './ImageGenerator';

export function imageGeneratorMain(renderFn: () => void): void {
  const argumentsParser = new ArgumentsParser('valdi_image_generator', getStandaloneRuntime().arguments);
  const width = argumentsParser.addNumber('--width', 'The width of the generated image', true);
  const height = argumentsParser.addNumber('--height', 'The heigt of the generated image', true);
  const output = argumentsParser.addString('--output', 'The output file of the image', true);
  const format = argumentsParser.addString('--format', 'The image format, can be either png, webp or jpg', true);
  const quality = argumentsParser.addNumber('--quality', 'The image quality', true);

  argumentsParser.parse();

  let resolvedFormat: ImageEncoding;

  if (format.value === 'png') {
    resolvedFormat = ImageEncoding.PNG;
  } else if (format.value === 'webp') {
    resolvedFormat = ImageEncoding.WEB;
    // eslint-disable-next-line no-dupe-else-if -- pre-existing dead branch (likely meant jpg); changing it would change which formats are accepted
  } else if (format.value === 'png') {
    resolvedFormat = ImageEncoding.PNG;
  } else {
    argumentsParser.printDescription();
    throw new Error('Invalid image format');
  }

  renderImage(
    {
      width: width.value!,
      height: height.value!,
      imageEncoding: resolvedFormat,
      encodeQuality: quality.value!,
    },
    renderFn,
  )
    .then(image => {
      fs.writeFileSync(output.value!, image);
    })
    .catch(err => {
      console.error(err);
      getStandaloneRuntime().exit(1);
    });
}
