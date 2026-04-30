//
//  SnapModules.h
//  valdi-desktop-apple
//
//  Created by Simon Corsin on 1/14/22.
//

#import "valdi/macos/SCValdiRuntime.h"
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif

void SCValdiNativeModulesRegister(SCValdiRuntime* runtime);

/// Optional component context merged into the root component.
/// Set by the host (e.g. composer_snap_modules) so desktop apps can provide context (e.g. viewFactoryContext).
typedef id _Nullable (^SCValdiDesktopComponentContextProvider)(SCValdiRuntime * _Nonnull runtime);
void SCValdiSetDesktopComponentContextProvider(SCValdiDesktopComponentContextProvider _Nullable provider);
id _Nullable SCValdiGetDesktopComponentContext(SCValdiRuntime * _Nonnull runtime);

#ifdef __cplusplus
}
#endif
