import type { Argv } from 'yargs';
import { DebuggerInputType, sendDebuggerInput, validateDebuggerInputRequest } from '../../debugger/inputClient';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { DEFAULT_PORT, connectToDaemon, resolveClientId, resolveContextId } from '../../utils/daemonClient';
import { makeCommandHandler } from '../../utils/errorUtils';

interface CommandParameters {
  action: DebuggerInputType;
  contextId: string | undefined;
  port: number;
  client: string | undefined;
  elementId: number | undefined;
  accessibilityId: string | undefined;
  selector: string | undefined;
  focused: boolean;
  text: string | undefined;
  key: string | undefined;
  selectionStart: number | undefined;
  selectionEnd: number | undefined;
  x: number | undefined;
  y: number | undefined;
  deltaX: number | undefined;
  deltaY: number | undefined;
}

interface InputSelector {
  elementId?: number | undefined;
  accessibilityId?: string | undefined;
  tag?: string | undefined;
}

interface InputCommandArguments {
  action: DebuggerInputType;
  elementId?: number | undefined;
  accessibilityId?: string | undefined;
  selector?: string | undefined;
  focused: boolean;
  text?: string | undefined;
  key?: string | undefined;
  selectionStart?: number | undefined;
  selectionEnd?: number | undefined;
  x?: number | undefined;
  y?: number | undefined;
  deltaX?: number | undefined;
  deltaY?: number | undefined;
}

const INPUT_SELECTOR_FIELDS: ReadonlySet<string> = new Set(['elementId', 'accessibilityId', 'tag']);

function parseSelector(selector: string): string | InputSelector {
  const trimmed = selector.trim();
  if (!trimmed.startsWith('{')) {
    return selector;
  }

  let parsed: unknown;
  try {
    parsed = JSON.parse(trimmed);
  } catch {
    throw new Error('--selector must be a selector string or a JSON object.');
  }
  if (parsed === null || Array.isArray(parsed) || typeof parsed !== 'object') {
    throw new Error('--selector JSON must be an object.');
  }

  const record = parsed as Record<string, unknown>;
  const unknownKey = Object.keys(record)
    .sort()
    .find(key => !INPUT_SELECTOR_FIELDS.has(key));
  if (unknownKey) {
    throw new Error(`Unsupported selector field "${unknownKey}".`);
  }
  return record as InputSelector;
}

function addDefined(request: Record<string, unknown>, key: string, value: unknown): void {
  if (value !== undefined) {
    request[key] = value;
  }
}

export function buildInputRequest(args: InputCommandArguments): Record<string, unknown> {
  const selectorCount = [args.elementId, args.accessibilityId, args.selector].filter(
    value => value !== undefined,
  ).length;
  if (selectorCount > 1) {
    throw new Error('Use only one of --element-id, --accessibility-id, or --selector.');
  }
  if (args.action === DebuggerInputType.Text && args.text === undefined) {
    throw new Error('The text action requires --text.');
  }
  if (args.action === DebuggerInputType.Key && args.key === undefined) {
    throw new Error('The key action requires --key.');
  }

  const request: Record<string, unknown> = { type: args.action };
  addDefined(request, 'elementId', args.elementId);
  addDefined(request, 'accessibilityId', args.accessibilityId);
  addDefined(request, 'selector', args.selector === undefined ? undefined : parseSelector(args.selector));
  if (args.action === DebuggerInputType.Focus) {
    request['focused'] = args.focused;
  }
  addDefined(request, 'text', args.text);
  addDefined(request, 'key', args.key);
  addDefined(request, 'selectionStart', args.selectionStart);
  addDefined(request, 'selectionEnd', args.selectionEnd);
  addDefined(request, 'x', args.x);
  addDefined(request, 'y', args.y);
  addDefined(request, 'deltaX', args.deltaX);
  addDefined(request, 'deltaY', args.deltaY);
  const validationError = validateDebuggerInputRequest(request);
  if (validationError) {
    throw new Error(validationError);
  }
  return request;
}

async function inspectInput(argv: ArgumentsResolver<CommandParameters>): Promise<void> {
  const action = argv.getArgument('action');
  const contextIdArg = argv.getArgument('contextId');
  const port = argv.getArgument('port');
  const clientOverride = argv.getArgument('client');
  const request = buildInputRequest({
    action,
    elementId: argv.getArgument('elementId'),
    accessibilityId: argv.getArgument('accessibilityId'),
    selector: argv.getArgument('selector'),
    focused: argv.getArgument('focused'),
    text: argv.getArgument('text'),
    key: argv.getArgument('key'),
    selectionStart: argv.getArgument('selectionStart'),
    selectionEnd: argv.getArgument('selectionEnd'),
    x: argv.getArgument('x'),
    y: argv.getArgument('y'),
    deltaX: argv.getArgument('deltaX'),
    deltaY: argv.getArgument('deltaY'),
  });

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clientId = await resolveClientId(conn, clientOverride);
    const contextId =
      action === DebuggerInputType.Capabilities ? undefined : await resolveContextId(conn, clientId, contextIdArg);
    const requestWithContext = contextId === undefined ? request : { ...request, contextId };
    const input = await sendDebuggerInput(conn, clientId, requestWithContext);
    console.log(JSON.stringify({ port, clientId, contextId, input }));
  } finally {
    conn.close();
  }
}

export const command = 'input <action> [contextId]';
export const describe = 'Query and control a live Valdi app through the default debugger input contract';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .positional('action', {
      describe: 'Input operation',
      choices: Object.values(DebuggerInputType),
      type: 'string',
    })
    .positional('contextId', {
      describe: 'Context ID (omit to auto-select or be prompted)',
      type: 'string',
    })
    .option('port', {
      describe: 'Daemon TCP port (use 13591 for standalone macOS apps)',
      type: 'number',
      default: DEFAULT_PORT,
    })
    .option('client', {
      describe: 'Client ID to target (from "valdi inspect devices")',
      type: 'string',
    })
    .option('element-id', {
      describe: 'Target a rendered element ID',
      type: 'number',
    })
    .option('accessibility-id', {
      describe: 'Target a stable accessibility ID',
      type: 'string',
    })
    .option('selector', {
      describe: 'Target #accessibilityId, [accessibilityId="..."], or a JSON selector object',
      type: 'string',
    })
    .option('focused', {
      describe: 'Focus state for the focus action; use --no-focused to blur',
      type: 'boolean',
      default: true,
    })
    .option('text', {
      describe: 'Replacement text for the text action',
      type: 'string',
    })
    .option('key', {
      describe: 'Key for the key action (Enter, Return, Escape, Backspace, Delete, or one printable grapheme)',
      type: 'string',
    })
    .option('selection-start', {
      describe: 'Selection start for text and key actions',
      type: 'number',
    })
    .option('selection-end', {
      describe: 'Selection end for text and key actions',
      type: 'number',
    })
    .option('x', {
      describe: 'Absolute X coordinate for tap',
      type: 'number',
    })
    .option('y', {
      describe: 'Absolute Y coordinate for tap',
      type: 'number',
    })
    .option('delta-x', {
      describe: 'Horizontal content-offset delta for scroll',
      type: 'number',
    })
    .option('delta-y', {
      describe: 'Vertical content-offset delta for scroll',
      type: 'number',
    });
};
export const handler = makeCommandHandler(inspectInput);
