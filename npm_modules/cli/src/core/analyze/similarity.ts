import type { ComponentFingerprint } from './fingerprint';

const VM_WEIGHT = 0.4;
const RENDER_WEIGHT = 0.35;
const IMPORT_WEIGHT = 0.25;

export function computeSimilarity(a: ComponentFingerprint, b: ComponentFingerprint): number {
  const vmSim = jaccardSimilarity(a.viewModelProps, b.viewModelProps);
  const renderSim = lcsRatio(a.renderElements, b.renderElements);
  const importSim = jaccardSimilarity(a.importPaths, b.importPaths);

  return VM_WEIGHT * vmSim + RENDER_WEIGHT * renderSim + IMPORT_WEIGHT * importSim;
}

export function jaccardSimilarity(a: string[], b: string[]): number {
  if (a.length === 0 && b.length === 0) return 1;
  if (a.length === 0 || b.length === 0) return 0;

  const setA = new Set(a);
  const setB = new Set(b);
  let intersection = 0;

  for (const item of setA) {
    if (setB.has(item)) intersection++;
  }

  const union = setA.size + setB.size - intersection;
  return union === 0 ? 0 : intersection / union;
}

export function lcsRatio(a: string[], b: string[]): number {
  if (a.length === 0 && b.length === 0) return 1;
  if (a.length === 0 || b.length === 0) return 0;

  const lcsLen = lcsLength(a, b);
  return (2 * lcsLen) / (a.length + b.length);
}

function lcsLength(a: string[], b: string[]): number {
  const m = a.length;
  const n = b.length;
  let prev = new Array<number>(n + 1).fill(0);
  let curr = new Array<number>(n + 1).fill(0);

  for (let i = 1; i <= m; i++) {
    for (let j = 1; j <= n; j++) {
      if (a[i - 1] === b[j - 1]) {
        curr[j] = prev[j - 1]! + 1;
      } else {
        curr[j] = Math.max(prev[j]!, curr[j - 1]!);
      }
    }
    [prev, curr] = [curr, prev];
    curr.fill(0);
  }

  return prev[n]!;
}
