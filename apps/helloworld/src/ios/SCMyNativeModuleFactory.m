#import "valdi_core/SCValdiModuleFactoryRegistry.h"
#import <SCCHelloWorldTypes/SCCHelloWorldTypes.h>
#import <Foundation/Foundation.h>

#include <stdlib.h>
#include <string.h>

#import "SCValdiTeardownRepro.h"

@interface SCMyNativeModule: NSObject<SCCHelloWorldNativeModuleModule>

@end

@implementation SCMyNativeModule

- (NSString *)APP_NAME
{
    return @"Valdi iOS";
}

- (void)setAPP_NAME:(NSString *)appName
{

}

- (void)reproduceTeardownDegraded
{
    // Degrade ON (shipped default): resolution after teardown returns a no-op, no crash.
    [SCValdiTeardownRepro reproduceWithDegradeEnabled:YES];
}

- (void)reproduceTeardownCrash
{
    // Degrade OFF (kill switch disabled): resolution after teardown aborts the process (SIGABRT).
    [SCValdiTeardownRepro reproduceWithDegradeEnabled:NO];
}

- (void)reproduceTeardownInvocationCrash
{
    // Invocation after teardown: resolution degrades to a no-op, invoking it returns a null value in
    // a _Nonnull slot; passing it to a non-null-requiring API (+[NSURL fileURLWithPath:]) aborts
    // (SIGABRT) — the invocation-teardown nil-in-nonnull crash.
    [SCValdiTeardownRepro reproduceInvocationCrash];
}

@end

@interface SCMyNativeModuleFactory : SCCHelloWorldNativeModuleModuleFactory

@end

@implementation SCMyNativeModuleFactory

VALDI_REGISTER_MODULE()

- (id<SCCHelloWorldNativeModuleModule>)onLoadModule
{
    SCMyNativeModule *module = [SCMyNativeModule new];
    // Debug-only: let tools trigger a repro without a tap, via an env var (e.g.
    // `SIMCTL_CHILD_TEARDOWN_REPRO_AUTORUN=invocation`). Shipping state is button-driven; with no env
    // var set this does nothing.
    const char *autorun = getenv("TEARDOWN_REPRO_AUTORUN");
    if (autorun != NULL && strcmp(autorun, "invocation") == 0) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [module reproduceTeardownInvocationCrash];
        });
    }
    return module;
}

@end