import { Asset, makeAssetFromBytes } from 'valdi_core/src/Asset';

// 40x20 PNG: red left half, blue right half, yellow 8x8 corner at top-left. Asymmetric so every
// object-fit mode and any flip or crop is visible. Inlined so the case needs no bundled resource.
const PNG_HEX =
  '89504e470d0a1a0a0000000d4948445200000028000000140806000000ff467fbb000000394944415478daedce310d00400804c193737290fd22f0020e4848f86e8be947f55493b4571c794a04091224f839b80d10244890204182044f35a91a2c0a702217940000000049454e44ae426082';

export function testImageAsset(): Asset {
  const bytes = new Uint8Array(PNG_HEX.length / 2);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = parseInt(PNG_HEX.substr(i * 2, 2), 16);
  }
  return makeAssetFromBytes(bytes);
}
