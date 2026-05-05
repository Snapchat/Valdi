import type { Argv } from 'yargs';
import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, resolveClientId, resolveContextId, DEFAULT_PORT } from '../../utils/daemonClient';
import { ANSI_COLORS } from '../../core/constants';
import { wrapInColor } from '../../utils/logUtils';

interface CommandParameters {
  elementId: string;
  contextId: string | undefined;
  port: number;
  client: string | undefined;
  output: string | undefined;
  key: string | undefined;
}

export function firstElementId(node: unknown): string | null {
  if (node === null || typeof node !== 'object') return null;
  const obj = node as Record<string, unknown>;
  if (obj['element'] && typeof obj['element'] === 'object') {
    const elem = obj['element'] as Record<string, unknown>;
    if (elem['id'] !== undefined) return String(elem['id']);
  }
  const children = obj['children'];
  if (Array.isArray(children)) {
    for (const child of children) {
      const found = firstElementId(child);
      if (found) return found;
    }
  }
  return null;
}

export function findElementByKey(node: unknown, targetKey: string): string | null {
  if (node === null || typeof node !== 'object') return null;
  const obj = node as Record<string, unknown>;
  // Match by explicit key on an element node
  if (obj['key'] === targetKey && obj['element'] && typeof obj['element'] === 'object') {
    const elem = obj['element'] as Record<string, unknown>;
    if (elem['id'] !== undefined) return String(elem['id']);
  }
  // Match by tag on an element node
  if (obj['tag'] === targetKey && obj['element'] && typeof obj['element'] === 'object') {
    const elem = obj['element'] as Record<string, unknown>;
    if (elem['id'] !== undefined) return String(elem['id']);
  }
  // Match by tag on a component node (no element.id directly) — use first child element
  if (obj['tag'] === targetKey && obj['component'] !== undefined) {
    return firstElementId(obj);
  }
  const children = obj['children'];
  if (Array.isArray(children)) {
    for (const child of children) {
      const found = findElementByKey(child, targetKey);
      if (found) return found;
    }
  }
  return null;
}

async function inspectSnapshot(argv: ArgumentsResolver<CommandParameters>) {
  const elementIdArg = argv.getArgument('elementId') as string;
  const contextIdArg = argv.getArgument('contextId') as string | undefined;
  const port = argv.getArgument('port') as number;
  const clientOverride = argv.getArgument('client') as string | undefined;
  const outputOverride = argv.getArgument('output') as string | undefined;
  const keyOverride = argv.getArgument('key') as string | undefined;

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clientId = await resolveClientId(conn, clientOverride);
    const contextId = await resolveContextId(conn, clientId, contextIdArg);

    let elementId = elementIdArg;
    if (keyOverride) {
      const tree = await conn.getContextTree(clientId, contextId);
      const found = findElementByKey(tree, keyOverride);
      if (!found) {
        throw new Error(`No element found with key "${keyOverride}". Use "valdi inspect tree" to see available keys.`);
      }
      elementId = found;
      console.error(wrapInColor(`Resolved key "${keyOverride}" to element ID ${elementId}`, ANSI_COLORS.GREEN_COLOR));
    }

    const base64Data = await conn.takeSnapshot(clientId, elementId, contextId);

    const snapshotName = keyOverride || elementId;
    const outPath = outputOverride
      ? path.resolve(outputOverride)
      : path.join(os.tmpdir(), `valdi-snapshot-${snapshotName}.png`);

    fs.writeFileSync(outPath, Buffer.from(base64Data, 'base64'));
    console.log(JSON.stringify({ path: outPath }));
    console.error(wrapInColor(`Screenshot saved: ${outPath}`, ANSI_COLORS.GREEN_COLOR));
  } finally {
    conn.close();
  }
}

export const command = 'snapshot [elementId] [contextId]';
export const describe = 'Capture a screenshot of an element by ID or --key';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .positional('elementId', {
      describe: 'Element ID to screenshot (not needed when using --key)',
      type: 'string',
      default: '0',
    })
    .positional('contextId', {
      describe: 'Context ID containing the element (omit to auto-select or be prompted)',
      type: 'string',
    })
    .option('key', {
      describe: 'Find element by key or tag name in the tree (stable across hot-reloads)',
      type: 'string',
      alias: 'k',
    })
    .option('port', {
      describe: 'Daemon TCP port',
      type: 'number',
      default: DEFAULT_PORT,
    })
    .option('client', {
      describe: 'Client ID to target (from "valdi inspect devices")',
      type: 'string',
    })
    .option('output', {
      describe: 'Output PNG file path (defaults to /tmp/valdi-snapshot-<elementId>.png)',
      type: 'string',
      alias: 'o',
    });
};
export const handler = makeCommandHandler(inspectSnapshot);
