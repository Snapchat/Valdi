//
//  ValueMarshallerFunctionTrampoline.hpp
//  valdi_core
//
//  Created by Simon Corsin on 1/25/23.
//

#pragma once

#include "valdi_core/cpp/Threading/IDispatchQueue.hpp"
#include "valdi_core/cpp/Utils/InlineContainerAllocator.hpp"
#include "valdi_core/cpp/Utils/PlatformValueDelegate.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"
#include <fmt/format.h>
#include <vector>

namespace Valdi {

template<typename ValueType>
class ValueMarshallerFunctionTrampoline : public PlatformFunctionTrampoline<ValueType> {
public:
    ValueMarshallerFunctionTrampoline(Ref<ValueMarshaller<ValueType>> returnValueMarshaller,
                                      size_t parametersSize,
                                      ValueFunctionFlags callFlags,
                                      bool isPromiseReturnType,
                                      const Ref<IDispatchQueue>& callQueue,
                                      const StringBox& cppToPlatformTraceName,
                                      const StringBox& platformToCppTraceName)
        : _returnValueMarshaller(std::move(returnValueMarshaller)),
          _parametersSize(parametersSize),
          _callFlags(callFlags),
          _isPromiseReturnType(isPromiseReturnType),
          _callQueue(callQueue),
          _cppToPlatformTraceName(cppToPlatformTraceName),
          _platformToCppTraceName(platformToCppTraceName) {
        InlineContainerAllocator<ValueMarshallerFunctionTrampoline<ValueType>, Ref<ValueMarshaller<ValueType>>>
            allocator;
        allocator.constructContainerEntries(this, parametersSize);
    }

    ~ValueMarshallerFunctionTrampoline() override {
        InlineContainerAllocator<ValueMarshallerFunctionTrampoline<ValueType>, Ref<ValueMarshaller<ValueType>>>
            allocator;
        allocator.deallocate(this, _parametersSize);
    }

    void setParameterValueMarshaller(size_t index, Ref<ValueMarshaller<ValueType>> parameterValueMarshaller) {
        InlineContainerAllocator<ValueMarshallerFunctionTrampoline<ValueType>, Ref<ValueMarshaller<ValueType>>>
            allocator;
        allocator.getContainerStartPtr(this)[index] = std::move(parameterValueMarshaller);
    }

    const Ref<ValueMarshaller<ValueType>>& getParameterValueMarshaller(size_t index) const {
        InlineContainerAllocator<ValueMarshallerFunctionTrampoline<ValueType>, Ref<ValueMarshaller<ValueType>>>
            allocator;
        return allocator.getContainerStartPtr(this)[index];
    }

    std::optional<ValueType> forwardCall(ValueFunction* valueFunction,
                                         const ValueType* parameters,
                                         size_t parametersSize,
                                         const ReferenceInfoBuilder& referenceInfoBuilder,
                                         const StringBox* precomputedFunctionIdentifier,
                                         ExceptionTracker& exceptionTracker) final {
        SmallVector<Value, 8> convertedParameters;
        auto resolvedParametersSize = std::min(_parametersSize, parametersSize);
        if (resolvedParametersSize > 0) {
            VALDI_TRACE(_platformToCppTraceName);
            for (size_t i = 0; i < resolvedParametersSize; i++) {
                const auto& parameter = parameters[i];
                const auto& parameterMarshaller = getParameterValueMarshaller(i);

                convertedParameters.emplace_back(parameterMarshaller->marshall(
                    nullptr, parameter, referenceInfoBuilder.withParameter(i), exceptionTracker));
                if (!exceptionTracker) {
                    exceptionTracker.onError(fmt::format("Failed to marshall parameter '{}' of function {}",
                                                         i,
                                                         referenceInfoBuilder.build().toString()));
                    return std::nullopt;
                }
            }
        }

        ValueFunctionCallContext callContext(
            _callFlags, convertedParameters.data(), convertedParameters.size(), exceptionTracker);
        callContext.setFunctionIdentifier(precomputedFunctionIdentifier);

        auto unconvertedReturnValue = (*valueFunction)(callContext);
        if (!exceptionTracker) {
            return std::nullopt;
        }

        if (_isPromiseReturnType && unconvertedReturnValue.isUndefined()) {
            // A Promise-returning function can only yield 'undefined' when the JS call was
            // skipped or failed without the error reaching this tracker: the runtime was
            // torn down (dead task scheduler / disposed JS value) or the JS-side error was
            // swallowed because Promise calls don't set PropagatesError. Unmarshalling
            // 'undefined' would raise a fatal platform exception at the call site, so
            // deliver the failure as a rejected promise instead — the same outcome
            // callPromise() produces for these failures when called off the JS thread.
            auto rejectedPromise = makeShared<ResolvablePromise>();
            auto errorMessage = fmt::format("Function {} returned no value because the call was skipped or failed "
                                            "(runtime destroyed or JS error swallowed)",
                                            referenceInfoBuilder.build().toString());
            rejectedPromise->fulfill(Result<Value>(Error(std::string_view(errorMessage))));
            unconvertedReturnValue = Value(rejectedPromise);
        }

        if (!_isPromiseReturnType && unconvertedReturnValue.isUndefined() && valueFunction->ownerIsTearingDown()) {
            // The sync counterpart of the Promise guard above. A non-Promise typed function can
            // only yield 'undefined' with a clean tracker when its JS body was skipped during
            // teardown: the owning runtime/context was disposed or requested execution termination
            // between dispatch and execution (see JavaScriptRuntime::makeJsThreadDispatchFunction's
            // _isDisposed early-return), so nothing ran and nothing threw. Unmarshalling 'undefined'
            // into the declared return type would raise a fatal, uncatchable platform exception
            // (SCValdiError) at the call site. Gated on ownerIsTearingDown() so a genuine
            // undefined-return bug from a live context still surfaces below. Deliver a type-safe
            // default instead (platform null for object/optional returns, a typed zero/void/first
            // enum case for non-nullable primitive returns), matching the skipped-call outcome;
            // an object-typed null for a primitive return would itself crash the platform
            // trampoline (std::abort on iOS / NPE on Android).
            return std::make_optional<ValueType>(_returnValueMarshaller->makeDefaultReturnValue(exceptionTracker));
        }

        VALDI_TRACE(_cppToPlatformTraceName);
        auto returnValue = _returnValueMarshaller->unmarshall(
            unconvertedReturnValue, referenceInfoBuilder.withReturnValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError(fmt::format("Failed to unmarshall return value of function {}",
                                                 referenceInfoBuilder.build().toString()));
            return std::nullopt;
        }

        return std::make_optional<ValueType>(std::move(returnValue));
    }

    bool unmarshallParameters(const Value* inputParameters,
                              size_t inputParametersSize,
                              ValueType* outputParameters,
                              size_t outputParametersSize,
                              const ReferenceInfoBuilder& referenceInfoBuilder,
                              ExceptionTracker& exceptionTracker) final {
        auto resolvedParametersSize = std::min(_parametersSize, outputParametersSize);
        if (resolvedParametersSize > 0) {
            VALDI_TRACE(_cppToPlatformTraceName);
            for (size_t i = 0; i < resolvedParametersSize; i++) {
                const auto& parameterMarshaller = getParameterValueMarshaller(i);
                const auto& inputParameter = (i < inputParametersSize) ? inputParameters[i] : Value::undefined();
                outputParameters[i] = parameterMarshaller->unmarshall(
                    inputParameter, referenceInfoBuilder.withParameter(i), exceptionTracker);
                if (!exceptionTracker) {
                    exceptionTracker.onError(fmt::format("Failed to unmarshall parameter '{}' of function {}",
                                                         i,
                                                         referenceInfoBuilder.build().getName()));
                    return false;
                }
            }
        }

        return true;
    }

    Value marshallReturnValue(const ValueType& returnValue,
                              const ReferenceInfoBuilder& referenceInfoBuilder,
                              ExceptionTracker& exceptionTracker) final {
        VALDI_TRACE(_platformToCppTraceName);
        auto value = _returnValueMarshaller->marshall(
            nullptr, returnValue, referenceInfoBuilder.withReturnValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError(
                fmt::format("Failed to marshall return value of function {}", referenceInfoBuilder.build().toString()));
        }

        return value;
    }

    size_t getParametersSize() const final {
        return _parametersSize;
    }

    const Ref<IDispatchQueue>& getCallQueue() const final {
        return _callQueue;
    }

    bool isPromiseReturnType() const final {
        return _isPromiseReturnType;
    }

    static Ref<ValueMarshallerFunctionTrampoline<ValueType>> make(Ref<ValueMarshaller<ValueType>> returnValueMarshaller,
                                                                  size_t parametersSize,
                                                                  ValueFunctionFlags callFlags,
                                                                  bool isPromiseReturnType,
                                                                  const Ref<IDispatchQueue>& callQueue,
                                                                  const StringBox& cppToPlatformTraceName,
                                                                  const StringBox& platformToCppTraceName) {
        InlineContainerAllocator<ValueMarshallerFunctionTrampoline<ValueType>, Ref<ValueMarshaller<ValueType>>>
            allocator;
        return allocator.allocate(parametersSize,
                                  std::move(returnValueMarshaller),
                                  parametersSize,
                                  callFlags,
                                  isPromiseReturnType,
                                  callQueue,
                                  cppToPlatformTraceName,
                                  platformToCppTraceName);
    }

private:
    Ref<ValueMarshaller<ValueType>> _returnValueMarshaller;
    size_t _parametersSize;
    ValueFunctionFlags _callFlags;
    bool _isPromiseReturnType;
    Ref<IDispatchQueue> _callQueue;
    StringBox _cppToPlatformTraceName;
    StringBox _platformToCppTraceName;

    friend InlineContainerAllocator<ValueMarshallerFunctionTrampoline<ValueType>, Ref<ValueMarshaller<ValueType>>>;
};

} // namespace Valdi
