import 'jasmine/src/jasmine';
import {
  captureDebuggerPropertiesSnapshot,
  captureDebuggerPropertiesSnapshotWithReflectionLimit,
  type DebuggerValueSnapshotLimits,
} from '../src/debug/DebuggerValueSnapshot';

const LIMITS: DebuggerValueSnapshotLimits = {
  maximumDepth: 4,
  maximumEntries: 50,
  maximumPropertyNameCharacters: 256,
  maximumStringBytes: 65_536,
};

describe('DebuggerValueSnapshot', () => {
  it('captures only own enumerable data descriptors without invoking accessors', () => {
    let getterCalls = 0;
    const inherited = { inherited: 'hidden' };
    const source = Object.create(inherited) as Record<PropertyKey, unknown>;
    source.visible = { nested: true };
    source[Symbol('symbol-property')] = 'hidden';
    Object.defineProperty(source, 'secret', {
      enumerable: true,
      get: () => {
        getterCalls++;
        return 'must not be read';
      },
    });

    const captured = captureDebuggerPropertiesSnapshot(source, 65_536, LIMITS);

    expect(getterCalls).toBe(0);
    expect(JSON.stringify(captured?.value.visible)).toBe('{"nested":true}');
    expect(Object.prototype.hasOwnProperty.call(captured?.value, 'secret')).toBeFalse();
    expect(Object.prototype.hasOwnProperty.call(captured?.value, 'inherited')).toBeFalse();
    expect(Object.getOwnPropertySymbols(captured?.value ?? {})).toEqual([]);
  });

  it('bounds keys and entries', () => {
    const source: Record<string, unknown> = {};
    source['k'.repeat(257)] = 'hidden';
    for (let index = 0; index < 60; index++) {
      source[`property-${index}`] = index;
    }
    const captured = captureDebuggerPropertiesSnapshot(source, 65_536, LIMITS);

    expect(captured).toBeDefined();
    expect(Object.keys(captured!.value).length).toBeLessThanOrEqual(50);
    expect(Object.prototype.hasOwnProperty.call(captured!.value, 'k'.repeat(257))).toBeFalse();
  });

  it('limits every nested container to 50 entries including truncation markers', () => {
    const nestedObject: Record<string, number> = {};
    for (let index = 0; index < 60; index++) {
      nestedObject[`property-${index}`] = index;
    }
    const captured = captureDebuggerPropertiesSnapshot(
      { nestedArray: Array.from({ length: 60 }, (_value, index) => index), nestedObject },
      65_536,
      LIMITS,
    );

    expect(captured).toBeDefined();
    expect((captured!.value.nestedArray as unknown[]).length).toBeLessThanOrEqual(50);
    expect(Object.keys(captured!.value.nestedObject as Record<string, unknown>).length).toBeLessThanOrEqual(50);
  });

  it('bounds nested depth, strings, and the complete UTF-8 payload', () => {
    const captured = captureDebuggerPropertiesSnapshot(
      {
        nested: { one: { two: { three: { four: 'hidden' } } } },
        unicode: '😀'.repeat(65_536),
      },
      65_536,
      LIMITS,
    );
    const serialized = JSON.stringify(captured?.value);

    expect(captured).toBeDefined();
    expect(captured!.serializedBytes).toBeLessThanOrEqual(65_536);
    expect(captured!.serializedBytes).toBeGreaterThanOrEqual(serialized.length);
    expect(JSON.stringify(captured!.value.nested)).toContain('... <truncated>');
    expect(String(captured!.value.unicode)).toContain('... <truncated>');
  });

  it('uses bounded markers for bigint and symbol values', () => {
    const bigintFactory = Reflect.get(globalThis, 'BigInt') as (value: number) => unknown;
    const captured = captureDebuggerPropertiesSnapshot(
      { bigint: bigintFactory(1), symbol: Symbol('description is intentionally not copied') },
      65_536,
      LIMITS,
    );

    expect(captured?.value.bigint).toBe('<bigint/>');
    expect(captured?.value.symbol).toBe('<symbol/>');
  });

  it('fails closed when a Proxy refuses or revokes descriptor inspection', () => {
    const source = new Proxy(
      { visible: 'value' },
      {
        ownKeys: () => {
          throw new Error('unavailable');
        },
      },
    );

    expect(captureDebuggerPropertiesSnapshot(source, 65_536, LIMITS)).toBeUndefined();

    const revocable = Proxy.revocable({ visible: 'value' }, {});
    revocable.revoke();
    expect(captureDebuggerPropertiesSnapshot(revocable.proxy, 65_536, LIMITS)).toBeUndefined();
  });

  it('reports all materialized own names and array descriptor inspections as reflection work', () => {
    const source: Record<string, unknown> = {};
    for (let index = 0; index < 100; index++) {
      Object.defineProperty(source, `hidden-${index}`, { enumerable: false, value: index });
    }
    source.visible = [1, 2];

    const captured = captureDebuggerPropertiesSnapshot(source, 65_536, LIMITS);

    expect(captured?.reflectionOperations).toBe(104);
    expect(captured?.unmeasurable).toBeFalse();
  });

  it('bounds descriptor reflection immediately after own-name materialization', () => {
    const propertyNames = Array.from({ length: 16_385 }, (_value, index) => `hidden-${index}`);
    let descriptorReads = 0;
    const state = new Proxy(
      {},
      {
        getOwnPropertyDescriptor: () => {
          descriptorReads++;
          return { configurable: true, enumerable: false, value: true };
        },
        ownKeys: () => propertyNames,
      },
    );

    const captured = captureDebuggerPropertiesSnapshotWithReflectionLimit({ state }, 65_536, 16_384, LIMITS);

    expect(descriptorReads).toBe(0);
    expect(captured?.unmeasurable).toBeTrue();
    expect(captured?.reflectionOperations).toBeGreaterThan(16_384);
  });

  it('checks the bounded allowance before each object descriptor lookup', () => {
    const descriptorNames: string[] = [];
    const source = new Proxy(
      { first: 1, second: 2, third: 3 },
      {
        getOwnPropertyDescriptor: (target, propertyName) => {
          descriptorNames.push(String(propertyName));
          return Reflect.getOwnPropertyDescriptor(target, propertyName);
        },
      },
    );

    const captured = captureDebuggerPropertiesSnapshotWithReflectionLimit(source, 65_536, 4, LIMITS);

    expect(descriptorNames).toEqual(['first']);
    expect(captured?.unmeasurable).toBeTrue();
  });

  it('checks the bounded allowance before each array descriptor lookup', () => {
    const descriptorNames: string[] = [];
    const values = new Proxy([1, 2, 3], {
      getOwnPropertyDescriptor: (target, propertyName) => {
        descriptorNames.push(String(propertyName));
        return Reflect.getOwnPropertyDescriptor(target, propertyName);
      },
    });

    const captured = captureDebuggerPropertiesSnapshotWithReflectionLimit({ values }, 65_536, 4, LIMITS);

    expect(descriptorNames).toEqual(['length', '0']);
    expect(captured?.unmeasurable).toBeTrue();
  });

  it('accepts a document that uses the exact bounded reflection allowance', () => {
    const captured = captureDebuggerPropertiesSnapshotWithReflectionLimit({ value: true }, 65_536, 2, LIMITS);

    expect(captured?.value.value).toBeTrue();
    expect(captured?.reflectionOperations).toBe(2);
    expect(captured?.unmeasurable).toBeFalse();
  });

  it('accepts an array that uses the exact bounded reflection allowance', () => {
    const descriptorNames: string[] = [];
    const values = new Proxy([1], {
      getOwnPropertyDescriptor: (target, propertyName) => {
        descriptorNames.push(String(propertyName));
        return Reflect.getOwnPropertyDescriptor(target, propertyName);
      },
    });

    const captured = captureDebuggerPropertiesSnapshotWithReflectionLimit({ values }, 65_536, 4, LIMITS);

    expect(descriptorNames).toEqual(['length', '0']);
    expect(captured?.value.values).toEqual([1]);
    expect(captured?.reflectionOperations).toBe(4);
    expect(captured?.unmeasurable).toBeFalse();
  });

  it('marks nested reflection recovery as unmeasurable', () => {
    const nested = new Proxy(
      { value: true },
      {
        getOwnPropertyDescriptor: () => {
          throw new Error('unavailable');
        },
      },
    );

    const captured = captureDebuggerPropertiesSnapshot({ nested, safe: true }, 65_536, LIMITS);

    expect(captured?.value.nested).toBe('<unavailable/>');
    expect(captured?.value.safe).toBeTrue();
    expect(captured?.unmeasurable).toBeTrue();
  });

  it('does not traverse Proxy prototypes while capturing own fields', () => {
    let prototypeTrapCalls = 0;
    const source = new Proxy(
      { visible: 'value' },
      {
        getPrototypeOf: () => {
          prototypeTrapCalls++;
          throw new Error('prototype traversal is forbidden');
        },
      },
    );

    const captured = captureDebuggerPropertiesSnapshot(source, 65_536, LIMITS);

    expect(prototypeTrapCalls).toBe(0);
    expect(captured?.value.visible).toBe('value');
  });
});
