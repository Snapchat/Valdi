//
//  SCValdiDesktopModuleInitTests.mm
//  valdi-macos
//
//  Unit tests for the desktop module init C API (ValdiRegisterDesktopModuleInit,
//  ValdiRunDesktopModuleInits, context provider, file picker handler).
//

#import <XCTest/XCTest.h>

#import "valdi/standalone_desktop/NativeModules.h"
#import "valdi/standalone_desktop/ValdiDesktopModuleInit.h"

static int g_initCallCount = 0;
static void TestModuleInitFn(ValdiDesktopRuntimeHandle runtime) {
    (void)runtime;
    g_initCallCount++;
}

static NSObject *g_returnedContextObject = nil;
static void* g_returnedContext = nullptr;
static void* TestContextProviderFn(ValdiDesktopRuntimeHandle runtime) {
    (void)runtime;
    return g_returnedContext;
}

static void* g_filePickerCallbackContext = nullptr;
static void TestFilePickerCallback(void* context, void* result) {
    g_filePickerCallbackContext = context;
    (void)result;
}

static BOOL g_filePickerHandlerCalled = NO;
static void TestFilePickerHandler(void* context, ValdiDesktopRequestResultCallback resultCallback) {
    g_filePickerHandlerCalled = YES;
    if (resultCallback) {
        resultCallback(context, NULL);
    }
}

static BOOL g_filePickerCallbackInvokedWhenNoHandler = NO;
static void TestFilePickerCallbackNoHandler(void* context, void* result) {
    (void)context;
    (void)result;
    g_filePickerCallbackInvokedWhenNoHandler = YES;
}

@interface SCValdiDesktopModuleInitTests : XCTestCase
@end

@implementation SCValdiDesktopModuleInitTests

- (void)setUp {
    [super setUp];
    g_initCallCount = 0;
    g_returnedContextObject = [NSObject new];
    g_returnedContext = (__bridge void*)g_returnedContextObject;
    g_filePickerCallbackContext = nullptr;
    g_filePickerHandlerCalled = NO;
    g_filePickerCallbackInvokedWhenNoHandler = NO;
}

- (void)testRegisterAndRunModuleInit {
    ValdiRegisterDesktopModuleInit(TestModuleInitFn);
    XCTAssertEqual(g_initCallCount, 0, @"Init should not run until ValdiRunDesktopModuleInits");
    ValdiRunDesktopModuleInits(nullptr);
    XCTAssertEqual(g_initCallCount, 1, @"Registered init should run once");
    ValdiRunDesktopModuleInits(nullptr);
    XCTAssertEqual(g_initCallCount, 2, @"Inits run again when called again");
}

- (void)testSetComponentContextProviderFn_andGetContext {
    ValdiSetDesktopComponentContextProviderFn(TestContextProviderFn);
    id context = SCValdiGetDesktopComponentContext(nil);
    XCTAssertEqual((__bridge void*)context, g_returnedContext, @"GetDesktopComponentContext should return what provider fn returns");
}

- (void)testRequestHandler_invokesCallbackWhenSet {
    ValdiDesktopRegisterRequestHandler("filePicker", TestFilePickerHandler);
    ValdiDesktopInvokeRequest("filePicker", (void*)0x1234, TestFilePickerCallback);
    XCTAssertTrue(g_filePickerHandlerCalled, @"Handler should be called");
    XCTAssertEqual(g_filePickerCallbackContext, (void*)0x1234, @"Handler should invoke callback with same context");
}

- (void)testRequestHandler_noOpWhenHandlerNotSet {
    ValdiDesktopInvokeRequest("filePickerNoHandler", nullptr, TestFilePickerCallbackNoHandler);
    XCTAssertFalse(g_filePickerCallbackInvokedWhenNoHandler, @"Callback should not be invoked when no handler set");
}

- (void)testRequestHandler_noOpWhenCallbackNull {
    ValdiDesktopRegisterRequestHandler("filePicker", TestFilePickerHandler);
    g_filePickerHandlerCalled = NO;
    ValdiDesktopInvokeRequest("filePicker", nullptr, nullptr);
    XCTAssertFalse(g_filePickerHandlerCalled, @"Handler should not be called when resultCallback is null");
}

@end
