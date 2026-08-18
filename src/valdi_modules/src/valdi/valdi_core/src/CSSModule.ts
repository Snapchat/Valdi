import { StringMap } from 'coreutils/src/StringMap';
import { CSSModuleNative } from './ValdiRuntime';
import { getValdiRuntime } from './ValdiRuntimeProvider';
import { setObjectId } from './utils/IdentifyableObject';

const runtime = getValdiRuntime();

export interface CSSRule {}

export type CSSModule = StringMap<CSSRule>;

export function makeModule(compiledCSSPath: string, ruleMapping: StringMap<string>): CSSModule {
  const cssModule = getNativeModule(compiledCSSPath);

  const target = {} as any;
  return new Proxy(target, {
    get(target, key) {
      let rule = target[key];
      if (typeof rule === 'undefined') {
        const ruleName = ruleMapping[key as string];
        rule = ruleName && cssModule.getRule(ruleName);
        if (typeof rule === 'undefined') {
          // 'null' means already fetched and not found
          rule = null;
        }
        target[key] = rule;
        setObjectId(rule);
      }
      return rule;
    },
  });
}

export function getNativeModule(compiledCSSPath: string): CSSModuleNative {
  const cssModule = runtime.getCSSModule(compiledCSSPath);
  setObjectId(cssModule);
  return cssModule;
}
