//
//  SCValdiConfiguration.h
//  valdi-ios
//
//  Created by Simon Corsin on 5/8/20.
//

#import <Foundation/Foundation.h>

#import "valdi_core/SCNValdiCoreHTTPRequestManager.h"
#import "valdi_core/SCNValdiCoreJavaScriptEngineType.h"
#import "valdi_core/SCValdiCustomModuleProvider.h"
#import "valdi_core/SCValdiDebugMessageDisplayer.h"
#import "valdi_core/SCValdiExceptionReporter.h"
#import "valdi_core/SCValdiFontLoaderProtocol.h"
#import "valdi_core/SCValdiImageLoader.h"
#import "valdi_core/SCValdiVideoLoader.h"

NS_ASSUME_NONNULL_BEGIN

typedef void (^SCValdiPerformHapticFeedbackBlock)(NSString* type);

@interface SCValdiConfiguration : NSObject

@property (copy, nonatomic, nullable) NSArray<id<SCValdiImageLoader>>* imageLoaders;
@property (copy, nonatomic, nullable) NSArray<id<SCValdiVideoLoader>>* videoLoaders;
@property (strong, nonatomic, nullable) id<SCNValdiCoreHTTPRequestManager> requestManager;
@property (strong, nonatomic, nullable) id<SCValdiDebugMessageDisplayer> debugMessageDisplayer;
@property (strong, nonatomic, nullable) id<SCValdiExceptionReporter> exceptionReporter;
@property (strong, nonatomic, nullable) id<SCValdiFontLoaderProtocol> fontLoader;
@property (strong, nonatomic, nullable) id<SCValdiCustomModuleProvider> customModuleProvider;

@property (copy, nonatomic, nullable) NSString* userId;

@property (copy, nonatomic, nullable) SCValdiPerformHapticFeedbackBlock performHapticFeedbackBlock;

// This is controlled by the iOS App Platform dark mode tweak
@property (assign, nonatomic) BOOL allowDarkMode;

// Use the root containing view controller's userInterfaceStyle instead of getting it from UIScreen.mainScreen
@property (assign, nonatomic) BOOL useViewControllerBasedUserInterfaceStyleForDarkMode;

@property (assign, nonatomic) BOOL disableLegacyMeasureBehaviorByDefault;

/**
 By default, the Garbage Collector of the JavaScriptCore JS engine detects whether
 JS objects are used by looking at the native stack of each thread, and mark any
 JS pointers within the stack as reachable. If this detection does not work properly,
 this can result in objects being incorrectly freed and cause undefined behaviors. This
 flag enforces that the JS objects used within the Valdi C++ runtime are protected,
 even if they are seemingly reachable from the stack. This comes at a performance cost
 when marshalling JS objects.
 */
@property (assign, nonatomic) BOOL disableGcStackUsageDetection;

/**
 Whether emitted Objective-C references should be tracked within the SCValdiContext's Objective-C instance.
 When enabled, this allows memory leak detectors to track Objective-C retain cycles.
 This has a small performance cost, and should ideally only be enabled on non release builds.
 */
@property (assign, nonatomic) BOOL enableReferenceTracking;

/**
 * The currently selected JavaScript engine type.
 * In production, iOS uses the JavaScriptCore engine and Android uses QuickJS.
 * In non-production builds, this is controlled by a tweak
 * NOTE(1923): JS engine can't be changed once RuntimeManager was initialized.
 */
@property (assign, nonatomic) SCNValdiCoreJavaScriptEngineType javaScriptEngineType;

// Controls the result of Application.isTestEnvironment() Javascript call
@property (assign, nonatomic) BOOL isTestEnvironment;

/**
 * ANR (Application Not Responding) detector timeout in milliseconds for the JS thread.
 * When greater than 0, the JavaScript ANR detector is started with this threshold.
 * When 0 (default), the ANR detector is not started.
 */
@property (assign, nonatomic) NSInteger anrTimeoutMs;

@end

NS_ASSUME_NONNULL_END
