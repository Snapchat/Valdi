import { WebValdiLayout } from './WebValdiLayout';
import { UpdateAttributeDelegate } from '../ValdiWebRendererDelegate';

export class WebValdiVideo extends WebValdiLayout {
  public type = 'video';
  public declare htmlElement: HTMLDivElement;
  private videoEl!: HTMLVideoElement;
  private _onVideoLoaded?: (duration: number) => void;
  private _onBeginPlaying?: () => void;
  private _onError?: (error: string) => void;
  private _onCompleted?: () => void;
  private _onProgressUpdated?: (time: number, duration: number) => void;
  private _progressInterval: number | undefined;
  private _destroyed = false;

  constructor(id: number, attributeDelegate?: UpdateAttributeDelegate) {
    super(id, attributeDelegate);
  }

  createHtmlElement(): HTMLElement {
    const videoEl = document.createElement('video');
    videoEl.style.width = '100%';
    videoEl.style.height = '100%';
    videoEl.style.objectFit = 'contain';
    videoEl.preload = 'metadata';

    videoEl.addEventListener('loadedmetadata', () => {
      if (this._destroyed) return;
      const durationMs = videoEl.duration * 1000;
      this._onVideoLoaded?.(Math.round(durationMs));
    });
    videoEl.addEventListener('play', () => {
      if (this._destroyed) return;
      this._onBeginPlaying?.();
    });
    videoEl.addEventListener('error', () => {
      if (this._destroyed) return;
      const msg = videoEl.error?.message ?? 'Unknown error';
      this._onError?.(msg);
    });
    videoEl.addEventListener('ended', () => {
      if (this._destroyed) return;
      this._onCompleted?.();
    });
    videoEl.addEventListener('timeupdate', () => {
      if (this._destroyed) return;
      if (this._onProgressUpdated && videoEl.duration) {
        this._onProgressUpdated(
          Math.round(videoEl.currentTime * 1000),
          Math.round(videoEl.duration * 1000),
        );
      }
    });

    this.videoEl = videoEl;

    const wrapper = document.createElement('div');
    Object.assign(wrapper.style, {
      backgroundColor: 'black',
      boxSizing: 'border-box',
      display: 'flex',
      margin: 0,
      padding: 0,
      position: 'relative',
      overflow: 'hidden',
      pointerEvents: 'auto',
    });
    wrapper.appendChild(videoEl);
    return wrapper;
  }

  private resolveSrc(src: unknown): string | undefined {
    if (typeof src === 'string') return src;
    if (src && typeof src === 'object' && 'src' in src) return this.resolveSrc((src as { src: unknown }).src);
    return undefined;
  }

  changeAttribute(attributeName: string, attributeValue: unknown): void {
    switch (attributeName) {
      case 'src': {
        const url = this.resolveSrc(attributeValue);
        if (url && this.videoEl.src !== url) {
          this.videoEl.src = url;
        }
        return;
      }
      case 'volume':
        this.videoEl.volume = Math.max(0, Math.min(1, Number(attributeValue) ?? 1));
        return;
      case 'playbackRate': {
        const rate = Number(attributeValue) ?? 0;
        this.videoEl.playbackRate = rate <= 0 ? 1 : rate;
        if (rate > 0) {
          this.videoEl.play().catch(() => {});
        } else {
          this.videoEl.pause();
        }
        return;
      }
      case 'seekToTime':
        if (typeof attributeValue === 'number') {
          this.videoEl.currentTime = attributeValue / 1000;
        }
        return;
      case 'onVideoLoaded':
        this._onVideoLoaded = attributeValue as (duration: number) => void;
        return;
      case 'onBeginPlaying':
        this._onBeginPlaying = attributeValue as () => void;
        return;
      case 'onError':
        this._onError = attributeValue as (error: string) => void;
        return;
      case 'onCompleted':
        this._onCompleted = attributeValue as () => void;
        return;
      case 'onProgressUpdated':
        this._onProgressUpdated = attributeValue as (time: number, duration: number) => void;
        if (attributeValue) {
          if (this._progressInterval) {
            clearInterval(this._progressInterval);
            this._progressInterval = undefined;
          }
          this._progressInterval = window.setInterval(() => {
            if (this._destroyed) return;
            if (this.videoEl.duration && this._onProgressUpdated) {
              this._onProgressUpdated(
                Math.round(this.videoEl.currentTime * 1000),
                Math.round(this.videoEl.duration * 1000),
              );
            }
          }, 250);
        } else if (this._progressInterval) {
          clearInterval(this._progressInterval);
          this._progressInterval = undefined;
        }
        return;
      default:
        break;
    }
    super.changeAttribute(attributeName, attributeValue);
  }

  override destroy(): void {
    this._destroyed = true;
    if (this._progressInterval) {
      clearInterval(this._progressInterval);
      this._progressInterval = undefined;
    }
    this.videoEl.pause();
    this.videoEl.removeAttribute('src');
    super.destroy();
  }
}
