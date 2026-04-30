//
//  SCValdiMacOSViewManagerTests.mm
//  valdi-macos
//
//  Unit tests for MacOS ViewManager: createViewFactory and supportsClassNameNatively
//  (polyglot <custom-view> support via getEffectiveClassName + NSClassFromString).
//

#import <AppKit/AppKit.h>
#import <XCTest/XCTest.h>
#import "valdi/macos/SCValdiMacOSViewManager.h"
#import "valdi/macos/SCValdiObjCUtils.h"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi/runtime/Attributes/BoundAttributes.hpp"

using namespace ValdiMacOS;
using namespace Valdi;

@interface SCValdiMacOSViewManagerTests : XCTestCase
@property (nonatomic, assign) ViewManager* viewManager;
@end

@implementation SCValdiMacOSViewManagerTests

- (void)setUp {
    [super setUp];
    self.viewManager = new ViewManager();
}

- (void)tearDown {
    delete self.viewManager;
    self.viewManager = nullptr;
    [super tearDown];
}

- (void)testSupportsClassNameNatively_mappedTextField {
    // SCValdiTextField is mapped to SCValdiMacOSTextField (linked in valdi_macos).
    StringBox className = STRING_LITERAL("SCValdiTextField");
    BOOL supported = self.viewManager->supportsClassNameNatively(className);
    XCTAssertTrue(supported, @"SCValdiTextField should be supported (mapped to SCValdiMacOSTextField)");
}

- (void)testSupportsClassNameNatively_systemNSView {
    // Any resolvable class name is supported after the getEffectiveClassName fix.
    StringBox className = STRING_LITERAL("NSView");
    BOOL supported = self.viewManager->supportsClassNameNatively(className);
    XCTAssertTrue(supported, @"NSView should be supported (resolvable via NSClassFromString)");
}

- (void)testSupportsClassNameNatively_unknownClass {
    StringBox className = STRING_LITERAL("NonExistentClassXYZ123");
    BOOL supported = self.viewManager->supportsClassNameNatively(className);
    XCTAssertFalse(supported, @"Unknown class should not be supported");
}

- (void)testCreateViewFactory_mappedTextField {
    StringBox className = STRING_LITERAL("SCValdiTextField");
    auto boundAttributes = Valdi::Ref<Valdi::BoundAttributes>();
    auto factory = self.viewManager->createViewFactory(className, boundAttributes);
    XCTAssertTrue(factory != nullptr, @"createViewFactory(SCValdiTextField) should return non-null");
}

- (void)testCreateViewFactory_resolvableClass {
    StringBox className = STRING_LITERAL("NSView");
    auto boundAttributes = Valdi::Ref<Valdi::BoundAttributes>();
    auto factory = self.viewManager->createViewFactory(className, boundAttributes);
    XCTAssertTrue(factory != nullptr, @"createViewFactory(NSView) should return non-null for resolvable class");
}

- (void)testCreateViewFactory_unknownClass {
    StringBox className = STRING_LITERAL("NonExistentClassXYZ123");
    auto boundAttributes = Valdi::Ref<Valdi::BoundAttributes>();
    auto factory = self.viewManager->createViewFactory(className, boundAttributes);
    XCTAssertTrue(factory == nullptr, @"createViewFactory(unknown) should return null");
}

- (void)testGetPlatformType {
    Valdi::PlatformType type = self.viewManager->getPlatformType();
    XCTAssertEqual(type, Valdi::PlatformTypeMacOS, @"MacOS ViewManager reports PlatformTypeMacOS");
}

@end
