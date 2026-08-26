#import "valdi/ios/Bootstrap/SCValdiBootstrappingAppDelegate.h"
#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiComponentContainerView.h"
#import "valdi_core/SCValdiDefaultContainerViewController.h"
#import "valdi_core/SCValdiDefaultNavigator.h"

@implementation SCValdiBootstrappingDependencies
@end

@implementation SCValdiBootstrappingAppDelegate {
    UIWindow* _rootWindow;
    SCValdiRuntimeManager* _runtimeManager;
}

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    _runtimeManager = [SCValdiRuntimeManager new];

    [_runtimeManager updateConfiguration:^(SCValdiConfiguration* configuration) {
        configuration.allowDarkMode = YES;
        // Bootstrap apps are local-development hosts, so keep their previous hot-reload behavior explicit.
        configuration.enableDebuggerService = YES;
    }];

    id<SCValdiRuntimeProtocol> runtime = _runtimeManager.mainRuntime;

    UIWindow* window = [UIWindow new];

    SCValdiDefaultNavigator* navigator = [[SCValdiDefaultNavigator alloc] initWithRuntime:runtime];

    SCValdiBootstrappingDependencies* dependencies = [SCValdiBootstrappingDependencies new];
    dependencies.navigator = navigator;
    dependencies.runtime = runtime;
    UIView<SCValdiRootViewProtocol>* rootView = [self createRootViewWithDependencies:dependencies];

    SCValdiDefaultContainerViewController* viewController =
        [[SCValdiDefaultContainerViewController alloc] initWithValdiView:rootView];
    navigator.managedViewController = viewController;

    UINavigationController* navigationController =
        [[UINavigationController alloc] initWithRootViewController:viewController];
    navigationController.navigationBar.translucent = NO;

    window.rootViewController = navigationController;
    [window makeKeyAndVisible];

    _rootWindow = window;

    return YES;
}

- (NSString*)rootValdiComponentPath
{
    [NSException raise:NSInternalInconsistencyException format:@"rootValdiComponentPath unimplemented"];
    return nil;
}

- (UIView<SCValdiRootViewProtocol>*)createRootViewWithDependencies:(SCValdiBootstrappingDependencies*)dependencies
{
    NSString* componentPath = [self rootValdiComponentPath];

    return [[SCValdiComponentContainerView alloc] initWithComponentPath:componentPath
                                                                     owner:nil
                                                                 viewModel:nil
                                                          componentContext:@{@"navigator": dependencies.navigator}
                                                                runtime:dependencies.runtime];
}

@end
