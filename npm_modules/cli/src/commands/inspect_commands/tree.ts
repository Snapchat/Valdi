import type { Argv } from 'yargs';
import { makeCommandHandler } from '../../utils/errorUtils';
import type { ArgumentsResolver } from '../../utils/ArgumentsResolver';
import { connectToDaemon, resolveClientId, resolveContextId, DEFAULT_PORT } from '../../utils/daemonClient';

interface CommandParameters {
  contextId: string | undefined;
  port: number;
  client: string | undefined;
  pretty: boolean;
  maxDepth: number | undefined;
}

function trimDepth(node: unknown, depth: number): unknown {
  if (depth <= 0 || node === null || typeof node !== 'object') return node;
  const obj = node as Record<string, unknown>;
  const result: Record<string, unknown> = {};
  for (const key of Object.keys(obj)) {
    if (key === 'children') {
      const children = obj['children'];
      if (depth === 1) {
        // Replace children with a count sentinel so callers know the tree was trimmed
        const count = Array.isArray(children) ? children.length : 0;
        if (count > 0) result['_childrenTrimmed'] = count;
      } else if (Array.isArray(children)) {
        result['children'] = children.map((c) => trimDepth(c, depth - 1));
      } else {
        result['children'] = children;
      }
    } else {
      result[key] = obj[key];
    }
  }
  return result;
}

async function inspectTree(argv: ArgumentsResolver<CommandParameters>) {
  const contextIdArg = argv.getArgument('contextId') as string | undefined;
  const port = argv.getArgument('port') as number;
  const clientOverride = argv.getArgument('client') as string | undefined;
  const pretty = argv.getArgument('pretty') as boolean;
  const maxDepth = argv.getArgument('maxDepth') as number | undefined;

  const conn = await connectToDaemon(port);
  try {
    await conn.configure();
    const clientId = await resolveClientId(conn, clientOverride);
    const contextId = await resolveContextId(conn, clientId, contextIdArg);
    let tree = await conn.getContextTree(clientId, contextId);

    if (maxDepth !== undefined) {
      tree = trimDepth(tree, maxDepth);
    }

    if (pretty) {
      console.log(JSON.stringify(tree, null, 2));
    } else {
      console.log(JSON.stringify(tree));
    }
  } finally {
    conn.close();
  }
}

export const command = 'tree [contextId]';
export const describe = 'Get the full rendered virtual node tree for a context';
export const builder = (yargs: Argv<CommandParameters>) => {
  yargs
    .positional('contextId', {
      describe: 'Context ID (omit to auto-select or be prompted)',
      type: 'string',
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
    .option('pretty', {
      describe: 'Pretty-print JSON output',
      type: 'boolean',
      default: false,
    })
    .option('max-depth', {
      describe: 'Trim the tree to this many levels deep (children beyond the limit are replaced with _childrenTrimmed count)',
      type: 'number',
    });
};
export const handler = makeCommandHandler(inspectTree);
