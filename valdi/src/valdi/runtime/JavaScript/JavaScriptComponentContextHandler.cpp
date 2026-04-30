//
//  JavaScriptComponentContextHandler.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 7/17/19.
//

#include "valdi/runtime/JavaScript/JavaScriptComponentContextHandler.hpp"
#include "valdi/runtime/Context/Context.hpp"
#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptContextEntryPoint.hpp"
#include "valdi/runtime/JavaScript/JavaScriptFunctionCallContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/Metrics/Metrics.hpp"
#include "valdi/runtime/Utils/MainThreadManager.hpp"
#include "valdi_core/cpp/Utils/LoggerUtils.hpp"
#include "valdi_core/cpp/Utils/SmallVector.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"

namespace Valdi {

constexpr size_t kStashDataPropertyName = 0;
constexpr size_t kRestoreDataPropertyName = 1;
constexpr size_t kCreatePropertyName = 2;
constexpr size_t kDestroyPropertyName = 3;
constexpr size_t kRenderPropertyName = 4;
constexpr size_t kOnRuntimeIssuePropertyName = 5;
constexpr size_t kCallComponentFunctionPropertyName = 6;
constexpr size_t kAttributeChangePropertyName = 7;

static JSValueRef makeJsContextId(JavaScriptEntryParameters& jsEntry) {
    return jsEntry.jsContext.newNumber(static_cast<int32_t>(jsEntry.valdiContext->getContextId()));
}

static void resolveParentContext(const RefCountable* value, Ref<Context>& parentContext) {
    const auto* jsValueRefHolder = dynamic_cast<const JSValueRefHolder*>(value);
    if (jsValueRefHolder == nullptr) {
        return;
    }
    if (parentContext == nullptr || jsValueRefHolder->getContext()->getContextId() > parentContext->getContextId()) {
        parentContext = jsValueRefHolder->getContext();
    }
}

static void resolveParentContext(const Value& value, Ref<Context>& parentContext) {
    if (value.isArray()) {
        for (const auto& item : *value.getArray()) {
            resolveParentContext(item, parentContext);
        }
    } else if (value.isMap()) {
        for (const auto& it : *value.getMap()) {
            resolveParentContext(it.second, parentContext);
        }
    } else if (value.isValdiObject()) {
        resolveParentContext(value.getValdiObject().get(), parentContext);
    } else if (value.isFunction()) {
        resolveParentContext(value.getFunction(), parentContext);
    }
}

inline static JavaScriptTaskScheduleType resolveScheduleType(bool sync) {
    return sync ? JavaScriptTaskScheduleType::JavaScriptTaskScheduleTypeAlwaysSync :
                  JavaScriptTaskScheduleType::JavaScriptTaskScheduleTypeDefault;
}

JavaScriptComponentContextHandler::JavaScriptComponentContextHandler(
    JavaScriptTaskScheduler& jsTaskScheduler, JavaScriptComponentContextHandlerListener* listener, ILogger& logger)
    : _jsTaskScheduler(weakRef(&jsTaskScheduler)), _listener(listener), _logger(logger) {
    _propertyNames.set(kStashDataPropertyName, "stashData");
    _propertyNames.set(kRestoreDataPropertyName, "restoreData");
    _propertyNames.set(kCreatePropertyName, "create");
    _propertyNames.set(kDestroyPropertyName, "destroy");
    _propertyNames.set(kRenderPropertyName, "render");
    _propertyNames.set(kOnRuntimeIssuePropertyName, "onRuntimeIssue");
    _propertyNames.set(kCallComponentFunctionPropertyName, "callComponentFunction");
    _propertyNames.set(kAttributeChangePropertyName, "attributeChanged");
}

JavaScriptComponentContextHandler::~JavaScriptComponentContextHandler() = default;

void JavaScriptComponentContextHandler::clear() {
    _propertyNames.setContext(nullptr);
}

void JavaScriptComponentContextHandler::removeJsContextHandler() {
    setJsContextHandler(Result<Shared<JSValueRefHolder>>());
}

void JavaScriptComponentContextHandler::setJsContextHandler(const Result<Shared<JSValueRefHolder>>& jsContextHandler) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (_jsContextHandler) {
        if (jsTaskScheduler != nullptr) {
            jsTaskScheduler->dispatchOnJsThreadSync(nullptr, [&](JavaScriptEntryParameters& jsEntry) {
                JSFunctionCallContext callContext(jsEntry.jsContext, nullptr, 0, jsEntry.exceptionTracker);

                auto stashResult = callJsContextHandlerFunction(kStashDataPropertyName, jsEntry, callContext);
                if (jsEntry.exceptionTracker) {
                    static auto kStashedDataKey = STRING_LITERAL("stashedData");
                    _stashedHotReloadData =
                        std::make_unique<JSValueRefHolder>(jsEntry.jsContext,
                                                           stashResult.get(),
                                                           ReferenceInfoBuilder().withObject(kStashedDataKey).build(),
                                                           jsEntry.exceptionTracker,
                                                           false);
                } else {
                    _stashedHotReloadData = nullptr;
                }
            });
        }
    }

    _jsContextHandler = jsContextHandler;

    if (_jsContextHandler && _stashedHotReloadData) {
        auto stashedHotReloadData = std::move(_stashedHotReloadData);
        _stashedHotReloadData = nullptr;

        if (jsTaskScheduler != nullptr) {
            jsTaskScheduler->dispatchOnJsThreadSync(nullptr, [&](JavaScriptEntryParameters& jsEntry) {
                auto jsValue = stashedHotReloadData->getJsValue(jsEntry.jsContext, jsEntry.exceptionTracker);
                if (!jsEntry.exceptionTracker) {
                    return;
                }

                std::initializer_list<JSValueRef> parameters = {JSValueRef::makeUnretained(jsEntry.jsContext, jsValue)};

                JSFunctionCallContext callContext(
                    jsEntry.jsContext, parameters.begin(), parameters.size(), jsEntry.exceptionTracker);
                callJsContextHandlerFunction(kRestoreDataPropertyName, jsEntry, callContext);
            });
        }
    }
}

bool JavaScriptComponentContextHandler::needsJsContextHandler() const {
    return !_contextsById.empty();
}

void JavaScriptComponentContextHandler::onCreate(const Ref<Context>& context,
                                                 const Shared<ValueConvertible>& viewModel,
                                                 const Shared<ValueConvertible>& componentContext,
                                                 ContextUpdateId updateId,
                                                 bool shouldUpdateSync) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (jsTaskScheduler == nullptr) {
        return;
    }

    jsTaskScheduler->dispatchOnJsThread(
        context,
        resolveScheduleType(shouldUpdateSync),
        0,
        [self = strongSmallRef(this), viewModel, componentContext, updateId](JavaScriptEntryParameters& jsEntry) {
            self->doOnCreate(jsEntry, viewModel, componentContext);
            jsEntry.valdiContext->markUpdateCompleted(updateId);
        });
}

void JavaScriptComponentContextHandler::onDestroy(const Ref<Context>& context, bool shouldUpdateSync) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (jsTaskScheduler == nullptr) {
        return;
    }

    jsTaskScheduler->dispatchOnJsThread(context,
                                        resolveScheduleType(shouldUpdateSync),
                                        0,
                                        [self = strongSmallRef(this)](auto& jsEntry) { self->doOnDestroy(jsEntry); });
}

void JavaScriptComponentContextHandler::onViewModelUpdate(const Ref<Context>& context,
                                                          const Shared<ValueConvertible>& viewModel,
                                                          ContextUpdateId updateId,
                                                          bool shouldUpdateSync) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (jsTaskScheduler == nullptr) {
        return;
    }

    jsTaskScheduler->dispatchOnJsThread(
        context,
        resolveScheduleType(shouldUpdateSync),
        0,
        [self = strongSmallRef(this), viewModel, updateId](JavaScriptEntryParameters& jsEntry) {
            self->doOnViewModelUpdate(jsEntry, viewModel);
            jsEntry.valdiContext->markUpdateCompleted(updateId);
        });
}

void JavaScriptComponentContextHandler::onAttributeChange(const Ref<Context>& context,
                                                          RawViewNodeId viewNodeId,
                                                          const StringBox& attributeName,
                                                          const Value& attributeValue,
                                                          bool shouldUpdateSync) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (jsTaskScheduler == nullptr) {
        return;
    }

    jsTaskScheduler->dispatchOnJsThread(
        context,
        resolveScheduleType(shouldUpdateSync),
        0,
        [self = strongSmallRef(this), viewNodeId, attributeName, attributeValue](auto& jsEntry) {
            self->doOnAttributeChange(jsEntry, viewNodeId, attributeName, attributeValue);
        });
}

void JavaScriptComponentContextHandler::onRuntimeIssue(const Ref<Context>& context,
                                                       bool isError,
                                                       const StringBox& message,
                                                       bool shouldUpdateSync) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (jsTaskScheduler == nullptr) {
        return;
    }

    jsTaskScheduler->dispatchOnJsThread(context,
                                        resolveScheduleType(shouldUpdateSync),
                                        0,
                                        [self = strongSmallRef(this), isError, message](auto& jsEntry) {
                                            self->doOnRuntimeIssue(jsEntry, isError, message);
                                        });
}

void JavaScriptComponentContextHandler::doOnCreate(JavaScriptEntryParameters& jsEntry,
                                                   const Shared<ValueConvertible>& viewModel,
                                                   const Shared<ValueConvertible>& componentContext) {
    if (jsEntry.valdiContext->isDestroyed()) {
        // Bail context creation immediately if the context was destroyed before we got to create it
        return;
    }

    const auto& componentPath = jsEntry.valdiContext->getPath();
    ScopedMetrics metrics = Metrics::scopedOnCreateLatency(_listener != nullptr ? _listener->getMetrics() : nullptr,
                                                           componentPath.getResourceId().bundleName);

    auto componentPathString = componentPath.toString();
    VALDI_TRACE_META("Valdi.setupRootComponent", componentPathString);

    _contextsById[jsEntry.valdiContext->getContextId()] = jsEntry.valdiContext;

    auto viewModelAsValue = Value::undefined();
    if (viewModel != nullptr) {
        viewModelAsValue = viewModel->toValue();
    }
    auto componentContextAsValue = Value::undefined();
    if (componentContext != nullptr) {
        componentContextAsValue = componentContext->toValue();
    }

    Ref<Context> parentContext;
    resolveParentContext(viewModelAsValue, parentContext);
    resolveParentContext(componentContextAsValue, parentContext);

    if (parentContext != nullptr && parentContext->getContextId() != 1 /* Ignore root context */) {
        if (parentContext->isDestroyed() && Context::isDestroyedContextFixEnabled()) {
            VALDI_WARN(_logger,
                       "Skipping destroyed parent context {} for {}",
                       parentContext->getContextId(),
                       componentPathString);
        } else {
            VALDI_DEBUG(_logger,
                        "Resolved parent context of {} to {}",
                        componentPathString,
                        parentContext->getPath().toString());
            jsEntry.valdiContext->setParent(parentContext);
            parentContext->getRoot()->retainDisposables();
        }
    }

    auto jsComponentPath = jsEntry.jsContext.newStringUTF8(componentPathString, jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    static auto kViewModelKey = STRING_LITERAL("viewModel");

    auto jsViewModel = valueToJSValue(jsEntry.jsContext,
                                      viewModelAsValue,
                                      ReferenceInfoBuilder().withObject(kViewModelKey),
                                      jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    static auto kContextKey = STRING_LITERAL("context");

    auto jsContext = valueToJSValue(jsEntry.jsContext,
                                    componentContextAsValue,
                                    ReferenceInfoBuilder().withObject(kContextKey),
                                    jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    auto jsContextId = makeJsContextId(jsEntry);

    std::initializer_list<JSValueRef> parameters = {
        std::move(jsContextId), std::move(jsComponentPath), std::move(jsViewModel), std::move(jsContext)};

    JSFunctionCallContext callContext(
        jsEntry.jsContext, parameters.begin(), parameters.size(), jsEntry.exceptionTracker);

    callJsContextHandlerFunction(kCreatePropertyName, jsEntry, callContext);
}

void JavaScriptComponentContextHandler::doOnDestroy(JavaScriptEntryParameters& jsEntry) {
    if (_contextsById.find(jsEntry.valdiContext->getContextId()) == _contextsById.end()) {
        return;
    }

    ScopedMetrics metrics = Metrics::scopedOnDestroyLatency(_listener != nullptr ? _listener->getMetrics() : nullptr,
                                                            jsEntry.valdiContext->getPath().getResourceId().bundleName);

    auto jsContextId = makeJsContextId(jsEntry);
    JSFunctionCallContext callContext(jsEntry.jsContext, &jsContextId, 1, jsEntry.exceptionTracker);

    callJsContextHandlerFunction(kDestroyPropertyName, jsEntry, callContext);

    deassociateContext(jsEntry.valdiContext->getContextId());
}

void JavaScriptComponentContextHandler::doOnViewModelUpdate(JavaScriptEntryParameters& jsEntry,
                                                            const Shared<ValueConvertible>& viewModel) {
    const auto& it = _contextsById.find(jsEntry.valdiContext->getContextId());
    if (it == _contextsById.end()) {
        return;
    }

    if (jsEntry.valdiContext->isDestroyed()) {
        // Ignore view model update if the context was destroyed before we got to process it
        return;
    }

    ScopedMetrics metrics =
        Metrics::scopedOnViewModelUpdatedLatency(_listener != nullptr ? _listener->getMetrics() : nullptr,
                                                 jsEntry.valdiContext->getPath().getResourceId().bundleName);

    static auto kViewModelKey = STRING_LITERAL("viewModel");

    auto result = valueToJSValue(jsEntry.jsContext,
                                 viewModel != nullptr ? viewModel->toValue() : Value::undefined(),
                                 ReferenceInfoBuilder().withObject(kViewModelKey),
                                 jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    auto jsContextId = makeJsContextId(jsEntry);

    std::initializer_list<JSValueRef> parameters = {
        std::move(jsContextId),
        std::move(result),
    };

    JSFunctionCallContext callContext(
        jsEntry.jsContext, parameters.begin(), parameters.size(), jsEntry.exceptionTracker);

    callJsContextHandlerFunction(kRenderPropertyName, jsEntry, callContext);
}

void JavaScriptComponentContextHandler::doOnRuntimeIssue(JavaScriptEntryParameters& jsEntry,
                                                         bool isError,
                                                         const StringBox& message) {
    const auto& it = _contextsById.find(jsEntry.valdiContext->getContextId());
    if (it == _contextsById.end()) {
        return;
    }

    auto jsIsError = jsEntry.jsContext.newBool(isError);
    auto jsMessage = jsEntry.jsContext.newStringUTF8(message.toStringView(), jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        onFailedToNotifyRuntimeIssue(jsEntry);
        return;
    }

    auto jsContextId = makeJsContextId(jsEntry);

    std::initializer_list<JSValueRef> parameters = {std::move(jsContextId), std::move(jsIsError), std::move(jsMessage)};

    JSFunctionCallContext callContext(
        jsEntry.jsContext, parameters.begin(), parameters.size(), jsEntry.exceptionTracker);

    callJsContextHandlerFunction(kOnRuntimeIssuePropertyName, jsEntry, callContext);

    if (!jsEntry.exceptionTracker) {
        onFailedToNotifyRuntimeIssue(jsEntry);
        return;
    }
}

void JavaScriptComponentContextHandler::deassociateContext(ContextId contextId) {
    const auto& it = _contextsById.find(contextId);
    if (it != _contextsById.end()) {
        auto context = it->second;
        _contextsById.erase(it);

        if (context->getParent() != nullptr) {
            context->getParent()->getRoot()->releaseDisposables();
        }
    }
}

void JavaScriptComponentContextHandler::callComponentFunction(const Ref<Context>& context,
                                                              const StringBox& functionName,
                                                              const Ref<ValueArray>& parameters) {
    auto jsTaskScheduler = _jsTaskScheduler.lock();
    if (jsTaskScheduler == nullptr) {
        return;
    }

    jsTaskScheduler->dispatchOnJsThreadAsync(context,
                                             [self = strongSmallRef(this), functionName, parameters](auto& jsEntry) {
                                                 self->doCallComponentFunction(jsEntry, functionName, parameters);
                                             });
}

Result<Ref<Context>> JavaScriptComponentContextHandler::getContextForId(ContextId contextId) const {
    const auto& it = _contextsById.find(contextId);
    if (it == _contextsById.end()) {
        return Error(STRING_FORMAT("No Context associated with id {}", contextId));
    }

    return it->second;
}

void JavaScriptComponentContextHandler::doCallComponentFunction(JavaScriptEntryParameters& jsEntry,
                                                                const StringBox& functionName,
                                                                const Ref<ValueArray>& parameters) {
    const auto& it = _contextsById.find(jsEntry.valdiContext->getContextId());
    if (it == _contextsById.end()) {
        jsEntry.exceptionTracker.onError(Error("ContextId not associated"));
        return;
    }

    auto jsFunctionName = jsEntry.jsContext.newStringUTF8(functionName.toStringView(), jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    JSValueRef jsParameters;

    if (parameters != nullptr && !parameters->empty()) {
        auto referenceInfo = ReferenceInfoBuilder().withProperty(functionName).asFunction().withParameter(2).build();
        jsParameters =
            jsEntry.jsContext.newArrayWithValues(parameters->size(), jsEntry.exceptionTracker, [&](size_t i) {
                return valueToJSValue(jsEntry.jsContext,
                                      (*parameters)[i],
                                      ReferenceInfoBuilder(referenceInfo).withArrayIndex(i),
                                      jsEntry.exceptionTracker);
            });
        if (!jsEntry.exceptionTracker) {
            return;
        }
    } else {
        jsParameters = jsEntry.jsContext.newUndefined();
    }

    auto jsContextId = makeJsContextId(jsEntry);

    std::initializer_list<JSValueRef> callParameters = {
        std::move(jsContextId), std::move(jsFunctionName), std::move(jsParameters)};

    JSFunctionCallContext callContext(
        jsEntry.jsContext, callParameters.begin(), callParameters.size(), jsEntry.exceptionTracker);

    callJsContextHandlerFunction(kCallComponentFunctionPropertyName, jsEntry, callContext);
}

void JavaScriptComponentContextHandler::doOnAttributeChange(JavaScriptEntryParameters& jsEntry,
                                                            RawViewNodeId viewNodeId,
                                                            const StringBox& attributeName,
                                                            const Value& attributeValue) {
    auto jsViewNodeId = jsEntry.jsContext.newNumber(static_cast<int32_t>(viewNodeId));

    auto attributeNameResult = jsEntry.jsContext.newStringUTF8(attributeName.toStringView(), jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    auto attributeValueResult = valueToJSValue(jsEntry.jsContext,
                                               attributeValue,
                                               ReferenceInfoBuilder().withProperty(attributeName),
                                               jsEntry.exceptionTracker);
    if (!jsEntry.exceptionTracker) {
        return;
    }

    auto jsContextId = makeJsContextId(jsEntry);

    std::initializer_list<JSValueRef> callParameters = {
        std::move(jsContextId),
        std::move(jsViewNodeId),
        std::move(attributeNameResult),
        std::move(attributeValueResult),
    };

    JSFunctionCallContext callContext(
        jsEntry.jsContext, callParameters.begin(), callParameters.size(), jsEntry.exceptionTracker);

    callJsContextHandlerFunction(kAttributeChangePropertyName, jsEntry, callContext);
}

JSValueRef JavaScriptComponentContextHandler::callJsContextHandlerFunction(size_t propertyNameIndex,
                                                                           JavaScriptEntryParameters& jsEntry,
                                                                           JSFunctionCallContext& callContext) {
    if (_jsContextHandler.empty() && _listener != nullptr) {
        _listener->requestUpdateJsContextHandler(jsEntry, *this);
        CHECK_CALL_CONTEXT(callContext);
    }

    if (!_jsContextHandler) {
        auto error = _jsContextHandler.empty() ? Error("No JSContextHandler associated") : _jsContextHandler.error();
        return callContext.throwError(std::move(error));
    }

    _propertyNames.setContext(&callContext.getContext());

    const auto& jsContextHandler = _jsContextHandler.value();
    if (jsContextHandler == nullptr) {
        return callContext.throwError(Error("No JS context handler associated"));
    }
    auto jsValue = jsContextHandler->getJsValue(jsEntry.jsContext, callContext.getExceptionTracker());
    CHECK_CALL_CONTEXT(callContext);

    VALDI_TRACE_META("Valdi.rootComponentEntry", _propertyNames.getCppName(propertyNameIndex));

    return callContext.getContext().callObjectProperty(
        jsValue, _propertyNames.getJsName(propertyNameIndex), callContext);
}

void JavaScriptComponentContextHandler::onFailedToNotifyRuntimeIssue(JavaScriptEntryParameters& jsEntry) {
    auto error = jsEntry.exceptionTracker.extractError();
    VALDI_ERROR(_logger, "Failed to handle onRuntimeIssue: {}", error);
}

} // namespace Valdi
