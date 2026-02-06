# The `<video>`

## Introduction

Valdi supports video playback through the `<video>` element in TSX.

> [!Note]
> The `<video>` element is implemented as a **Custom View Class**. Because implementing a video player from scratch in TypeScript would be slow and impractical, Valdi leverages the native video player implementations on iOS and Android. TypeScript provides the configuration (source, volume, seek time), while the native platform handles the high-performance rendering and decoding.

Unlike with images, Valdi does not support videos in the `res/` folder. We recommend you do not bundle videos for app size reasons.

By default the tag supports `https` and local `file` objects.

## Using `<video>`


```tsx
import { StatefulComponent } from 'valdi_core/src/Component';
import { CoreButton, CoreButtonColoring, CoreButtonSizing } from 'widgets/src/components/button/CoreButton';

export class VideoExample extends StatefulComponent<{}, State> {
  state: State = { volume: 0, playbackRate: 1, playbackPct: 0 };

  private videoLength?: number;

  onRender(): void {
    <view style={styles.root}>
      <view style={styles.container} width={`100%`}>
        <view width={200} height={200}>
          <video
            style={videoStyle}
            volume={this.state.volume}
            seekToTime={this.state.seekToTime}
            playbackRate={this.state.playbackRate}
            onVideoLoaded={this.onVideoLoaded}
            onBeginPlaying={this.onBeginPlaying}
            onError={this.onVideoError}
            onCompleted={this.onCompleted}
            onProgressUpdated={this.onProgressUpdated}
            src='https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ElephantsDream.mp4'
          ></video>
          <view style={controlsStyle}>
            <CoreButton
              sizing={CoreButtonSizing.SMALL}
              coloring={CoreButtonColoring.SECONDARY}
              onTap={this.onPlayTapped}
              icon={res.play}
            />
            <layout width='140' alignItems='center' flexDirection='row' padding='10'>
              <Slider onChange={this.onSeek} initialValue={this.state.playbackPct} />
            </layout>
          </view>
        </view>
        <layout width={200}>
          <label value='Volume'></label>
          <Slider onChange={this.onVolumeChanged} initialValue={0} />
        </layout>
      </view>
    </view>;
  }

  private onVideoLoaded = (duration: number): void => {
    this.videoLength = duration;
  };

  private onSeek = (value: number): void => {
    if (this.videoLength) {
      this.setState({ seekToTime: this.videoLength * value });
    }
  };

  private onVolumeChanged = (value: number): void => {
    this.setState({ volume: value });
  };

  private onPlayTapped = (): void => {
    this.setState({ playbackRate: this.state.playbackRate < 1 ? 1 : 0 });
  };

  private onBeginPlaying = (): void => {
    console.log('onBeginPlaying');
  };

  private onVideoError = (error: string): void => {
    console.log('onError ' + error);
  };

  private onCompleted = (): void => {
    console.log('onCompleted');
  };

  private onProgressUpdated = (time: number, duration: number): void => {
    let progress = 0;
    if (this.videoLength) {
      progress = time / this.videoLength;
    }
    this.setState({ playbackPct: progress });
  };
}
```

> [!NOTE]
> URLs must be HTTPS, HTTP is unsecured and typically automatically blocked

## Custom video loading

If you need a different data format for more functionality from the video player, you can do that by building a 
custom video loader.

## Complete API Reference

For a comprehensive list of all properties and methods available on `<video>` elements, including playback control, volume, seeking, and all callbacks, see the [API Reference](../api/api-reference-elements.md#videoview).
