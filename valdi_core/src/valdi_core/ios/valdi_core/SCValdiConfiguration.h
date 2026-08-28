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
 Killswitch for including each line's font `leading` when measuring text
 (SCValdiTextLayout measureSizeWithMaxSize:), which keeps measurement in agreement with the
 TextKit stack the views render with. Defaults to NO (leading included). Set to YES to restore
 the legacy NSStringDrawing measurement that excludes it, which under-measures fonts with
 nonzero leading and can bottom-clip container-sized text.
 */
@property (assign, nonatomic) BOOL disableFontLeadingInTextMeasure;

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
 * Local-development-only debugger and hot-reload settings.
 *
 * These values are read when SCValdiRuntimeManager initializes its underlying
 * RuntimeManager. Configure them through -[SCValdiRuntimeManager updateConfiguration:]
 * before first accessing APIs that initialize the runtime, such as mainRuntime.
 * The runtime still applies its existing compile-time debugger-service gate,
 * so setting enableDebuggerService does not broaden release-build availability.
 * Debugger service requests are enabled by default for compatibility with direct
 * development hosts; hosts can set enableDebuggerService to NO to opt out.
 */
@property (assign, nonatomic) BOOL enableDebuggerService;
@property (assign, nonatomic) BOOL disableHotReloader;

/**
 * Optional port for the debugger / hot-reload service.
 *
 * Leave this as 0 to use VALDI_DEBUGGER_SERVICE_PORT when present, or Valdi's platform
 * default when it is not. Valid explicit ports are in the range 1...65535.
 *
 * `valdi hotreload --port` currently targets simulator / localhost reloads.
 * Physical-device USB auto-connectors still use Valdi's default mobile port.
 */
@property (assign, nonatomic) NSInteger debuggerServicePort;

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
