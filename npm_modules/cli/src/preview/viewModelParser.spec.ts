import 'jasmine';
import * as fs from 'fs';
import * as os from 'os';
import * as path from 'path';
import { parseComponentFile, detectInteractiveState, sampleValueForType } from './viewModelParser';
import type { ParsedProp } from './viewModelParser';

describe('viewModelParser', () => {
  let tmpDir: string;

  beforeEach(() => {
    tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'vmp-test-'));
  });

  afterEach(() => {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  });

  function writeComponent(filename: string, content: string): string {
    const filePath = path.join(tmpDir, filename);
    fs.writeFileSync(filePath, content, 'utf8');
    return filePath;
  }

  describe('parseComponentFile', () => {
    it('parses a simple Component with ViewModel', () => {
      const filePath = writeComponent('Simple.tsx', `
import { Component } from 'valdi_core/src/Component';

export interface SimpleViewModel {
  title: string;
  count: number;
  enabled: boolean;
}

export class Simple extends Component<SimpleViewModel, {}> {
  onRender(): void {}
}
`);
      const result = parseComponentFile(filePath);
      expect(result.componentName).toBe('Simple');
      expect(result.viewModelName).toBe('SimpleViewModel');
      expect(result.props.length).toBe(3);
      expect(result.props[0]!.name).toBe('title');
      expect(result.props[0]!.typeString).toBe('string');
      expect(result.props[0]!.optional).toBe(false);
      expect(result.props[1]!.name).toBe('count');
      expect(result.props[1]!.typeString).toBe('number');
      expect(result.props[2]!.name).toBe('enabled');
      expect(result.props[2]!.isBoolean).toBe(true);
    });

    it('parses optional props', () => {
      const filePath = writeComponent('Optional.tsx', `
export interface OptViewModel {
  label: string;
  subtitle?: string;
  icon?: number;
}
export class OptComponent extends Component<OptViewModel, {}> {
  onRender(): void {}
}
`);
      const result = parseComponentFile(filePath);
      expect(result.props.length).toBe(3);
      expect(result.props[0]!.optional).toBe(false);
      expect(result.props[1]!.optional).toBe(true);
      expect(result.props[2]!.optional).toBe(true);
    });

    it('parses callback props', () => {
      const filePath = writeComponent('Callback.tsx', `
export interface CbViewModel {
  onTap: () => void;
  onChange: (val: boolean) => void;
  onSelect: (item: string) => void;
}
export class CbComponent extends Component<CbViewModel, {}> {
  onRender(): void {}
}
`);
      const result = parseComponentFile(filePath);
      expect(result.props.length).toBe(3);
      expect(result.props[0]!.isCallback).toBe(true);
      expect(result.props[0]!.callbackParamType).toBeUndefined();
      expect(result.props[1]!.isCallback).toBe(true);
      expect(result.props[1]!.callbackParamType).toBe('boolean');
      expect(result.props[2]!.isCallback).toBe(true);
      expect(result.props[2]!.callbackParamType).toBe('string');
    });

    it('parses StatefulComponent', () => {
      const filePath = writeComponent('Stateful.tsx', `
export interface StatefulVM {
  checked: boolean;
}
export class MyStateful extends StatefulComponent<StatefulVM> {
  onRender(): void {}
}
`);
      const result = parseComponentFile(filePath);
      expect(result.componentName).toBe('MyStateful');
      expect(result.viewModelName).toBe('StatefulVM');
    });

    it('throws when no Component class is found', () => {
      const filePath = writeComponent('NoClass.tsx', `
export interface SomeViewModel { title: string; }
`);
      expect(() => parseComponentFile(filePath)).toThrowError(/Could not find an exported Component class/);
    });

    it('throws when ViewModel interface is missing', () => {
      const filePath = writeComponent('NoVM.tsx', `
export class Broken extends Component<MissingViewModel, {}> {
  onRender(): void {}
}
`);
      expect(() => parseComponentFile(filePath)).toThrowError(/Could not find interface MissingViewModel/);
    });
  });

  describe('detectInteractiveState', () => {
    it('pairs boolean prop with onChange callback', () => {
      const props: ParsedProp[] = [
        { name: 'on', typeString: 'boolean', optional: false, isBoolean: true, isCallback: false, callbackParamType: undefined },
        { name: 'onChange', typeString: '(val: boolean) => void', optional: false, isBoolean: false, isCallback: true, callbackParamType: 'boolean' },
      ];
      const pairs = detectInteractiveState(props);
      expect(pairs.length).toBe(1);
      expect(pairs[0]!.boolPropName).toBe('on');
      expect(pairs[0]!.callbackPropName).toBe('onChange');
    });

    it('pairs boolean prop with onTap callback', () => {
      const props: ParsedProp[] = [
        { name: 'selected', typeString: 'boolean', optional: false, isBoolean: true, isCallback: false, callbackParamType: undefined },
        { name: 'onTap', typeString: '(val: boolean) => void', optional: false, isBoolean: false, isCallback: true, callbackParamType: 'boolean' },
      ];
      const pairs = detectInteractiveState(props);
      expect(pairs.length).toBe(1);
      expect(pairs[0]!.boolPropName).toBe('selected');
      expect(pairs[0]!.callbackPropName).toBe('onTap');
    });

    it('returns empty when no matching pair exists', () => {
      const props: ParsedProp[] = [
        { name: 'title', typeString: 'string', optional: false, isBoolean: false, isCallback: false, callbackParamType: undefined },
        { name: 'onTap', typeString: '() => void', optional: false, isBoolean: false, isCallback: true, callbackParamType: undefined },
      ];
      const pairs = detectInteractiveState(props);
      expect(pairs.length).toBe(0);
    });

    it('skips optional boolean props', () => {
      const props: ParsedProp[] = [
        { name: 'checked', typeString: 'boolean', optional: true, isBoolean: true, isCallback: false, callbackParamType: undefined },
        { name: 'onToggle', typeString: '(val: boolean) => void', optional: false, isBoolean: false, isCallback: true, callbackParamType: 'boolean' },
      ];
      const pairs = detectInteractiveState(props);
      expect(pairs.length).toBe(0);
    });
  });

  describe('sampleValueForType', () => {
    it('returns correct sample for primitive types', () => {
      expect(sampleValueForType('boolean')).toBe('false');
      expect(sampleValueForType('string')).toBe("'Sample'");
      expect(sampleValueForType('number')).toBe('42');
      expect(sampleValueForType('Date')).toBe('new Date()');
    });

    it('returns noop for function types', () => {
      expect(sampleValueForType('() => void')).toBe('() => {}');
      expect(sampleValueForType('(val: boolean) => void')).toBe('() => {}');
    });

    it('returns empty array for array types', () => {
      expect(sampleValueForType('string[]')).toBe('[]');
      expect(sampleValueForType('Array<number>')).toBe('[]');
    });

    it('falls back to undefined for unknown types', () => {
      expect(sampleValueForType('SomeCustomType')).toBe('undefined as any');
    });
  });
});
