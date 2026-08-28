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
#import "valdi/macos/SCValdiMacOSFunction.h"
#import "valdi/macos/Views/SCValdiMacOSTextField.h"
#import "valdi/macos/SCValdiObjCUtils.h"
#include "valdi/runtime/Attributes/AttributeIds.hpp"
#include "valdi_core/cpp/Attributes/ColorPalette.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi/runtime/Interfaces/IViewTransaction.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/DeferredViewTransaction.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"

#include <vector>

using namespace ValdiMacOS;
using namespace Valdi;

@interface NSView (SCValdiAccessibilityTests)
- (BOOL)valdi_hasAttachedViewNode;
- (BOOL)valdi_hasAttachedViewNodeHandle;
- (BOOL)valdi_isAttachedToViewNode:(Valdi::ViewNode *)viewNode;
- (void)valdi_didChangeValue:(id)value forAttribute:(NSString *)attributeName;
- (void)valdi_setAccessibilityCategory:(nullable NSString *)category;
- (void)valdi_setAccessibilityStateDisabled:(nullable NSNumber *)disabled;
- (void)valdi_setAccessibilityStateSelected:(nullable NSNumber *)selected;
@end

namespace {

class QueuedMainThreadDispatcher final : public IMainThreadDispatcher {
public:
    ~QueuedMainThreadDispatcher() override {
        for (auto *function : _pendingFunctions) {
            delete function;
        }
    }

    void dispatch(DispatchFunction *function, bool sync) override {
        if (sync) {
            (*function)();
            delete function;
        } else {
            _pendingFunctions.emplace_back(function);
        }
    }

    size_t pendingCount() const {
        return _pendingFunctions.size();
    }

    void runNext() {
        auto *function = _pendingFunctions.front();
        _pendingFunctions.erase(_pendingFunctions.begin());
        (*function)();
        delete function;
    }

private:
    std::vector<DispatchFunction *> _pendingFunctions;
};

static Ref<ViewNode> makeTestViewNode(AttributeIds& attributeIds) {
    return makeShared<ViewNode>(nullptr, attributeIds, nullptr, ConsoleLogger::getLogger());
}

} // namespace

@interface SCValdiMacOSTextField (SCValdiMacOSTextFieldTests)
- (NSDictionary *)_editTextEvent;
- (NSDictionary *)_editTextEndEvent;
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector;
- (void)valdi_setFocused:(nullable NSNumber *)focused;
- (void)valdi_setSelection:(nullable NSArray<NSNumber *> *)selection;
- (void)valdi_setOnChange:(nullable SCValdiMacOSFunction *)onChange;
- (void)valdi_setOnWillChange:(nullable SCValdiMacOSFunction *)onWillChange;
@end

@interface SCValdiTrackingMacOSTextField : SCValdiMacOSTextField
@property (nonatomic, readonly) NSMutableDictionary<NSString *, id> *changedValues;
@property (nullable, nonatomic, strong) NSText *fieldEditorOverride;
@end

@implementation SCValdiTrackingMacOSTextField

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        _changedValues = [NSMutableDictionary new];
    }
    return self;
}

- (void)valdi_didChangeValue:(id)value forAttribute:(NSString *)attributeName
{
    self.changedValues[attributeName] = value;
}

- (NSText *)currentEditor
{
    return self.fieldEditorOverride ?: [super currentEditor];
}

@end

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

- (void)testAccessibilityCategoryExposesSemanticRoleAndRestoresNativeRole {
    SCValdiMacOSTextField *textField = [[SCValdiMacOSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    NSAccessibilityRole originalRole = textField.accessibilityRole;

    [textField valdi_setAccessibilityCategory:@"button"];

    XCTAssertTrue(textField.isAccessibilityElement);
    XCTAssertEqualObjects(textField.accessibilityRole, NSAccessibilityButtonRole);

    [textField valdi_setAccessibilityCategory:@"auto"];

    XCTAssertEqualObjects(textField.accessibilityRole, originalRole);
}

- (void)testAccessibilityStateMapsToNativeEnabledAndSelectedState {
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    BOOL originalAccessibilityEnabled = view.isAccessibilityEnabled;

    [view valdi_setAccessibilityStateDisabled:@YES];
    [view valdi_setAccessibilityStateSelected:@YES];

    XCTAssertFalse(view.isAccessibilityEnabled);
    XCTAssertTrue(view.isAccessibilitySelected);

    [view valdi_setAccessibilityStateDisabled:nil];
    [view valdi_setAccessibilityStateSelected:nil];

    XCTAssertEqual(view.isAccessibilityEnabled, originalAccessibilityEnabled);
    XCTAssertFalse(view.isAccessibilitySelected);
}

- (void)testRemovingAccessibilityDisabledPreservesDisabledControlState {
    NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    button.enabled = NO;

    [button valdi_setAccessibilityStateDisabled:@YES];
    [button valdi_setAccessibilityStateDisabled:nil];

    XCTAssertFalse(button.isAccessibilityEnabled);

    button.enabled = YES;
    [button valdi_setAccessibilityStateDisabled:@YES];
    button.enabled = YES;
    [button valdi_setAccessibilityStateDisabled:nil];

    XCTAssertTrue(button.isAccessibilityEnabled);
}

- (void)testRetainedRootNativeViewDoesNotKeepViewNodeAlive {
    auto factory = self.viewManager->createViewFactory(STRING_LITERAL("NSView"), nullptr);
    auto view = factory->createView(nullptr, nullptr, false);
    NSView *nativeView = fromValdiView(view);
    AttributeIds attributeIds;
    auto viewNode = makeTestViewNode(attributeIds);
    auto weakViewNode = weakRef(viewNode.get());
    auto transaction = self.viewManager->createViewTransaction(nullptr, false);

    transaction->moveViewToTree(view, nullptr, viewNode.get());
    XCTAssertTrue([nativeView valdi_isAttachedToViewNode:viewNode.get()]);

    viewNode = nullptr;

    XCTAssertTrue(weakViewNode.expired());
    XCTAssertTrue([nativeView valdi_hasAttachedViewNodeHandle]);
    XCTAssertFalse([nativeView valdi_hasAttachedViewNode]);
    [nativeView valdi_didChangeValue:@"ignored" forAttribute:@"value"];
}

- (void)testDestroyingDetachedViewWrapperClearsNativeOverrideHandle {
    auto factory = self.viewManager->createViewFactory(STRING_LITERAL("NSView"), nullptr);
    auto view = factory->createView(nullptr, nullptr, false);
    NSView *nativeView = fromValdiView(view);
    AttributeIds attributeIds;
    auto viewNode = makeTestViewNode(attributeIds);
    auto transaction = self.viewManager->createViewTransaction(nullptr, false);

    transaction->moveViewToTree(view, nullptr, viewNode.get());
    XCTAssertTrue([nativeView valdi_isAttachedToViewNode:viewNode.get()]);

    view = nullptr;

    XCTAssertFalse([nativeView valdi_hasAttachedViewNodeHandle]);
    XCTAssertFalse([nativeView valdi_hasAttachedViewNode]);
}

- (void)testRemoveWithClearDropsNativeOverrideHandle {
    auto factory = self.viewManager->createViewFactory(STRING_LITERAL("NSView"), nullptr);
    auto view = factory->createView(nullptr, nullptr, false);
    NSView *nativeView = fromValdiView(view);
    AttributeIds attributeIds;
    auto viewNode = makeTestViewNode(attributeIds);
    auto transaction = self.viewManager->createViewTransaction(nullptr, false);

    transaction->moveViewToTree(view, nullptr, viewNode.get());
    transaction->removeViewFromParent(view, nullptr, true);

    XCTAssertFalse([nativeView valdi_hasAttachedViewNodeHandle]);
    XCTAssertFalse([nativeView valdi_hasAttachedViewNode]);
}

- (void)testDeferredMoveLeavesOnlyExpiredWeakHandleAfterOperationRuns {
    auto factory = self.viewManager->createViewFactory(STRING_LITERAL("NSView"), nullptr);
    auto view = factory->createView(nullptr, nullptr, false);
    NSView *nativeView = fromValdiView(view);
    AttributeIds attributeIds;
    auto viewNode = makeTestViewNode(attributeIds);
    auto weakViewNode = weakRef(viewNode.get());
    auto dispatcher = makeShared<QueuedMainThreadDispatcher>();
    auto mainThreadManager = makeShared<MainThreadManager>(dispatcher);
    auto transaction = makeShared<DeferredViewTransaction>(*self.viewManager, *mainThreadManager);

    transaction->moveViewToTree(view, nullptr, viewNode.get());
    transaction->flush(false);
    viewNode = nullptr;

    XCTAssertEqual(dispatcher->pendingCount(), 1u);
    XCTAssertFalse(weakViewNode.expired());
    XCTAssertFalse([nativeView valdi_hasAttachedViewNodeHandle]);

    dispatcher->runNext();

    XCTAssertTrue(weakViewNode.expired());
    XCTAssertTrue([nativeView valdi_hasAttachedViewNodeHandle]);
    XCTAssertFalse([nativeView valdi_hasAttachedViewNode]);
    [nativeView valdi_didChangeValue:@"ignored" forAttribute:@"value"];
}

- (void)testDeferredRemovalIsSafeWhenViewNodeDiesBeforeDelivery {
    auto factory = self.viewManager->createViewFactory(STRING_LITERAL("NSView"), nullptr);
    auto view = factory->createView(nullptr, nullptr, false);
    NSView *nativeView = fromValdiView(view);
    AttributeIds attributeIds;
    auto viewNode = makeTestViewNode(attributeIds);
    auto directTransaction = self.viewManager->createViewTransaction(nullptr, false);
    directTransaction->moveViewToTree(view, nullptr, viewNode.get());
    auto dispatcher = makeShared<QueuedMainThreadDispatcher>();
    auto mainThreadManager = makeShared<MainThreadManager>(dispatcher);
    auto deferredTransaction = makeShared<DeferredViewTransaction>(*self.viewManager, *mainThreadManager);

    deferredTransaction->removeViewFromParent(view, nullptr, true);
    deferredTransaction->flush(false);
    viewNode = nullptr;

    XCTAssertEqual(dispatcher->pendingCount(), 1u);
    XCTAssertTrue([nativeView valdi_hasAttachedViewNodeHandle]);
    XCTAssertFalse([nativeView valdi_hasAttachedViewNode]);
    [nativeView valdi_didChangeValue:@"ignored" forAttribute:@"value"];

    dispatcher->runNext();

    XCTAssertFalse([nativeView valdi_hasAttachedViewNodeHandle]);
}

- (void)testDeferredPoolEnqueueClearsNativeOverrideBeforeCallback {
    auto factory = self.viewManager->createViewFactory(STRING_LITERAL("NSView"), nullptr);
    auto view = factory->createView(nullptr, nullptr, false);
    NSView *nativeView = fromValdiView(view);
    AttributeIds attributeIds;
    auto viewNode = makeTestViewNode(attributeIds);
    auto directTransaction = self.viewManager->createViewTransaction(nullptr, false);
    directTransaction->moveViewToTree(view, nullptr, viewNode.get());
    auto dispatcher = makeShared<QueuedMainThreadDispatcher>();
    auto mainThreadManager = makeShared<MainThreadManager>(dispatcher);
    auto deferredTransaction = makeShared<DeferredViewTransaction>(*self.viewManager, *mainThreadManager);
    bool callbackCalled = false;
    bool handleWasCleared = false;

    deferredTransaction->willEnqueueViewToPool(view, [&](View&) {
        callbackCalled = true;
        handleWasCleared = ![nativeView valdi_hasAttachedViewNodeHandle];
    });
    deferredTransaction->flush(false);

    XCTAssertEqual(dispatcher->pendingCount(), 1u);
    XCTAssertTrue([nativeView valdi_isAttachedToViewNode:viewNode.get()]);

    dispatcher->runNext();

    XCTAssertTrue(callbackCalled);
    XCTAssertTrue(handleWasCleared);
    XCTAssertFalse([nativeView valdi_hasAttachedViewNodeHandle]);
}

- (void)testTextDidChangeAppliesWillChangeAndSynchronizesNativeOverrides {
    SCValdiTrackingMacOSTextField *textField =
        [[SCValdiTrackingMacOSTextField alloc] initWithFrame:NSMakeRect(10, 10, 200, 30)];
    textField.stringValue = @"seed";
    NSTextView *editor = [[NSTextView alloc] initWithFrame:NSMakeRect(10, 10, 200, 30)];
    editor.string = @"seed";
    textField.fieldEditorOverride = editor;

    __block NSDictionary *changeEvent = nil;
    __block NSArray<id> *willChangeParameters = nil;
    __block id willChangeEvent = nil;
    __block id willChangeText = nil;
    __block NSUInteger willChangeInvocationCount = 0;
    __block NSUInteger invalidWillChangeInvocationCount = 0;
    SCValdiMacOSFunction *onWillChange = [[SCValdiMacOSFunction alloc]
        initWithBlock:^id(NSArray<id> *parameters) {
            willChangeInvocationCount++;
            willChangeParameters = parameters;
            willChangeEvent = parameters.firstObject;
            if (![willChangeEvent isKindOfClass:NSDictionary.class]) {
                invalidWillChangeInvocationCount++;
                return nil;
            }
            willChangeText = ((NSDictionary *)willChangeEvent)[@"text"];
            if (![willChangeText isKindOfClass:NSString.class]) {
                invalidWillChangeInvocationCount++;
                return nil;
            }
            NSString *uppercaseText = [willChangeText uppercaseString];
            return @{
                @"text": uppercaseText,
                @"selectionStart": @(uppercaseText.length),
                @"selectionEnd": @(uppercaseText.length),
            };
        }];
    SCValdiMacOSFunction *onChange = [[SCValdiMacOSFunction alloc]
        initWithBlock:^id(NSArray<id> *parameters) {
            changeEvent = parameters.firstObject;
            return nil;
        }];
    [textField valdi_setOnWillChange:onWillChange];
    [textField valdi_setOnChange:onChange];

    [textField valdi_setSelection:@[@1, @1]];
    XCTAssertEqual(editor.selectedRange.location, 1u);
    XCTAssertEqual(editor.selectedRange.length, 0u);

    editor.string = @"draft";
    editor.selectedRange = NSMakeRange(5, 0);
    NSDictionary *sourceEvent = [textField _editTextEvent];
    XCTAssertEqualObjects(sourceEvent[@"text"], @"draft");
    XCTAssertTrue([sourceEvent[@"text"] isKindOfClass:NSString.class]);

    [textField textDidChange:[NSNotification notificationWithName:NSTextDidChangeNotification
                                                           object:editor]];

    XCTAssertEqual(willChangeParameters.count, 1u);
    XCTAssertEqual(willChangeInvocationCount, 1u);
    XCTAssertEqual(invalidWillChangeInvocationCount, 0u);
    XCTAssertTrue([willChangeEvent isKindOfClass:NSDictionary.class]);
    XCTAssertTrue([willChangeText isKindOfClass:NSString.class]);
    XCTAssertEqualObjects(willChangeText, @"draft");
    XCTAssertEqualObjects(editor.string, @"DRAFT");
    XCTAssertEqual(editor.selectedRange.location, 5u);
    XCTAssertEqualObjects(changeEvent[@"text"], @"DRAFT");
    XCTAssertEqualObjects(textField.changedValues[@"value"], @"DRAFT");
    XCTAssertEqualObjects(textField.changedValues[@"selection"], (@[@5, @5]));
    textField.fieldEditorOverride = nil;
}

- (void)testTextEditingLifecyclePreservesFocusedAndSelectionOverrides {
    SCValdiTrackingMacOSTextField *textField =
        [[SCValdiTrackingMacOSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    textField.stringValue = @"seed";
    [textField valdi_setSelection:@[@1, @1]];

    NSTextView *firstEditor = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    firstEditor.string = @"seed";
    textField.fieldEditorOverride = firstEditor;
    NSNotification *beginEditing =
        [NSNotification notificationWithName:NSTextDidBeginEditingNotification object:firstEditor];

    [textField textDidBeginEditing:beginEditing];

    XCTAssertEqualObjects(textField.changedValues[@"focused"], @YES);
    XCTAssertEqual(firstEditor.selectedRange.location, 1u);
    XCTAssertEqual(firstEditor.selectedRange.length, 0u);

    firstEditor.selectedRange = NSMakeRange(3, 0);
    [[NSNotificationCenter defaultCenter] postNotificationName:NSTextViewDidChangeSelectionNotification
                                                        object:firstEditor];
    NSNotification *endEditing =
        [NSNotification notificationWithName:NSTextDidEndEditingNotification
                                      object:firstEditor
                                    userInfo:@{NSTextMovementUserInfoKey: @(NSOtherTextMovement)}];

    [textField textDidEndEditing:endEditing];

    XCTAssertEqualObjects(textField.changedValues[@"focused"], @NO);
    XCTAssertEqualObjects(textField.changedValues[@"selection"], (@[@3, @3]));

    NSTextView *secondEditor = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    secondEditor.string = @"seed";
    textField.fieldEditorOverride = secondEditor;
    beginEditing = [NSNotification notificationWithName:NSTextDidBeginEditingNotification object:secondEditor];

    [textField textDidBeginEditing:beginEditing];

    XCTAssertEqualObjects(textField.changedValues[@"focused"], @YES);
    XCTAssertEqual(secondEditor.selectedRange.location, 3u);
    XCTAssertEqual(secondEditor.selectedRange.length, 0u);

    endEditing = [NSNotification notificationWithName:NSTextDidEndEditingNotification
                                               object:secondEditor
                                             userInfo:@{NSTextMovementUserInfoKey: @(NSOtherTextMovement)}];
    [textField textDidEndEditing:endEditing];

    XCTAssertEqualObjects(textField.changedValues[@"focused"], @NO);
}

- (void)testMacOSFunctionRequestsSynchronousReturnAndConvertsMaps {
    ValueFunctionFlags receivedFlags = ValueFunctionFlagsNone;
    auto cppFunction = makeShared<ValueFunctionWithCallable>(
        [&receivedFlags](const ValueFunctionCallContext& callContext) -> Value {
            receivedFlags = callContext.getFlags();
            return ValueFromNSObject(@{
                @"text": @"replacement",
                @"selectionStart": @11,
                @"selectionEnd": @11,
            });
        });
    SCValdiMacOSFunction *function =
        [[SCValdiMacOSFunction alloc] initWithCppInstance:(void *)cppFunction.get()];

    NSDictionary *result = [function performWithParametersAndReturnValue:@[]];

    XCTAssertNotNil(result);
    XCTAssertEqualObjects(result[@"text"], @"replacement");
    XCTAssertEqualObjects(result[@"selectionStart"], @11);
    XCTAssertTrue((receivedFlags & ValueFunctionFlagsCallSync) != ValueFunctionFlagsNone);
}

- (void)testMacOSFunctionConvertsMapParametersAndReturnValue {
    __block NSArray<id> *receivedParameters = nil;
    SCValdiMacOSFunction *function = [[SCValdiMacOSFunction alloc]
        initWithBlock:^id(NSArray<id> *parameters) {
            receivedParameters = parameters;
            return parameters.firstObject;
        }];
    NSDictionary *input = @{
        @"text": @"draft",
        @"selectionStart": @5,
        @"selectionEnd": @5,
    };

    NSDictionary *result = [function performWithParametersAndReturnValue:@[input]];

    XCTAssertEqual(receivedParameters.count, 1u);
    XCTAssertTrue([receivedParameters.firstObject isKindOfClass:NSDictionary.class]);
    NSDictionary *receivedEvent = receivedParameters.firstObject;
    XCTAssertEqualObjects(receivedEvent[@"text"], @"draft");
    XCTAssertTrue([receivedEvent[@"selectionStart"] isKindOfClass:NSNumber.class]);
    XCTAssertTrue([receivedEvent[@"selectionEnd"] isKindOfClass:NSNumber.class]);
    XCTAssertEqualObjects(result[@"text"], @"draft");
    XCTAssertEqualObjects(result[@"selectionStart"], @5);
    XCTAssertEqualObjects(result[@"selectionEnd"], @5);
}

- (void)testTextFieldChangeEventIncludesValueAndSelection {
    SCValdiMacOSTextField *textField = [[SCValdiMacOSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    textField.stringValue = @"draft";
    NSDictionary *editTextEvent = [textField _editTextEvent];

    XCTAssertEqualObjects(editTextEvent[@"text"], @"draft");
    XCTAssertEqualObjects(editTextEvent[@"selectionStart"], @5);
    XCTAssertEqualObjects(editTextEvent[@"selectionEnd"], @5);
}

- (void)testTextFieldEditEndEventIncludesUnknownReturnAndDismissReasons {
    SCValdiMacOSTextField *unknownTextField =
        [[SCValdiMacOSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    XCTAssertEqualObjects([unknownTextField _editTextEndEvent][@"reason"], @0);

    SCValdiMacOSTextField *returnTextField =
        [[SCValdiMacOSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];
    NSTextView *fieldEditor = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];

    [returnTextField control:returnTextField textView:fieldEditor doCommandBySelector:@selector(insertNewline:)];

    XCTAssertEqualObjects([returnTextField _editTextEndEvent][@"reason"], @1);

    SCValdiMacOSTextField *dismissTextField =
        [[SCValdiMacOSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 30)];

    [dismissTextField control:dismissTextField textView:fieldEditor doCommandBySelector:@selector(cancelOperation:)];

    XCTAssertEqualObjects([dismissTextField _editTextEndEvent][@"reason"], @2);
}

@end
