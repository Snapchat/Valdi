import { ANSI, ColorLevel, styleToSgr } from './ansi';
import { Cluster, clusterize } from './textWidth';
import { TextStyle, isEmptyStyle, mergeStyles, stylesEqual } from './TextStyle';
import { StyledSegment } from './wrap';

export interface Rect {
  readonly x: number;
  readonly y: number;
  readonly width: number;
  readonly height: number;
}

export function intersectRect(a: Rect, b: Rect): Rect {
  const x = Math.max(a.x, b.x);
  const y = Math.max(a.y, b.y);
  const right = Math.min(a.x + a.width, b.x + b.width);
  const bottom = Math.min(a.y + a.height, b.y + b.height);
  return { x, y, width: Math.max(0, right - x), height: Math.max(0, bottom - y) };
}

function rectContains(rect: Rect, x: number, y: number): boolean {
  return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

interface Cell {
  text: string;
  /** 0 marks the second column of a wide character. */
  width: number;
  style?: TextStyle;
}

/**
 * A grid of terminal cells that nodes paint into. Serialized once per frame with
 * {@link Screen.toLines}, which emits the minimal SGR changes along each row.
 */
export class Screen {
  private readonly cells: (Cell | undefined)[][];

  constructor(
    readonly width: number,
    readonly height: number,
  ) {
    this.cells = [];
    for (let y = 0; y < height; y++) {
      this.cells.push(new Array<Cell | undefined>(width));
    }
  }

  get bounds(): Rect {
    return { x: 0, y: 0, width: this.width, height: this.height };
  }

  private clearCell(x: number, y: number): void {
    const row = this.cells[y]!;
    const existing = row[x];
    if (!existing) {
      return;
    }
    // Overwriting half of a wide character leaves the other half as a blank cell.
    if (existing.width === 2 && x + 1 < this.width && row[x + 1]?.width === 0) {
      row[x + 1] = { text: ' ', width: 1, style: existing.style };
    } else if (existing.width === 0 && x > 0 && row[x - 1]?.width === 2) {
      row[x - 1] = { text: ' ', width: 1, style: row[x - 1]!.style };
    }
    row[x] = undefined;
  }

  private putCluster(x: number, y: number, cluster: Cluster, style: TextStyle | undefined, clip: Rect): void {
    if (!rectContains(clip, x, y)) {
      return;
    }
    const row = this.cells[y]!;
    const inheritedBackground = row[x]?.style?.backgroundColor;
    const resolvedStyle =
      inheritedBackground && !style?.backgroundColor
        ? mergeStyles({ backgroundColor: inheritedBackground }, style)
        : style;
    if (cluster.width === 2) {
      if (!rectContains(clip, x + 1, y)) {
        this.clearCell(x, y);
        row[x] = { text: ' ', width: 1, style: resolvedStyle };
        return;
      }
      this.clearCell(x, y);
      this.clearCell(x + 1, y);
      row[x] = { text: cluster.text, width: 2, style: resolvedStyle };
      row[x + 1] = { text: '', width: 0, style: resolvedStyle };
      return;
    }
    this.clearCell(x, y);
    row[x] = { text: cluster.text, width: 1, style: resolvedStyle };
  }

  /** Writes clusters left to right starting at (x, y); zero-width clusters attach to the previous cell. */
  writeClusters(
    x: number,
    y: number,
    clusters: ReadonlyArray<Cluster>,
    style: TextStyle | undefined,
    clip: Rect,
  ): number {
    let cursor = x;
    for (const cluster of clusters) {
      if (cluster.width === 0) {
        if (rectContains(clip, cursor - 1, y)) {
          const cell = this.cells[y]![cursor - 1];
          if (cell && cell.width > 0) {
            cell.text += cluster.text;
          }
        }
        continue;
      }
      this.putCluster(cursor, y, cluster, style, clip);
      cursor += cluster.width;
    }
    return cursor;
  }

  writeText(x: number, y: number, text: string, style: TextStyle | undefined, clip: Rect): number {
    return this.writeClusters(x, y, clusterize(text), style, clip);
  }

  writeSegments(x: number, y: number, segments: ReadonlyArray<StyledSegment>, clip: Rect): number {
    let cursor = x;
    for (const segment of segments) {
      cursor = this.writeText(cursor, y, segment.text, segment.style, clip);
    }
    return cursor;
  }

  /** Applies a background color to every cell of `rect`, creating blank cells where needed. */
  fillBackground(rect: Rect, backgroundColor: string, clip: Rect): void {
    const area = intersectRect(rect, clip);
    for (let y = area.y; y < area.y + area.height; y++) {
      const row = this.cells[y]!;
      for (let x = area.x; x < area.x + area.width; x++) {
        const cell = row[x];
        if (!cell) {
          row[x] = { text: ' ', width: 1, style: { backgroundColor } };
        } else {
          cell.style = mergeStyles({ backgroundColor }, cell.style);
        }
      }
    }
  }

  /** Plain text of a row span, with wide characters counted once and empty cells as spaces. */
  getRowText(y: number, x: number, width: number): string {
    if (y < 0 || y >= this.height) {
      return '';
    }
    let out = '';
    const row = this.cells[y]!;
    for (let i = x; i < x + width && i < this.width; i++) {
      const cell = row[i];
      if (!cell) {
        out += ' ';
      } else if (cell.width > 0) {
        out += cell.text;
      }
    }
    return out;
  }

  /**
   * Replaces a row span with new text. Existing cell styles are kept per column so a
   * transform that only changes characters preserves colors.
   */
  setRowText(y: number, x: number, width: number, text: string, clip: Rect): void {
    if (y < 0 || y >= this.height) {
      return;
    }
    const row = this.cells[y]!;
    const previousStyles: (TextStyle | undefined)[] = [];
    for (let i = x; i < x + width; i++) {
      previousStyles.push(row[i]?.style);
    }
    const area = intersectRect({ x, y, width, height: 1 }, clip);
    for (let i = area.x; i < area.x + area.width; i++) {
      this.clearCell(i, y);
    }
    let cursor = x;
    for (const cluster of clusterize(text)) {
      if (cursor >= x + width) {
        break;
      }
      const style = previousStyles[cursor - x];
      if (cluster.width === 0) {
        const cell = row[cursor - 1];
        if (cell && cell.width > 0 && rectContains(area, cursor - 1, y)) {
          cell.text += cluster.text;
        }
        continue;
      }
      this.putCluster(cursor, y, cluster, style, area);
      cursor += cluster.width;
    }
  }

  /** Serializes each row to a string with ANSI styling. Trailing unstyled blanks are trimmed. */
  toLines(level: ColorLevel): string[] {
    const lines: string[] = [];
    for (let y = 0; y < this.height; y++) {
      const row = this.cells[y]!;
      let lastContent = -1;
      for (let x = 0; x < this.width; x++) {
        const cell = row[x];
        if (cell && (cell.text.trim() !== '' || (level !== ColorLevel.None && !isEmptyStyle(cell.style)))) {
          lastContent = x;
        }
      }
      let line = '';
      let currentStyle: TextStyle | undefined;
      let styled = false;
      for (let x = 0; x <= lastContent; x++) {
        const cell = row[x];
        if (cell && cell.width === 0) {
          continue;
        }
        const style = cell?.style;
        if (!stylesEqual(style, currentStyle)) {
          if (styled) {
            line += ANSI.reset;
            styled = false;
          }
          const sgr = styleToSgr(style, level);
          if (sgr) {
            line += sgr;
            styled = true;
          }
          currentStyle = style;
        }
        line += cell ? cell.text : ' ';
      }
      if (styled) {
        line += ANSI.reset;
      }
      lines.push(line);
    }
    return lines;
  }
}
