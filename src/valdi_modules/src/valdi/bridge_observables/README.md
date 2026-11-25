# Bridge Observables

Provide bridging types and conversion functions for observable types.

# Example

**Declaration:**
```ts
export interface MyFeatureContext {
    myDataStream?: BridgeObservable<string>
}
```

**Valdi / Usage:**
```ts
const bridgeObservable = context.myDataStream ?? [...];
const rxObservable = convertBridgeObservableToObservable(bridgeObservable);
rxObservable.subscribe() // as usual
```

**Android / Kotlin:**
```kotlin
val context = MyFeatureContext();
context.myDataStream = Observable.just("hello world!").toBridgeObservable();
```

**iOS / Obj-C:**
```obj-c
MyFeatureContext *context = [MyFeatureContext new];
context.myDataStream = [[SCObservable just:@"hello world!"] toBridgeObservable];
```

# iOS

### iOS -> Valdi (RxJS):

**Available conversions:**

TODO: consider moving iOS implementation into open source

### Valdi (RxJS) -> iOS:

**Available conversions:**

TODO: consider moving iOS implementatino into open source

# Android

### Android (RxJava) -> Valdi (RxJS):

**Available conversions:**

Observable(RxJava) -> toBridgeObservable -> BridgeObservable -> convertBridgeObservableToObservable -> Observable(RxJS)
Observer(RxJava) -> toBridgeObserver -> BridgeObserver -> convertBridgeObserverToObserver -> Observer(RxJS)

Subject(RxJava) -> toBridgeSubject -> BridgeSubject -> convertBridgeSubjectToObservable -> Observable(RxJS)
Subject(RxJava) -> toBridgeSubject -> BridgeSubject -> convertBridgeSubjectToObserver -> Observer(RxJS)

### Valdi (RxJS) -> Android (RxJava):

**Available conversions:**

Observable(RxJs) -> convertObservableToBridgeObservable -> BridgeObservable -> toObservable -> Observable(RxJava)
Observer(RxJs) -> convertObserverToBridgeObserver -> BridgeObserver -> toObserver -> Observer(RxJava)

Subject(RxJs) -> convertSubjectToBridgeSubject -> BridgeSubject -> toObservable -> Observable(RxJava)
Subject(RxJs) -> convertSubjectToBridgeSubject -> BridgeSubject -> toObserver -> Observer(RxJava)
