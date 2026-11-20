import 'jasmine';
import { BazelLabel } from '../src/utils/BazelClient';

describe('BazelLabel', () => {
  const buildable = (label: string) => BazelLabel.fromString(label).toBuildableString();

  it('keeps apparent repo names as is', () => {
    expect(buildable('@valdi//modules:valdi_base')).toBe('@valdi//modules:valdi_base');
  });

  it('strips the Bazel 7 canonical suffix', () => {
    expect(buildable('@valdi~//modules:valdi_base')).toBe('@valdi//modules:valdi_base');
    expect(buildable('@valdi~1.0//modules:valdi_base')).toBe('@valdi//modules:valdi_base');
  });

  it('strips the Bazel 8 canonical suffix', () => {
    expect(buildable('@valdi+//modules:valdi_base')).toBe('@valdi//modules:valdi_base');
    expect(buildable('@valdi_widgets+//valdi_modules/widgets:widgets')).toBe(
      '@valdi_widgets//valdi_modules/widgets:widgets',
    );
  });

  it('handles the @@ canonical prefix', () => {
    expect(buildable('@@valdi+//modules:valdi_base')).toBe('@valdi//modules:valdi_base');
  });

  it('leaves main repo labels without a repo prefix', () => {
    expect(buildable('//src/foo:bar')).toBe('//src/foo:bar');
  });
});
