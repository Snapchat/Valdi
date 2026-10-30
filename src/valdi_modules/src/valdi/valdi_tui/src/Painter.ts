import { Rect, Screen, intersectRect } from './Screen';
import { TextStyle } from './TextStyle';
import { BorderCharacters, resolveBorderStyle } from './borders';
import { TextRun, WrapMode, layoutText } from './wrap';

/** View classes rendered by the painter. Components emit them through `<custom-view iosClass=...>`. */
export const TUI_BOX_CLASS = 'ValdiTUIBox';
export const TUI_TEXT_CLASS = 'ValdiTUIText';
export const TUI_STATIC_CLASS = 'ValdiTUIStatic';

/** Attribute names carried on TUI elements and read back from the rendered tree. */
export const TUI_ATTRIBUTES = {
  runs: 'tuiRuns',
  wrap: 'tuiWrap',
  borderStyle: 'tuiBorderStyle',
  borderColor: 'tuiBorderColor',
  borderDimColor: 'tuiBorderDimColor',
  borderTop: 'tuiBorderTop',
  borderBottom: 'tuiBorderBottom',
  borderLeft: 'tuiBorderLeft',
  borderRight: 'tuiBorderRight',
  borderTopColor: 'tuiBorderTopColor',
  borderBottomColor: 'tuiBorderBottomColor',
  borderLeftColor: 'tuiBorderLeftColor',
  borderRightColor: 'tuiBorderRightColor',
  transform: 'tuiTransform',
} as const;

/** Structural subset of the standalone runtime's RenderedElement. */
export interface PaintNode {
  readonly viewClass: string;
  readonly frame: { readonly x: number; readonly y: number; readonly width: number; readonly height: number };
  readonly attributes: { readonly [name: string]: any };
  readonly children: ReadonlyArray<PaintNode>;
}

export interface PaintedFrame {
  /** The live region. */
  readonly screen: Screen;
  /** Content of `<Static>` elements, rendered in document order. */
  readonly staticScreens: ReadonlyArray<Screen>;
}

function roundRect(frame: PaintNode['frame'], originX: number, originY: number): Rect {
  const x = Math.round(originX + frame.x);
  const y = Math.round(originY + frame.y);
  const right = Math.round(originX + frame.x + frame.width);
  const bottom = Math.round(originY + frame.y + frame.height);
  return { x, y, width: Math.max(0, right - x), height: Math.max(0, bottom - y) };
}

function hasBorderSide(attributes: PaintNode['attributes'], name: string): boolean {
  const value = attributes[name];
  return value === undefined || value === true;
}

function sideStyle(attributes: PaintNode['attributes'], sideColorAttribute: string): TextStyle | undefined {
  const color = attributes[sideColorAttribute] ?? attributes[TUI_ATTRIBUTES.borderColor];
  const dim = attributes[TUI_ATTRIBUTES.borderDimColor] === true;
  if (color === undefined && !dim) {
    return undefined;
  }
  return { color, dim };
}

function paintBorder(screen: Screen, rect: Rect, attributes: PaintNode['attributes'], clip: Rect): void {
  const characters: BorderCharacters | undefined = resolveBorderStyle(attributes[TUI_ATTRIBUTES.borderStyle]);
  if (!characters || rect.width === 0 || rect.height === 0) {
    return;
  }
  const top = hasBorderSide(attributes, TUI_ATTRIBUTES.borderTop);
  const bottom = hasBorderSide(attributes, TUI_ATTRIBUTES.borderBottom);
  const left = hasBorderSide(attributes, TUI_ATTRIBUTES.borderLeft);
  const right = hasBorderSide(attributes, TUI_ATTRIBUTES.borderRight);
  const x0 = rect.x;
  const x1 = rect.x + rect.width - 1;
  const y0 = rect.y;
  const y1 = rect.y + rect.height - 1;

  if (top) {
    const style = sideStyle(attributes, TUI_ATTRIBUTES.borderTopColor);
    const innerWidth = rect.width - (left ? 1 : 0) - (right ? 1 : 0);
    const line =
      (left ? characters.topLeft : '') +
      characters.top.repeat(Math.max(0, innerWidth)) +
      (right ? characters.topRight : '');
    screen.writeText(x0, y0, line, style, clip);
  }
  if (bottom && rect.height > 1) {
    const style = sideStyle(attributes, TUI_ATTRIBUTES.borderBottomColor);
    const innerWidth = rect.width - (left ? 1 : 0) - (right ? 1 : 0);
    const line =
      (left ? characters.bottomLeft : '') +
      characters.bottom.repeat(Math.max(0, innerWidth)) +
      (right ? characters.bottomRight : '');
    screen.writeText(x0, y1, line, style, clip);
  }
  const firstRow = top ? y0 + 1 : y0;
  const lastRow = bottom ? y1 - 1 : y1;
  if (left) {
    const style = sideStyle(attributes, TUI_ATTRIBUTES.borderLeftColor);
    for (let y = firstRow; y <= lastRow; y++) {
      screen.writeText(x0, y, characters.left, style, clip);
    }
  }
  if (right && rect.width > 1) {
    const style = sideStyle(attributes, TUI_ATTRIBUTES.borderRightColor);
    for (let y = firstRow; y <= lastRow; y++) {
      screen.writeText(x1, y, characters.right, style, clip);
    }
  }
}

function textRuns(attributes: PaintNode['attributes']): ReadonlyArray<TextRun> {
  const runs = attributes[TUI_ATTRIBUTES.runs];
  if (Array.isArray(runs)) {
    return runs as TextRun[];
  }
  // Plain <label> elements: render their value with the label color.
  const value = attributes.value;
  if (typeof value === 'string') {
    return [{ text: value, style: attributes.color ? { color: attributes.color } : undefined }];
  }
  return [];
}

export function paintTextNode(
  screen: Screen,
  rect: Rect,
  layoutWidth: number,
  attributes: PaintNode['attributes'],
  clip: Rect,
): void {
  const runs = textRuns(attributes);
  if (runs.length === 0) {
    return;
  }
  const wrap = (attributes[TUI_ATTRIBUTES.wrap] as WrapMode | undefined) ?? 'wrap';
  // Wrap at the same rounded width the measure callback used, so lines match the measured height
  // even when the rounded frame edges give a width one cell off.
  const wrapWidth = Math.max(0, Math.round(layoutWidth));
  const lines = layoutText(runs, wrapWidth, wrap);
  const area = intersectRect(
    { x: rect.x, y: rect.y, width: Math.max(rect.width, wrapWidth), height: rect.height },
    clip,
  );
  for (let i = 0; i < lines.length && rect.y + i < area.y + area.height; i++) {
    screen.writeSegments(rect.x, rect.y + i, lines[i]!.segments, area);
  }
}

function applyTransform(screen: Screen, rect: Rect, transform: unknown, clip: Rect): void {
  if (typeof transform !== 'function') {
    return;
  }
  const area = intersectRect(rect, clip);
  for (let y = area.y, index = 0; y < area.y + area.height; y++, index++) {
    const text = screen.getRowText(y, rect.x, rect.width);
    let replaced: unknown;
    try {
      replaced = (transform as (line: string, index: number) => string)(text, index);
    } catch (error) {
      console.error('Transform callback failed:', error);
      return;
    }
    if (typeof replaced === 'string' && replaced !== text) {
      screen.setRowText(y, rect.x, rect.width, replaced, area);
    }
  }
}

class Painter {
  readonly staticScreens: Screen[] = [];

  constructor(readonly screen: Screen) {}

  paintNode(node: PaintNode, originX: number, originY: number, clip: Rect, target: Screen): void {
    const rect = roundRect(node.frame, originX, originY);
    const attributes = node.attributes ?? {};
    if (attributes.display === 'none') {
      return;
    }

    if (node.viewClass === TUI_STATIC_CLASS) {
      if (target !== this.screen) {
        // Nested static content is painted as part of its enclosing static block.
        this.paintChildren(node, rect, attributes, clip, target);
        return;
      }
      const staticScreen = new Screen(Math.max(0, rect.width), Math.max(0, rect.height));
      this.paintChildren(
        node,
        { x: 0, y: 0, width: rect.width, height: rect.height },
        attributes,
        staticScreen.bounds,
        staticScreen,
      );
      this.staticScreens.push(staticScreen);
      return;
    }

    if (node.viewClass === TUI_TEXT_CLASS || node.viewClass === 'SCValdiLabel') {
      paintTextNode(target, rect, node.frame.width, attributes, clip);
      return;
    }

    const backgroundColor = attributes.backgroundColor;
    if (typeof backgroundColor === 'string' && backgroundColor !== '' && backgroundColor !== 'transparent') {
      target.fillBackground(rect, backgroundColor, clip);
    }
    paintBorder(target, rect, attributes, clip);
    this.paintChildren(node, rect, attributes, clip, target);
    applyTransform(target, rect, attributes[TUI_ATTRIBUTES.transform], clip);
  }

  private paintChildren(
    node: PaintNode,
    rect: Rect,
    attributes: PaintNode['attributes'],
    clip: Rect,
    target: Screen,
  ): void {
    const childClip = attributes.overflow === 'hidden' ? intersectRect(rect, clip) : clip;
    for (const child of node.children) {
      this.paintNode(child, rect.x, rect.y, childClip, target);
    }
  }
}

/**
 * Paints a rendered element tree into a screen of the given size. Frames are expected in
 * the parent's coordinate space, as reported by the standalone runtime.
 */
export function paintTree(root: PaintNode, width: number, height: number): PaintedFrame {
  const screen = new Screen(Math.max(0, width), Math.max(0, height));
  const painter = new Painter(screen);
  painter.paintNode(root, 0, 0, screen.bounds, screen);
  return { screen, staticScreens: painter.staticScreens };
}
