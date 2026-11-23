#import "valdi/ios/Bootstrap/SCValdiBootstrappingAppDelegate.h"
#import "valdi/ios/SCValdiRuntimeManager.h"
#import "valdi_core/SCValdiComponentContainerView.h"
#import "valdi_core/SCValdiDefaultContainerViewController.h"
#import "valdi_core/SCValdiDefaultNavigator.h"

@implementation SCValdiBootstrappingDependencies
@end

@interface SCValdiBootstrappingAppDelegate ()

- (UIViewController*)createRootViewController;

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

    // With a scene manifest, UIKit ignores a window made here; SCValdiBootstrappingSceneDelegate makes it instead.
    if ([NSBundle.mainBundle objectForInfoDictionaryKey:@"UIApplicationSceneManifest"] != nil) {
        return YES;
    }

    UIWindow* window = [UIWindow new];
    window.rootViewController = [self createRootViewController];
    [window makeKeyAndVisible];

    _rootWindow = window;

    return YES;
}

- (UIViewController*)createRootViewController
{
    id<SCValdiRuntimeProtocol> runtime = _runtimeManager.mainRuntime;

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

    return navigationController;
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

@implementation SCValdiBootstrappingSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions
{
    if (![scene isKindOfClass:[UIWindowScene class]]) {
        return;
    }
    id<UIApplicationDelegate> applicationDelegate = UIApplication.sharedApplication.delegate;
    if (![applicationDelegate isKindOfClass:[SCValdiBootstrappingAppDelegate class]]) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"SCValdiBootstrappingSceneDelegate requires an SCValdiBootstrappingAppDelegate"];
        return;
    }

    UIWindow* window = [[UIWindow alloc] initWithWindowScene:(UIWindowScene*)scene];
    window.rootViewController =
        [(SCValdiBootstrappingAppDelegate*)applicationDelegate createRootViewController];
    [window makeKeyAndVisible];

    self.window = window;
}

@end
