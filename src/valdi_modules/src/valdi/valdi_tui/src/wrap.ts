import { Cluster, clusterize } from './textWidth';
import { TextStyle, stylesEqual } from './TextStyle';

/**
 * Text overflow behavior, same vocabulary as Ink's `wrap` prop.
 * `wrap` breaks between words (and inside words that are wider than the available width).
 * The `truncate*` modes keep a single line per paragraph and replace the overflow with an ellipsis.
 */
export type WrapMode =
  'wrap' | 'end' | 'middle' | 'start' | 'truncate' | 'truncate-end' | 'truncate-middle' | 'truncate-start';

/** A piece of text sharing one style. */
export interface TextRun {
  readonly text: string;
  readonly style?: TextStyle;
}

export interface StyledSegment {
  readonly text: string;
  readonly width: number;
  readonly style?: TextStyle;
}

export interface StyledLine {
  readonly segments: ReadonlyArray<StyledSegment>;
  readonly width: number;
}

interface StyledCluster extends Cluster {
  readonly style?: TextStyle;
}

const ELLIPSIS: StyledCluster = { text: '…', width: 1 };

function isWhitespace(cluster: Cluster): boolean {
  return cluster.text === ' ' || cluster.text === '\t';
}

function toSegments(clusters: ReadonlyArray<StyledCluster>): StyledLine {
  const segments: StyledSegment[] = [];
  let width = 0;
  let text = '';
  let segmentWidth = 0;
  let style: TextStyle | undefined;
  let hasSegment = false;
  for (const cluster of clusters) {
    if (hasSegment && !stylesEqual(style, cluster.style)) {
      segments.push({ text, width: segmentWidth, style });
      text = '';
      segmentWidth = 0;
    }
    hasSegment = true;
    style = cluster.style;
    text += cluster.text;
    segmentWidth += cluster.width;
    width += cluster.width;
  }
  if (hasSegment) {
    segments.push({ text, width: segmentWidth, style });
  }
  return { segments, width };
}

function splitParagraphs(runs: ReadonlyArray<TextRun>): StyledCluster[][] {
  const paragraphs: StyledCluster[][] = [[]];
  for (const run of runs) {
    const normalized = run.text.replace(/\r\n?/g, '\n').replace(/\t/g, ' ');
    const parts = normalized.split('\n');
    for (let i = 0; i < parts.length; i++) {
      if (i > 0) {
        paragraphs.push([]);
      }
      const target = paragraphs[paragraphs.length - 1]!;
      for (const cluster of clusterize(parts[i]!)) {
        target.push({ text: cluster.text, width: cluster.width, style: run.style });
      }
    }
  }
  return paragraphs;
}

function wrapParagraph(clusters: ReadonlyArray<StyledCluster>, maxWidth: number): StyledCluster[][] {
  const lines: StyledCluster[][] = [];
  let line: StyledCluster[] = [];
  let lineWidth = 0;

  const pushLine = () => {
    // Whitespace before a wrap point is invisible; dropping it keeps measured widths tight.
    while (line.length > 0 && isWhitespace(line[line.length - 1]!)) {
      line.pop();
    }
    lines.push(line);
    line = [];
    lineWidth = 0;
  };

  let index = 0;
  while (index < clusters.length) {
    // A token is a run of whitespace or a run of non-whitespace.
    const whitespace = isWhitespace(clusters[index]!);
    let end = index;
    let tokenWidth = 0;
    while (end < clusters.length && isWhitespace(clusters[end]!) === whitespace) {
      tokenWidth += clusters[end]!.width;
      end++;
    }
    const token = clusters.slice(index, end);
    index = end;

    if (whitespace) {
      // Trailing whitespace that does not fit is dropped rather than wrapped; it would be invisible.
      for (const cluster of token) {
        if (lineWidth + cluster.width > maxWidth) {
          break;
        }
        line.push(cluster);
        lineWidth += cluster.width;
      }
      continue;
    }

    if (lineWidth + tokenWidth <= maxWidth) {
      line.push(...token);
      lineWidth += tokenWidth;
      continue;
    }

    if (line.length > 0) {
      pushLine();
    }
    if (tokenWidth <= maxWidth) {
      line.push(...token);
      lineWidth = tokenWidth;
      continue;
    }
    // Hard-break a word wider than the line.
    for (const cluster of token) {
      if (lineWidth + cluster.width > maxWidth && line.length > 0) {
        pushLine();
      }
      line.push(cluster);
      lineWidth += cluster.width;
    }
  }
  lines.push(line);
  return lines;
}

function takeFromStart(clusters: ReadonlyArray<StyledCluster>, maxWidth: number): StyledCluster[] {
  const out: StyledCluster[] = [];
  let width = 0;
  for (const cluster of clusters) {
    if (width + cluster.width > maxWidth) {
      break;
    }
    out.push(cluster);
    width += cluster.width;
  }
  return out;
}

function takeFromEnd(clusters: ReadonlyArray<StyledCluster>, maxWidth: number): StyledCluster[] {
  const out: StyledCluster[] = [];
  let width = 0;
  for (let i = clusters.length - 1; i >= 0; i--) {
    const cluster = clusters[i]!;
    if (width + cluster.width > maxWidth) {
      break;
    }
    out.unshift(cluster);
    width += cluster.width;
  }
  return out;
}

function truncateParagraph(clusters: ReadonlyArray<StyledCluster>, maxWidth: number, mode: WrapMode): StyledCluster[] {
  let width = 0;
  for (const cluster of clusters) {
    width += cluster.width;
  }
  if (width <= maxWidth) {
    return clusters.slice();
  }
  if (maxWidth <= 0) {
    return [];
  }
  if (maxWidth === 1) {
    return [ELLIPSIS];
  }
  const available = maxWidth - ELLIPSIS.width;
  switch (mode) {
    case 'start':
    case 'truncate-start':
      return [ELLIPSIS, ...takeFromEnd(clusters, available)];
    case 'middle':
    case 'truncate-middle': {
      const headWidth = Math.ceil(available / 2);
      const head = takeFromStart(clusters, headWidth);
      let used = 0;
      for (const cluster of head) {
        used += cluster.width;
      }
      return [...head, ELLIPSIS, ...takeFromEnd(clusters, available - used)];
    }
    default:
      return [...takeFromStart(clusters, available), ELLIPSIS];
  }
}

/**
 * Lays out styled runs into lines. `maxWidth` undefined means unconstrained (no wrapping).
 * Hard line breaks in the text are always honored.
 */
export function layoutText(
  runs: ReadonlyArray<TextRun>,
  maxWidth: number | undefined,
  mode: WrapMode = 'wrap',
): StyledLine[] {
  const lines: StyledLine[] = [];
  const width = maxWidth === undefined ? undefined : Math.max(0, Math.floor(maxWidth));
  for (const paragraph of splitParagraphs(runs)) {
    if (width === undefined) {
      lines.push(toSegments(paragraph));
    } else if (mode === 'wrap') {
      if (width === 0) {
        lines.push(toSegments([]));
        continue;
      }
      for (const line of wrapParagraph(paragraph, width)) {
        lines.push(toSegments(line));
      }
    } else {
      lines.push(toSegments(truncateParagraph(paragraph, width, mode)));
    }
  }
  return lines;
}

/** Size in columns and rows of laid-out lines. */
export function measureLines(lines: ReadonlyArray<StyledLine>): [number, number] {
  let width = 0;
  for (const line of lines) {
    width = Math.max(width, line.width);
  }
  return [width, lines.length];
}

export function linesToPlainText(lines: ReadonlyArray<StyledLine>): string[] {
  return lines.map(line => line.segments.map(segment => segment.text).join(''));
}
