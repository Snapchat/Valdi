//
//  SCValdiViewManager.m
//  ValdiIOS
//
//  Created by Simon Corsin on 5/24/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi/ios/CPPBindings/SCValdiViewManager.h"
#import "valdi/ios/CPPBindings/SCValdiViewTransaction.h"

#import "valdi/runtime/Views/DeferredViewTransaction.hpp"
#import "valdi/runtime/Utils/MainThreadManager.hpp"

#import "valdi/ios/Utils/ContextUtils.h"
#import "valdi/ios/Animations/SCValdiAnimator.h"
#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi/ios/SCValdiAttributesBinder.h"
#import "valdi/ios/SCValdiContext.h"
#import "valdi/ios/SCValdiViewNode+CPP.h"

#import "valdi_core/SCValdiActions.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiView.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiValueUtils.h"
#import "valdi_core/SCNValdiCoreAnimator+Private.h"
#import "valdi_core/SCValdiRef.h"

#include "snap_drawing/cpp/Utils/BitmapFactory.hpp"

namespace ValdiIOS
{

ViewManager::ViewManager(id<SCValdiFontManagerProtocol> fontManager)
    :
_fontManager(fontManager),
_pointScale(static_cast<float>([UIScreen mainScreen].scale))
{
}

ViewManager::~ViewManager() = default;

Class ViewManager::viewClassFromClassName(const Valdi::StringBox &className)
{
    const auto &it = _classByName.find(className);
    if (it != _classByName.end()) {
        return it->second;
    }

    NSString *viewClassName = NSStringFromString(className);
    Class cls = NSClassFromString(viewClassName);
    if (!cls) {
        SCLogValdiError(@"Unable to instantiate class '%@'. Falling back to a SCValdiView instead.",
                           viewClassName);
        cls = [SCValdiView class];
    }

    _classByName.try_emplace(className, cls);

    return cls;
}

Valdi::Ref<Valdi::ViewFactory> ViewManager::createViewFactory(const Valdi::StringBox& className, const Valdi::Ref<Valdi::BoundAttributes> &boundAttributes) {
    return createGlobalViewFactory(className, *this, boundAttributes);
}

void ViewManager::callAction(Valdi::ViewNodeTree *viewNodeTree, const Valdi::StringBox &actionName,
                             const Valdi::Ref<Valdi::ValueArray> &parameters)
{
    SCValdiContext *valdiContext = getValdiContext(viewNodeTree->getContext());
    NSString *nsActionName = NSStringFromString(actionName);

    id<SCValdiAction> action = [valdiContext.actions actionForName:nsActionName];
    NSArray *additionalParameters = ObjectAs(ValdiIOS::NSObjectFromValue(Valdi::Value(parameters)), NSArray);
    [action performWithParameters:additionalParameters];
}

Valdi::PlatformType ViewManager::getPlatformType() const
{
    return Valdi::PlatformTypeIOS;
}

Valdi::RenderingBackendType ViewManager::getRenderingBackendType() const {
    return Valdi::RenderingBackendTypeNative;
}

Valdi::StringBox ViewManager::getDefaultViewClass()
{
    static auto kDefaultViewClass = STRING_LITERAL("SCValdiView");
    return kDefaultViewClass;
}

std::vector<Valdi::StringBox> ViewManager::getClassHierarchy(const Valdi::StringBox &className)
{
    std::vector<Valdi::StringBox> classes;

    Class cls = viewClassFromClassName(className);

    while (cls) {
        if (cls == [NSObject class] || cls == [UIResponder class]) {
            break;
        }

        classes.emplace_back(InternedStringFromNSString(NSStringFromClass(cls)));

        cls = [cls superclass];
    }

    return classes;
}

void ViewManager::bindAttributes(const Valdi::StringBox &className,
                                 Valdi::AttributesBindingContext& binder)
{
    Class cls = viewClassFromClassName(className);

    if (cls == [UIResponder class]) {
        return;
    }

    SEL sel = @selector(bindAttributes:);
    IMP method = [cls methodForSelector:sel];
    if (method == nil) {
        return;
    }

    Class superclass = [cls superclass];
    // If the superclass has the same method for bindAttributes: selector,
    // this means this class isn't overriding bindAttributes:
    if (superclass && [superclass methodForSelector:sel] == method) {
        return;
    }

    SCValdiAttributesBinder *wrapper =
        [[SCValdiAttributesBinder alloc] initWithNativeAttributesBindingContext:(SCValdiAttributesBinderNative *)&binder fontManager:_fontManager];

    ((void (*)(id, SEL, SCValdiAttributesBinder *))method)(cls, sel, wrapper);
}

Valdi::NativeAnimator ViewManager::createAnimator(snap::valdi_core::AnimationType type,
                                                     const std::vector<double> &controlPoints, double duration,
                                                     bool beginFromCurrentState,
                                                     bool crossfade,
                                                     double stiffness,
                                                     double damping)
{
    UIViewAnimationCurve curve;
    switch (type) {
    case snap::valdi_core::AnimationType::Linear:
        curve = UIViewAnimationCurveLinear;
        break;
    case snap::valdi_core::AnimationType::EaseIn:
        curve = UIViewAnimationCurveEaseIn;
        break;
    case snap::valdi_core::AnimationType::EaseOut:
        curve = UIViewAnimationCurveEaseOut;
        break;
    case snap::valdi_core::AnimationType::EaseInOut:
        curve = UIViewAnimationCurveEaseInOut;
        break;
    }

    NSMutableArray<NSNumber *> *outControlPoints = [NSMutableArray array];
    for (auto controlPoint : controlPoints) {
        [outControlPoints addObject:@(controlPoint)];
    }

    SCValdiAnimator *animator = [[SCValdiAnimator alloc] initWithCurve:curve
                                                               controlPoints:outControlPoints
                                                                    duration:duration
                                                       beginFromCurrentState:beginFromCurrentState
                                                                   crossfade:crossfade
                                                                   stiffness:stiffness
                                                                   damping:damping];
    [animator setDisableRemoveOnComplete:_disableAnimationRemoveOnComplete];

    return djinni_generated_client::valdi_core::Animator::toCpp(animator);
}

void ViewManager::setDisableAnimationRemoveOnCompleteIos(bool disable)
{
    _disableAnimationRemoveOnComplete = disable;
}

float ViewManager::getPointScale() const
{
    return _pointScale;
}

Valdi::Value ViewManager::createViewNodeWrapper(const Valdi::Ref<Valdi::ViewNode> &viewNode, bool wrapInPlatformReference) {
    SCValdiViewNode *objcViewNode = [[SCValdiViewNode alloc] initWithViewNode:viewNode.get()];
    if (wrapInPlatformReference) {
        SCValdiRef *ref = [[SCValdiRef alloc] initWithInstance:objcViewNode strong:YES];
        return Valdi::Value(ValdiObjectFromNSObject(ref));
    } else {
        return Valdi::Value(ValdiObjectFromNSObject(objcViewNode));
    }
}

void ViewManager::setDebugMessageDisplayer(id<SCValdiDebugMessageDisplayer> debugMessageDisplayer) {
    _debugMessageDisplayer = debugMessageDisplayer;
}

void ViewManager::onDebugMessage(int32_t level, const std::string &message)
{
    id<SCValdiDebugMessageDisplayer> debugMessageDisplayer = _debugMessageDisplayer;
    if (!debugMessageDisplayer) {
        return;
    }

    NSString *objcMessage = NSStringFromSTDStringView(message);
    dispatch_async(dispatch_get_main_queue(), ^{
        [debugMessageDisplayer displayMessage:objcMessage];
    });
}

void ViewManager::setExceptionReporter(id<SCValdiExceptionReporter> exceptionReporter) {
    _exceptionReporter = exceptionReporter;
}

void ViewManager::onUncaughtJsError(const int32_t errorCode, const Valdi::StringBox & moduleName, const std::string & errorMessage, const std::string & stackTrace)
{
    NSString *module = NSStringFromString(moduleName);
    NSString *message = NSStringFromSTDStringView(errorMessage);
    NSString *stack = NSStringFromSTDStringView(stackTrace);
    [_exceptionReporter reportNonFatalWithErrorCode:errorCode message:message module:module stackTrace:stack];
}

Valdi::Ref<Valdi::IViewTransaction> ViewManager::createViewTransaction(
    const Valdi::Ref<Valdi::MainThreadManager>& mainThreadManager, bool shouldDefer) {
    if (mainThreadManager == nullptr || mainThreadManager->currentThreadIsMainThread()) {
        return Valdi::makeShared<ValdiIOS::ViewTransaction>();
    } else {
        // Always use deferred view transaction on iOS if the current thread is not the main thread
        return Valdi::makeShared<Valdi::DeferredViewTransaction>(*this, *mainThreadManager);
    }
}

void ViewManager::onJsCrash(const Valdi::StringBox & moduleName, const std::string & errorMessage, const std::string & stackTrace, bool isANR)
{
    NSString *module = NSStringFromString(moduleName);
    NSString *message = NSStringFromSTDStringView(errorMessage);
    NSString *stack = NSStringFromSTDStringView(stackTrace);
    [_exceptionReporter reportCrashWithMessage:message module:module stackTrace:stack isANR:isANR];
}

Valdi::Ref<Valdi::IBitmapFactory> ViewManager::getViewRasterBitmapFactory() const {
    return snap::drawing::BitmapFactory::getInstance(Valdi::ColorType::ColorTypeBGRA8888);
}

} // namespace ValdiIOS
