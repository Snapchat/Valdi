import { getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';
import { ArgumentsParser } from 'valdi_standalone/src/ArgumentsParser';
import { beginKeepAlive, endKeepAlive } from 'valdi_core/src/utils/KeepAliveCallback';
import { FontManager } from 'drawing/src/FontManager';
import { FontWeight, FontStyle } from 'drawing/src/DrawingModuleProvider';
import { fs } from 'file_system/src/FileSystem';
import { renderAllTestCases } from './RenderTestCases';
import { compareSnapshots } from './CompareSnapshots';
import { TEST_FONT } from './cases/TestFont';

const fontManager = FontManager.getDefault();
const regularData = new Uint8Array(fs.readFileSync('valdi/res/font/roboto_mono_regular.ttf') as ArrayBuffer);
const boldData = new Uint8Array(fs.readFileSync('valdi/res/font/roboto_mono_bold.ttf') as ArrayBuffer);
fontManager.registerFontFromData(TEST_FONT, FontWeight.NORMAL, FontStyle.NORMAL, regularData);
fontManager.registerFontFromData(TEST_FONT, FontWeight.BOLD, FontStyle.NORMAL, boldData);

// Roboto Mono carries no emoji glyphs, so emoji can only render through per-character fallback.
// Registered by path rather than by data to keep this 23MB font out of the JS heap, and with
// canUseAsFallback so the fallback resolver will actually consider it — without that flag
// getEmojiFont finds nothing and emoji render as missing-glyph boxes.
//
// Reusing snap_drawing's test font rather than vendoring another copy. The CLI runs with the
// open_source root as its working directory, so this source-relative path resolves without a
// Bazel data dep.
fontManager.registerFontFromFilePath(
  'Noto Color Emoji',
  FontWeight.NORMAL,
  FontStyle.NORMAL,
  'snap_drawing/testdata/NotoColorEmoji-Regular.ttf',
  /* canUseAsFallback */ true,
);

const standalone = getStandaloneRuntime();
const args = standalone.arguments.slice();

args.shift();
const subcommand = args.shift();

if (subcommand === 'render') {
  const parser = new ArgumentsParser('snapshot_tests render', ['_', ...args]);
  const outputDir = parser.addString('--output-dir', 'Directory to write rendered PNGs', true);
  parser.parse();

  const keepAlive = beginKeepAlive();
  renderAllTestCases(outputDir.value!)
    .then(() => {
      endKeepAlive(keepAlive);
      standalone.exit(0);
    })
    .catch(err => {
      console.error(err);
      endKeepAlive(keepAlive);
      standalone.exit(1);
    });
} else if (subcommand === 'compare') {
  const parser = new ArgumentsParser('snapshot_tests compare', ['_', ...args]);
  const baselineDir = parser.addString('--baseline-dir', 'Directory of baseline PNGs', true);
  const actualDir = parser.addString('--actual-dir', 'Directory of actual PNGs', true);
  const outputDir = parser.addString('--output-dir', 'Directory for diff output PNGs', true);
  const tolerance = parser.addNumber('--tolerance', 'Per-channel tolerance (0-255)', false);
  parser.parse();

  const keepAlive = beginKeepAlive();
  compareSnapshots({
    baselineDir: baselineDir.value!,
    actualDir: actualDir.value!,
    outputDir: outputDir.value!,
    tolerance: tolerance.value ?? 0,
  })
    .then(passed => {
      endKeepAlive(keepAlive);
      standalone.exit(passed ? 0 : 1);
    })
    .catch(err => {
      console.error(err);
      endKeepAlive(keepAlive);
      standalone.exit(1);
    });
} else {
  console.error(`Usage: snapshot_tests <render|compare> [options]`);
  console.error('');
  console.error('Subcommands:');
  console.error('  render   --output-dir <path>');
  console.error('  compare  --baseline-dir <path> --actual-dir <path> --output-dir <path> [--tolerance N]');
  standalone.exit(1);
}
