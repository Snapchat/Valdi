#import "valdi_core/SCValdiModuleFactoryRegistry.h"
#import <SCCHelloWorldTypes/SCCHelloWorldTypes.h>
#import <Foundation/Foundation.h>

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

@end

@interface SCMyNativeModuleFactory : SCCHelloWorldNativeModuleModuleFactory

@end

@implementation SCMyNativeModuleFactory

VALDI_REGISTER_MODULE()

- (id<SCCHelloWorldNativeModuleModule>)onLoadModule
{
    return [SCMyNativeModule new];
}

@end