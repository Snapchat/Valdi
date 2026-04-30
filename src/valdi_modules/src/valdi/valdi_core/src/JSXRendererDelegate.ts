import { StringCache } from 'valdi_core/src/StringCache';
import { MergeType } from 'valdi_core/src/utility_types/MergeType';
import { getObjectId } from 'valdi_core/src/utils/IdentifyableObject';
import { StringMap } from 'coreutils/src/StringMap';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { NativeNode } from 'valdi_tsx/src/NativeNode';
import { NativeView } from 'valdi_tsx/src/NativeView';
import { AnimationCurve, AnimationOptions } from './AnimationOptions';
import { CancelToken } from './CancellableAnimation';
import { ValdiRuntime } from './ValdiRuntime';
import { FrameObserver, IRendererDelegate, VisibilityObserver } from './IRendererDelegate';
import { NativeFrameObserver, NativeVisibilityObserver } from './RenderRequest';
import { Style } from './Style';
import { Buffer } from './utils/Buffer';

declare const runtime: ValdiRuntime;

const enum RenderRequestEntryType {
  CREATE_ELEMENT = 1,
  DESTROY_ELEMENT = 2,
  SET_ROOT_ELEMENT = 3,
  MOVE_ELEMENT_TO_PARENT = 4,
  SET_ATTRIBUTE_UNDEFINED = 5,
  SET_ATTRIBUTE_NULL = 6,
  SET_ATTRIBUTE_FALSE = 7,
  SET_ATTRIBUTE_TRUE = 8,
  SET_ATTRIBUTE_INT = 9,
  SET_ATTRIBUTE_DOUBLE = 10,
  SET_ATTRIBUTE_ARRAY = 11,
  SET_ATTRIBUTE_STYLE = 12,
  SET_ATTRIBUTE_ATTACHED_VALUE = 13,
  BEGIN_ANIMATIONS = 14,
  END_ANIMATIONS = 15,
  ON_LAYOUT_COMPLETE = 16,
  CANCEL_ANIMATION = 17,
  ON_NEXT_DRAW = 18,
}

const bufferPool = [new Buffer(512)];

export class JSXRendererDelegate implements IRendererDelegate {
  private treeId: string;
  private destroyed: boolean;
  private stringCache: StringCache;
  private attributeCache: StringCache;
  private injectedAttributeCache: StringCache;

  private buffer: Buffer;
  private attachedValues: any[];
  private attachedValueIndexByString: StringMap<number>;
  private attachedValueIndexByObjectId: { [objectId: number]: number };
  private renderStartTime?: number;
  private pendingVisibilityObserver?: NativeVisibilityObserver;
  private pendingFrameObserver?: NativeFrameObserver;

  constructor(
    treeId: string,
    stringCache: StringCache,
    attributeCache: StringCache,
    injectedAttributeCache: StringCache,
  ) {
    this.treeId = treeId;
    this.destroyed = false;
    this.stringCache = stringCache;
    this.attributeCache = attributeCache;
    this.injectedAttributeCache = injectedAttributeCache;
    this.buffer = new Buffer(512);
    this.attachedValues = [];
    this.attachedValueIndexByString = {};
    this.attachedValueIndexByObjectId = {};
  }

  onDestroyed() {
    this.destroyed = true;
  }

  onElementCreated(id: number, viewClass: string): void {
    this.buffer.putUint32_2(RenderRequestEntryType.CREATE_ELEMENT | (id << 8), this.stringCache.get(viewClass));
  }

  onElementDestroyed(id: number): void {
    this.buffer.putUint32(RenderRequestEntryType.DESTROY_ELEMENT | (id << 8));
  }

  onElementBecameRoot(id: number): void {
    this.buffer.putUint32(RenderRequestEntryType.SET_ROOT_ELEMENT | (id << 8));
  }

  onElementMoved(id: number, parentId: number, parentIndex: number): void {
    this.buffer.putUint32_3(RenderRequestEntryType.MOVE_ELEMENT_TO_PARENT | (id << 8), parentId, parentIndex);
  }

  onAnimationStart(options: MergeType<AnimationOptions>, cancelToken: CancelToken) {
    const buffer = this.buffer;
    buffer.putUint32(RenderRequestEntryType.BEGIN_ANIMATIONS);

    buffer.putFloat64(options.duration ?? 0);
    let curve = options.curve;
    if (!curve) {
      curve = AnimationCurve.EaseInOut;
    }

    switch (curve) {
      case AnimationCurve.Linear:
        buffer.putUint32(1);
        break;
      case AnimationCurve.EaseIn:
        buffer.putUint32(2);
        break;
      case AnimationCurve.EaseOut:
        buffer.putUint32(3);
        break;
      case AnimationCurve.EaseInOut:
        buffer.putUint32(4);
        break;
    }

    if (options.beginFromCurrentState) {
      buffer.putUint32(1);
    } else {
      buffer.putUint32(0);
    }
    buffer.putUint32(options.crossfade ? 1 : 0);

    buffer.putFloat64(options.stiffness ?? 0);
    buffer.putFloat64(options.damping ?? 0);

    buffer.putUint32(this.getAttachedValueIndex(options.controlPoints));
    buffer.putUint32(this.getAttachedValueIndex(options.completion));
    buffer.putUint32(cancelToken);
  }

  onAnimationEnd() {
    this.buffer.putUint32(RenderRequestEntryType.END_ANIMATIONS);
  }

  onAnimationCancel(token: number) {
    this.buffer.putUint32(RenderRequestEntryType.CANCEL_ANIMATION);
    this.buffer.putUint32(token);
  }

  private getAttachedValueIndex(value: any): number {
    if (typeof value === 'string') {
      // For strings, we try to reuse them as much as possible to avoid
      // the marshalling
      let index = this.attachedValueIndexByString[value];
      if (!index) {
        const attachedValues = this.attachedValues;
        index = attachedValues.length;
        attachedValues.push(value);
        this.attachedValueIndexByString[value] = index;
      }
      return index;
    } else {
      const objectId = getObjectId(value);
      if (objectId) {
        let index = this.attachedValueIndexByObjectId[objectId];
        if (!index) {
          const attachedValues = this.attachedValues;
          index = attachedValues.length;
          attachedValues.push(value);
          this.attachedValueIndexByObjectId[objectId] = index;
        }
        return index;
      } else {
        // For unknown objects we have to marshall all of them individually,
        // unless we did a linear search to find them.
        const attachedValues = this.attachedValues;
        const attachedValuesIndex = attachedValues.length;
        attachedValues.push(value);
        return attachedValuesIndex;
      }
    }
  }

  onElementAttributeChangeNumber(id: number, attributeName: string, value: number): void {
    const attributeNameParam = this.attributeCache.get(attributeName);
    const buffer = this.buffer;

    buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_DOUBLE | (id << 8), attributeNameParam);
    buffer.putFloat64(value);
  }

  onElementAttributeChangeString(id: number, attributeName: string, value: string): void {
    const attributeNameParam = this.attributeCache.get(attributeName);

    let index = this.attachedValueIndexByString[value];
    if (!index) {
      const attachedValues = this.attachedValues;
      index = attachedValues.length;
      attachedValues.push(value);
      this.attachedValueIndexByString[value] = index;
    }

    this.buffer.putUint32_3(RenderRequestEntryType.SET_ATTRIBUTE_ATTACHED_VALUE | (id << 8), attributeNameParam, index);
  }

  onElementAttributeChangeTrue(id: number, attributeName: string): void {
    const attributeNameParam = this.attributeCache.get(attributeName);
    this.buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_TRUE | (id << 8), attributeNameParam);
  }

  onElementAttributeChangeFalse(id: number, attributeName: string): void {
    const attributeNameParam = this.attributeCache.get(attributeName);
    this.buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_FALSE | (id << 8), attributeNameParam);
  }

  onElementAttributeChangeUndefined(id: number, attributeName: string): void {
    const attributeNameParam = this.attributeCache.get(attributeName);
    this.buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_UNDEFINED | (id << 8), attributeNameParam);
  }

  onElementAttributeChangeStyle(id: number, attributeName: string, style: Style<any>): void {
    const attributeNameParam = this.attributeCache.get(attributeName);
    const index = style.toNative(runtime.createCSSRule);

    this.buffer.putUint32_3(RenderRequestEntryType.SET_ATTRIBUTE_STYLE | (id << 8), attributeNameParam, index);
  }

  onElementAttributeChangeFunction(id: number, attributeName: string, fn: () => void): void {
    const attributeNameParam = this.attributeCache.get(attributeName);
    const attachedValues = this.attachedValues;
    const attachedValuesIndex = attachedValues.length;
    attachedValues.push(fn);

    this.buffer.putUint32_3(
      RenderRequestEntryType.SET_ATTRIBUTE_ATTACHED_VALUE | (id << 8),
      attributeNameParam,
      attachedValuesIndex,
    );
  }

  onElementAttributeChangeAny(id: number, attributeName: string, attributeValue: any): void {
    let attributeNameParam: number;

    if (attributeName[0] === '$') {
      attributeNameParam = this.injectedAttributeCache.get(attributeName) | (1 << 24);
    } else {
      attributeNameParam = this.attributeCache.get(attributeName);
    }

    const buffer = this.buffer;

    if (attributeValue === undefined) {
      buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_UNDEFINED | (id << 8), attributeNameParam);
    } else if (attributeValue === null) {
      buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_NULL | (id << 8), attributeNameParam);
    } else if (typeof attributeValue === 'boolean') {
      if (!attributeValue) {
        buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_FALSE | (id << 8), attributeNameParam);
      } else {
        buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_TRUE | (id << 8), attributeNameParam);
      }
    } else if (typeof attributeValue === 'number') {
      buffer.putUint32_2(RenderRequestEntryType.SET_ATTRIBUTE_DOUBLE | (id << 8), attributeNameParam);
      buffer.putFloat64(attributeValue);
    } else if (Array.isArray(attributeValue)) {
      buffer.putUint32_3(
        RenderRequestEntryType.SET_ATTRIBUTE_ARRAY | (id << 8),
        attributeNameParam,
        attributeValue.length,
      );

      for (const value of attributeValue) {
        const index = this.getAttachedValueIndex(value);

        buffer.putUint32(index);
      }
    } else if (attributeValue instanceof Style) {
      const index = attributeValue.toNative(runtime.createCSSRule);

      buffer.putUint32_3(RenderRequestEntryType.SET_ATTRIBUTE_STYLE | (id << 8), attributeNameParam, index);
    } else {
      const attachedValuesIndex = this.getAttachedValueIndex(attributeValue);

      buffer.putUint32_3(
        RenderRequestEntryType.SET_ATTRIBUTE_ATTACHED_VALUE | (id << 8),
        attributeNameParam,
        attachedValuesIndex,
      );
    }
  }

  onNextLayoutComplete(cb: () => void) {
    const attachedValuesIndex = this.getAttachedValueIndex(cb);
    this.buffer.putUint32_2(RenderRequestEntryType.ON_LAYOUT_COMPLETE, attachedValuesIndex);
  }

  onNextDraw(cb: (hookTimeMs: number) => void) {
    const attachedValuesIndex = this.getAttachedValueIndex(cb);
    this.buffer.putUint32_2(RenderRequestEntryType.ON_NEXT_DRAW, attachedValuesIndex);
  }

  onRenderStart(): void {
    this.renderStartTime = new Date().getTime();
    this.buffer = this.getRenderBuffer();
  }

  onRenderEnd(): void {
    const visibilityObserver = this.pendingVisibilityObserver;
    const frameObserver = this.pendingFrameObserver;

    if (this.destroyed || (this.buffer.empty() && !visibilityObserver && !frameObserver)) {
      this.buffer.rewind();
      this.releaseRenderBuffer(this.buffer);
      return;
    }

    // const renderStartTime = this.renderStartTime || 0;
    // const renderEndTime = new Date().getTime();

    const descriptor = this.buffer.inner();
    const descriptorSize = this.buffer.size() / 4;
    this.buffer.rewind();

    const values = this.attachedValues;
    if (values.length) {
      this.attachedValues = [];
      this.attachedValueIndexByString = {};
      this.attachedValueIndexByObjectId = {};
    }

    this.pendingVisibilityObserver = undefined;
    this.pendingFrameObserver = undefined;

    // this.buffer may change in submitRawRenderRequest() if it re-enters JS code
    const buffer = this.buffer;

    runtime.submitRawRenderRequest(
      {
        treeId: this.treeId,
        descriptor,
        descriptorSize,
        values,
        visibilityObserver,
        frameObserver,
      },
      () => {
        this.releaseRenderBuffer(buffer);
        // const renderDuration = new Date().getTime() - renderStartTime;
        // console.log(
        //   `Completed render in ${renderEndTime -
        //     renderStartTime}ms, took ${renderDuration}ms in total (buffer size ${descriptorSize * 4} bytes, ${
        //     values.length
        //   } attached values)`,
        // );
      },
    );
  }

  private getRenderBuffer(): Buffer {
    return bufferPool.pop() || new Buffer(512);
  }

  private releaseRenderBuffer(buffer: Buffer) {
    bufferPool.push(buffer);
  }

  registerVisibilityObserver(observer: VisibilityObserver) {
    this.pendingVisibilityObserver = (
      appearingElements,
      disappearingElements,
      viewportUpdates,
      acknowledger,
      eventTime,
    ) => {
      try {
        observer(appearingElements, disappearingElements, viewportUpdates, eventTime);
      } finally {
        // The acknowledger is used to workaround the fact that calls between the main thread and the js threads
        // are asynchronous.
        acknowledger();
      }
    };
  }

  registerFrameObserver(observer: FrameObserver) {
    this.pendingFrameObserver = observer;
  }

  getNativeView(id: number, callback: (view: NativeView | undefined) => void) {
    runtime.getNativeViewForElementId(this.treeId, id, view => {
      if (view) {
        callback(view);
      } else {
        callback(undefined);
      }
    });
  }

  getNativeNode(id: number): NativeNode | undefined {
    return runtime.getNativeNodeForElementId(this.treeId, id);
  }

  getElementFrame(id: number, callback: (frame: ElementFrame | undefined) => void) {
    runtime.getFrameForElementId(this.treeId, id, callback);
  }

  takeElementSnapshot(id: number, callback: (snapshotBase64: string | undefined) => void) {
    runtime.takeElementSnapshot(this.treeId, id, callback);
  }

  onUncaughtError(message: string, error: Error) {
    runtime.onUncaughtError(message, error);
  }
}
