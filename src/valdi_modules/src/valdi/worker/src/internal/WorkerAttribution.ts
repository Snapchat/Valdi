import type { ValdiRuntime } from 'valdi_core/src/ValdiRuntime';
import type { AnyFunction } from 'valdi_core/src/utils/Callback';

declare const runtime: ValdiRuntime;

/** Labels a worker boundary using a stable service identity, excluding instance IDs and payloads. */
export function attributeWorkerCallback<F extends AnyFunction>(label: string | undefined, callback: F): F {
  return label && runtime.makeANRAttributionProxy ? runtime.makeANRAttributionProxy(label, callback) : callback;
}
