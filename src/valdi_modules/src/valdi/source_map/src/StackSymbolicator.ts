import { ISourceMap } from './ISourceMap';
import { SourceMap, LineOffsetSourceMap } from './SourceMap';

export type GetOrCreateSourceMapFunc = (
  path: string,
  sourceMapFactory: (
    sourceMapContent: string | undefined,
    sourceMapLineOffset: number | undefined,
  ) => ISourceMap | undefined,
) => ISourceMap | undefined;

export interface StackFrame {
  name?: string;
  fileName: string;
  column?: number;
  line?: number;
}

function parseJSCoreStackFrame(line: string): StackFrame {
  let name: string | undefined;
  let fileAndPosition: string;

  const components = line.split('@');

  if (components.length === 2) {
    name = components[0];
    fileAndPosition = components[1];
  } else {
    fileAndPosition = components[0];
  }

  const fileAndPositionSplit = fileAndPosition.split(':');
  const fileName = fileAndPositionSplit[0].replace(' ', '_');

  let columNumber: number | undefined;
  let lineNumber: number | undefined;

  if (fileAndPositionSplit.length > 1) {
    lineNumber = Number.parseInt(fileAndPositionSplit[1]);
  }
  if (fileAndPositionSplit.length > 2) {
    columNumber = Number.parseInt(fileAndPositionSplit[2]);
  }

  return {
    name,
    fileName,
    line: lineNumber,
    column: columNumber,
  };
}

function parseQuickJSStackFrame(line: string): StackFrame {
  const components = line.split(' ');
  if (components.length !== 3) {
    throw new Error('Invalid stack frame');
  }

  const name = components[1];
  let file = components[2];
  // Remove opening and trailing paren
  file = file.substr(1, file.length - 2);

  const fileAndLine = file.split(':');
  const fileName = fileAndLine[0];

  let lineNumber: number | undefined;
  if (fileAndLine.length > 1) {
    lineNumber = Number.parseInt(fileAndLine[1]);
  }

  let columnNumber: number | undefined;
  if (fileAndLine.length > 2) {
    columnNumber = Number.parseInt(fileAndLine[2]);
  }

  return {
    name,
    fileName,
    line: lineNumber,
    column: columnNumber,
  };
}

export function parseStackFrames(stack: string): StackFrame[] {
  const frames: StackFrame[] = [];

  const lines = stack.split('\n');

  for (let line of lines) {
    line = line.trim();
    if (!line) {
      continue;
    }

    try {
      if (line.startsWith('at ')) {
        frames.push(parseQuickJSStackFrame(line));
      } else {
        frames.push(parseJSCoreStackFrame(line));
      }
    } catch (err: any) {
      frames.push({
        name: line,
        fileName: `failed to symbolicate: ${err.message}`,
      });
    }
  }

  return frames;
}

function createSourceMap(
  sourceMapContent: string | undefined,
  sourceMapLineOffset: number | undefined,
): ISourceMap | undefined {
  if (!sourceMapContent) {
    return sourceMapLineOffset !== undefined ? new LineOffsetSourceMap(sourceMapLineOffset) : undefined;
  }

  const sourceMap = SourceMap.parse(sourceMapContent);

  if (sourceMapLineOffset) {
    sourceMap.lineOffset += sourceMapLineOffset;
  }

  return sourceMap;
}

function symbolicateStackFrame(stackFrame: StackFrame, getOrCreateSourceMapFunc: GetOrCreateSourceMapFunc): StackFrame {
  if (
    !stackFrame.line ||
    stackFrame.fileName.startsWith('<') ||
    stackFrame.fileName.endsWith('.ts') ||
    stackFrame.fileName.endsWith('.tsx')
  ) {
    return stackFrame;
  }

  try {
    const sourceMap = getOrCreateSourceMapFunc(stackFrame.fileName, createSourceMap);
    if (!sourceMap) {
      return stackFrame;
    }

    const resolved = sourceMap.resolve(stackFrame.line, stackFrame.column, stackFrame.name);
    if (!resolved) {
      return stackFrame;
    }

    const fileName = resolved.fileName ?? stackFrame.fileName;
    const name = resolved.name ?? stackFrame.name;

    return { fileName, name, line: resolved.line, column: resolved.column };
  } catch (err: any) {
    console.error(`Failed to symbolicate stack frame ${stackFrame.fileName}: ${err.message}`);
    return stackFrame;
  }
}

export function symbolicateStackFrames(
  stackFrames: StackFrame[],
  getOrCreateSourceMapFunc: GetOrCreateSourceMapFunc,
): StackFrame[] {
  const out: StackFrame[] = [];

  for (const stackFrame of stackFrames) {
    out.push(symbolicateStackFrame(stackFrame, getOrCreateSourceMapFunc));
  }

  return out;
}

export function symbolicateStack(stack: string, getOrCreateSourceMapFunc: GetOrCreateSourceMapFunc): string {
  try {
    let stackFrames = parseStackFrames(stack);
    stackFrames = symbolicateStackFrames(stackFrames, getOrCreateSourceMapFunc);

    let newStack = '';
    for (const stackFrame of stackFrames) {
      let name = stackFrame.name;
      if (!name) {
        name = '<anonymous>';
      }

      newStack += `    at ${name} (${stackFrame.fileName}`;
      if (stackFrame.line) {
        newStack += ':' + stackFrame.line.toString();
        if (stackFrame.column) {
          newStack += ':' + stackFrame.column.toString();
        }
      }

      newStack += ')\n';
    }

    return newStack;
  } catch (err: any) {
    console.error('Failed to process stack frames:', err.message, err.stack);
    return stack;
  }
}
