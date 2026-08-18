import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { IBenchmarkContext, randomInt, randomString } from './BenchmarkUtils';

interface KeyValue<T> {
  key: T;
  value: any;
}

function stringGenerator(length: number): () => string {
  return () => randomString(length);
}

function intGenerator(): () => number {
  return () => randomInt(0, 1000000);
}

function produceRandomKeyValues<T>(length: number, keyGenerator: () => T): KeyValue<T>[] {
  const out: KeyValue<T>[] = [];
  for (let index = 0; index < length; index++) {
    const key = keyGenerator();
    const value = {};
    out.push({ key, value });
  }
  return out;
}

function testMapVsObjectGet(context: IBenchmarkContext) {
  const keyValues = produceRandomKeyValues(24, stringGenerator(32));
  const obj: any = {};
  const map = new Map();
  for (const entry of keyValues) {
    obj[entry.key] = entry.value;
    map.set(entry.key, entry.value);
  }

  context.bench('Object', () => {
    for (const entry of keyValues) {
      obj[entry.key];
    }
  });

  context.bench('Map', () => {
    for (const entry of keyValues) {
      map.get(entry.key);
    }
  });
}

function testMapVsObjectSet(context: IBenchmarkContext) {
  const keyValues = produceRandomKeyValues(24, stringGenerator(32));

  context.bench('Object', () => {
    const obj: any = {};
    for (const entry of keyValues) {
      obj[entry.key] = entry.value;
    }
  });

  context.bench('Map', () => {
    const map = new Map();
    for (const entry of keyValues) {
      map.set(entry.key, entry.value);
    }
  });
}

function testObjectVsSpareArrayIntSet(context: IBenchmarkContext) {
  const keyValues = produceRandomKeyValues(32, intGenerator());

  context.bench('Object', () => {
    const obj: any = {};
    for (const entry of keyValues) {
      obj[entry.key] = entry.value;
    }
  });

  context.bench('Array', () => {
    const array = [];
    for (const entry of keyValues) {
      array[entry.key] = entry.value;
    }
  });
}

function testObjectIntVsStringKeys(context: IBenchmarkContext) {
  const keyValuesInt = produceRandomKeyValues(32, intGenerator());
  const keyValuesString = produceRandomKeyValues(32, stringGenerator(24));

  context.bench('String', () => {
    const obj: any = {};
    for (const entry of keyValuesString) {
      obj[entry.key] = entry.value;
    }
  });
  context.bench('Number', () => {
    const obj: any = {};
    for (const entry of keyValuesInt) {
      obj[entry.key] = entry.value;
    }
  });
}

const runtime = getValdiRuntime();

const CALL_PER_BENCH = 1000;

function anotherFunc(): any {
  return 1;
}

interface BenchFuncs {
  emptyCall: () => void;
  fourParamsCall: () => void;
  eightParamsCall: () => void;
}

function makeBenchFuncs(cb: () => void, str: string, anObject: any): BenchFuncs {
  let emptyCall = '() => {\n';
  for (let i = 0; i < CALL_PER_BENCH; i++) {
    emptyCall += 'cb();\n';
  }
  emptyCall += '\n}';

  let fourParamsCall = '() => {\n';
  for (let i = 0; i < CALL_PER_BENCH; i++) {
    fourParamsCall += 'cb(42, true, anObject, str);\n';
  }
  fourParamsCall += '\n}';

  let eightParamsCall = '() => {\n';
  for (let i = 0; i < CALL_PER_BENCH; i++) {
    eightParamsCall += 'cb(42, true, anObject, str, 42, true, anObject, str);\n';
  }
  eightParamsCall += '\n}';

  return {
    emptyCall: eval(emptyCall),
    fourParamsCall: eval(fourParamsCall),
    eightParamsCall: eval(eightParamsCall),
  };
}

function testFunctionCalls(context: IBenchmarkContext) {
  const nativeCb: () => void = runtime.getCurrentPlatform;
  const jsCb: () => void = anotherFunc;
  const str = 'A String';
  const anObject = {};

  context.benchDurationMs = 5000;

  const nativeBenchFuncs = makeBenchFuncs(nativeCb, str, anObject);
  const jsBenchFuncs = makeBenchFuncs(jsCb, str, anObject);

  context.bench('Native Call No Params', nativeBenchFuncs.emptyCall);

  context.bench('Native Call Four Params', nativeBenchFuncs.fourParamsCall);

  context.bench('Native Call Eight Params', nativeBenchFuncs.eightParamsCall);

  context.bench('JS Call No Params', jsBenchFuncs.emptyCall);

  context.bench('JS Call Four Params', jsBenchFuncs.fourParamsCall);

  context.bench('JS Call Eight Params', jsBenchFuncs.eightParamsCall);
}

// performBenchmark(testFunctionCalls);

// performBenchmark(testMapVsObjectGet);
// performBenchmark(testMapVsObjectSet);
// performBenchmark(testObjectVsSpareArrayIntSet);
// performBenchmark(testObjectIntVsStringKeys);
