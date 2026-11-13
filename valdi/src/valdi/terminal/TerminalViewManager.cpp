#include "valdi/terminal/TerminalViewManager.hpp"

#include "valdi/runtime/Attributes/BoundAttributes.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi/runtime/Views/DeferredViewTransaction.hpp"
#include "valdi/runtime/Views/ViewFactory.hpp"
#include "valdi/terminal/Classes/LabelClass.hpp"
#include "valdi/terminal/Classes/ScrollClass.hpp"
#include "valdi/terminal/Classes/StaticClass.hpp"
#include "valdi/terminal/Classes/TerminalAttributeMacros.hpp"
#include "valdi/terminal/Classes/ViewClass.hpp"
#include "valdi/terminal/TerminalViewTransaction.hpp"
#include "valdi_core/cpp/Interfaces/IBitmapFactory.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"

namespace Valdi {

namespace {

class TerminalViewFactory : public ViewFactory {
public:
    TerminalViewFactory(StringBox viewClassName,
                        IViewManager& viewManager,
                        Ref<BoundAttributes> boundAttributes,
                        Ref<ITerminalViewClass> viewClass,
                        snap::valdi_core::Platform platform)
        : ViewFactory(std::move(viewClassName), viewManager, std::move(boundAttributes)),
          _viewClass(std::move(viewClass)),
          _platform(platform) {}

protected:
    Ref<View> doCreateView(ViewNodeTree* /*viewNodeTree*/, ViewNode* /*viewNode*/) override {
        return _viewClass->instantiate(_viewClass, _platform);
    }

private:
    Ref<ITerminalViewClass> _viewClass;
    snap::valdi_core::Platform _platform;
};

} // namespace

TerminalViewManager::TerminalViewManager(PlatformType platformType)
    : TerminalViewManager(platformType, ConsoleLogger::getLogger()) {}

TerminalViewManager::TerminalViewManager(PlatformType platformType, ILogger& logger)
    : _platformType(platformType), _logger(logger) {
    auto viewClass = makeShared<ViewClass>();
    _baseClass = viewClass;
    registerClass(viewClass);
    registerClass(makeShared<LabelClass>(viewClass));
    registerClass(makeShared<ScrollClass>(viewClass));
    registerClass(makeShared<StaticClass>(viewClass));
}

TerminalViewManager::~TerminalViewManager() = default;

void TerminalViewManager::registerClass(const Ref<ITerminalViewClass>& viewClass) {
    std::lock_guard<Mutex> guard(_mutex);
    _classes[StringCache::getGlobal().makeString(std::string_view(viewClass->getIOSClassName()))] = viewClass;
    _classes[StringCache::getGlobal().makeString(std::string_view(viewClass->getAndroidClassName()))] = viewClass;
}

Ref<ITerminalViewClass> TerminalViewManager::getViewClass(const StringBox& className) const {
    std::lock_guard<Mutex> guard(_mutex);
    const auto it = _classes.find(className);
    return it == _classes.end() ? nullptr : it->second;
}

Ref<ITerminalViewClass> TerminalViewManager::resolveClass(const StringBox& className) {
    auto viewClass = getViewClass(className);
    if (viewClass != nullptr) {
        return viewClass;
    }

    bool firstReport;
    {
        std::lock_guard<Mutex> guard(_mutex);
        firstReport = _reportedUnknownClasses.insert(className).second;
    }
    if (firstReport) {
        VALDI_WARN(_logger,
                   "Terminal backend has no view class named '{}', falling back to {}",
                   className,
                   getDefaultViewClass());
    }
    return _baseClass;
}

StringBox TerminalViewManager::getClassName(const ITerminalViewClass& viewClass) const {
    const char* name =
        _platformType == PlatformTypeAndroid ? viewClass.getAndroidClassName() : viewClass.getIOSClassName();
    return StringCache::getGlobal().makeString(std::string_view(name));
}

PlatformType TerminalViewManager::getPlatformType() const {
    return _platformType;
}

RenderingBackendType TerminalViewManager::getRenderingBackendType() const {
    return RenderingBackendTypeTerminal;
}

StringBox TerminalViewManager::getDefaultViewClass() {
    return getClassName(*_baseClass);
}

Ref<ViewFactory> TerminalViewManager::createViewFactory(const StringBox& className,
                                                        const Ref<BoundAttributes>& boundAttributes) {
    const auto platform =
        _platformType == PlatformTypeAndroid ? snap::valdi_core::Platform::Android : snap::valdi_core::Platform::Ios;
    return makeShared<TerminalViewFactory>(className, *this, boundAttributes, resolveClass(className), platform);
}

void TerminalViewManager::callAction(ViewNodeTree* /*viewNodeTree*/,
                                     const StringBox& /*actionName*/,
                                     const Ref<ValueArray>& /*parameters*/) {}

NativeAnimator TerminalViewManager::createAnimator(snap::valdi_core::AnimationType /*type*/,
                                                   const std::vector<double>& /*controlPoints*/,
                                                   double /*duration*/,
                                                   bool /*beginFromCurrentState*/,
                                                   bool /*crossfade*/,
                                                   double /*stiffness*/,
                                                   double /*damping*/) {
    return nullptr;
}

std::vector<StringBox> TerminalViewManager::getClassHierarchy(const StringBox& className) {
    std::vector<StringBox> hierarchy;
    hierarchy.push_back(className);

    auto viewClass = getViewClass(className);
    if (viewClass == nullptr) {
        // Unknown classes inherit from the base class, which is where they are bound and painted.
        resolveClass(className);
        hierarchy.push_back(getClassName(*_baseClass));
        viewClass = _baseClass;
    }
    for (auto parent = viewClass->getParent(); parent != nullptr; parent = parent->getParent()) {
        hierarchy.push_back(getClassName(*parent));
    }
    return hierarchy;
}

void TerminalViewManager::bindAttributes(const StringBox& className, AttributesBindingContext& binder) {
    binder.setDefaultDelegate(TerminalAttributes::makeIgnoringDelegate());

    auto viewClass = resolveClass(className);
    if (viewClass->canBeMeasured()) {
        binder.setMeasureDelegate(viewClass);
    }
    viewClass->bindAttributes(binder);
}

float TerminalViewManager::getPointScale() const {
    return 1.0f;
}

Value TerminalViewManager::createViewNodeWrapper(const Ref<ViewNode>& /*viewNode*/, bool /*wrapInPlatformReference*/) {
    return Value();
}

bool TerminalViewManager::supportsClassNameNatively(const StringBox& className) {
    return getViewClass(className) != nullptr;
}

Ref<IViewTransaction> TerminalViewManager::createViewTransaction(const Ref<MainThreadManager>& mainThreadManager,
                                                                 bool shouldDefer) {
    if (mainThreadManager == nullptr || mainThreadManager->currentThreadIsMainThread() || !shouldDefer) {
        return makeShared<TerminalViewTransaction>();
    }
    return makeShared<DeferredViewTransaction>(*this, *mainThreadManager);
}

Ref<IBitmapFactory> TerminalViewManager::getViewRasterBitmapFactory() const {
    return nullptr;
}

} // namespace Valdi
