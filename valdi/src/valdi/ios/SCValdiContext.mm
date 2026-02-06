//
//  SCValdiContext.m
//  iOS
//
//  Created by Simon Corsin on 4/9/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi/ios/SCValdiContext.h"

#import "valdi/runtime/Attributes/AttributesBindingContextImpl.hpp"
#import "valdi/runtime/Context/ViewNodePath.hpp"
#import "valdi/runtime/Views/ViewFactory.hpp"
#import "valdi/runtime/Resources/Bundle.hpp"
#import "valdi/runtime/Context/ContextAttachedValdiObject.hpp"
#import "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"
#import "valdi_core/cpp/Utils/PlatformObjectAttachments.hpp"

#import "valdi_core/SCValdiViewOwner.h"
#import "valdi_core/SCValdiViewFactory.h"
#import "valdi_core/SCValdiInternedString+CPP.h"
#import "valdi_core/SCValdiActions.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiActionHandlerHolder.h"
#import "valdi_core/SCValdiActions.h"
#import "valdi_core/SCValdiReferenceTable+CPP.h"
#import "valdi_core/UIView+ValdiObjects.h"

#import "valdi/ios/CPPBindings/UIViewHolder.h"
#import "valdi/ios/SCValdiRuntime+Private.h"
#import "valdi/ios/SCValdiContext+CPP.h"
#import "valdi/ios/CPPBindings/SCValdiRuntimeListener.h"
#import "valdi/ios/SCValdiViewModel.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"
#import "valdi/ios/Utils/ContextUtils.h"

#import <limits>

@interface SCValdiContext ()
@end

@implementation SCValdiContext {
    Valdi::SharedContext _context;
    Valdi::Ref<Valdi::IOS::ObjCStrongReferenceTable> _strongReferenceTable;

    NSMutableArray *_initialRenderCompletions;
    NSMutableArray *_onLayoutDirtyCallbacks;
    NSMutableArray *_emittedObjCReferences;
    SCValdiActionHandlerHolder *_actionHandlerHolder;

    NSMutableArray<id<SCValdiDisposable>> *_disposables;
    NSMutableDictionary<NSString *, id> *_attachedObjects;

    BOOL _awakeCalled;
    BOOL _enableAccurateTouchGesturesInAnimations;
    BOOL _destroyed;
    NSInteger _renderCount;
    NSString *_componentPath;

    UITraitCollection *_traitCollection;
}

@synthesize gestureListener;

static Valdi::SharedRuntime getRuntimeFromContext(const Valdi::SharedContext &context) {
    if (context == nullptr) {
        return nil;
    }

    return Valdi::strongRef(context->getRuntime());
}

- (instancetype)initWithContext:(Valdi::SharedContext)context
        enableReferenceTracking:(BOOL)enableReferenceTracking
{
    self = [super init];

    if (self) {
        _context = std::move(context);

        if (enableReferenceTracking) {
            auto strongRefTable = Valdi::makeShared<Valdi::IOS::ObjCStrongReferenceTable>();
            _context->setWeakReferenceTable(Valdi::makeShared<Valdi::IOS::ObjCWeakReferenceTable>());
            _context->setStrongReferenceTable(strongRefTable);

            // Keep as an ivar to make it visible to tooling that looks at them
            _emittedObjCReferences = strongRefTable->unsafeGetInnerTable();
            _strongReferenceTable = std::move(strongRefTable);
        }

        _actionHandlerHolder = [SCValdiActionHandlerHolder new];
        _actions = [[SCValdiActions alloc] initWithActionByName:@{} actionHandlerHolder:_actionHandlerHolder];
        _componentPath = ValdiIOS::NSStringFromSTDStringView(_context->getPath().toString());
        _moduleOwnerName = ValdiIOS::NSStringFromString(_context->getAttribution().owner);
        _moduleName = ValdiIOS::NSStringFromString(_context->getAttribution().moduleName);
    }

    return self;
}

- (SCValdiContextId)contextId
{
    auto context = [self cppContext];
    if (context == nullptr) {
        return 0;
    }

    return context->getContextId();
}

- (void)setEnableAccessibility:(BOOL)enableAccessibility
{
    _enableAccessibility = enableAccessibility;
}

- (Valdi::SharedContext)cppContext
{
    @synchronized (self) {
        return _context;
    }
}

- (Valdi::SharedRuntime)cppRuntime
{
    auto cppContext = [self cppContext];
    return getRuntimeFromContext(cppContext);
}

- (Valdi::SharedViewNodeTree)viewNodeTree
{
    auto context = [self cppContext];
    auto runtime = getRuntimeFromContext(context);

    if (context == nullptr || runtime == nullptr) {
        return nullptr;
    }

    return runtime->getOrCreateViewNodeTreeForContextId(context->getContextId());
}

- (NSArray<id> *)trackedObjCReferences
{
    if (_strongReferenceTable == nullptr) {
        return nil;
    }

    return _strongReferenceTable->getObjects();
}

- (void)destroy
{
    auto context = [self cppContext];
    auto runtime = getRuntimeFromContext(context);

    if (context != nullptr && runtime != nullptr) {
        runtime->destroyContext(context);
    }
}

- (void)awakeIfNeeded
{
    if (!_awakeCalled) {
        _awakeCalled = YES;
        id<SCValdiViewOwner> owner = _owner;
        if ([owner respondsToSelector:@selector(didAwakeViewFromValdi:)]) {
            [owner didAwakeViewFromValdi:self.rootValdiView];
        }
    }
}

- (void)notifyDidRender
{
    _renderCount++;
    id<SCValdiViewOwner> owner = self.owner;
    if ([owner respondsToSelector:@selector(didRenderValdiView:)]) {
        [owner didRenderValdiView:self.rootValdiView];
    }
    if (_initialRenderCompletions && [self hasCompletedInitialRender]) {
        NSArray *completions = _initialRenderCompletions;
        _initialRenderCompletions = nil;
        for (void (^func)() in completions) {
            func();
        }
    }
}

- (void)notifyLayoutDidBecomeDirty
{
    for (void (^cb)() in _onLayoutDirtyCallbacks) {
        cb();
    }
}

- (void)onLayoutDirty:(void (^)())onLayoutDirtyCallback
{
    if (!_onLayoutDirtyCallbacks) {
        _onLayoutDirtyCallbacks = [NSMutableArray array];
    }

    [_onLayoutDirtyCallbacks addObject:onLayoutDirtyCallback];
}

- (BOOL)hasCompletedInitialRender
{
    return _renderCount > 0;
}

- (BOOL)hasCompletedInitialRenderIncludingChildComponents
{
    return [self hasCompletedInitialRender];
}

- (void)setViewModel:(id)viewModel
{
    if (_viewModel != viewModel) {
        _viewModel = viewModel;

        auto context = [self cppContext];
        if (context != nullptr) {
            context->setViewModel(ValdiIOS::toLazyValueConvertible(viewModel));
        }
    }
}

- (Valdi::Ref<Valdi::View>)_rootValdiView
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return nullptr;
    }

    return viewNodeTree->getRootView();
}

- (UIView *)rootValdiView
{
    auto rootView = [self _rootValdiView];
    return ValdiIOS::UIViewHolder::uiViewFromRef(rootView);
}

- (void)setRootValdiView:(UIView<SCValdiRootViewProtocol> *)rootView
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        SCLogValdiError(@"Cannot set rootView: No ViewNodeTree attached");
        return;
    }

    UIView *oldRootView = [self rootValdiView];
    if (oldRootView != rootView) {
        if (rootView) {
            BOOL shouldKeepAsStrong = !_rootValdiViewShouldDestroyContext;
            SCValdiRef *ref = [[SCValdiRef alloc] initWithInstance:rootView strong:shouldKeepAsStrong];
            viewNodeTree->setRootView(Valdi::makeShared<ValdiIOS::UIViewHolder>(ref));
        } else {
            viewNodeTree->setRootView(nullptr);
        }

        oldRootView.valdiViewNode = nil;
        oldRootView.valdiContext = nil;
    }

    if (rootView != nil && rootView.valdiContext == nil) {
        SCLogValdiError(@"Root view valdiContext is nil after setRootValdiView. "
                        @"contextId=%u, componentPath=%@, oldRootView=%p, rootView=%p, sameView=%d",
                        self.contextId, _componentPath, oldRootView, rootView, oldRootView == rootView);
    }

    [rootView updateTraitCollection];
}

- (UIView *)viewForNodeId:(NSString *)nodeId
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return nil;
    }

    auto result = Valdi::ViewNodePath::parse(ValdiIOS::StringViewFromNSString(nodeId));
    result.ensureSuccess();

    auto valdiView = viewNodeTree->getViewForNodePath(result.value());
    return ValdiIOS::UIViewHolder::uiViewFromRef(valdiView);
}

- (uint32_t)objectID
{
    return [self contextId];
}

static bool CGFloatIsUndefined(CGFloat cgFloat) {
    // Cap the float values we can represent as a float
    // to avoid conversions to infinity
    return isnan(cgFloat) ||
        cgFloat >= static_cast<CGFloat>(std::numeric_limits<float>::max()) ||
        cgFloat <= static_cast<CGFloat>(std::numeric_limits<float>::lowest());
}

static float CGFloatToFloat(CGFloat cgFloat) {
    if (CGFloatIsUndefined(cgFloat)) {
        return Valdi::kUndefinedFloatValue;
    }

    return static_cast<float>(cgFloat);
}

static Valdi::Size CGSizeToCpp(CGSize size) {
    return Valdi::Size(CGFloatToFloat(size.width),
                          CGFloatToFloat(size.height));
}

static Valdi::LayoutDirection SCValdiLayoutDirectionToCpp(SCValdiLayoutDirection direction) {
    auto cppDirection = Valdi::LayoutDirectionLTR;
    switch (direction) {
        case SCValdiLayoutDirectionLTR:
            cppDirection = Valdi::LayoutDirectionLTR;
            break;
        case SCValdiLayoutDirectionRTL:
            cppDirection = Valdi::LayoutDirectionRTL;
            break;
    }
    return cppDirection;
}

- (void)_scheduleReapplyAttributesRecursive:(NSArray<NSString *>*)attributeNames
                          invalidateMeasure:(BOOL)invalidateMeasure
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return;
    }

    std::vector<Valdi::StringBox> attributeNamesCpp;
    attributeNamesCpp.reserve([attributeNames count]);
    for (NSString *attributeName in attributeNames) {
        attributeNamesCpp.emplace_back(ValdiIOS::StringFromNSString(attributeName));
    }
    viewNodeTree->scheduleReapplyAttributesRecursive(attributeNamesCpp, invalidateMeasure);
}

- (UITraitCollection *)traitCollection
{
    return _traitCollection;
}

- (void)setTraitCollection:(UITraitCollection *)traitCollection
{
    UITraitCollection *prevTraitCollection = _traitCollection;
    UITraitCollection *nextTraitCollection = traitCollection;

    _traitCollection = traitCollection;

    bool changedContentSizeCategory = [prevTraitCollection preferredContentSizeCategory] != [nextTraitCollection preferredContentSizeCategory];
    bool changedLegibilityWeight = NO;
    if (@available(iOS 13, *)) {
        changedLegibilityWeight = [prevTraitCollection legibilityWeight] != [nextTraitCollection legibilityWeight];
    }

    if (changedContentSizeCategory || changedLegibilityWeight) {
        [self _scheduleReapplyAttributesRecursive:@[@"font", @"value"] invalidateMeasure:YES];
    }
}

- (void)setLayoutSize:(CGSize)size direction:(SCValdiLayoutDirection)direction
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return;
    }

    viewNodeTree->setLayoutSpecs(CGSizeToCpp(size),
                                   SCValdiLayoutDirectionToCpp(direction));
}

- (void)setVisibleViewportWithFrame:(CGRect)frame
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return;
    }

    viewNodeTree->setViewport({Valdi::Frame(
        CGFloatToFloat(frame.origin.x),
        CGFloatToFloat(frame.origin.y),
        CGFloatToFloat(frame.size.width),
        CGFloatToFloat(frame.size.height)
    )});
}

- (void)unsetVisibleViewport
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return;
    }

    viewNodeTree->setViewport(std::nullopt);
}

static Valdi::MeasureMode resolveMeasureMode(CGFloat size, BOOL useLegacyMeasureBehavior) {
    if (CGFloatIsUndefined(size)) {
        return Valdi::MeasureModeUnspecified;
    }
    if (useLegacyMeasureBehavior) {
        return Valdi::MeasureModeExactly;
    } else {
        return Valdi::MeasureModeAtMost;
    }
}

- (CGSize)measureLayoutWithMaxSize:(CGSize)size direction:(SCValdiLayoutDirection)direction
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return CGSizeZero;
    }

    auto lock = viewNodeTree->lock();

    auto width = CGFloatToFloat(size.width);
    auto height = CGFloatToFloat(size.height);

    Valdi::MeasureMode widthMode = resolveMeasureMode(size.width, _useLegacyMeasureBehavior);
    Valdi::MeasureMode heightMode = resolveMeasureMode(size.height, _useLegacyMeasureBehavior);

    auto measuredSize = viewNodeTree->measureLayout(width, widthMode, height, heightMode, SCValdiLayoutDirectionToCpp(direction));
    return CGSizeMake(CGFloatNormalize(measuredSize.width), CGFloatNormalize(measuredSize.height));
}

- (void)didChangeValue:(id)value
  forValdiAttribute:(NSString *)attributeName
            inViewNode:(SCValdiViewNode *)viewNode
{
    auto runtime = [self cppRuntime];
    auto cppViewNode = [viewNode cppViewNode];
    if (!viewNode || runtime == nullptr || cppViewNode == nullptr) {
        return;
    }
    auto cppAttributeName = ValdiIOS::StringFromNSString(attributeName);
    auto cppValue = ValdiIOS::ValueFromNSObject(value);

    runtime->updateAttributeState(*cppViewNode, cppAttributeName, cppValue);
}

- (void)didChangeValue:(id)value
  forInternedValdiAttribute:(SCValdiInternedStringRef)attributeName
            inViewNode:(SCValdiViewNode *)viewNode
{
    auto runtime = [self cppRuntime];
    auto cppViewNode = [viewNode cppViewNode];
    if (!viewNode || runtime == nullptr || cppViewNode == nullptr) {
        return;
    }
    auto cppValue = ValdiIOS::ValueFromNSObject(value);

    runtime->updateAttributeState(*cppViewNode, *SCValdiInternedStringUnwrap(attributeName), cppValue);
}

- (id<SCValdiRuntimeProtocol>)runtime
{
    auto cppRuntime = [self cppRuntime];
    if (cppRuntime == nullptr) {
        return nil;
    }
    return ValdiIOS::getObjCRuntime(*cppRuntime);
}

- (void)setViewModelNoUpdate:(id)viewModel
{
    _viewModel = viewModel;
}

- (void)waitUntilInitialRenderWithCompletion:(void (^)())completion
{
    if ([self hasCompletedInitialRender]) {
        completion();
        return;
    }

    if (!_initialRenderCompletions) {
        _initialRenderCompletions = [[NSMutableArray alloc] initWithCapacity:1];
    }
    [_initialRenderCompletions addObject:completion];
}

- (void)waitUntilRenderCompletedSync
{
    [self waitUntilRenderCompletedSyncWithFlush:NO];
}

- (void)waitUntilRenderCompletedSyncWithFlush:(BOOL)shouldFlushRenderRequests
{
    auto context = [self cppContext];
    if (context != nullptr) {
        context->waitUntilAllUpdatesCompletedSync(shouldFlushRenderRequests);
    }
}

- (void)waitUntilRenderCompletedWithCompletion:(void (^)())completion
{
    auto context = [self cppContext];
    if (context == nullptr) {
        completion();
        return;
    }

    context->waitUntilAllUpdatesCompleted(Valdi::makeShared<Valdi::ValueFunctionWithCallable>([=](const auto &/*callContext*/) -> Valdi::Value {
        completion();
        return Valdi::Value();
    }));
}

- (void)setActionHandler:(id)actionHandler
{
    _actionHandlerHolder.actionHandler = actionHandler;
}

- (id)actionHandler
{
    return _actionHandlerHolder.actionHandler;
}

- (SCValdiContext *)rootContext
{
    return self;
}

- (void)performJsAction:(NSString *)actionName parameters:(NSArray *)parameters
{
    auto context = [self cppContext];
    auto runtime = getRuntimeFromContext(context);
    if (runtime == nullptr || context == nullptr) {
        return;
    }

    auto additionalParameters = Valdi::ValueArray::make(parameters.count);
    size_t i = 0;
    for (id parameter in parameters) {
        additionalParameters->emplace(i++, ValdiIOS::ValueFromNSObject(parameter));
    }

    runtime->getJavaScriptRuntime()->callComponentFunction(context->getContextId(), ValdiIOS::StringFromNSString(actionName), additionalParameters);
}

- (void)registerViewFactory:(SCValdiViewFactoryBlock)viewFactory forClass:(__unsafe_unretained Class)viewClass
{
    [self registerViewFactory:viewFactory attributesBinder:nil forClass:viewClass];
}

- (void)registerViewFactory:(SCValdiViewFactoryBlock)viewFactory attributesBinder:(SCValdiBindAttributesCallback)attributesBinder forClass:(__unsafe_unretained Class)viewClass
{
    auto viewNodeTree = [self viewNodeTree];
    id<SCValdiRuntimeProtocol> runtime = [self runtime];

    if (viewNodeTree == nullptr || runtime == nil) {
        return;
    }

    id<SCValdiViewFactory> createdViewFactory = [runtime makeViewFactoryWithBlock:viewFactory attributesBinder:attributesBinder forClass:viewClass];

    Valdi::Value nativeRepr;
    [createdViewFactory valdi_toNative:&nativeRepr];

    auto cppViewFactory = Valdi::castOrNull<Valdi::ViewFactory>(nativeRepr.getValdiObject());

    viewNodeTree->registerLocalViewFactory(cppViewFactory->getViewClassName(), cppViewFactory);
}

- (void)setAttachedObject:(id)attachedObject forKey:(NSString *)key
{
    @synchronized (self) {
        if (!_attachedObjects) {
            _attachedObjects = [NSMutableDictionary dictionary];
        }
        if (attachedObject) {
            _attachedObjects[key] = attachedObject;
        } else {
            [_attachedObjects removeObjectForKey:key];
        }
    }
}

- (id)attachedObjectForKey:(NSString *)key
{
    @synchronized (self) {
        return _attachedObjects[key];
    }
}

- (void)addDisposable:(id<SCValdiDisposable>)disposable
{
    BOOL added = NO;

    @synchronized (self) {
        if (!_destroyed) {
            added = YES;
            if (!_disposables) {
                _disposables = [NSMutableArray array];
            }
            [_disposables addObject:disposable];
        }
    }

    if (!added) {
        [disposable dispose];
    }
}

- (void)onDestroyed
{
    NSMutableArray *disposables;
    Valdi::SharedContext context;

    @synchronized (self) {
        if (_destroyed) {
            return;
        }
        _destroyed = YES;
        disposables = _disposables;
        _disposables = nil;
        context = std::move(_context);
    }

    for (id<SCValdiDisposable> disposable in disposables) {
        [disposable dispose];
    }
}

- (void)setViewInflationEnabled:(BOOL)viewInflationEnabled
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree != nullptr) {
        viewNodeTree->setViewInflationEnabled(viewInflationEnabled);
    }
}

- (void)setRetainsLayoutSpecsOnInvalidateLayout:(BOOL)retainsLayoutSpecsOnInvalidateLayout
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree != nullptr) {
        viewNodeTree->setRetainsLayoutSpecsOnInvalidateLayout(retainsLayoutSpecsOnInvalidateLayout);
    }
}

- (BOOL)enableAccurateTouchGesturesInAnimations
{
    return _enableAccurateTouchGesturesInAnimations;
}

- (void)setEnableAccurateTouchGesturesInAnimations:(BOOL)enableAccurateTouchGesturesInAnimations
{
    _enableAccurateTouchGesturesInAnimations = enableAccurateTouchGesturesInAnimations;
}

- (BOOL)retainsLayoutSpecsOnInvalidateLayout
{
    auto viewNodeTree = [self viewNodeTree];
    return viewNodeTree != nullptr ? viewNodeTree->retainsLayoutSpecsOnInvalidateLayout() : false;
}

- (BOOL)viewInflationEnabled
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return NO;
    }

    return viewNodeTree->isViewInflationEnabled();
}

- (void)setParentContext:(id<SCValdiContextProtocol>)parentContext
{
    auto viewNodeTree = [self viewNodeTree];
    if (!parentContext || viewNodeTree == nullptr) {
        return;
    }
    self.enableAccessibility = parentContext.enableAccessibility;

    SCValdiContext *parentContextImpl = ObjectAs(parentContext, SCValdiContext);
    SC_ASSERT(parentContextImpl);

    viewNodeTree->setParentTree([parentContextImpl viewNodeTree]);
}

- (id<SCValdiViewNodeProtocol>)rootViewNode
{
    auto viewNodeTree = [self viewNodeTree];
    if (viewNodeTree == nullptr) {
        return nil;
    }
    auto rootViewNode = viewNodeTree->getRootViewNode();
    if (rootViewNode == nullptr) {
        return nil;
    }

    return [[SCValdiViewNode alloc] initWithViewNode:rootViewNode.get()];
}

- (BOOL)destroyed
{
    @synchronized (self) {
        return _destroyed;
    }
}

+ (SCValdiContext *)currentContext
{
    return ValdiIOS::getValdiContext(Valdi::Context::current());
}

#if !defined(NS_BLOCK_ASSERTIONS)
- (NSString *)description {
    return [NSString stringWithFormat:
            @"<%@: %p, viewModel: %@, actionHandler: %@, owner: %@, actions: %@, traitCollection: %@, rootView: %@, objectID: %u, runtime: %@, componentPath: %@, moduleOwnerName: %@, moduleName: %@, enableAccessibility: %d, viewInflationEnabled: %d, hasCompletedInitialRenderIncludingChildComponents: %d, rootViewShouldDestroyContext: %d, useLegacyMeasureBehavior: %d>",
            NSStringFromClass([self class]), self, self.viewModel, self.actionHandler, self.owner, self.actions, self.traitCollection, self.rootValdiView, self.objectID, self.runtime, self.componentPath, self.moduleOwnerName, self.moduleName, self.enableAccessibility, self.viewInflationEnabled, self.hasCompletedInitialRenderIncludingChildComponents, self.rootValdiViewShouldDestroyContext, self.useLegacyMeasureBehavior];
}
#endif

@end
