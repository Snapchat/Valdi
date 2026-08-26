import { AttributesBinder } from '../attributes/AttributesBinder';
import { ElementClass } from '../core/ElementClass';
import {
  assignStyles,
  AttributeApplierMap,
  replaceEventListener,
  setApplierCleanup,
  srcAttributeApplier,
} from './ElementClassSupport';
import { ViewElementClass } from './ViewElementClass';

function stringifyError(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function buildVideoAttributeAppliers(viewElementClass: ViewElementClass): AttributeApplierMap<HTMLVideoElement> {
  const binder = new AttributesBinder<HTMLVideoElement>();
  binder.bindAttribute('src', srcAttributeApplier<HTMLVideoElement>());
  binder.bindNumberAttribute(
    'volume',
    (element, value) => {
      element.volume = Math.max(0, Math.min(1, value));
    },
    element => {
      element.volume = 1;
    },
  );
  binder.bindNumberAttribute(
    'playbackRate',
    (element, value, context) => {
      element.playbackRate = value <= 0 ? 1 : value;
      if (value > 0) {
        element.play().catch(error => {
          console.error(
            `Valdi web renderer failed to play video on node ${context.id} (video) with playbackRate ${value}: ${stringifyError(error)}`,
          );
        });
      } else {
        element.pause();
      }
    },
    element => {
      element.playbackRate = 1;
      element.pause();
    },
  );
  binder.bindNumberAttribute(
    'seekToTime',
    (element, value) => {
      element.currentTime = value / 1000;
    },
    element => {
      element.currentTime = 0;
    },
  );
  binder.bindFunctionAttribute(
    'onVideoLoaded',
    (element, callback, context) => {
      replaceEventListener(element, context, 'video:onVideoLoaded', 'loadedmetadata', () => {
        callback(Math.round(element.duration * 1000));
      });
    },
    (element, context) => replaceEventListener(element, context, 'video:onVideoLoaded', 'loadedmetadata', undefined),
  );
  binder.bindFunctionAttribute(
    'onBeginPlaying',
    (element, callback, context) => {
      replaceEventListener(element, context, 'video:onBeginPlaying', 'play', () => callback());
    },
    (element, context) => replaceEventListener(element, context, 'video:onBeginPlaying', 'play', undefined),
  );
  binder.bindFunctionAttribute(
    'onError',
    (element, callback, context) => {
      replaceEventListener(element, context, 'video:onError', 'error', () => {
        callback(element.error?.message ?? 'Unknown error');
      });
    },
    (element, context) => replaceEventListener(element, context, 'video:onError', 'error', undefined),
  );
  binder.bindFunctionAttribute(
    'onCompleted',
    (element, callback, context) => {
      replaceEventListener(element, context, 'video:onCompleted', 'ended', () => callback());
    },
    (element, context) => replaceEventListener(element, context, 'video:onCompleted', 'ended', undefined),
  );
  binder.bindFunctionAttribute(
    'onProgressUpdated',
    (element, callback, context) => {
      const interval = window.setInterval(() => {
        if (element.duration) {
          callback(Math.round(element.currentTime * 1000), Math.round(element.duration * 1000));
        }
      }, 250);
      setApplierCleanup(context, 'video:onProgressUpdated', () => {
        clearInterval(interval);
      });
    },
    (_element, context) => setApplierCleanup(context, 'video:onProgressUpdated', undefined),
  );
  return {
    ...(viewElementClass.attributeAppliers as AttributeApplierMap<HTMLVideoElement>),
    ...binder.attributeAppliers,
  };
}

export class VideoElementClass extends ElementClass<HTMLVideoElement> {
  constructor(viewElementClass: ViewElementClass) {
    super('video', buildVideoAttributeAppliers(viewElementClass), viewElementClass.compositeAttributes);
  }

  protected onCreateElement(): HTMLVideoElement {
    const element = document.createElement('video');
    assignStyles(element, {
      display: 'block',
      objectFit: 'contain',
      position: 'relative',
    });
    return element;
  }
}
