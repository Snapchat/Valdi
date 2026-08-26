import 'jasmine/src/jasmine';
import { resolveAssetSourceUrl, resolveRenderableAssetSource } from '../src/utils/assetSource';

describe('assetSource', () => {
  it('resolves nested asset source fields consistently', () => {
    expect(resolveAssetSourceUrl({ src: { default: 'asset.png' } })).toBe('asset.png');
    expect(resolveAssetSourceUrl({ href: { url: 'https://example.test/asset.png' } })).toBe(
      'https://example.test/asset.png',
    );
  });

  it('falls back to renderable asset paths only for renderer sources', () => {
    expect(resolveAssetSourceUrl({ path: 'asset.png' })).toBeUndefined();
    expect(resolveRenderableAssetSource({ path: 'asset.png' })).toBe('asset.png');
    expect(resolveRenderableAssetSource({ path: 'asset-without-extension' })).toBeUndefined();
  });
});
