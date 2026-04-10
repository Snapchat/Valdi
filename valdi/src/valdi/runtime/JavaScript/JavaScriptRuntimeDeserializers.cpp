//
//  JavaScriptRuntimeDeserializers.cpp
//  valdi-ios
//
//  Created by Simon Corsin on 7/18/19.
//

#include "valdi/runtime/JavaScript/JavaScriptRuntimeDeserializers.hpp"

#include "utils/time/StopWatch.hpp"
#include "valdi/runtime/CSS/StyleAttributesCache.hpp"
#include "valdi/runtime/JavaScript/JavaScriptStringCache.hpp"
#include "valdi/runtime/JavaScript/JavaScriptUtils.hpp"
#include "valdi/runtime/JavaScript/ValueFunctionWithJSValue.hpp"
#include "valdi/runtime/Rendering/AnimationOptions.hpp"
#include "valdi/runtime/Rendering/RenderRequest.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/StringCache.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"

#include "valdi/runtime/Context/ContextManager.hpp"

#include <fmt/format.h>

namespace Valdi {

struct RenderRequestDescriptor {
    ContextId treeId;
    JSTypedArray descriptor;
    int64_t descriptorSize;
    JSValue values;
    size_t valuesLength = 0;
    Value visibilityObserver;
    Value frameObserver;

    RenderRequestDescriptor();
};

RenderRequestDescriptor::RenderRequestDescriptor() {
    treeId = ContextIdNull;
    descriptorSize = 0;
}

STRING_CONST(animateMessageOptionsCurveLinear, "linear")
STRING_CONST(animateMessageOptionsCurveEaseIn, "easeIn")
STRING_CONST(animateMessageOptionsCurveEaseOut, "easeOut")
STRING_CONST(animateMessageOptionsCurveEaseInOut, "easeInOut")

constexpr size_t kTreeIdPropertyName = 0;
constexpr size_t kDescriptorPropertyName = 1;
constexpr size_t kDescriptorSizePropertyName = 2;
constexpr size_t kValuesPropertyName = 3;
constexpr size_t kVisibilityObserverPropertyName = 4;
constexpr size_t kFrameObserverPropertyName = 5;

JavaScriptRuntimeDeserializers::JavaScriptRuntimeDeserializers(IJavaScriptContext& jsContext,
                                                               JavaScriptStringCache& stringCache,
                                                               StyleAttributesCache& styleAttributesCache)
    : _jsContext(jsContext), _stringCache(stringCache), _styleAttributesCache(styleAttributesCache) {
    _propertyNames.set(kTreeIdPropertyName, "treeId");
    _propertyNames.set(kDescriptorPropertyName, "descriptor");
    _propertyNames.set(kDescriptorSizePropertyName, "descriptorSize");
    _propertyNames.set(kValuesPropertyName, "values");
    _propertyNames.set(kVisibilityObserverPropertyName, "visibilityObserver");
    _propertyNames.set(kFrameObserverPropertyName, "frameObserver");

    _propertyNames.setContext(&jsContext);
}

JavaScriptRuntimeDeserializers::~JavaScriptRuntimeDeserializers() = default;

static void disableCallIfContextIsDestroyed(const Value& value) {
    if (!value.isFunction()) {
        return;
    }

    auto* jsValueFunction = dynamic_cast<ValueFunctionWithJSValue*>(value.getFunction());
    if (jsValueFunction == nullptr) {
        return;
    }

    jsValueFunction->setIgnoreIfValdiContextIsDestroyed(true);
}

Result<snap::valdi_core::AnimationType> animationTypeFromString(const StringBox& str) {
    if (str == animateMessageOptionsCurveLinear()) {
        return snap::valdi_core::AnimationType::Linear;
    } else if (str == animateMessageOptionsCurveEaseIn()) {
        return snap::valdi_core::AnimationType::EaseIn;
    } else if (str == animateMessageOptionsCurveEaseOut()) {
        return snap::valdi_core::AnimationType::EaseOut;
    } else if (str == animateMessageOptionsCurveEaseInOut()) {
        return snap::valdi_core::AnimationType::EaseInOut;
    }

    return Error(STRING_FORMAT("Unrecognized curve '{}', supported values are {}, {}, {} or {}",
                               str,
                               animateMessageOptionsCurveLinear(),
                               animateMessageOptionsCurveEaseIn(),
                               animateMessageOptionsCurveEaseOut(),
                               animateMessageOptionsCurveEaseInOut()));
}

static double consumeDouble(const uint32_t* descriptor, size_t* current) {
    double value = *reinterpret_cast<const double*>(&descriptor[*current]);
    *current += 2;
    return value;
}

static Ref<RenderRequest> onParseError(JSExceptionTracker& exceptionTracker) {
    exceptionTracker.onError("Invalid buffer descriptor");
    return nullptr;
}

const Value& JavaScriptRuntimeDeserializers::getAttachedValue(const RenderRequestDescriptor& requestDescriptor,
                                                              const Ref<ValueArray>& values,
                                                              size_t index,
                                                              const ReferenceInfoBuilder& referenceInfoBuilder,
                                                              JSExceptionTracker& exceptionTracker) const {
    if (values == nullptr || index >= values->size()) {
        onParseError(exceptionTracker);
        return Value::undefinedRef();
    }

    auto& entry = (*values)[index];
    if (entry.isNull()) {
        auto propertyValue = _jsContext.getObjectPropertyForIndex(requestDescriptor.values, index, exceptionTracker);
        if (!exceptionTracker) {
            return entry;
        }

        entry = jsValueToValue(_jsContext, propertyValue.get(), referenceInfoBuilder, exceptionTracker);
    }

    return entry;
}

Ref<RenderRequest> JavaScriptRuntimeDeserializers::parseRequestDescriptor(
    const RenderRequestDescriptor& requestDescriptor, JSExceptionTracker& exceptionTracker) {
    static auto kAnimationCompletion = STRING_LITERAL("animationCompletion");
    static auto kOnLayoutComplete = STRING_LITERAL("onLayoutComplete");
    static auto kOnNextDraw = STRING_LITERAL("onNextDraw");

    auto renderRequest = Valdi::makeShared<RenderRequest>();
    renderRequest->setContextId(requestDescriptor.treeId);

    const auto* descriptor = reinterpret_cast<const uint32_t*>(requestDescriptor.descriptor.data);

    auto valuesSize = requestDescriptor.valuesLength;
    Ref<ValueArray> values = valuesSize > 0 ? ValueArray::make(valuesSize) : nullptr;
    auto length = static_cast<size_t>(requestDescriptor.descriptorSize);
    if (requestDescriptor.descriptor.length / 4 < length) {
        return onParseError(exceptionTracker);
    }

    size_t current = 0;
    while (current < length) {
        auto typeHeader = descriptor[current++];
        auto type = typeHeader & 0xff;

        RawViewNodeId nodeId = 0;

        if (type < 14) {
            // Those are always tied to a node id.
            nodeId = static_cast<RawViewNodeId>(typeHeader >> 8);
        }

        /**
         * See protocol definition in JSXRendererDelegate.ts
         */

        if (type == 1) {
            if (current + 1 > length) {
                return onParseError(exceptionTracker);
            }
            auto viewClassIndex = descriptor[current++];

            auto viewClass = _stringCache.get(viewClassIndex);
            if (!viewClass) {
                return onParseError(exceptionTracker);
            }
            auto* entry = renderRequest->appendCreateElement();
            entry->setElementId(nodeId);
            entry->setViewClassName(std::move(*viewClass));
        } else if (type == 2) {
            renderRequest->appendDestroyElement()->setElementId(nodeId);
        } else if (type == 3) {
            renderRequest->appendSetRootElement()->setElementId(nodeId);
        } else if (type == 4) {
            // moveElementToParent

            if (current + 2 > length) {
                return onParseError(exceptionTracker);
            }

            auto parentId = descriptor[current++] & 0xffffff;
            auto parentIndex = descriptor[current++];

            auto* entry = renderRequest->appendMoveElementToParent();
            entry->setElementId(nodeId);
            entry->setParentElementId(static_cast<RawViewNodeId>(parentId));
            entry->setParentIndex(static_cast<int>(parentIndex));
        } else if (type >= 5 && type <= 13) {
            if (current + 1 > length) {
                return onParseError(exceptionTracker);
            }

            auto attributeHeader = descriptor[current++];
            auto attributeId = static_cast<AttributeId>(attributeHeader & 0xffffff);
            auto isInjected = (attributeHeader >> 24) != 0;

            auto* entry = renderRequest->appendSetElementAttribute();
            entry->setElementId(nodeId);
            entry->setAttributeId(attributeId);
            entry->setInjectedFromParent(isInjected);

            if (type == 5) {
                // undefined
                entry->setAttributeValue(Value::undefined());
            } else if (type == 6) {
                // null
                entry->setAttributeValue(Value());
            } else if (type == 7) {
                // false
                entry->setAttributeValue(Value(false));
            } else if (type == 8) {
                // true
                entry->setAttributeValue(Value(true));
            } else if (type == 9) {
                // int
                if (current + 1 > length) {
                    return onParseError(exceptionTracker);
                }

                auto i = descriptor[current++];

                // Convert to double to avoid regressions for now.
                // We should add full support for passing ints from JS
                // at some point.
                entry->setAttributeValue(Value(static_cast<double>(i)));
            } else if (type == 10) {
                // number
                if (current + 2 > length) {
                    return onParseError(exceptionTracker);
                }

                double d = consumeDouble(descriptor, &current);

                entry->setAttributeValue(Value(d));
            } else if (type == 11) {
                // array
                if (current + 1 > length) {
                    return onParseError(exceptionTracker);
                }

                auto arrayLength = descriptor[current++];
                if (current + arrayLength > length) {
                    return onParseError(exceptionTracker);
                }

                auto arrayLengthSizeT = static_cast<size_t>(arrayLength);
                auto valueArray = ValueArray::make(arrayLengthSizeT);
                auto attributeName = _styleAttributesCache.getAttributeIds().getNameForId(attributeId);

                for (size_t i = 0; i < arrayLengthSizeT; i++) {
                    auto valueIndex = static_cast<size_t>(descriptor[current++]);

                    valueArray->emplace(
                        i,
                        getAttachedValue(requestDescriptor,
                                         values,
                                         valueIndex,
                                         ReferenceInfoBuilder().withProperty(attributeName).withArrayIndex(i),
                                         exceptionTracker));
                    if (!exceptionTracker) {
                        return nullptr;
                    }
                }

                entry->setAttributeValue(Value(valueArray));
            } else if (type == 12) {
                // CSS style
                if (current + 1 > length) {
                    return onParseError(exceptionTracker);
                }

                auto cssAttributesIndex = static_cast<size_t>(descriptor[current++]);
                auto cssAttributes = _styleAttributesCache.getAttributesForIndex(cssAttributesIndex);
                if (cssAttributes == nullptr) {
                    return onParseError(exceptionTracker);
                }

                entry->setAttributeValue(Value(cssAttributes));
            } else if (type == 13) {
                // Attached value
                if (current + 1 > length) {
                    return onParseError(exceptionTracker);
                }
                auto valueIndex = static_cast<size_t>(descriptor[current++]);
                auto attributeName = _styleAttributesCache.getAttributeIds().getNameForId(attributeId);

                entry->setAttributeValue(getAttachedValue(requestDescriptor,
                                                          values,
                                                          valueIndex,
                                                          ReferenceInfoBuilder().withProperty(attributeName),
                                                          exceptionTracker));
                if (!exceptionTracker) {
                    return nullptr;
                }
                disableCallIfContextIsDestroyed(entry->getAttributeValue());
            }
        } else if (type == 14) {
            if (current + 12 > length) {
                return onParseError(exceptionTracker);
            }

            double duration = consumeDouble(descriptor, &current);
            auto curveInt = descriptor[current++];
            auto beginFromCurrentState = descriptor[current++] != 0;
            auto crossfade = descriptor[current++] != 0;
            double stiffness = consumeDouble(descriptor, &current);
            double damping = consumeDouble(descriptor, &current);
            auto controlPointsIndex = static_cast<size_t>(descriptor[current++]);
            auto completionIndex = static_cast<size_t>(descriptor[current++]);
            auto tokenInt = descriptor[current++];

            if (curveInt < 1 || curveInt - 1 > static_cast<int>(snap::valdi_core::AnimationType::EaseInOut) ||
                controlPointsIndex >= valuesSize || completionIndex >= valuesSize) {
                return onParseError(exceptionTracker);
            }

            auto* entry = renderRequest->appendStartAnimations();
            auto& animationOptions = entry->getAnimationOptions();

            animationOptions.duration = duration;
            animationOptions.type = static_cast<snap::valdi_core::AnimationType>(curveInt - 1);
            animationOptions.beginFromCurrentState = beginFromCurrentState;
            animationOptions.crossfade = crossfade;
            animationOptions.stiffness = stiffness;
            animationOptions.damping = damping;
            animationOptions.completionCallback =
                getAttachedValue(requestDescriptor,
                                 values,
                                 completionIndex,
                                 ReferenceInfoBuilder().withObject(kAnimationCompletion),
                                 exceptionTracker);
            if (!exceptionTracker) {
                return nullptr;
            }
            animationOptions.cancelToken = tokenInt;
            disableCallIfContextIsDestroyed(animationOptions.completionCallback);

            const auto& controlPoints = getAttachedValue(
                requestDescriptor, values, controlPointsIndex, ReferenceInfoBuilder(), exceptionTracker);
            if (!exceptionTracker) {
                return nullptr;
            }
            if (controlPoints.isArray()) {
                for (const auto& value : *controlPoints.getArray()) {
                    animationOptions.controlPoints.push_back(value.toDouble());
                }
            }
        } else if (type == 15) {
            renderRequest->appendEndAnimations();
        } else if (type == 16) {
            if (current + 1 > length) {
                return onParseError(exceptionTracker);
            }
            auto valueIndex = static_cast<size_t>(descriptor[current++]);

            auto* entry = renderRequest->appendOnLayoutComplete();
            entry->setCallback(getAttachedValue(requestDescriptor,
                                                values,
                                                valueIndex,
                                                ReferenceInfoBuilder().withObject(kOnLayoutComplete),
                                                exceptionTracker)
                                   .getFunctionRef());
            if (!exceptionTracker) {
                return nullptr;
            }
        } else if (type == 17) {
            // Cancel animation
            if (current + 1 > length) {
                return onParseError(exceptionTracker);
            }

            auto tokenInt = descriptor[current++];

            auto* entry = renderRequest->appendCancelAnimation();
            entry->setToken(tokenInt);
        } else if (type == 18) {
            if (current + 1 > length) {
                return onParseError(exceptionTracker);
            }
            auto valueIndex = static_cast<size_t>(descriptor[current++]);

            auto* entry = renderRequest->appendOnNextDraw();
            entry->setCallback(getAttachedValue(requestDescriptor,
                                                values,
                                                valueIndex,
                                                ReferenceInfoBuilder().withObject(kOnNextDraw),
                                                exceptionTracker)
                                   .getFunctionRef());
            if (!exceptionTracker) {
                return nullptr;
            }
        } else {
            return onParseError(exceptionTracker);
        }
    }

    renderRequest->setVisibilityObserverCallback(requestDescriptor.visibilityObserver);
    renderRequest->setFrameObserverCallback(requestDescriptor.frameObserver);

    return renderRequest;
}

Ref<RenderRequest> JavaScriptRuntimeDeserializers::deserializeRenderRequest(const JSValue& jsValue,
                                                                            const ReferenceInfo& referenceInfo,
                                                                            JSExceptionTracker& exceptionTracker) {
    static auto kVisibilityObserver = STRING_LITERAL("visibilityObserver");
    static auto kFrameObserver = STRING_LITERAL("frameObserver");

    VALDI_TRACE("Valdi.jsRenderRequestToCpp");

    RenderRequestDescriptor descriptor;

    auto treeId =
        _jsContext.getObjectProperty(jsValue, _propertyNames.getJsName(kTreeIdPropertyName), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    auto descriptorResult =
        _jsContext.getObjectProperty(jsValue, _propertyNames.getJsName(kDescriptorPropertyName), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    auto descriptorSizeResult =
        _jsContext.getObjectProperty(jsValue, _propertyNames.getJsName(kDescriptorSizePropertyName), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    auto valuesResult =
        _jsContext.getObjectProperty(jsValue, _propertyNames.getJsName(kValuesPropertyName), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    auto visibilityObserverResult = _jsContext.getObjectProperty(
        jsValue, _propertyNames.getJsName(kVisibilityObserverPropertyName), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    auto frameObserverResult =
        _jsContext.getObjectProperty(jsValue, _propertyNames.getJsName(kFrameObserverPropertyName), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    descriptor.treeId = static_cast<ContextId>(_jsContext.valueToInt(treeId.get(), exceptionTracker));
    if (!exceptionTracker) {
        return nullptr;
    }
    descriptor.descriptor = _jsContext.valueToTypedArray(descriptorResult.get(), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    descriptor.descriptorSize =
        static_cast<int64_t>(_jsContext.valueToInt(descriptorSizeResult.get(), exceptionTracker));
    if (!exceptionTracker) {
        return nullptr;
    }

    descriptor.values = valuesResult.get();
    descriptor.valuesLength = jsArrayGetLength(_jsContext, valuesResult.get(), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    descriptor.visibilityObserver = jsValueToValue(_jsContext,
                                                   visibilityObserverResult.get(),
                                                   ReferenceInfoBuilder().withProperty(kVisibilityObserver),
                                                   exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }
    descriptor.frameObserver = jsValueToValue(
        _jsContext, frameObserverResult.get(), ReferenceInfoBuilder().withProperty(kFrameObserver), exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    return parseRequestDescriptor(descriptor, exceptionTracker);
}

} // namespace Valdi
