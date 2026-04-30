import { createBitmap } from 'drawing/src/BitmapFactory';
import { BitmapAlphaType, BitmapColorType } from 'drawing/src/IBitmap';
import { MeasureMode } from 'drawing/src/IManagedContext';
import { createManagedContext } from 'drawing/src/ManagedContextFactory';
import { ElementRef } from 'valdi_core/src/ElementRef';
import { Layout, View } from 'valdi_tsx/src/NativeTemplateElements';

import 'jasmine/src/jasmine';
import 'valdi_tsx/src/JSX';

describe('ManagedContextFactory', () => {
  it('can be created and destroyed', () => {
    const context = createManagedContext();
    expect(context.renderer.getRootVirtualNode()).toBeUndefined();

    context.render(() => {
      <view />;
    });

    expect(context.renderer.getRootVirtualNode()).not.toBeUndefined();
    expect(context.renderer.getRootVirtualNode()?.element).not.toBeUndefined();
    context.dispose();
    expect(context.renderer.getRootVirtualNode()).toBeUndefined();
  });

  it('can layout', async () => {
    const context = createManagedContext();

    const roofRef = new ElementRef<Layout>();
    const viewRef = new ElementRef<View>();
    context.render(() => {
      <layout ref={roofRef} width={'100%'} height={'100%'} padding={16} alignItems="stretch" alignContent="stretch">
        <view ref={viewRef} flexGrow={1} />
      </layout>;
    });

    await context.layout(100, 100, false);

    expect(roofRef.single()?.frame).toEqual({
      width: 100,
      height: 100,
      x: 0,
      y: 0,
    });

    expect(viewRef.single()?.frame).toEqual({
      width: 68,
      height: 68,
      x: 16,
      y: 16,
    });

    context.dispose();
  });

  it('can measure', async () => {
    const context = createManagedContext();
    context.render(() => {
      <layout padding={20} alignItems="stretch" alignContent="stretch">
        <view width={40} height={30} />
      </layout>;
    });

    const result = await context.measure(1000, MeasureMode.AT_MOST, 1000, MeasureMode.AT_MOST, false);

    expect(result).toEqual({ width: 80, height: 70 });
  });

  it('can draw', async () => {
    const context = createManagedContext();
    context.render(() => {
      <view width={2} height={2} flexDirection="column">
        <layout flexDirection="row">
          <view width={1} height={1} backgroundColor="rgba(255, 0, 0, 1.0)" />
          <view width={1} height={1} backgroundColor="rgba(0, 255, 0, 1.0)" />
        </layout>
        <layout flexDirection="row">
          <view width={1} height={1} backgroundColor="rgba(0, 0, 255, 1.0)" />
          <view width={1} height={1} backgroundColor="rgba(255, 255, 255, 1.0)" />
        </layout>
      </view>;
    });

    await context.layout(2, 2, false);

    const { frame } = await context.draw();
    const bitmap = createBitmap({
      width: 2,
      height: 2,
      colorType: BitmapColorType.RGBA8888,
      alphaType: BitmapAlphaType.OPAQUE,
      rowBytes: 2 * 4,
    });

    frame.rasterInto(bitmap, true);

    const { byteLength, topLeftPixel, topRightPixel, bottomLeftPixel, bottomRightPixel } = bitmap.accessPixels(
      pixels => {
        const byteLength = pixels.byteLength;
        const topLeftPixel = pixels.getUint32(0);
        const topRightPixel = pixels.getUint32(4);
        const bottomLeftPixel = pixels.getUint32(8);
        const bottomRightPixel = pixels.getUint32(12);
        return { byteLength, topLeftPixel, topRightPixel, bottomLeftPixel, bottomRightPixel };
      },
    );

    expect(byteLength).toBe(16);

    expect(topLeftPixel).toBe(0xff0000ff);
    expect(topRightPixel).toBe(0x00ff00ff);
    expect(bottomLeftPixel).toBe(0x0000ffff);
    expect(bottomRightPixel).toBe(0xffffffff);

    frame.dispose();
    context.dispose();
  });

  it('can observe asset load', async () => {
    const image =
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEkAAAA7CAYAAAA3macWAAAAAXNSR0IArs4c6QAAAERlWElmTU0AKgAAAAgAAYdpAAQAAAABAAAAGgAAAAAAA6ABAAMAAAABAAEAAKACAAQAAAABAAAASaADAAQAAAABAAAAOwAAAACp5RezAAAEs0lEQVR4Ae2azY8URRiHGUFBYqIsgp8kBggaL3owJoZE44GokKgXLya4ZEXgICfix1+gRw2iETRGQEk8KDGc96YiiigXkKhwRfCDj6BiTJbnIdPLOtP99uzszNDVu2/yS3XVW11d75Oq6uqamTWr4jY2NrYaHUTHmvqMdMkgu90Y5MMm+yxgLOWefWhOy72nyD/XaDROtJT3JXtNX1rtXaOraKoVkK0vRrubEM331aoOaW4Q/SJ8uwYBquqQvg0g6RoIqEpDYs3ZD4jPOwDV16lX6YVbOEyn2SRvoMfMB/YbvrWAPR7U6cpVeUhGBSgX7zeRC3lkgvKt90tUabK+JCAZFKCuJXkLPWo+sN/xOaJ6BioZSEJpgnqby0fMByYoR9TPQZ2OXUlBMipAXUfyLlppPrCegUoOklAA5f5pO3rIfGCCGmZE/RTUKXUlCcmoADWP5D30oPnA/sDn1OsaVLKQhAKo60neRw+YD2xKoCq9mQyCvuxidPzNxQZ0sKTuEH4/YVaU1Mt1Jw3JiAB1gWQEjZoPTFA7uwGVPCShAOoiyYvoU/OBZaDuDuq0uZJek9qioYCRsoXEKRiZa9Q64B6LKmW+2kEyMECtI3kVRfH9iV9QP5KGFjUS3lh1J6CepI+vo7xDu6z7HYGqLSQpAOphkq3IrUKRlYKqNSSpAOo+kh3oJvMFdoZyd+a5U6/2kIQCqGUkH6BbzReYoFyjjrb6pwUkgwaUgAQlsCLLBVWLfVJRxBPLGSEnyT+LDk8sb7l2SrrhvHdi+fhIwuGh1kLkcWnKNkbn/Vw5D5j/WgMhThdxF3MX9SI7i8Opd8QKDW4SyktIyh5B1MmEdQ6dbyq7/ou8WwRPEopsHJSQNlPLLf2MtRM4TdETrknPtPtmSpoEFpGuEdLQDJKQwGIh9exXhfBR6TqPCMmV3jfCjLUT+Jqi0ctbABZvD9SH0W2oDluAf4gje5P5ZvM6y5vegF5B81GR/YBjhG3AhfF9UlHNupUzINxxl32ifE+d5wVk/E63aWMAup9g96DoG+5/gIQzbSAByF99P0R+ehTZIRzjIyirNC0gAegpAn4HRedKAlqfTbEMkGnt1yQAjRDnyyWxfof/hTxAlNcbEoB8gwkpMgE5gvyey7VajiTguI15DT2dG/WVQn/UdAQVArJq7SAByC/7rciFOrKOANlArSAB6EZi8jzbV31kHQOykdpAAlAnx7PGLCDXIM+aOrJabAEA5C76E2QamX95nhQgG0t+JAHoHuLYiaJNorF+gzZMZgR5k5Y0JAD5V5pdaIHBBNY1INtMFhKAltP/3WjIQAI7gG9jNyMoazNJSABaSgACujkLpCAVkFPMo5OuLbmFG0B3Ea1rUBkgD8ymDIg20ppuAFpCnz9Gt9j5wATkFJvSCMraT2YkAehOOu0UKwO0v5eABJXEmgSg2+nrR+gOOx1YBsi/B/bMKg8JQI4cp5hTLbKvcG5iivUUkA+sNCQA+bP7XlS2k+4bICFVfU16/GoDSgFS2Qj6kiD6MsWEk1n0p8usztVMTwYPzwD9G9Tpiavq020fUf6aE+kXlDmC+g4o59nVK3J/hLahA2gUbUYDnQGXAGz/ceUh+eOJAAAAAElFTkSuQmCC';
    const context = createManagedContext();
    context.render(() => {
      <view width={100} height={100} flexDirection="column">
        <image width={'100%'} height={'100%'} src={image} />
      </view>;
    });

    await context.layout(100, 100, false);

    const result = await context.onAllAssetsLoaded();
    expect(result.loadedAssetsCount).toBe(1);
    expect(result.errors).toBeUndefined();
  });

  it('can observe asset load with error', async () => {
    const image = 'doesnotexist://whatisup';
    const context = createManagedContext();
    context.render(() => {
      <view width={100} height={100} flexDirection="column">
        <image width={'100%'} height={'100%'} src={image} />
      </view>;
    });
    await context.layout(100, 100, false);

    const result = await context.onAllAssetsLoaded();
    expect(result.loadedAssetsCount).toBe(1);
    expect(result.errors).not.toBeUndefined();
    expect(result.errors![0]).toBe(
      "Cannot resolve AssetLoader for URL scheme 'doesnotexist' and output type 'imageSnapDrawing'",
    );
  });
});
