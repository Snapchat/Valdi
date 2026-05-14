import fs from 'fs';

export interface ParsedProp {
  name: string;
  typeString: string;
  optional: boolean;
  isBoolean: boolean;
  isCallback: boolean;
  /** For callbacks like (val: boolean) => void, the parameter type */
  callbackParamType: string | undefined;
}

export interface ParsedViewModel {
  componentName: string;
  viewModelName: string;
  props: ParsedProp[];
  /** Extra imports needed (e.g. enums referenced in the ViewModel) */
  extraImports: string[];
}

/**
 * Parse a Valdi .tsx component file and extract its ViewModel interface.
 * Uses regex — no TypeScript compiler dependency.
 */
export function parseComponentFile(filePath: string): ParsedViewModel {
  const source = fs.readFileSync(filePath, 'utf8');

  // 1. Find the exported component class and its ViewModel type parameter
  const classMatch = source.match(
    /export\s+class\s+(\w+)\s+extends\s+(?:Stateful)?Component<(\w+)/,
  );
  if (!classMatch) {
    throw new Error(`Could not find an exported Component class in ${filePath}`);
  }
  const componentName = classMatch[1]!;
  const viewModelName = classMatch[2]!;

  // 2. Find the ViewModel interface body
  // Match `interface ViewModelName {` through the closing `}`
  const ifaceRegex = new RegExp(
    `(?:export\\s+)?interface\\s+${escapeRegex(viewModelName)}\\s*\\{([^}]*)\\}`,
    's',
  );
  const ifaceMatch = source.match(ifaceRegex);
  if (!ifaceMatch) {
    throw new Error(`Could not find interface ${viewModelName} in ${filePath}`);
  }
  const body = ifaceMatch[1]!;

  // 3. Parse each property line
  const props: ParsedProp[] = [];
  // Match lines like:  propName?: TypeExpression;
  const propRegex = /^\s*(\w+)(\??):\s*(.+?)\s*;/gm;
  let m: RegExpExecArray | null;
  while ((m = propRegex.exec(body)) !== null) {
    const name = m[1]!;
    const optional = m[2] === '?';
    const typeString = m[3]!.trim();

    const isBoolean = typeString === 'boolean';
    const isCallback = /=>/.test(typeString);
    let callbackParamType: string | undefined;

    if (isCallback) {
      // Extract param type from patterns like (val: boolean) => void
      const cbMatch = typeString.match(/\(\s*\w+\s*:\s*(\w+)\s*\)\s*=>/);
      if (cbMatch) {
        callbackParamType = cbMatch[1];
      }
    }

    props.push({ name: name, typeString: typeString, optional, isBoolean, isCallback, callbackParamType });
  }

  return { componentName: componentName, viewModelName: viewModelName, props, extraImports: [] };
}

/**
 * Detect interactive boolean pairs: a boolean prop + a callback that takes boolean.
 * Returns state entries to wire them together in the preview.
 */
export interface InteractiveState {
  stateName: string;
  boolPropName: string;
  callbackPropName: string;
}

export function detectInteractiveState(props: ParsedProp[]): InteractiveState[] {
  const boolProps = props.filter(p => p.isBoolean && !p.optional);
  const callbackProps = props.filter(p => p.isCallback && p.callbackParamType === 'boolean');

  const pairs: InteractiveState[] = [];

  for (const bp of boolProps) {
    // Look for a callback that takes boolean — common patterns:
    // on + onTap, selected + onSelect, checked + onChecked, foo + onFoo
    const candidate = callbackProps.find(cp => {
      const cpLower = cp.name.toLowerCase();
      const bpLower = bp.name.toLowerCase();
      // onTap / onPress / onChange are generic callbacks often paired with the boolean
      if (['ontap', 'onpress', 'onchange', 'ontoggle'].includes(cpLower)) return true;
      // onFoo paired with foo
      if (cpLower === `on${bpLower}`) return true;
      return false;
    });

    if (candidate) {
      pairs.push({
        stateName: bp.name,
        boolPropName: bp.name,
        callbackPropName: candidate.name,
      });
    }
  }

  return pairs;
}

/**
 * Generate a sample value expression for a prop type.
 */
export function sampleValueForType(typeString: string): string {
  const t = typeString.trim();

  if (t === 'boolean') return 'false';
  if (t === 'string') return "'Sample'";
  if (t === 'number') return '42';
  if (t === 'Date') return 'new Date()';

  // Function types
  if (/^\(\s*\)\s*=>/.test(t)) return '() => {}';
  if (/=>/.test(t)) return '() => {}';

  // Array
  if (t.endsWith('[]') || t.startsWith('Array<')) return '[]';

  // Fallback
  return 'undefined as any';
}

function escapeRegex(str: string): string {
  return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
