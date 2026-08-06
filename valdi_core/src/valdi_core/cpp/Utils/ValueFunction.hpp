//
//  ValueFunction.hpp
//  valdi-ios
//
//  Created by Simon Corsin on 2/4/19.
//

#pragma once

#include "valdi_core/cpp/Utils/ExceptionTracker.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValdiObject.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"

#include <chrono>

namespace Valdi {

enum ValueFunctionFlags : uint8_t {
    ValueFunctionFlagsNone = 0,
    /**
     Whether the function should be called synchronously if the receiver
     expects to be called in a different thread.
     */
    ValueFunctionFlagsCallSync = 1 << 0,
    /**
     Whether the function should never be called synchronously, even if
     other options would otherwise make the call synchronous. This only
     applies if the receiver expects to be called in a different thread.
     When this option is set, the implementation can synchronously return
    a promise object over the result that will be asynchronously resolved.
     */
    ValueFunctionFlagsNeverCallSync = 1 << 1,
    /**
     Whether the function call can be be ignored if a new call is made before
     this function call finishes. This only
     applies if the receiver expects to be called in a different thread.
     */
    ValueFunctionFlagsAllowThrottling = 1 << 2,
    /**
     Whether the function call should always propagate the error to the caller.
     With some implementations of ValueFunction, a call error might be handled by
     a subsystem and will not be exposed to the caller. When this flag is set,
     the error will always be propagated to the caller.
     */
    ValueFunctionFlagsPropagatesError = 1 << 3,
    /**
     Whether a call that would otherwise block the calling thread until the function
     completes should instead give up after a short deadline. The function still runs
     to completion on its own thread, but its return value is dropped once the deadline
     is exceeded. Only meaningful for calls that would block the main thread; ignored
     otherwise. Use for calls that must not wedge the main thread and whose return value
     is optional, such as touch event delivery.
     */
    ValueFunctionFlagsBoundedMainThreadSync = 1 << 4,
};

class ValueFunctionCallContext {
public:
    constexpr ValueFunctionCallContext(ValueFunctionFlags flags,
                                       const Value* parameters,
                                       size_t parametersSize,
                                       ExceptionTracker& exceptionTracker)
        : _flags(flags),
          _parameters(parameters),
          _parametersSize(parametersSize),
          _exceptionTracker(exceptionTracker) {}

    constexpr const Value* getParameters() const {
        return _parameters;
    }

    constexpr size_t getParametersSize() const {
        return _parametersSize;
    }

    constexpr ValueFunctionFlags getFlags() const {
        return _flags;
    }

    constexpr ExceptionTracker& getExceptionTracker() const {
        return _exceptionTracker;
    }

    constexpr const StringBox* getFunctionIdentifier() const {
        return _functionIdentifier;
    }

    constexpr void setFunctionIdentifier(const StringBox* functionIdentifier) {
        _functionIdentifier = functionIdentifier;
    }

    const Value& getParameter(size_t index) const;

    bool getParameterAsBool(size_t index) const;
    StringBox getParameterAsString(size_t index) const;
    int32_t getParameterAsInt(size_t index) const;
    int64_t getParameterAsLong(size_t index) const;
    double getParameterAsDouble(size_t index) const;
    Ref<ValueArray> getParameterAsArray(size_t index) const;
    Ref<ValueMap> getParameterAsMap(size_t index) const;
    Ref<ValueFunction> getParameterAsFunction(size_t index) const;
    Ref<ValueArray> getParametersAsArray() const;

private:
    ValueFunctionFlags _flags;
    const Value* _parameters;
    size_t _parametersSize;
    ExceptionTracker& _exceptionTracker;
    const StringBox* _functionIdentifier = nullptr;
};

class ValueFunction : public SimpleRefCountable {
public:
    /**
     Call the function with the given parameters.
     */
    Result<Value> operator()(const Value* parameters, size_t size) noexcept;

    /**
     Call the function with the given parameters provided as std::initializer_list.
     */
    Result<Value> operator()(std::initializer_list<Value> parameters) noexcept {
        return (*this)(parameters.begin(), parameters.size());
    }

    /**
     Call the function with no parameters.
     */
    Result<Value> operator()() noexcept;

    /**
     Call the function with the given parameters and call flags.
     */
    Result<Value> operator()(ValueFunctionFlags flags, const Value* parameters, size_t size) noexcept;
    Result<Value> operator()(ValueFunctionFlags flags, std::initializer_list<Value> parameters) noexcept {
        return (*this)(flags, parameters.begin(), parameters.size());
    }

    /**
     Call the function with the given call context. Return the result as a Value.
     If call fails, the exception tracker inside the call context will be set with an error
     */
    virtual Value operator()(const ValueFunctionCallContext& callContext) noexcept = 0;

    virtual std::string_view getFunctionType() const = 0;

    /**
     * Returns whether the Context which was resolved when emitting this ValueFunction
     * should be made current when this ValueFunction call operator is invoked. This will
     * cause any emitted native objects during this ValueFunction call operator to be assigned
     * to the Context used when creating this ValueFunction, instead of using the Context
     * that was current during the call.
     */
    virtual bool propagatesOwnerContextOnCall() const;

    /**
     Returns whether the function prefers a synchronous calls over async ones.
     */
    virtual bool prefersSyncCalls() const;

    /**
     Call the function with the given parameters and return the return value.
     If the given call flags is not ValueFunctionFlagsCallSync, the return value
     might be undefined if the underlying ValueFunction implementation had to dispatch
     onto another thread.
     */
    Result<Value> call(ValueFunctionFlags flags, const Value* parameters, size_t size) noexcept;
    Result<Value> call(ValueFunctionFlags flags, std::initializer_list<Value> parameters) noexcept {
        return call(flags, parameters.begin(), parameters.size());
    }

    /**
     Try to call the function synchronously and get the result. But if the
     deadline is exceeded before the function is completed, drop the call and
     return an error.  This function implies `ValueFunctionFlagsCallSync`.

     The deadline is not implemented by all function types, but it is
     implemented by JS functions.
     */
    template<class R, class P>
    Result<Value> callSyncWithDeadline(const std::chrono::duration<R, P>& maxDispatchDelay,
                                       Value* parameters,
                                       size_t size) noexcept {
        auto deadline = std::chrono::steady_clock::now() + maxDispatchDelay;
        return callSyncWithDeadline(deadline, parameters, size);
    }

    StringBox getDebugDescription() const;

protected:
    virtual Result<Value> callSyncWithDeadline(const std::chrono::steady_clock::time_point& deadline,
                                               Value* parameters,
                                               size_t size) noexcept {
        return call(ValueFunctionFlags::ValueFunctionFlagsCallSync, parameters, size);
    }
};

} // namespace Valdi
