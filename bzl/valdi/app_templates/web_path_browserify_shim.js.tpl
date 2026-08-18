function parts(value) {
  return String(value || '').split('/').filter(Boolean);
}

function normalize(value) {
  const absolute = String(value || '').startsWith('/');
  const out = [];
  for (const part of parts(value)) {
    if (part === '.') {
      continue;
    }
    if (part === '..') {
      out.pop();
    } else {
      out.push(part);
    }
  }
  return (absolute ? '/' : '') + out.join('/');
}

function join() {
  return normalize(Array.prototype.slice.call(arguments).join('/'));
}

function basename(value) {
  const normalized = normalize(value);
  const index = normalized.lastIndexOf('/');
  return index === -1 ? normalized : normalized.slice(index + 1);
}

module.exports = { basename, join, normalize };
