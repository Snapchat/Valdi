//
//  SCValdiDeviceModule.m
//  Valdi
//
//  Created by Simon Corsin on 9/6/18.
//

#import "SCValdiDeviceModule.h"

#import "SCValdiBridgeModuleUtils.h"

#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiRootView.h"

#import <UIKit/UIKit.h>
#import <libkern/OSAtomic.h>

@interface SCValdiDeviceModule () <SCValdiBridgeObserverDelegate>

@end

@implementation SCValdiDeviceModule {
    // Those fields are non mutable
    NSString *_systemVersion;
    NSString *_model;
    CGFloat _displayWidth;
    CGFloat _displayHeight;
    CGFloat _displayScale;

    // This field is mutable
    UIEdgeInsets _insets;
    UITraitCollection *_traitCollection;

    SCValdiBridgeObserver *_displayInsetsObserver;
    SCValdiBridgeObserver *_darkModeObserver;

    __weak id<SCValdiJSQueueDispatcher> _jsQueueDispatcher;
    dispatch_group_t _deviceSettingsReadyGroup;

    NSLocale *_locale;
    BOOL _allowDarkMode;
    BOOL _useScreenUserInterfaceStyleForDarkMode;
    BOOL _deviceSettingsReady;
    int _scheduledEnsureDeviceModuleIsReady;
}

- (instancetype)initWithJSQueueDispatcher:(id<SCValdiJSQueueDispatcher>)jsQueueDispatcher
{
    self = [super init];

    if (self) {
        _jsQueueDispatcher = jsQueueDispatcher;
        _displayInsetsObserver = [SCValdiBridgeObserver new];
        _darkModeObserver = [SCValdiBridgeObserver new];
        _darkModeObserver.delegate = self;
        _locale = [NSLocale autoupdatingCurrentLocale];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(notifyDisplayInsetChanged)
                                                     name:UIApplicationDidChangeStatusBarFrameNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(notifyDisplayInsetChanged)
                                                     name:SCValdiRootViewDisplayInsetDidChangeNotificationKey
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(_handleTraitCollectionDidChange:)
                                                     name:SCValdiRootViewTraitCollectionDidChangeNotificationKey
                                                   object:nil];

     [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(_handleOrientationDidChange:)
                                                 name:UIDeviceOrientationDidChangeNotification
                                               object:nil];

        _deviceSettingsReadyGroup = dispatch_group_create();
        dispatch_group_enter(_deviceSettingsReadyGroup);

        if (NSThread.isMainThread) {
            [self _updateDeviceSettingsIfNeeded];
        } else {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self _updateDeviceSettingsIfNeeded];
            });
        }
    }

    return self;
}

- (void)_updateDeviceSettingsIfNeeded
{
    if (_deviceSettingsReady) {
        return;
    }

    UIDevice *device = UIDevice.currentDevice;
    _systemVersion = device.systemVersion;
    _model = device.model;

    UIScreen *screen = UIScreen.mainScreen;
    _displayWidth = screen.bounds.size.width;
    _displayHeight = screen.bounds.size.height;
    _displayScale = screen.scale;
    _insets = [self _currentInsets];

    _deviceSettingsReady = YES;
    [self _updateTraitCollection:[screen.traitCollection copy] shouldNotify:NO];
    dispatch_group_leave(_deviceSettingsReadyGroup);
}

- (void)performHapticFeedback:(SCValdiMarshaller *)marshaller
{
    id<SCValdiFunction> function = self.performHapticFeedbackFunction;
    if (!function) {
        SCValdiMarshallerPushUndefined(marshaller);
        return;
    }
    [function performWithMarshaller:marshaller];
}

- (UIEdgeInsets)_currentInsets
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    UIEdgeInsets keyWindowInsets = UIApplication.sharedApplication.keyWindow.safeAreaInsets;
#pragma clang diagnostic pop
    SCLogValdiDebug(@"SCValdiDeviceModule: [legacy] insets from safeAreaInsets (%0.1f,%0.1f,%0.1f,%0.1f)",
                    keyWindowInsets.top, keyWindowInsets.left, keyWindowInsets.bottom, keyWindowInsets.right);
    if (UIEdgeInsetsEqualToEdgeInsets(keyWindowInsets, UIEdgeInsetsZero)) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGFloat statusBarHeight = UIApplication.sharedApplication.statusBarFrame.size.height;
#pragma clang diagnostic pop
        SCLogValdiDebug(@"SCValdiDeviceModule: [legacy] using status bar fallback height=%0.1f", statusBarHeight);
        return UIEdgeInsetsMake(statusBarHeight, 0, 0, 0);
    }
    return keyWindowInsets;
}

- (void)_dispatchOnJsQueue:(dispatch_block_t)block
{
    if (_jsQueueDispatcher) {
        [_jsQueueDispatcher dispatchOnJSQueueWithBlock:block sync:NO];
    } else {
        block();
    }
}


- (void)_updateDisplayInsetsAndNotify:(BOOL)notify
{
    UIEdgeInsets insets = [self _currentInsets];


    [self _dispatchOnJsQueue:^{
        if (!UIEdgeInsetsEqualToEdgeInsets(insets, self->_insets)) {
            SCLogValdiDebug(@"SCValdiDeviceModule: insets changed from (%0.1f,%0.1f,%0.1f,%0.1f) to (%0.1f,%0.1f,%0.1f,%0.1f), notify=%@",
                              self->_insets.top, self->_insets.left, self->_insets.bottom, self->_insets.right,
                              insets.top, insets.left, insets.bottom, insets.right,
                              notify ? @"YES" : @"NO");
            self->_insets = insets;

            if (notify) {
                [self->_displayInsetsObserver notifyWithMarshaller:nil];
            }
        } else {
            SCLogValdiDebug(@"SCValdiDeviceModule: insets unchanged (%0.1f,%0.1f,%0.1f,%0.1f), notify=%@",
                              insets.top, insets.left, insets.bottom, insets.right,
                              notify ? @"YES" : @"NO");
        }
    }];
}

- (void)_handleTraitCollectionDidChange:(NSNotification *)notification
{
    UITraitCollection *traitCollection;
    if (_useScreenUserInterfaceStyleForDarkMode) {
        traitCollection = UIScreen.mainScreen.traitCollection;
    } else {
        traitCollection = ((UIView *)notification.object).traitCollection;
    }
    [self _updateTraitCollection:traitCollection shouldNotify:YES];
}

- (void)_handleOrientationDidChange:(NSNotification *)notification
{
    // screen bounds change after the notification event so we have to post back to the main queue
    dispatch_async(dispatch_get_main_queue(), ^{
        UIScreen *screen = UIScreen.mainScreen;
        _displayWidth = screen.bounds.size.width;
        _displayHeight = screen.bounds.size.height;
        _displayScale = screen.scale;
        [self _dispatchOnJsQueue:^{
            [self->_displayInsetsObserver notifyWithMarshaller:nil];
        }];
    });
}

- (void)ensureDeviceModuleIsReadyForContextCreation
{
    if (![NSThread isMainThread]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (OSAtomicCompareAndSwapInt(0, 1, &_scheduledEnsureDeviceModuleIsReady)) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self ensureDeviceModuleIsReadyForContextCreation];
            });
        }
#pragma clang diagnostic pop

        return;
    }

    [self _updateDeviceSettingsIfNeeded];
    [self _pollTraitCollectionIfNecessary];
    _scheduledEnsureDeviceModuleIsReady = 0;
}

- (void)_pollTraitCollectionIfNecessary
{
    if (!_useScreenUserInterfaceStyleForDarkMode) {
        // In this case we rely on the view controller to update us when the trait collection changes
        return;
    }
    [self _updateTraitCollection:UIScreen.mainScreen.traitCollection shouldNotify:YES];
}

- (void)_updateTraitCollection:(UITraitCollection *)traitCollection shouldNotify:(BOOL)shouldNotify
{
    if (!traitCollection) {
        return;
    }
    if ([_traitCollection isEqual:traitCollection]) {
        return;
    }
    _traitCollection = [traitCollection copy];
    if (shouldNotify) {
        [self notifyJSDarkModeChanged];
    }
}

- (void)setAllowDarkMode:(BOOL)allowDarkMode
    useScreenUserInterfaceStyleForDarkMode:(BOOL)useScreenUserInterfaceStyleForDarkMode
{
    @synchronized (self) {
        _allowDarkMode = allowDarkMode;
        _useScreenUserInterfaceStyleForDarkMode = useScreenUserInterfaceStyleForDarkMode;
    }
    [self notifyJSDarkModeChanged];
}

- (BOOL)allowDarkMode
{
    @synchronized (self) {
        return _allowDarkMode;
    }
}

- (BOOL)useScreenUserInterfaceStyleForDarkMode
{
    @synchronized (self) {
        return _useScreenUserInterfaceStyleForDarkMode;
    }
}

- (void)notifyJSDarkModeChanged
{
    if (!self.allowDarkMode) {
        return;
    }

    if (@available(iOS 12.0, *)) {
        UIUserInterfaceStyle userInterfaceStyle;

        if (_traitCollection) {
            userInterfaceStyle = _traitCollection.userInterfaceStyle;
        } else if (_useScreenUserInterfaceStyleForDarkMode) {
            userInterfaceStyle = UIScreen.mainScreen.traitCollection.userInterfaceStyle;
        } else {
            return;
        }

        BOOL isDarkMode = userInterfaceStyle == UIUserInterfaceStyleDark;

        [self _dispatchOnJsQueue:^{
            SCValdiMarshallerScoped(marshaller, {
                SCValdiMarshallerPushBool(marshaller, isDarkMode);
                [self->_darkModeObserver notifyWithMarshaller:marshaller];
            });
        }];
    }
}

- (void)notifyDisplayInsetChanged
{
    [self _updateDisplayInsetsAndNotify:YES];
}

- (void)systemType:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushString(marshaller, @"ios");
}

- (void)systemVersion:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushString(marshaller, _systemVersion);
}

- (void)model:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushString(marshaller, _model);
}

- (void)copyToClipBoard:(SCValdiMarshaller *)marshaller
{
    NSString *content = SCValdiMarshallerGetString(marshaller, 0);
    dispatch_async(dispatch_get_main_queue(), ^{
        UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
        pasteboard.string = content;
      });
}

- (void)deviceLocales:(SCValdiMarshaller *)marshaller
{
    NSArray<NSString *>* languages = [NSLocale preferredLanguages];
    SCValdiMarshallerMarshallArray(marshaller, languages, ^(NSString *languageCode) {
        SCValdiMarshallerPushString(marshaller, languageCode);
    });
}

- (void)displayWidth:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _displayWidth);
}

- (void)displayHeight:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _displayHeight);
}

- (void)displayScale:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _displayScale);
}

- (void)displayLeftInset:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _insets.left);
}

- (void)displayTopInset:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _insets.top);
}

- (void)displayRightInset:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _insets.right);
}

- (void)displayBottomInset:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, _insets.bottom);
}

- (void)localeUsesMetricSystem:(SCValdiMarshaller *)marshaller
{
    BOOL usesMetricSystem = _locale.usesMetricSystem;
    SCValdiMarshallerPushBool(marshaller, usesMetricSystem);
}

- (void)timeZoneName:(SCValdiMarshaller *)marshaller
{
    NSTimeZone *timeZone = [NSTimeZone localTimeZone];
    SCValdiMarshallerPushString(marshaller, timeZone.name);
}

- (NSTimeZone *)_timeZoneFromMarshaller:(SCValdiMarshaller *)marshaller
{
    NSTimeZone *timeZone;
    if (SCValdiMarshallerIsString(marshaller, 0)) {
        NSString *timeZoneName = SCValdiMarshallerGetString(marshaller, 0);
        timeZone = [NSTimeZone timeZoneWithName:timeZoneName];
    } else {
        // Once we drop iOS 10 we can cache a reference to [NSTimeZone local] and
        // keep that around since it returns a proxy object that always reflects the
        // device's current time zone.
        //
        // For now - just query the system every time.
        timeZone = [NSTimeZone localTimeZone];
    }
    return timeZone;
}

- (void)timeZoneRawSecondsFromGMT:(SCValdiMarshaller *)marshaller
{
    NSTimeZone *timeZone = [self _timeZoneFromMarshaller:marshaller];
    SCValdiMarshallerPushDouble(marshaller, timeZone.secondsFromGMT - timeZone.daylightSavingTimeOffset);
}

- (void)timeZoneDstSecondsFromGMT:(SCValdiMarshaller *)marshaller
{
    NSTimeZone *timeZone = [self _timeZoneFromMarshaller:marshaller];
    SCValdiMarshallerPushDouble(marshaller, timeZone.secondsFromGMT);
}

- (void)uptimeMs:(SCValdiMarshaller *)marshaller
{
    SCValdiMarshallerPushDouble(marshaller, CACurrentMediaTime() * 1000);
}

- (NSString *)getModulePath
{
    return @"valdi_core/src/DeviceBridge";
}

- (NSObject *)loadModule
{
    // Synchronously wait until the settings have been loaded, so that we don't provide incorrect cached
    // values to TypeScript
    dispatch_group_wait(_deviceSettingsReadyGroup, DISPATCH_TIME_FOREVER);

    return @{
        @"getDeviceLocales" : BRIDGE_METHOD(deviceLocales),
        @"getDisplayWidth" : BRIDGE_METHOD(displayWidth),
        @"getDisplayHeight" : BRIDGE_METHOD(displayHeight),
        @"getDisplayScale" : BRIDGE_METHOD(displayScale),
        @"getWindowWidth" : BRIDGE_METHOD(displayWidth),
        @"getWindowHeight" : BRIDGE_METHOD(displayHeight),
        @"getDisplayLeftInset" : BRIDGE_METHOD(displayLeftInset),
        @"getDisplayRightInset" : BRIDGE_METHOD(displayRightInset),
        @"getDisplayBottomInset" : BRIDGE_METHOD(displayBottomInset),
        @"getDisplayTopInset" : BRIDGE_METHOD(displayTopInset),
        @"getModel" : BRIDGE_METHOD(model),
        @"getSystemType" : BRIDGE_METHOD(systemType),
        @"copyToClipBoard" : BRIDGE_METHOD(copyToClipBoard),
        @"getSystemVersion" : BRIDGE_METHOD(systemVersion),
        @"observeDisplayInsetChange" : _displayInsetsObserver,
        @"observeDarkMode" : _darkModeObserver,
        @"performHapticFeedback" : BRIDGE_METHOD(performHapticFeedback),
        @"getLocaleUsesMetricSystem" : BRIDGE_METHOD(localeUsesMetricSystem),
        @"getTimeZoneName" : BRIDGE_METHOD(timeZoneName),
        @"getTimeZoneRawSecondsFromGMT" : BRIDGE_METHOD(timeZoneRawSecondsFromGMT),
        @"getTimeZoneDstSecondsFromGMT" : BRIDGE_METHOD(timeZoneDstSecondsFromGMT),
        @"getUptimeMs" : BRIDGE_METHOD(uptimeMs),
    };
}

- (void)bridgeObserverDidAddNewCallback:(SCValdiBridgeObserver *)observer
{
    if (observer == _darkModeObserver) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self notifyJSDarkModeChanged];
        });
    }
}

@end
