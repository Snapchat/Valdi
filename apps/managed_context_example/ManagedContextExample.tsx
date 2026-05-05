import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AnyRenderFunction } from 'valdi_core/src/AnyRenderFunction';
import { createManagedContext, EmbeddedPlatformViewRasterMethod } from 'drawing/src/ManagedContextFactory';
import { BitmapAlphaType, BitmapColorType, ImageEncoding } from 'drawing/src/IBitmap';
import { createBitmap, createBitmapWithBuffer } from 'drawing/src/BitmapFactory';
import { Asset } from 'valdi_tsx/src/Asset';
import { makeAssetFromBytes } from 'valdi_core/src/Asset';
import { Device } from 'valdi_core/src/Device';

const COMPONENT_HEIGHT = 200;

class InnerComponent extends Component {
  onRender(): void {
    <view width={'100%'} height={'100%'} backgroundColor="black" padding={60} touchEnabled={false} slowClipping>
      <view
        padding={8}
        background="linear-gradient(90deg, lightgray, gray)"
        borderRadius={16}
        alignSelf="center"
        rotation={Math.PI / -8}
        borderWidth={1}
        borderColor="red"
      >
        <view marginLeft={8} marginRight={8} width={200} height={60} borderRadius={'50%'}>
          <textfield
            width={230}
            height={60}
            limitToViewport={false}
            position="absolute"
            value={'Hello, World!'}
            color="red"
            font={systemBoldFont(40)}
            touchEnabled={false}
          />
        </view>
      </view>
    </view>;
  }
}

function Section(viewModel: { name: string; children?: AnyRenderFunction | void }) {
  <layout flexDirection="column">
    <layout padding={16}>
      <label value={viewModel.name} font={systemBoldFont(17)} color="black" />
    </layout>
    {viewModel.children?.()}
  </layout>;
}

function Button(viewModel: { title: string; onTap: () => void }) {
  <view
    backgroundColor="cornflowerblue"
    padding={16}
    borderRadius={16}
    boxShadow={'1 1 3 rgba(0, 0, 0, 0.16)'}
    onTap={viewModel.onTap}
    marginBottom={8}
  >
    <label value={viewModel.title} font={systemBoldFont(14)} color="black" />
  </view>;
}

const DOWNSCALE_RATIO = [1, 2, 4, 8];

interface State {
  rasterMethod: EmbeddedPlatformViewRasterMethod;
  rasterizedAsset?: Asset;
  downscaleRatioIndex: number;
}

export class App extends StatefulComponent<{}, State> {
  state: State = {
    rasterMethod: EmbeddedPlatformViewRasterMethod.FAST,
    downscaleRatioIndex: 0,
  };

  onRender(): void {
    <view width={'100%'} height={'100%'} paddingTop={Device.getDisplayTopInset()} backgroundColor="white">
      <Section name="Preview">
        {
          <layout height={COMPONENT_HEIGHT}>
            <InnerComponent />
          </layout>
        }
      </Section>
      <Section name="Rasterized">
        {<image src={this.state.rasterizedAsset} width={'100%'} height={COMPONENT_HEIGHT} />}
      </Section>
      <Section name="Controls">
        <layout flexDirection="column" padding={8}>
          <Button
            title={`Raster method: ${
              this.state.rasterMethod === EmbeddedPlatformViewRasterMethod.FAST ? 'Fast' : 'Accurate'
            }`}
            onTap={this.toggleRasterMethod}
          />
          <Button
            title={`Downscale ratio: ${DOWNSCALE_RATIO[this.state.downscaleRatioIndex]}`}
            onTap={this.toggleDownscaleRatio}
          />
          <Button title="Rasterize" onTap={this.rasterize} />
        </layout>
      </Section>
    </view>;
  }

  toggleDownscaleRatio = () => {
    const nextDownscaleRatioIndex = (this.state.downscaleRatioIndex + 1) % DOWNSCALE_RATIO.length;
    this.setState({
      downscaleRatioIndex: nextDownscaleRatioIndex,
    });
  };

  toggleRasterMethod = () => {
    this.setState({
      rasterMethod:
        this.state.rasterMethod === EmbeddedPlatformViewRasterMethod.FAST
          ? EmbeddedPlatformViewRasterMethod.ACCURATE
          : EmbeddedPlatformViewRasterMethod.FAST,
    });
  };

  rasterize = () => {
    this.doRasterize()
      .then(asset => {
        console.log('Rasterized asset:', asset);
        this.setState({ rasterizedAsset: asset });
      })
      .catch(error => {
        console.error('Error rasterizing:', error);
      });
  };

  private getComponentSize(): [number, number] {
    const width = this.renderer.getComponentRootElements(this, true)[0].frame.width;
    const height = COMPONENT_HEIGHT;
    return [width, height];
  }

  private async doRasterize(): Promise<Asset> {
    const [componentWidth, componentHeight] = this.getComponentSize();
    const context = createManagedContext({
      embeddedPlatformViewRasterMethod: this.state.rasterMethod,
      deltaRasterization: true,
    });
    context.render(() => {
      <InnerComponent />;
    });

    await context.onAllAssetsLoaded();

    await context.layout(componentWidth, componentHeight, false);

    const { frame } = await context.draw();

    const scale = Device.getDisplayScale() / DOWNSCALE_RATIO[this.state.downscaleRatioIndex];
    const rasterWidth = Math.floor(scale * componentWidth);
    const rasterHeight = Math.floor(scale * componentHeight);

    const bytesPerPixel = 4;

    const bitmap = createBitmap({
      width: rasterWidth,
      height: rasterHeight,
      colorType: BitmapColorType.RGBA8888,
      alphaType: BitmapAlphaType.OPAQUE,
      rowBytes: rasterWidth * bytesPerPixel,
    });

    frame.rasterInto(bitmap, true);

    const buffer = bitmap.encode(ImageEncoding.PNG, 1.0);

    frame.dispose();
    bitmap.dispose();
    context.dispose();

    return makeAssetFromBytes(buffer);
  }
}

const styles = {};
