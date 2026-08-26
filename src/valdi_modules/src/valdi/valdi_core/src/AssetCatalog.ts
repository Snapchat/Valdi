import { StringMap } from 'coreutils/src/StringMap';
import { Asset } from './Asset';
import { AssetEntry } from './ValdiRuntime';
import { getValdiRuntime } from './ValdiRuntimeProvider';
import { toCamelCase } from './utils/StringUtils';

export type GetFuncFn = (size: number) => string;

export type AssetCatalogEntry = Asset | GetFuncFn;
export type AssetCatalog = StringMap<AssetCatalogEntry>;

const runtime = getValdiRuntime();

function makeGetFuncFn(fontName: string): GetFuncFn {
  return (size: number) => {
    return `${fontName} ${size}`;
  };
}

export function makeCatalog(entries: AssetEntry[]): AssetCatalog {
  const out: AssetCatalog = {};

  for (const entry of entries) {
    if (typeof entry === 'string') {
      const components = entry.split(':');
      const propertyName = toCamelCase(components[components.length - 1]);
      out[propertyName] = makeGetFuncFn(entry);
    } else {
      const components = entry.path.split(':');
      const propertyName = toCamelCase(components[components.length - 1]);
      out[propertyName] = entry;
    }
  }

  return out;
}

export function loadCatalog(catalogPath: string): any {
  const assets = runtime.getAssets(catalogPath);
  return makeCatalog(assets);
}
