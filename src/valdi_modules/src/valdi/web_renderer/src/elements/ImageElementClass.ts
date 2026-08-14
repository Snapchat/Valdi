import { AttributesBinder } from '../attributes/AttributesBinder';
import { parseBoolean, parseCssLength, parseNumber, parseString } from '../attributes/AttributeApplierHelpers';
import {
  AttributeApplierContext,
  CompositeAttribute,
  ElementClass,
  LayoutAnimationSizeApplier,
  LayoutAnimationTranslationCorrection,
} from '../core/ElementClass';
import { ParsedCssColor, parseCssColor } from '../utils/cssColor';
import { ImageFilterOperation, parseImageFilterOperations } from '../utils/imageFilterOperations';
import { svgViewBoxIntrinsicSize } from '../utils/imageSource';
import { ImageObjectFit } from './CanvasImageRenderer';
import { AttributeApplierMap, assignStyles, resolveRenderableSrc } from './ElementClassSupport';
import {
  ImageAssetLoadCallback,
  ImageDecodedCallback,
  ImageElement,
  ImageElementConfiguration,
  ImageLogicalSize,
} from './ImageElement';
import { ViewElementClass } from './ViewElementClass';

const IMAGE_ELEMENT_STATE = '__imageElementClassState';

interface TransformOrigin {
  readonly x: number;
  readonly y: number;
}

interface IndependentScale {
  readonly x: number;
  readonly y: number;
}

class ImageLayoutAnimationSizeApplier implements LayoutAnimationSizeApplier {
  private readonly originalScale: string;
  private readonly origin: TransformOrigin;
  private readonly originalScaleX: number;
  private readonly originalScaleY: number;

  constructor(
    private readonly element: HTMLElement,
    private readonly imageElement: ImageElement | undefined,
    finalWidth: number,
    finalHeight: number,
  ) {
    this.originalScale = element.style.getPropertyValue('scale');
    const originalScale = parseIndependentScale(this.originalScale);
    this.originalScaleX = originalScale.x;
    this.originalScaleY = originalScale.y;
    this.origin = resolveTransformOrigin(element, finalWidth, finalHeight);
    imageElement?.setLayoutAnimationSize(finalWidth, finalHeight);
  }

  apply(scaleX: number, scaleY: number): LayoutAnimationTranslationCorrection {
    this.element.style.setProperty('scale', `${this.originalScaleX * scaleX} ${this.originalScaleY * scaleY}`);
    return {
      x: -(1 - scaleX) * this.origin.x * this.originalScaleX,
      y: -(1 - scaleY) * this.origin.y * this.originalScaleY,
    };
  }

  reset(): void {
    this.imageElement?.clearLayoutAnimationSize();
    if (this.originalScale) {
      this.element.style.setProperty('scale', this.originalScale);
    } else {
      this.element.style.removeProperty('scale');
    }
  }
}

function parseIndependentScale(value: string): IndependentScale {
  const source = value.trim();
  if (!source || source === 'none') {
    return { x: 1, y: 1 };
  }
  const tokens = source.split(/\s+/);
  const x = parseScaleComponent(tokens[0]);
  return { x, y: tokens.length > 1 ? parseScaleComponent(tokens[1]) : x };
}

function parseScaleComponent(value: string): number {
  const parsed = Number.parseFloat(value);
  return value.endsWith('%') ? parsed / 100 : parsed;
}

function resolveTransformOrigin(element: HTMLElement, width: number, height: number): TransformOrigin {
  const value =
    typeof getComputedStyle === 'function' ? getComputedStyle(element).transformOrigin : element.style.transformOrigin;
  const tokens = value.trim().split(/\s+/).filter(Boolean).slice(0, 2);
  let horizontal: string | undefined;
  let vertical: string | undefined;
  const remaining: string[] = [];
  for (const token of tokens) {
    if ((token === 'left' || token === 'right') && horizontal === undefined) {
      horizontal = token;
    } else if ((token === 'top' || token === 'bottom') && vertical === undefined) {
      vertical = token;
    } else {
      remaining.push(token);
    }
  }
  if (horizontal === undefined && remaining.length > 0) {
    horizontal = remaining.shift();
  }
  if (vertical === undefined && remaining.length > 0) {
    vertical = remaining.shift();
  }
  return {
    x: resolveTransformOriginComponent(horizontal, width, 'left', 'right'),
    y: resolveTransformOriginComponent(vertical, height, 'top', 'bottom'),
  };
}

function resolveTransformOriginComponent(
  value: string | undefined,
  size: number,
  startKeyword: string,
  endKeyword: string,
): number {
  if (value === undefined || value === 'center') {
    return size / 2;
  }
  if (value === startKeyword) {
    return 0;
  }
  if (value === endKeyword) {
    return size;
  }
  const parsed = Number.parseFloat(value);
  if (!Number.isFinite(parsed)) {
    return size / 2;
  }
  return value.endsWith('%') ? (parsed / 100) * size : parsed;
}

interface ResolvedImageSource {
  logicalSize: ImageLogicalSize | undefined;
  source: string | undefined;
}

interface ResolvedImageFilter {
  cssFilter: string;
  operations: ImageFilterOperation[];
}

const DEFAULT_IMAGE_CONFIGURATION: ImageElementConfiguration = {
  contentRotation: 0,
  contentScaleX: 1,
  contentScaleY: 1,
  cssFilter: '',
  explicitHeight: undefined,
  explicitWidth: undefined,
  filterOperations: [],
  flipOnRtl: false,
  logicalSize: undefined,
  objectFit: 'fill',
  source: undefined,
  tint: undefined,
};

function getImageElement(element: HTMLElement, context: AttributeApplierContext): ImageElement {
  const existing = context.getState<ImageElement>(IMAGE_ELEMENT_STATE);
  if (existing) {
    return existing;
  }
  const imageElement = new ImageElement(
    element,
    callback => context.enqueuePostLayoutCallback(callback),
    () => context.requestLayoutPass(),
  );
  context.setState(IMAGE_ELEMENT_STATE, imageElement);
  context.addCleanup(() => imageElement.destroy());
  return imageElement;
}

function resolveImageSource(value: unknown): ResolvedImageSource {
  const source = resolveRenderableSrc(value);
  return {
    source,
    logicalSize: source ? svgViewBoxIntrinsicSize(source) : undefined,
  };
}

function resolveImageFilter(value: unknown): ResolvedImageFilter {
  const operations = parseImageFilterOperations(value);
  return operations
    ? { cssFilter: '', operations }
    : { cssFilter: typeof value === 'string' ? value : '', operations: [] };
}

function configureImageElement(
  element: HTMLElement,
  context: AttributeApplierContext,
  attributeName: string,
  values: ReadonlyArray<unknown>,
): void {
  const source = values[0] as ResolvedImageSource | undefined;
  const objectFit = values[1] as ImageObjectFit | undefined;
  const tint = values[2] as ParsedCssColor | undefined;
  const flipOnRtl = values[3] as boolean | undefined;
  const contentScaleX = values[4] as number | undefined;
  const contentScaleY = values[5] as number | undefined;
  const contentRotation = values[6] as number | undefined;
  const filter = values[7] as ResolvedImageFilter | undefined;
  const explicitWidth = values[8] as string | undefined;
  const explicitHeight = values[9] as string | undefined;
  const imageElement = getImageElement(element, context);
  imageElement.configure({
    contentRotation: contentRotation || 0,
    contentScaleX: contentScaleX || 1,
    contentScaleY: contentScaleY || 1,
    cssFilter: filter?.cssFilter ?? '',
    explicitHeight,
    explicitWidth,
    filterOperations: filter?.operations ?? [],
    flipOnRtl: flipOnRtl ?? false,
    logicalSize: source?.logicalSize,
    objectFit: objectFit ?? 'fill',
    source: source?.source,
    tint,
  });
  context.setLayoutObserver(attributeName, source?.source ? imageElement : undefined);
}

const imageRenderComposite: CompositeAttribute<HTMLElement> = {
  name: 'imageRenderComposite',
  parts: [
    { name: 'src', optional: true, layoutDependent: true, parse: (_element, value) => resolveImageSource(value) },
    { name: 'objectFit', optional: true, parse: (_element, value, name) => parseString(value, name) },
    {
      name: 'tint',
      optional: true,
      colorDependent: true,
      parse: (_element, value, name, context) => parseCssColor(context.resolveColor(parseString(value, name))),
    },
    { name: 'flipOnRtl', optional: true, parse: (_element, value, name) => parseBoolean(value, name) },
    { name: 'contentScaleX', optional: true, parse: (_element, value, name) => parseNumber(value, name) },
    { name: 'contentScaleY', optional: true, parse: (_element, value, name) => parseNumber(value, name) },
    { name: 'contentRotation', optional: true, parse: (_element, value, name) => parseNumber(value, name) },
    { name: 'filter', optional: true, parse: (_element, value) => resolveImageFilter(value) },
    {
      name: 'width',
      optional: true,
      layoutDependent: true,
      parse: (_element, value, name) => parseCssLength(value, name),
    },
    {
      name: 'height',
      optional: true,
      layoutDependent: true,
      parse: (_element, value, name) => parseCssLength(value, name),
    },
  ],
  apply(element, values, attributeName, context) {
    configureImageElement(element, context, attributeName, values);
  },
  reset(element, attributeName, context) {
    getImageElement(element, context).configure(DEFAULT_IMAGE_CONFIGURATION);
    context.setLayoutObserver(attributeName, undefined);
  },
};

function buildImageAttributeAppliers(viewElementClass: ViewElementClass): AttributeApplierMap<HTMLElement> {
  const binder = new AttributesBinder<HTMLElement>();
  binder.bindFunctionAttribute(
    'onAssetLoad',
    (element, callback, context) =>
      getImageElement(element, context).setOnAssetLoad(callback as ImageAssetLoadCallback),
    (element, context) => getImageElement(element, context).setOnAssetLoad(undefined),
  );
  binder.bindFunctionAttribute(
    'onImageDecoded',
    (element, callback, context) =>
      getImageElement(element, context).setOnImageDecoded(callback as ImageDecodedCallback),
    (element, context) => getImageElement(element, context).setOnImageDecoded(undefined),
  );
  binder.bindNoOpAttribute('ref');
  return {
    ...(viewElementClass.attributeAppliers as AttributeApplierMap<HTMLElement>),
    ...binder.attributeAppliers,
  };
}

export class ImageElementClass extends ElementClass<HTMLElement> {
  constructor(viewElementClass: ViewElementClass) {
    super('image', buildImageAttributeAppliers(viewElementClass), {
      ...viewElementClass.compositeAttributes,
      imageRenderComposite,
    });
  }

  protected onCreateElement(): HTMLElement {
    const element = document.createElement('div');
    assignStyles(element, {
      display: 'block',
      overflow: 'hidden',
      position: 'relative',
    });
    return element;
  }

  override makeLayoutAnimationSizeApplier(
    element: HTMLElement,
    context: AttributeApplierContext,
    finalWidth: number,
    finalHeight: number,
  ): LayoutAnimationSizeApplier {
    return new ImageLayoutAnimationSizeApplier(
      element,
      context.getState<ImageElement>(IMAGE_ELEMENT_STATE),
      finalWidth,
      finalHeight,
    );
  }
}
