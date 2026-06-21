//
//  ContextUtils.cpp
//  Valdi
//
//  Created by Simon Corsin on 6/10/18.
//  Copyright © 2018 Snap Inc. All rights reserved.
//

#import "valdi/ios/Utils/ContextUtils.h"
#import "valdi/ios/CPPBindings/UIViewHolder.h"

#import "valdi/ios/Categories/UIView+Valdi.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiLogger.h"
#import "valdi_core/SCValdiViewOwner.h"
#import "valdi_core/SCValdiView.h"
#import "valdi_core/cpp/Utils/StringCache.hpp"

namespace ValdiIOS
{

static bool viewClassManagesChildFrames(Class cls) {
    if (![cls respondsToSelector:@selector(valdi_managesChildFrames)]) {
        return false;
    }
    return [cls valdi_managesChildFrames];
}

class IOSViewFactory: public Valdi::ViewFactory {
public:
    IOSViewFactory(Valdi::StringBox viewClassName, Valdi::IViewManager& viewManager, Valdi::Ref<Valdi::BoundAttributes> boundAttributes)
        : Valdi::ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)) {}

    ~IOSViewFactory() override = default;

    Class lookupClass() {
        if (_cachedClass != nullptr) {
            return _cachedClass;
        }

        const auto& className = getViewClassName();

        NSString *viewClassName = NSStringFromString(className);
        Class cls = NSClassFromString(viewClassName);
        if (!cls) {
            SCLogValdiError(@"Unable to instantiate class '%@'. Falling back to a SCValdiView instead.",
                               viewClassName);
            cls = [SCValdiView class];
        }
        _cachedClass = cls;
        return cls;
    }

private:
    Class _cachedClass = nullptr;
};

class LocalViewFactory : public IOSViewFactory {
public:
    LocalViewFactory(SCValdiViewFactoryBlock viewFactory,
                             Valdi::StringBox viewClassName,
                             Valdi::IViewManager& viewManager,
                             Valdi::Ref<Valdi::BoundAttributes> boundAttributes)
        : IOSViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)),
          _viewFactory(viewFactory) {
        CFBridgingRetain(_viewFactory);
    }

    ~LocalViewFactory() override {
        CFBridgingRelease((__bridge CFTypeRef _Nullable)(_viewFactory));
    }

protected:
    Valdi::Ref<Valdi::View> doCreateView(Valdi::ViewNodeTree* viewNodeTree, Valdi::ViewNode* viewNode) override {
        UIView* view = _viewFactory();
        if (!view) {
            return nullptr;
        }

        return UIViewHolder::makeWithUIView(view);
    }

private:
    SCValdiViewFactoryBlock _viewFactory;
};

class GlobalViewFactory : public IOSViewFactory {
public:
    GlobalViewFactory(Valdi::StringBox viewClassName, Valdi::IViewManager& viewManager, Valdi::Ref<Valdi::BoundAttributes> boundAttributes)
        : IOSViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)) {}
    ~GlobalViewFactory() override = default;

protected:
    Valdi::Ref<Valdi::View> doCreateView(Valdi::ViewNodeTree* viewNodeTree, Valdi::ViewNode* viewNode) override {
        Class cls = lookupClass();
        UIView *view = nil;
        id<SCValdiViewOwner> owner;

        if (viewNodeTree != nullptr) {
            // If the current context doesn't have an owner, find one by recursively
            // traversing up the context tree.
            SCValdiContext *valdiContext = getValdiContext(viewNodeTree->getContext());
            owner = [valdiContext owner];

            if ([owner respondsToSelector:@selector(valdiWillCreateViewForClass:nodeId:)]) {
                view = [owner valdiWillCreateViewForClass:cls nodeId:nil];
            }
        }

        if (!view) {
            ValdiIOS::UIViewHolder::executeSyncInMainThread([&]() {
                view = [[cls alloc] initWithFrame:CGRectZero];
            });
        }

        return ValdiIOS::UIViewHolder::makeWithUIView(view);
    }
};

SCValdiContext *getValdiContext(const Valdi::Context *context) {
    if (context == nullptr) {
        return nil;
    }

    return NSObjectFromValdiObject(context->getUserData(), NO);
}

SCValdiContext *getValdiContext(const Valdi::SharedContext &context) {
    return getValdiContext(context.get());
}

SCValdiContext *getValdiContext(const Valdi::Shared<Valdi::Context> &context) {
    return getValdiContext(context.get());
}

Valdi::Ref<Valdi::ViewFactory> createLocalViewFactory(SCValdiViewFactoryBlock viewFactory,
                                                               const Valdi::StringBox &viewClassName,
                                                               Valdi::IViewManager& viewManager,
                                                               const Valdi::Ref<Valdi::BoundAttributes> &boundAttributes) {
    auto factory = Valdi::makeShared<LocalViewFactory>(viewFactory, viewClassName, viewManager, boundAttributes);
    factory->setIsUserSpecified(true);
    factory->setManagesChildFrames(viewClassManagesChildFrames(factory->lookupClass()));
    return factory;
}

Valdi::Ref<Valdi::ViewFactory> createGlobalViewFactory(const Valdi::StringBox &viewClassName,
                                                                Valdi::IViewManager& viewManager,
                                                                const Valdi::Ref<Valdi::BoundAttributes> &boundAttributes) {
    auto factory = Valdi::makeShared<GlobalViewFactory>(viewClassName, viewManager, boundAttributes);
    factory->setManagesChildFrames(viewClassManagesChildFrames(factory->lookupClass()));
    return factory;
}

}
