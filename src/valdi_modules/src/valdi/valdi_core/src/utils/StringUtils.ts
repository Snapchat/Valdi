import { lazyImport } from '../LazyImport';

const Symbolicator = lazyImport<typeof import('../Symbolicator')>(require, '../Symbolicator');

export function toCamelCase(snakeCaseString: string): string {
  const components = snakeCaseString.split('_');
  let out = '';

  for (const component of components) {
    if (!component) {
      continue;
    }

    if (!out) {
      out = component;
    } else {
      if (component.length === 1) {
        out += component.toUpperCase();
      } else {
        out += component[0].toUpperCase() + component.substr(1);
      }
    }
  }

  return out;
}

export function capitalize(str: string): string {
  if (str.length == 0) {
    return str;
  }
  return str.charAt(0).toUpperCase() + str.slice(1);
}

export function debugStringify(object: any, maxDepth: number, pretty: boolean): string {
  if (object === undefined) {
    return 'undefined';
  }
  if (typeof object === 'string') {
    return object;
  }
  const alreadyVisited = new Set();
  return stringifyInnerSafely(object, alreadyVisited, 0, maxDepth, pretty);
}

function stringifyIndent(depth: number) {
  let indent = '';
  for (let i = 0; i < depth; i++) {
    indent += '  ';
  }
  return '\n' + indent;
}

function stringifyInnerSafely(
  object: any,
  alreadyVisited: Set<any>,
  currentDepth: number,
  maxDepth: number,
  pretty: boolean,
) {
  try {
    return stringifyInner(object, alreadyVisited, currentDepth, maxDepth, pretty);
  } catch (e: any) {
    if (e) {
      return `<failure ${e.message}/>`;
    } else {
      return `<failure unknown/>`;
    }
  }
}

function stringifyInner(
  object: any,
  alreadyVisited: Set<any>,
  currentDepth: number,
  maxDepth: number,
  pretty: boolean,
): string {
  if (currentDepth >= maxDepth) {
    return '...';
  }

  if (object === undefined) {
    return 'undefined';
  }
  if (object === null) {
    return 'null';
  }
  if (object === true) {
    return 'true';
  }
  if (object === false) {
    return 'false';
  }

  const type = typeof object;
  if (type === 'string') {
    return `\"${object}\"`;
  }
  if (type === 'number' && typeof object.toString === 'function') {
    return object.toString();
  }
  if (type === 'function') {
    if ((object as any).componentPath) {
      return `<function ${object.name} -> ${object.componentPath}/>`;
    } else {
      return `<function ${object.name}/>`;
    }
  }
  if (object instanceof Date && typeof object.toISOString === 'function') {
    return `<date ${object.toISOString()}/>`;
  }
  if (object instanceof Error) {
    const symbolicatedError = Symbolicator.get.symbolicate(object);
    let errorString = `${symbolicatedError.name}:${symbolicatedError.message}`;

    if (symbolicatedError.stack) {
      errorString += '\n';
      errorString += symbolicatedError.stack;
    }
    return errorString;
  }

  if (alreadyVisited.has(object)) {
    return `<circular ${type}/>`;
  }
  alreadyVisited.add(object);

  const indentCurrent = pretty ? stringifyIndent(currentDepth) : '';
  const indentNext = pretty ? stringifyIndent(currentDepth + 1) : '';
  const ellipsisAfter = 50;

  if (Array.isArray(object)) {
    if (object.length <= 0) {
      return '[]';
    }
    const parts = [];
    for (const item of object) {
      parts.push(stringifyInnerSafely(item, alreadyVisited, currentDepth + 1, maxDepth, pretty));
      if (parts.length >= ellipsisAfter) {
        parts.push(`... ${object.length - ellipsisAfter} more item(s) ...`);
        break;
      }
    }
    return `[${indentNext}${parts.join(',' + indentNext)}${indentCurrent}]`;
  }

  if (object instanceof Map) {
    if (object.size <= 0) {
      return 'Map{}';
    }
    const parts = [];
    for (const key of Array.from(object.keys())) {
      const value = object.get(key);
      const str = stringifyInnerSafely(value, alreadyVisited, currentDepth + 1, maxDepth, pretty);
      parts.push(`${key}: ${str}`);
      if (parts.length >= ellipsisAfter) {
        parts.push(`... ${object.size - ellipsisAfter} more item(s) ...`);
        break;
      }
    }
    return `Map{${indentNext}${parts.join(',' + indentNext)}${indentCurrent}}`;
  }

  if (object instanceof Set) {
    if (object.size <= 0) {
      return 'Set()';
    }
    const parts = [];
    for (const item of Array.from(object)) {
      parts.push(stringifyInnerSafely(item, alreadyVisited, currentDepth + 1, maxDepth, pretty));
      if (parts.length >= ellipsisAfter) {
        parts.push(`... ${object.size - ellipsisAfter} more item(s) ...`);
        break;
      }
    }
    return `Set(${indentNext}${parts.join(',' + indentNext)}${indentCurrent})`;
  }

  if (typeof object.toConsoleRepresentation === 'function') {
    const naming = object.constructor?.name ?? '';
    const debug = stringifyInnerSafely(
      object.toConsoleRepresentation(),
      alreadyVisited,
      currentDepth,
      maxDepth,
      pretty,
    );
    return `<${naming} ...${debug}/>`;
  }

  if (object.toString !== Object.prototype.toString && typeof object.toString === 'function') {
    return object.toString();
  }

  if (type === 'object') {
    let prefix = '';
    let postfix = '';
    const name = object.constructor?.name;
    if (name !== undefined && name !== 'Object') {
      prefix = '<' + name + ' ...';
      postfix = '/>';
    }
    const keys = Object.keys(object);
    if (keys.length <= 0) {
      return `${prefix}{}${postfix}`;
    }
    const parts = [];
    for (const key of keys) {
      const value = object[key];
      const str = stringifyInnerSafely(value, alreadyVisited, currentDepth + 1, maxDepth, pretty);
      parts.push(`${key}: ${str}`);
      if (parts.length >= ellipsisAfter) {
        parts.push(`... ${keys.length - ellipsisAfter} more item(s) ...`);
        break;
      }
    }
    return `${prefix}{${indentNext}${parts.join(',' + indentNext)}${indentCurrent}}${postfix}`;
  }

  if (typeof object.toString === 'function') {
    return object.toString();
  }

  return `<unknown ${type}>`;
}

export function titleCase(str: string): string {
  return str
    .toLocaleLowerCase()
    .split(' ')
    .map(subString => {
      return `${subString.charAt(0).toLocaleUpperCase()}${subString.slice(1)}`;
    })
    .join(' ');
}
