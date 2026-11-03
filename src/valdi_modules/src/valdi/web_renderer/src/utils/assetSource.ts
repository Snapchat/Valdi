import { isAsciiAlphaCode, isAsciiDigitCode } from './cssScanner';

const ASSET_SOURCE_FIELDS = ['default', 'src', 'url', 'href'] as const;

function isSchemeCode(code: number): boolean {
  return isAsciiAlphaCode(code) || isAsciiDigitCode(code) || code === 43 || code === 45 || code === 46;
}

function hasUrlScheme(value: string): boolean {
  if (value.length < 2 || !isAsciiAlphaCode(value.charCodeAt(0))) {
    return false;
  }
  for (let index = 1; index < value.length; index++) {
    const code = value.charCodeAt(index);
    if (code === 58) {
      return true;
    }
    if (!isSchemeCode(code)) {
      return false;
    }
  }
  return false;
}

function hasFileExtension(value: string): boolean {
  let end = value.length;
  for (let index = 0; index < value.length; index++) {
    const code = value.charCodeAt(index);
    if (code === 63 || code === 35) {
      end = index;
      break;
    }
  }

  let extensionLength = 0;
  for (let index = end - 1; index >= 0; index--) {
    const code = value.charCodeAt(index);
    if (code === 47) {
      return false;
    }
    if (code === 46) {
      return extensionLength > 0;
    }
    if (!isAsciiAlphaCode(code) && !isAsciiDigitCode(code)) {
      return false;
    }
    extensionLength++;
  }
  return false;
}

function isRenderableAssetPath(value: string): boolean {
  return (
    hasUrlScheme(value) ||
    value.startsWith('/') ||
    value.startsWith('.') ||
    value.indexOf('/') >= 0 ||
    hasFileExtension(value)
  );
}

export function resolveAssetSourceUrl(source: unknown): string | undefined {
  if (typeof source === 'string') {
    return source;
  }
  if (!source || typeof source !== 'object') {
    return undefined;
  }

  const objectSource = source as Record<string, unknown>;
  for (const field of ASSET_SOURCE_FIELDS) {
    if (field in objectSource) {
      const resolved = resolveAssetSourceUrl(objectSource[field]);
      if (resolved !== undefined) {
        return resolved;
      }
    }
  }
  return undefined;
}

export function resolveRenderableAssetSource(source: unknown): string | undefined {
  const resolved = resolveAssetSourceUrl(source);
  if (resolved !== undefined) {
    return resolved;
  }
  if (!source || typeof source !== 'object') {
    return undefined;
  }

  const path = (source as { path?: unknown }).path;
  return typeof path === 'string' && isRenderableAssetPath(path) ? path : undefined;
}
