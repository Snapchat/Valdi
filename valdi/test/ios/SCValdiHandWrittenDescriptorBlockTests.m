//
//  SCValdiHandWrittenDescriptorBlockTests.m
//  ios_tests
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "valdi_core/SCValdiFunction.h"
#import "valdi_core/SCValdiFunctionWithBlock.h"
#import "valdi_core/SCValdiINavigator.h"
#import "valdi_core/SCValdiMarshallableObject.h"
#import "valdi_core/SCValdiMarshallableObjectDescriptor.h"
#import "valdi_core/SCValdiMarshallableObjectRegistry.h"
#import "valdi_core/SCValdiMarshaller.h"

/// Hand-written descriptors are not produced by the compiler, so nothing generates the block helpers
/// their function-typed parameters need. A missing helper leaves the runtime without a function class
/// for that signature, and the call fails (or crashes) the first time a callback is marshalled.
@interface SCValdiHandWrittenDescriptorBlockTests : XCTestCase
@end

@interface SCValdiHandWrittenDescriptorTestNavigator : NSObject <SCValdiINavigator>
@property (copy, nonatomic) SCValdiINavigatorSetBackButtonObserverObserverBlock backButtonObserver;
@property (copy, nonatomic) SCValdiINavigatorSetPageVisibilityObserverObserverBlock pageVisibilityObserver;
@end

@implementation SCValdiHandWrittenDescriptorTestNavigator

- (void)pushComponentWithPage:(SCValdiINavigatorPageConfig *)page animated:(BOOL)animated {}
- (void)popWithAnimated:(BOOL)animated {}
- (void)popToSelfWithAnimated:(BOOL)animated {}
- (void)presentComponentWithPage:(SCValdiINavigatorPageConfig *)page animated:(BOOL)animated {}
- (void)dismissWithAnimated:(BOOL)animated {}
- (void)forceDisableDismissalGestureWithForceDisable:(BOOL)forceDisable {}

- (void)setBackButtonObserverWithObserver:(SCValdiINavigatorSetBackButtonObserverObserverBlock)observer
{
    self.backButtonObserver = observer;
}

- (void)setPageVisibilityObserverWithObserver:(SCValdiINavigatorSetPageVisibilityObserverObserverBlock)observer
{
    self.pageVisibilityObserver = observer;
}

- (NSInteger)pushToValdiMarshaller:(SCValdiMarshallerRef)marshaller
{
    return SCValdiINavigatorMarshall(marshaller, self);
}

@end

static Class SCValdiNavigatorDescriptorClass(void)
{
    return NSClassFromString(@"SCValdiINavigator");
}

static NSSet<NSString *> *SCValdiFieldsWithFunctionParameters(SCValdiMarshallableObjectDescriptor descriptor)
{
    NSMutableSet<NSString *> *names = [NSMutableSet set];
    for (const SCValdiMarshallableObjectFieldDescriptor *field = descriptor.fields; field->name != NULL; field++) {
        NSString *type = @(field->type);
        NSRange parameters = [type rangeOfString:@"("];
        if (parameters.location != NSNotFound &&
            [[type substringFromIndex:parameters.location + 1] rangeOfString:@"f"].location != NSNotFound) {
            [names addObject:@(field->name)];
        }
    }
    return names;
}

@implementation SCValdiHandWrittenDescriptorBlockTests

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
}

- (id<SCValdiFunction>)_marshalledFunctionNamed:(NSString *)name
                                  fromNavigator:(SCValdiHandWrittenDescriptorTestNavigator *)navigator
                                       registry:(SCValdiMarshallableObjectRegistry *)registry
{
    __block NSDictionary *result;
    SCValdiMarshallerScoped(marshaller, {
        NSInteger objectIndex = [registry marshallObject:navigator
                                                 ofClass:SCValdiNavigatorDescriptorClass()
                                            toMarshaller:marshaller];
        SCValdiMarshallerCheck(marshaller);
        result = SCValdiMarshallerGetUntyped(marshaller, SCValdiMarshallerUnwrapProxy(marshaller, objectIndex));
    });
    XCTAssertTrue([result isKindOfClass:[NSDictionary class]]);
    id<SCValdiFunction> function = result[name];
    XCTAssertNotNil(function, @"%@ was not marshalled", name);
    return function;
}

- (void)_passCallback:(id<SCValdiFunction>)callback toFunction:(id<SCValdiFunction>)function
{
    SCValdiMarshallerScoped(marshaller, {
        SCValdiMarshallerPushFunction(marshaller, callback);
        [function performWithMarshaller:marshaller];
        SCValdiMarshallerCheck(marshaller);
    });
}

// Every field of the hand-written navigator descriptor that takes a callback must be exercised below.
- (void)testNavigatorCallbackFieldsAreCovered
{
    Class cls = SCValdiNavigatorDescriptorClass();
    XCTAssertNotNil(cls);
    SCValdiMarshallableObjectDescriptor descriptor = [cls valdiMarshallableObjectDescriptor];

    NSSet *expected = [NSSet setWithArray:@[@"setBackButtonObserver", @"setPageVisibilityObserver"]];
    XCTAssertEqualObjects(expected, SCValdiFieldsWithFunctionParameters(descriptor));
}

- (void)testNavigatorPageVisibilityObserverReceivesDouble
{
    @autoreleasepool {
        SCValdiMarshallableObjectRegistry *registry = [SCValdiMarshallableObjectRegistry new];
        SCValdiHandWrittenDescriptorTestNavigator *navigator = [SCValdiHandWrittenDescriptorTestNavigator new];

        id<SCValdiFunction> setObserver = [self _marshalledFunctionNamed:@"setPageVisibilityObserver"
                                                           fromNavigator:navigator
                                                                registry:registry];

        __block NSNumber *receivedVisibility;
        id<SCValdiFunction> observer = [SCValdiFunctionWithBlock functionWithBlock:^BOOL(SCValdiMarshallerRef parameters) {
            receivedVisibility = SCValdiMarshallerGetOptionalDouble(parameters, 0);
            return NO;
        }];

        [self _passCallback:observer toFunction:setObserver];

        XCTAssertNotNil(navigator.pageVisibilityObserver);
        navigator.pageVisibilityObserver(0.75);
        XCTAssertEqualObjects(@(0.75), receivedVisibility);
    }
}

- (void)testNavigatorBackButtonObserverIsCallable
{
    @autoreleasepool {
        SCValdiMarshallableObjectRegistry *registry = [SCValdiMarshallableObjectRegistry new];
        SCValdiHandWrittenDescriptorTestNavigator *navigator = [SCValdiHandWrittenDescriptorTestNavigator new];

        id<SCValdiFunction> setObserver = [self _marshalledFunctionNamed:@"setBackButtonObserver"
                                                           fromNavigator:navigator
                                                                registry:registry];

        __block NSInteger callCount = 0;
        id<SCValdiFunction> observer = [SCValdiFunctionWithBlock functionWithBlock:^BOOL(SCValdiMarshallerRef parameters) {
            callCount++;
            return NO;
        }];

        [self _passCallback:observer toFunction:setObserver];

        XCTAssertNotNil(navigator.backButtonObserver);
        navigator.backButtonObserver();
        XCTAssertEqual(1, callCount);
    }
}

@end
