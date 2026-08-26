// Transport-neutral Valdi hierarchy helpers shared by standalone and embedded DevTools.
const MAX_DEBUGGER_TREE_NODES = 25_000;
const MAX_DEBUGGER_PROJECTION_VALUES = 250_000;
const MAX_DEBUGGER_PROJECTION_DEPTH = 64;
const MAX_DEBUGGER_PROJECTION_STRING_LENGTH = 50_000;

function debuggerJsonRecord() {
  return Object.create(null);
}

function setDebuggerJsonProperty(target, key, value) {
  Object.defineProperty(target, key, {
    configurable: true,
    enumerable: true,
    value,
    writable: true,
  });
}

function debuggerProjectionTruncation(reason, path) {
  const marker = debuggerJsonRecord();
  setDebuggerJsonProperty(marker, '$at', path);
  setDebuggerJsonProperty(marker, '$truncated', reason);
  return marker;
}

function markDebuggerProjectionTruncated(target, reason, path) {
  const marker = debuggerProjectionTruncation(reason, path);
  if (Array.isArray(target)) {
    target.push(marker);
  } else if (target.$type === 'array' && Array.isArray(target.$entries)) {
    target.$entries.push(marker);
  } else {
    setDebuggerJsonProperty(target, '$at', marker.$at);
    setDebuggerJsonProperty(target, '$truncated', marker.$truncated);
  }
}

function debuggerPrimitiveProjection(value, projectionState) {
  if (typeof value === 'string') {
    if (value.length <= MAX_DEBUGGER_PROJECTION_STRING_LENGTH) return value;
    projectionState.complete = false;
    const suffix = '…[truncated]';
    return `${value.slice(0, MAX_DEBUGGER_PROJECTION_STRING_LENGTH - suffix.length)}${suffix}`;
  }
  if (value === null || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isFinite(value) ? value : String(value);
  if (typeof value === 'bigint') return `${value}n`;
  if (typeof value === 'undefined') return '[undefined]';
  if (typeof value === 'function') return `[Function ${value.name || 'anonymous'}]`;
  if (typeof value === 'symbol') return String(value);
  return undefined;
}

function isDebuggerArrayIndex(key) {
  const index = Number(key);
  return Number.isInteger(index) && index >= 0 && index < 4_294_967_295 && String(index) === key;
}

function debuggerOwnEntries(source, path, projectionState) {
  let descriptors;
  try {
    descriptors = Object.getOwnPropertyDescriptors(source);
  } catch {
    projectionState.complete = false;
    return { entries: [], inspectionError: debuggerProjectionTruncation('unavailable-properties', path) };
  }

  const entries = [];
  for (const key of Object.keys(descriptors)) {
    const descriptor = descriptors[key];
    if (!descriptor?.enumerable) continue;
    if (entries.length >= MAX_DEBUGGER_PROJECTION_VALUES) {
      projectionState.complete = false;
      break;
    }
    const childPath = isDebuggerArrayIndex(key) ? `${path}[${key}]` : `${path}.${key}`;
    if (!Object.prototype.hasOwnProperty.call(descriptor, 'value')) {
      projectionState.complete = false;
      entries.push({ accessor: true, childPath, key, value: debuggerProjectionTruncation('accessor', childPath) });
      continue;
    }
    entries.push({ accessor: false, childPath, key, value: descriptor.value });
  }
  return { descriptors, entries, inspectionError: null };
}

function debuggerProjectionFrame(source, targetPath, projectionState) {
  const inspection = debuggerOwnEntries(source, targetPath, projectionState);
  if (inspection.inspectionError) {
    return { entries: [], sparse: false, target: inspection.inspectionError };
  }

  if (!Array.isArray(source)) {
    return { entries: inspection.entries, sparse: false, target: debuggerJsonRecord() };
  }

  const lengthDescriptor = inspection.descriptors.length;
  const length =
    lengthDescriptor && Object.prototype.hasOwnProperty.call(lengthDescriptor, 'value')
      ? Number(lengthDescriptor.value)
      : 0;
  let expectedIndex = 0;
  let dense = Number.isSafeInteger(length) && length >= 0;
  for (const key of Object.keys(inspection.descriptors)) {
    const descriptor = inspection.descriptors[key];
    if (!descriptor?.enumerable) continue;
    if (
      !Object.prototype.hasOwnProperty.call(descriptor, 'value') ||
      !isDebuggerArrayIndex(key) ||
      Number(key) !== expectedIndex
    ) {
      dense = false;
      break;
    }
    expectedIndex += 1;
  }
  if (expectedIndex !== length) dense = false;
  if (dense) return { entries: inspection.entries, sparse: false, target: [] };

  projectionState.complete = false;
  const target = debuggerJsonRecord();
  setDebuggerJsonProperty(target, '$at', targetPath);
  setDebuggerJsonProperty(target, '$entries', []);
  setDebuggerJsonProperty(target, '$length', Number.isSafeInteger(length) && length >= 0 ? length : '[unavailable]');
  setDebuggerJsonProperty(target, '$truncated', 'sparse-array');
  setDebuggerJsonProperty(target, '$type', 'array');
  return {
    entries: inspection.entries,
    sparse: true,
    target,
  };
}

function setDebuggerProjectionEntry(frame, entry, value) {
  if (frame.sparse) {
    const projectedEntry = debuggerJsonRecord();
    setDebuggerJsonProperty(
      projectedEntry,
      isDebuggerArrayIndex(entry.key) ? '$index' : '$key',
      isDebuggerArrayIndex(entry.key) ? Number(entry.key) : entry.key,
    );
    setDebuggerJsonProperty(projectedEntry, 'value', value);
    frame.target.$entries.push(projectedEntry);
  } else {
    setDebuggerJsonProperty(frame.target, entry.key, value);
  }
}

function projectDebuggerValue(value, projectionState, path) {
  if (value === null || typeof value !== 'object') {
    if (projectionState.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
      projectionState.complete = false;
      return debuggerProjectionTruncation('value-limit', path);
    }
    projectionState.valueCount += 1;
    return debuggerPrimitiveProjection(value, projectionState);
  }
  const knownPath = projectionState.seen.get(value);
  if (knownPath !== undefined) {
    if (projectionState.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
      projectionState.complete = false;
      return debuggerProjectionTruncation('value-limit', path);
    }
    projectionState.valueCount += 1;
    const reference = debuggerJsonRecord();
    setDebuggerJsonProperty(reference, '$ref', knownPath);
    return reference;
  }
  if (projectionState.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
    projectionState.complete = false;
    return debuggerProjectionTruncation('value-limit', path);
  }

  const rootFrame = debuggerProjectionFrame(value, path, projectionState);
  const root = rootFrame.target;
  projectionState.seen.set(value, path);
  projectionState.valueCount += 1;
  const stack = [{ ...rootFrame, depth: 0, index: 0, path }];
  while (stack.length) {
    const frame = stack.at(-1);
    if (frame.index >= frame.entries.length) {
      stack.pop();
      continue;
    }
    if (projectionState.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
      projectionState.complete = false;
      markDebuggerProjectionTruncated(frame.target, 'value-limit', frame.path);
      stack.pop();
      continue;
    }

    const entry = frame.entries[frame.index];
    frame.index += 1;
    const childValue = entry.value;
    if (entry.accessor || childValue === null || typeof childValue !== 'object') {
      setDebuggerProjectionEntry(
        frame,
        entry,
        entry.accessor ? childValue : debuggerPrimitiveProjection(childValue, projectionState),
      );
      projectionState.valueCount += 1;
      continue;
    }
    const childKnownPath = projectionState.seen.get(childValue);
    if (childKnownPath !== undefined) {
      const reference = debuggerJsonRecord();
      setDebuggerJsonProperty(reference, '$ref', childKnownPath);
      setDebuggerProjectionEntry(frame, entry, reference);
      projectionState.valueCount += 1;
      continue;
    }
    if (frame.depth + 1 >= MAX_DEBUGGER_PROJECTION_DEPTH) {
      projectionState.complete = false;
      setDebuggerProjectionEntry(frame, entry, debuggerProjectionTruncation('depth-limit', entry.childPath));
      projectionState.valueCount += 1;
      continue;
    }

    const childFrame = debuggerProjectionFrame(childValue, entry.childPath, projectionState);
    setDebuggerProjectionEntry(frame, entry, childFrame.target);
    projectionState.seen.set(childValue, entry.childPath);
    projectionState.valueCount += 1;
    stack.push({ ...childFrame, depth: frame.depth + 1, index: 0, path: entry.childPath });
  }
  return root;
}

function debuggerChildEntries(node) {
  let childrenDescriptor;
  try {
    childrenDescriptor = Object.getOwnPropertyDescriptor(node, 'children');
  } catch {
    return { complete: false, entries: [] };
  }
  if (!childrenDescriptor) return { complete: true, entries: [] };
  if (!Object.prototype.hasOwnProperty.call(childrenDescriptor, 'value')) {
    return { complete: false, entries: [] };
  }
  const children = childrenDescriptor.value;
  if (!Array.isArray(children)) return { complete: true, entries: [] };

  let descriptors;
  try {
    descriptors = Object.getOwnPropertyDescriptors(children);
  } catch {
    return { complete: false, entries: [] };
  }
  const lengthDescriptor = descriptors.length;
  const length =
    lengthDescriptor && Object.prototype.hasOwnProperty.call(lengthDescriptor, 'value')
      ? Number(lengthDescriptor.value)
      : 0;
  const entries = [];
  let complete = true;
  let numericProperties = 0;
  for (const key of Object.keys(descriptors)) {
    if (!isDebuggerArrayIndex(key)) continue;
    numericProperties += 1;
    const descriptor = descriptors[key];
    if (!descriptor?.enumerable || !Object.prototype.hasOwnProperty.call(descriptor, 'value')) {
      complete = false;
      continue;
    }
    if (entries.length >= MAX_DEBUGGER_TREE_NODES) {
      complete = false;
      break;
    }
    if (descriptor.value) entries.push({ index: Number(key), node: descriptor.value });
  }
  if (!Number.isSafeInteger(length) || length < 0 || numericProperties !== length) complete = false;
  return { complete, entries };
}

function walkDebuggerTree(node, visitor, ancestors, depth, shouldDescend) {
  if (!node) return true;

  const path = ancestors.slice();
  const visited = new Set(ancestors);
  if (visited.has(node)) return true;
  const stack = [{ childEntries: null, childIndex: 0, depth, entered: false, node, sourceChildIndex: null }];
  let complete = true;
  let visitedCount = 0;
  while (stack.length) {
    const frame = stack.at(-1);
    if (!frame.entered) {
      if (visited.has(frame.node)) {
        stack.pop();
        continue;
      }
      if (visitedCount >= MAX_DEBUGGER_TREE_NODES) return false;
      visited.add(frame.node);
      visitedCount += 1;
      frame.entered = true;
      if (visitor(frame.node, path, frame.depth, frame.sourceChildIndex) === false) return false;
      path.push(frame.node);
      if (!shouldDescend(frame.node, frame.depth)) {
        frame.childEntries = [];
        continue;
      }
      const children = debuggerChildEntries(frame.node);
      if (!children.complete) complete = false;
      frame.childEntries = children.entries;
      continue;
    }

    let childEntry = null;
    while (frame.childIndex < frame.childEntries.length && !childEntry) {
      const candidate = frame.childEntries[frame.childIndex];
      frame.childIndex += 1;
      if (candidate.node && !visited.has(candidate.node)) childEntry = candidate;
    }
    if (childEntry) {
      stack.push({
        childEntries: null,
        childIndex: 0,
        depth: frame.depth + 1,
        entered: false,
        node: childEntry.node,
        sourceChildIndex: childEntry.index,
      });
      continue;
    }
    stack.pop();
    path.pop();
  }
  return complete;
}

const valdiDebuggerTreeModel = Object.freeze({
  attributes(node) {
    return node?.element?.attributes || {};
  },

  id(node) {
    if (node?.id !== undefined) return String(node.id);
    if (node?.element?.id !== undefined) return String(node.element.id);
    if (node?.key !== undefined) return `${node.tag}:${node.key}`;
    return node?.tag || '';
  },

  children(node) {
    return node ? debuggerChildEntries(node).entries.map(entry => entry.node) : [];
  },

  hasChildren(node) {
    return node ? debuggerChildEntries(node).entries.length > 0 : false;
  },

  walk(node, visitor, ancestors, depth) {
    return walkDebuggerTree(node, visitor, ancestors, depth, () => true);
  },

  walkVisible(node, visitor, isExpanded, ancestors, depth) {
    return walkDebuggerTree(node, visitor, ancestors, depth, isExpanded);
  },

  findNode(root, id) {
    if (id === null || id === undefined) return null;
    let found = null;
    valdiDebuggerTreeModel.walk(
      root,
      node => {
        if (valdiDebuggerTreeModel.id(node) !== String(id)) return true;
        found = node;
        return false;
      },
      [],
      0,
    );
    return found;
  },

  pathToNode(root, id) {
    if (id === null || id === undefined) return [];
    let path = [];
    valdiDebuggerTreeModel.walk(
      root,
      (node, ancestors) => {
        if (valdiDebuggerTreeModel.id(node) !== String(id)) return true;
        path = [...ancestors, node];
        return false;
      },
      [],
      0,
    );
    return path;
  },

  projectValue(value) {
    const projectionState = {
      complete: true,
      seen: new Map(),
      valueCount: 0,
    };
    const projected = projectDebuggerValue(value, projectionState, '$');
    return {
      complete: projectionState.complete,
      value: projected,
    };
  },

  stringifyValue(value, spacing) {
    const projection = valdiDebuggerTreeModel.projectValue(value);
    return JSON.stringify(projection.value, null, spacing);
  },

  formatValue(value, spacing) {
    const projection = valdiDebuggerTreeModel.projectValue(value);
    return typeof projection.value === 'string' ? projection.value : JSON.stringify(projection.value, null, spacing);
  },

  projectTree(root) {
    if (!root) {
      return {
        complete: true,
        format: 'valdi-debugger-tree-v1',
        nodeCount: 0,
        nodes: [],
        rootIndex: null,
      };
    }

    const nodes = [];
    const nodeIndexes = new Map();
    const projectionState = {
      complete: true,
      seen: new Map(),
      valueCount: 0,
    };
    const traversalComplete = valdiDebuggerTreeModel.walk(
      root,
      (node, ancestors, depth, sourceChildIndex) => {
        const index = nodes.length;
        const parent = ancestors.at(-1);
        const parentIndex = parent ? nodeIndexes.get(parent) : null;
        nodeIndexes.set(node, index);
        projectionState.seen.set(node, `$.nodes[${index}].data`);
        const data = debuggerJsonRecord();
        const inspection = debuggerOwnEntries(node, `$.nodes[${index}].data`, projectionState);
        if (inspection.inspectionError) {
          markDebuggerProjectionTruncated(data, 'unavailable-properties', `$.nodes[${index}].data`);
        }
        for (const entry of inspection.entries) {
          if (entry.key === 'children') continue;
          if (projectionState.valueCount >= MAX_DEBUGGER_PROJECTION_VALUES) {
            projectionState.complete = false;
            markDebuggerProjectionTruncated(data, 'value-limit', `$.nodes[${index}].data`);
            break;
          }
          setDebuggerJsonProperty(
            data,
            entry.key,
            entry.accessor ? entry.value : projectDebuggerValue(entry.value, projectionState, entry.childPath),
          );
        }
        nodes.push({
          childIndexes: [],
          data,
          depth,
          index,
          parentIndex: parentIndex === undefined ? null : parentIndex,
          sourceChildIndex,
        });
        if (parentIndex !== null && parentIndex !== undefined) {
          nodes[parentIndex].childIndexes.push(index);
        }
      },
      [],
      0,
    );
    return {
      complete: traversalComplete && projectionState.complete,
      format: 'valdi-debugger-tree-v1',
      nodeCount: nodes.length,
      nodes,
      rootIndex: 0,
    };
  },

  projectSnapshot(snapshot) {
    let metadata = debuggerJsonRecord();
    try {
      const descriptors = Object.getOwnPropertyDescriptors(snapshot && typeof snapshot === 'object' ? snapshot : {});
      for (const key of Object.keys(descriptors)) {
        if (key !== 'tree') Object.defineProperty(metadata, key, descriptors[key]);
      }
    } catch {
      metadata = debuggerProjectionTruncation('unavailable-properties', '$');
    }
    const projectedMetadata = valdiDebuggerTreeModel.projectValue(metadata);
    const projectedTree = valdiDebuggerTreeModel.projectTree(snapshot?.tree);
    const projection = debuggerJsonRecord();
    for (const key of Object.keys(projectedMetadata.value)) {
      setDebuggerJsonProperty(projection, key, projectedMetadata.value[key]);
    }
    setDebuggerJsonProperty(projection, 'projectionComplete', projectedMetadata.complete && projectedTree.complete);
    setDebuggerJsonProperty(projection, 'tree', projectedTree);
    return projection;
  },

  restoreTree(value) {
    if (
      !value ||
      value.format !== 'valdi-debugger-tree-v1' ||
      !Array.isArray(value.nodes) ||
      !Number.isInteger(value.rootIndex)
    ) {
      return value;
    }
    const nodes = value.nodes.map(record => {
      const node = debuggerJsonRecord();
      let descriptors;
      try {
        descriptors = Object.getOwnPropertyDescriptors(record?.data || debuggerJsonRecord());
      } catch {
        descriptors = debuggerJsonRecord();
      }
      for (const key of Object.keys(descriptors)) {
        const descriptor = descriptors[key];
        if (descriptor?.enumerable && Object.prototype.hasOwnProperty.call(descriptor, 'value')) {
          setDebuggerJsonProperty(node, key, descriptor.value);
        }
      }
      return node;
    });
    for (let index = 0; index < value.nodes.length; index += 1) {
      const childIndexes = Array.isArray(value.nodes[index]?.childIndexes) ? value.nodes[index].childIndexes : [];
      setDebuggerJsonProperty(
        nodes[index],
        'children',
        childIndexes
          .filter(childIndex => Number.isInteger(childIndex) && childIndex >= 0 && childIndex < nodes.length)
          .map(childIndex => nodes[childIndex]),
      );
    }
    return nodes[value.rootIndex] || null;
  },
});
