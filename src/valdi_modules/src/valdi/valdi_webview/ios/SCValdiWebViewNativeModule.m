#import <Foundation/Foundation.h>

#import "SCValdiWebViewControllerImpl.h"
#import "valdi_core/SCValdiModuleFactoryRegistry.h"
#import <SCCValdiWebViewTypes/SCCValdiWebViewTypes.h>

@interface SCValdiWebViewNativeModuleImpl : NSObject <SCCValdiWebViewWebViewNativeModule>
@end

@implementation SCValdiWebViewNativeModuleImpl

- (id<SCValdiWebViewController>)createNativeController
{
    return [SCValdiWebViewControllerImpl new];
}

@end

@interface SCValdiWebViewNativeModuleFactory : SCCValdiWebViewWebViewNativeModuleFactory
@end

@implementation SCValdiWebViewNativeModuleFactory

VALDI_REGISTER_MODULE()

- (id<SCCValdiWebViewWebViewNativeModule>)onLoadModule
{
    return [SCValdiWebViewNativeModuleImpl new];
}

@end
