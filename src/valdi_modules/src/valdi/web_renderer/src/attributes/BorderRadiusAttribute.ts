import type { AnimationInterpolator } from './AttributeAnimation';
import { parseCssLength } from './AttributeApplierHelpers';
import { MIN_VISIBLE_CHANGE_PIXEL } from './AttributesBinder';
import type { AttributeApplier, AttributeApplierContext, ElementLayoutObserver } from '../core/ElementClass';
import { getViewPresentationState, type ViewPresentationState } from '../elements/ViewElementState';
import { readCssNumber, readWhitespaceSeparatedToken } from '../utils/cssScanner';

interface BorderRadiusCorner {
  readonly pixels: number;
  readonly percent: number;
}

interface BorderRadiusCorners {
  readonly topLeft: BorderRadiusCorner;
  readonly topRight: BorderRadiusCorner;
  readonly bottomRight: BorderRadiusCorner;
  readonly bottomLeft: BorderRadiusCorner;
}

interface ParsedBorderRadius {
  readonly corners: BorderRadiusCorners | undefined;
  readonly css: string;
  readonly usesPercent: boolean;
}

class AnimatedBorderRadius {
  constructor(
    readonly corners: BorderRadiusCorners,
    readonly usesPercent: boolean,
  ) {}
}

const ZERO_CORNER: BorderRadiusCorner = { pixels: 0, percent: 0 };
const ZERO_BORDER_RADIUS = new AnimatedBorderRadius(
  {
    topLeft: ZERO_CORNER,
    topRight: ZERO_CORNER,
    bottomRight: ZERO_CORNER,
    bottomLeft: ZERO_CORNER,
  },
  false,
);

function formatNumber(value: number): string {
  return String(Object.is(value, -0) ? 0 : value);
}

function formatPixels(value: number): string {
  return `${formatNumber(value)}px`;
}

function formatPercent(value: number): string {
  return `${formatNumber(value)}%`;
}

function parseBorderRadiusToken(
  token: string,
): { corner: BorderRadiusCorner; css: string; usesPercent: boolean } | undefined {
  const number = readCssNumber(token, 0);
  if (!number || !Number.isFinite(number.value)) {
    return undefined;
  }
  const unit = token.slice(number.nextIndex);
  if (unit === '%') {
    return {
      corner: { pixels: 0, percent: number.value },
      css: formatPercent(number.value),
      usesPercent: true,
    };
  }
  if (unit === 'px' || unit === 'pt') {
    return {
      corner: { pixels: number.value, percent: 0 },
      css: formatPixels(number.value),
      usesPercent: false,
    };
  }
  return undefined;
}

function expandCorners(corners: BorderRadiusCorner[]): BorderRadiusCorners {
  if (corners.length === 1) {
    return {
      topLeft: corners[0],
      topRight: corners[0],
      bottomRight: corners[0],
      bottomLeft: corners[0],
    };
  }
  if (corners.length === 2) {
    return {
      topLeft: corners[0],
      topRight: corners[1],
      bottomRight: corners[0],
      bottomLeft: corners[1],
    };
  }
  if (corners.length === 3) {
    return {
      topLeft: corners[0],
      topRight: corners[1],
      bottomRight: corners[2],
      bottomLeft: corners[1],
    };
  }
  return {
    topLeft: corners[0],
    topRight: corners[1],
    bottomRight: corners[2],
    bottomLeft: corners[3],
  };
}

function parseBorderRadius(value: unknown, attributeName: string): ParsedBorderRadius {
  const source = parseCssLength(value, attributeName).trim();
  const corners: BorderRadiusCorner[] = [];
  const cssTokens: string[] = [];
  let usesPercent = false;
  let index = 0;
  while (index < source.length) {
    const token = readWhitespaceSeparatedToken(source, index);
    if (!token) {
      break;
    }
    const parsedToken = parseBorderRadiusToken(token.token);
    if (!parsedToken) {
      return { corners: undefined, css: source, usesPercent: false };
    }
    corners.push(parsedToken.corner);
    cssTokens.push(parsedToken.css);
    usesPercent = usesPercent || parsedToken.usesPercent;
    index = token.nextIndex;
  }
  if (corners.length === 0 || corners.length > 4) {
    return { corners: undefined, css: source, usesPercent: false };
  }
  return {
    corners: expandCorners(corners),
    css: cssTokens.join(' '),
    usesPercent,
  };
}

function parseAnimationEndpoint(value: unknown): AnimatedBorderRadius | undefined {
  if (value === undefined || value === null) {
    return ZERO_BORDER_RADIUS;
  }
  if (value instanceof AnimatedBorderRadius) {
    return value;
  }
  const parsed = parseBorderRadius(value, 'borderRadius');
  if (!parsed.corners) {
    return undefined;
  }
  if (
    !isValidAnimationCorner(parsed.corners.topLeft) ||
    !isValidAnimationCorner(parsed.corners.topRight) ||
    !isValidAnimationCorner(parsed.corners.bottomRight) ||
    !isValidAnimationCorner(parsed.corners.bottomLeft)
  ) {
    return undefined;
  }
  return new AnimatedBorderRadius(parsed.corners, parsed.usesPercent);
}

function isValidAnimationCorner(corner: BorderRadiusCorner): boolean {
  return corner.pixels >= 0 && corner.percent >= 0;
}

function interpolateCorner(from: BorderRadiusCorner, to: BorderRadiusCorner, progress: number): BorderRadiusCorner {
  return {
    pixels: from.pixels + (to.pixels - from.pixels) * progress,
    percent: from.percent + (to.percent - from.percent) * progress,
  };
}

function makeBorderRadiusInterpolator(from: unknown, to: unknown): AnimationInterpolator | undefined {
  const start = parseAnimationEndpoint(from);
  const end = parseAnimationEndpoint(to);
  if (!start || !end) {
    return undefined;
  }
  const usesPercent = start.usesPercent || end.usesPercent;
  return progress => {
    const clampedProgress = Math.max(0, Math.min(1, progress));
    return new AnimatedBorderRadius(
      {
        topLeft: interpolateCorner(start.corners.topLeft, end.corners.topLeft, clampedProgress),
        topRight: interpolateCorner(start.corners.topRight, end.corners.topRight, clampedProgress),
        bottomRight: interpolateCorner(start.corners.bottomRight, end.corners.bottomRight, clampedProgress),
        bottomLeft: interpolateCorner(start.corners.bottomLeft, end.corners.bottomLeft, clampedProgress),
      },
      usesPercent,
    );
  };
}

function formatResolvedBorderRadius(corners: BorderRadiusCorners, sideLength: number): string {
  const sizeRatio = sideLength / 100;
  const topLeft = Math.max(0, corners.topLeft.pixels + corners.topLeft.percent * sizeRatio);
  const topRight = Math.max(0, corners.topRight.pixels + corners.topRight.percent * sizeRatio);
  const bottomRight = Math.max(0, corners.bottomRight.pixels + corners.bottomRight.percent * sizeRatio);
  const bottomLeft = Math.max(0, corners.bottomLeft.pixels + corners.bottomLeft.percent * sizeRatio);
  if (topLeft === topRight && topLeft === bottomRight && topLeft === bottomLeft) {
    return formatPixels(topLeft);
  }
  if (topLeft === bottomRight && topRight === bottomLeft) {
    return `${formatPixels(topLeft)} ${formatPixels(topRight)}`;
  }
  if (topRight === bottomLeft) {
    return `${formatPixels(topLeft)} ${formatPixels(topRight)} ${formatPixels(bottomRight)}`;
  }
  return `${formatPixels(topLeft)} ${formatPixels(topRight)} ${formatPixels(bottomRight)} ${formatPixels(bottomLeft)}`;
}

function hasPercentComponent(corners: BorderRadiusCorners): boolean {
  return (
    corners.topLeft.percent !== 0 ||
    corners.topRight.percent !== 0 ||
    corners.bottomRight.percent !== 0 ||
    corners.bottomLeft.percent !== 0
  );
}

function applyBorderRadiusCss(element: HTMLElement, css: string, updatesClipPath: boolean): void {
  element.style.borderRadius = css;
  if (updatesClipPath) {
    element.style.clipPath = css ? `inset(0 round ${css})` : '';
  }
}

function applyResolvedBorderRadius(
  element: HTMLElement,
  viewAttributeElement: HTMLElement,
  state: ViewPresentationState,
  css: string,
  updatesClipPath: boolean,
): void {
  state.borderRadiusCss = css;
  applyBorderRadiusCss(viewAttributeElement, css, updatesClipPath);
  if (viewAttributeElement !== element) {
    element.style.borderRadius = state.slowClipping ? css : '';
  }
}

class BorderRadiusLayoutObserver implements ElementLayoutObserver {
  private corners: BorderRadiusCorners = ZERO_BORDER_RADIUS.corners;
  private css: string | undefined = '';
  private hasSize = false;
  private sideLength = 0;

  constructor(
    private readonly hostElement: HTMLElement,
    private readonly viewAttributeElement: HTMLElement,
    private readonly state: ViewPresentationState,
    private readonly updatesClipPath: boolean,
  ) {}

  update(corners: BorderRadiusCorners, fallbackCss: string | undefined): string | undefined {
    this.corners = corners;
    if (this.hasSize) {
      this.css = formatResolvedBorderRadius(corners, this.sideLength);
    } else if (!hasPercentComponent(corners)) {
      this.css = formatResolvedBorderRadius(corners, 0);
    } else {
      this.css = fallbackCss;
    }
    return this.css;
  }

  onSizeChanged(width: number, height: number): void {
    this.hasSize = true;
    this.sideLength = Math.min(width, height);
    this.css = formatResolvedBorderRadius(this.corners, this.sideLength);
  }

  onCommit(_element: HTMLElement): void {
    if (this.css !== undefined) {
      applyResolvedBorderRadius(
        this.hostElement,
        this.viewAttributeElement,
        this.state,
        this.css,
        this.updatesClipPath,
      );
    }
  }
}

function applyObservedBorderRadius(
  element: HTMLElement,
  corners: BorderRadiusCorners,
  fallbackCss: string | undefined,
  attributeName: string,
  context: AttributeApplierContext,
  updatesClipPath: boolean,
): void {
  const viewAttributeElement = context.getViewAttributeElement();
  const state = getViewPresentationState(context);
  const existingObserver = context.getLayoutObserver(attributeName);
  const observer =
    existingObserver instanceof BorderRadiusLayoutObserver
      ? existingObserver
      : new BorderRadiusLayoutObserver(element, viewAttributeElement, state, updatesClipPath);
  const css = observer.update(corners, fallbackCss);
  if (css !== undefined) {
    applyResolvedBorderRadius(element, viewAttributeElement, state, css, updatesClipPath);
  }
  if (observer !== existingObserver) {
    context.setLayoutObserver(attributeName, observer);
  }
}

export function createBorderRadiusAttributeApplier(updatesClipPath: boolean): AttributeApplier {
  return {
    animationMinimumVisibleChange: MIN_VISIBLE_CHANGE_PIXEL,
    makeAnimationInterpolator(_element, from, to, _context) {
      return makeBorderRadiusInterpolator(from, to);
    },
    apply(element, value, attributeName, context) {
      const viewAttributeElement = context.getViewAttributeElement();
      const state = getViewPresentationState(context);
      if (value instanceof AnimatedBorderRadius) {
        if (value.usesPercent) {
          applyObservedBorderRadius(element, value.corners, undefined, attributeName, context, updatesClipPath);
        } else {
          context.setLayoutObserver(attributeName, undefined);
          applyResolvedBorderRadius(
            element,
            viewAttributeElement,
            state,
            formatResolvedBorderRadius(value.corners, 0),
            updatesClipPath,
          );
        }
        return;
      }

      const parsed = parseBorderRadius(value, attributeName);
      if (!parsed.corners || !parsed.usesPercent) {
        context.setLayoutObserver(attributeName, undefined);
        applyResolvedBorderRadius(element, viewAttributeElement, state, parsed.css, updatesClipPath);
        return;
      }

      applyObservedBorderRadius(element, parsed.corners, parsed.css, attributeName, context, updatesClipPath);
    },
    reset(element, attributeName, context) {
      context.setLayoutObserver(attributeName, undefined);
      const viewAttributeElement = context.getViewAttributeElement();
      const state = getViewPresentationState(context);
      applyResolvedBorderRadius(element, viewAttributeElement, state, '', updatesClipPath);
    },
  };
}
