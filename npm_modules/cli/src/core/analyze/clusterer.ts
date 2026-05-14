import type { ComponentFingerprint, GenericCluster, ScanConfig } from './fingerprint';
import { isSharedLibFile } from './scanner';
import { computeSimilarity } from './similarity';

export function clusterFingerprints(
  fingerprints: ComponentFingerprint[],
  config: ScanConfig,
  threshold: number,
): GenericCluster[] {
  const nonSharedFingerprints = fingerprints.filter(fp => !isSharedLibFile(fp.filePath, config));

  const buckets = bucketBySharedImports(nonSharedFingerprints);

  const allPairs: Array<{ a: ComponentFingerprint; b: ComponentFingerprint; similarity: number }> = [];

  for (const bucket of buckets.values()) {
    if (bucket.length < 2) continue;
    for (let i = 0; i < bucket.length; i++) {
      for (let j = i + 1; j < bucket.length; j++) {
        if (bucket[i]!.moduleName === bucket[j]!.moduleName) continue;
        const similarity = computeSimilarity(bucket[i]!, bucket[j]!);
        if (similarity >= threshold) {
          allPairs.push({ a: bucket[i]!, b: bucket[j]!, similarity });
        }
      }
    }
  }

  const clusters = singleLinkageMerge(allPairs, threshold);

  return clusters
    .filter(c => c.length >= 2)
    .map((cluster, idx) => buildCluster(idx, cluster))
    .sort((a, b) => b.files.length - a.files.length);
}

function bucketBySharedImports(fingerprints: ComponentFingerprint[]): Map<string, ComponentFingerprint[]> {
  const buckets = new Map<string, ComponentFingerprint[]>();

  for (const fp of fingerprints) {
    const key = fp.sharedLibImports.length > 0 ? [...fp.sharedLibImports].sort().join('|') : '__no_shared_imports__';

    let bucket = buckets.get(key);
    if (!bucket) {
      bucket = [];
      buckets.set(key, bucket);
    }
    bucket.push(fp);
  }

  const noShared = buckets.get('__no_shared_imports__');
  if (noShared && noShared.length > 0) {
    const subBuckets = bucketByRenderSignature(noShared);
    buckets.delete('__no_shared_imports__');
    for (const [key, bucket] of subBuckets) {
      buckets.set(`render:${key}`, bucket);
    }
  }

  return buckets;
}

function bucketByRenderSignature(fingerprints: ComponentFingerprint[]): Map<string, ComponentFingerprint[]> {
  const buckets = new Map<string, ComponentFingerprint[]>();

  for (const fp of fingerprints) {
    const nativeElements = fp.renderElements.filter(el => el === el.toLowerCase());
    const key = nativeElements.length > 0 ? [...new Set(nativeElements)].sort().join('|') : '__misc__';

    let bucket = buckets.get(key);
    if (!bucket) {
      bucket = [];
      buckets.set(key, bucket);
    }
    bucket.push(fp);
  }

  return buckets;
}

function singleLinkageMerge(
  pairs: Array<{ a: ComponentFingerprint; b: ComponentFingerprint; similarity: number }>,
  _threshold: number,
): ComponentFingerprint[][] {
  const parent = new Map<string, string>();
  const nodes = new Map<string, ComponentFingerprint>();

  function find(x: string): string {
    let root = x;
    while (parent.get(root) !== root) root = parent.get(root)!;
    let curr = x;
    while (curr !== root) {
      const next = parent.get(curr)!;
      parent.set(curr, root);
      curr = next;
    }
    return root;
  }

  function union(x: string, y: string): void {
    const rx = find(x);
    const ry = find(y);
    if (rx !== ry) parent.set(rx, ry);
  }

  for (const { a, b } of pairs) {
    if (!parent.has(a.filePath)) parent.set(a.filePath, a.filePath);
    if (!parent.has(b.filePath)) parent.set(b.filePath, b.filePath);
    nodes.set(a.filePath, a);
    nodes.set(b.filePath, b);
    union(a.filePath, b.filePath);
  }

  const clusters = new Map<string, ComponentFingerprint[]>();
  for (const [filePath, fp] of nodes) {
    const root = find(filePath);
    let cluster = clusters.get(root);
    if (!cluster) {
      cluster = [];
      clusters.set(root, cluster);
    }
    cluster.push(fp);
  }

  return [...clusters.values()];
}

function buildCluster(id: number, members: ComponentFingerprint[]): GenericCluster {
  let totalSim = 0;
  let pairCount = 0;
  for (let i = 0; i < members.length; i++) {
    for (let j = i + 1; j < members.length; j++) {
      totalSim += computeSimilarity(members[i]!, members[j]!);
      pairCount++;
    }
  }

  const allVmProps = members.map(m => m.viewModelProps);
  const sharedVmProps = intersectAll(allVmProps);

  const allImports = members.map(m => m.importPaths);
  const sharedImports = intersectAll(allImports);

  const avgSimilarity = pairCount > 0 ? totalSim / pairCount : 0;

  return {
    id,
    avgSimilarity: Math.round(avgSimilarity * 1000) / 1000,
    files: members.map(m => ({ path: m.filePath, module: m.moduleName, className: m.className })),
    sharedViewModelProps: sharedVmProps,
    sharedImports,
    suggestedExtraction: suggestExtraction(members, sharedVmProps, sharedImports),
  };
}

function intersectAll(arrays: string[][]): string[] {
  if (arrays.length === 0) return [];
  let result = new Set(arrays[0]);
  for (let i = 1; i < arrays.length; i++) {
    const current = new Set(arrays[i]);
    result = new Set([...result].filter(x => current.has(x)));
  }
  return [...result];
}

function suggestExtraction(members: ComponentFingerprint[], sharedProps: string[], sharedImports: string[]): string {
  const allStateful = members.every(m => m.flags.isStateful);
  const allLoading = members.every(m => m.flags.hasLoadingState);

  if (allLoading) return 'Extract async state container with loading/error/content rendering';
  if (allStateful && sharedProps.length >= 3)
    return `Extract stateful component with shared props: ${sharedProps.slice(0, 5).join(', ')}`;
  if (sharedImports.length >= 2) return `Extract shared component reusing: ${sharedImports.slice(0, 3).join(', ')}`;
  return `Extract shared pattern across ${members.length} modules`;
}
