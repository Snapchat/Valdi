import { Observable } from 'valdi_rxjs/src/Observable';
import { Subscriber } from 'valdi_rxjs/src/Subscriber';
import { BridgeObservable } from '../types/BridgeObservable';
import { BridgeObserverEvent } from '../types/BridgeObserverEvent';

export function convertBridgeObservableToObservable<T>(observable: BridgeObservable<T>): Observable<T> {
  return new Observable<T>((subscriber: Subscriber<T>) => {
    let storedSubscription: (() => void) | undefined;
    let disposed = false;

    observable.subscribe((type, subscription, value, error) => {
      switch (type) {
        case BridgeObserverEvent.RECEIVE_SUBSCRIPTION:
          if (disposed) {
            subscription?.();
          } else {
            storedSubscription = subscription;
          }
          break;
        case BridgeObserverEvent.NEXT:
          subscriber.next(value);
          break;
        case BridgeObserverEvent.ERROR: {
          const jsErr = new Error(error?.message);
          jsErr.stack = error?.stack;
          subscriber.error(jsErr);
          break;
        }
        case BridgeObserverEvent.COMPLETE:
          subscriber.complete();
          break;
      }
    });

    return () => {
      disposed = true;
      storedSubscription?.();
      storedSubscription = undefined;
    };
  });
}
