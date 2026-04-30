# View Model

## Passing view model from native code

View models can be passed from native code. To do so, you first need to generate a strong typed interface for the view model.
To leverage the compiler's ability to generate Objective-C, Swift, and Kotlin view model classes, add `@ExportModel` and `@ViewModel` annotations, as follow:

```ts
// @ExportModel({ios: 'SCMyViewModel', android: 'com.snap.valdi.MyViewModel'})
// @ViewModel
interface ViewModel {
  title?: string;
  subtitle?: string;
}
```

In iOS you will get a generated view model like:
```objectivec
@interface SCMyViewModel: NSObject

@property (readonly, copy, nonatomic) NSString *title;
@property (readonly, copy, nonatomic) NSString *subtitle;

- (instancetype)initWithTitle:(NSString *)title subtitle:(NSString *)subtitle;

@end
```

On Android, you would get this:
```java (works better than kotlin syntax highlighting)
package com.snap.valdi

class MyViewModel(val title: String, val subtitle: String): ValdiViewModel
```
