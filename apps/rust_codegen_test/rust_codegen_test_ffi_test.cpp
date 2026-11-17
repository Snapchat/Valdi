#include "valdi_modules/rust_codegen_test/rust_codegen_test_ffi.h"
#include "valdi_core/cpp/Marshalling/RustValdi.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

extern "C" bool rust_codegen_test_inline_values_do_not_allocate_handles();
extern "C" bool rust_codegen_test_inline_values_materialize_through_marshaller(size_t expectedTrackedHandles);

using std::string_view;

ValdiRustSlice slice(string_view value) {
    return {reinterpret_cast<const uint8_t*>(value.data()), value.size()};
}

void expectOk(ValdiRustStatus status) {
    if (status.code == 0) {
        return;
    }
    const string_view message(reinterpret_cast<const char*>(status.message.data), status.message.len);
    ADD_FAILURE() << message;
    valdi_rust_buffer_drop(status.message);
}

void expectError(ValdiRustStatus status, string_view expectedMessage) {
    ASSERT_NE(status.code, 0);
    const string_view message(reinterpret_cast<const char*>(status.message.data), status.message.len);
    EXPECT_EQ(message, expectedMessage);
    valdi_rust_buffer_drop(status.message);
}

class ValueHandle {
public:
    explicit ValueHandle(ValdiRustHandle* handle = nullptr) : _handle(handle) {}

    ValueHandle(ValueHandle&& other) noexcept : _handle(std::exchange(other._handle, nullptr)) {}

    ValueHandle& operator=(ValueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            _handle = std::exchange(other._handle, nullptr);
        }
        return *this;
    }

    ValueHandle(const ValueHandle&) = delete;
    ValueHandle& operator=(const ValueHandle&) = delete;

    ~ValueHandle() {
        reset();
    }

    ValdiRustHandle* get() const {
        return _handle;
    }

    const void* borrow() const {
        return valdi_rust_value_borrow(_handle);
    }

private:
    void reset() {
        if (_handle != nullptr) {
            valdi_rust_handle_release(_handle);
            _handle = nullptr;
        }
    }

    ValdiRustHandle* _handle;
};

ValueHandle number(double value) {
    return ValueHandle(Valdi::makeRustHandle(Valdi::Value(value)));
}

ValueHandle boolean(bool value) {
    return ValueHandle(Valdi::makeRustHandle(Valdi::Value(value)));
}

ValueHandle string(string_view value) {
    return ValueHandle(Valdi::makeRustHandle(Valdi::Value(value)));
}

ValueHandle ownedBytes(ValdiRustBuffer buffer) {
    return ValueHandle(Valdi::makeRustHandle(Valdi::Value(
        Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::TypedArrayType::Uint8Array, Valdi::takeRustBytes(buffer)))));
}

ValueHandle array(std::initializer_list<const ValueHandle*> values) {
    std::vector<const void*> references;
    references.reserve(values.size());
    for (const auto* value : values) {
        references.push_back(value->borrow());
    }
    ValdiRustHandle* result = nullptr;
    expectOk(valdi_rust_value_new_array({references.data(), references.size()}, &result));
    return ValueHandle(result);
}

ValueHandle object(std::initializer_list<std::pair<string_view, const ValueHandle*>> entries) {
    std::vector<ValdiRustObjectEntry> values;
    values.reserve(entries.size());
    for (const auto& [key, value] : entries) {
        values.push_back({slice(key), value->borrow()});
    }
    ValdiRustHandle* result = nullptr;
    expectOk(valdi_rust_value_new_object({values.data(), values.size()}, &result));
    return ValueHandle(result);
}

ValdiRustStatus invokeStatus(void* module,
                             size_t method,
                             std::initializer_list<const ValueHandle*> arguments,
                             ValdiRustHandle** out) {
    Valdi::SimpleExceptionTracker exceptionTracker;
    Valdi::Marshaller marshaller(exceptionTracker);
    for (const auto* argument : arguments) {
        marshaller.push(Valdi::getRustHandleValue<Valdi::Value>(argument->get()));
    }
    const auto status = valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_invoke(
        module, method, {&marshaller, arguments.size()});
    if (status.code != 0) {
        return status;
    }
    if (static_cast<size_t>(marshaller.size()) != arguments.size() + 1) {
        return Valdi::makeRustErrorStatus("Rust module did not produce exactly one Valdi value");
    }
    *out = Valdi::makeRustHandle(marshaller.get(static_cast<int>(arguments.size())));
    return status;
}

ValueHandle invoke(void* module, size_t method, std::initializer_list<const ValueHandle*> arguments) {
    ValdiRustHandle* result = nullptr;
    expectOk(invokeStatus(module, method, arguments, &result));
    return ValueHandle(result);
}

double asNumber(const ValueHandle& value) {
    double result = 0;
    expectOk(valdi_rust_value_ref_get_number(value.borrow(), &result));
    return result;
}

std::string asString(const ValueHandle& value) {
    ValdiRustString result{};
    expectOk(valdi_rust_value_ref_get_string(value.borrow(), &result));
    std::string string(reinterpret_cast<const char*>(result.slice.data), result.slice.len);
    valdi_rust_buffer_drop(result.owned);
    return string;
}

ValueHandle property(const ValueHandle& value, string_view key) {
    const void* property = nullptr;
    expectOk(valdi_rust_value_ref_get_property(value.borrow(), slice(key), &property));
    ValdiRustHandle* result = nullptr;
    expectOk(valdi_rust_value_clone_ref(property, &result));
    return ValueHandle(result);
}

struct FutureObservation {
    size_t successes = 0;
    size_t errors = 0;
    size_t drops = 0;
    double value = 0;
};

void observeFutureSuccess(void* context, const void* value) {
    auto& observation = *static_cast<FutureObservation*>(context);
    observation.successes++;
    expectOk(valdi_rust_value_ref_get_number(value, &observation.value));
}

void observeFutureError(void* context, ValdiRustSlice) {
    static_cast<FutureObservation*>(context)->errors++;
}

void dropFutureObservation(void* context) {
    static_cast<FutureObservation*>(context)->drops++;
}

TEST(RustCodegenFfiTest, TransfersOwnedRustBytesWithoutCopying) {
    const uint8_t source[] = {1, 2, 3, 4};
    auto buffer = valdi_rust_buffer_clone({source, sizeof(source)});
    const auto* transferredData = buffer.data;
    auto value = ownedBytes(buffer);
    ValdiRustSlice returned{};
    expectOk(valdi_rust_value_ref_get_bytes(value.borrow(), &returned));

    EXPECT_EQ(returned.data, transferredData);
    ASSERT_EQ(returned.len, sizeof(source));
    EXPECT_EQ(std::memcmp(returned.data, source, sizeof(source)), 0);
}

TEST(RustCodegenFfiTest, RetainedByteViewKeepsTheValdiBufferAliveWithoutCopying) {
    const uint8_t source[] = {5, 6, 7, 8};
    ValdiRustBytesView retained{};
    const uint8_t* originalData = nullptr;

    {
        auto buffer = valdi_rust_buffer_clone({source, sizeof(source)});
        originalData = buffer.data;
        auto value = ownedBytes(buffer);
        expectOk(valdi_rust_value_ref_get_bytes_view(value.borrow(), &retained));
        EXPECT_EQ(retained.data, originalData);
        valdi_rust_bytes_view_retain(retained.owner);
    }

    Valdi::SimpleExceptionTracker exceptionTracker;
    Valdi::Marshaller marshaller(exceptionTracker);
    valdi_rust_marshaller_push_bytes_view(&marshaller, retained.owner);
    valdi_rust_bytes_view_release(retained.owner);

    ValdiRustSlice returned{};
    expectOk(valdi_rust_value_ref_get_bytes(marshaller.getValues(), &returned));
    EXPECT_EQ(returned.data, originalData);
    ASSERT_EQ(returned.len, sizeof(source));
    EXPECT_EQ(std::memcmp(returned.data, source, sizeof(source)), 0);
}

TEST(RustCodegenFfiTest, RustEchoRetainsAndReturnsTheOriginalValdiBuffer) {
    const auto baseline = valdi_rust_handle_live_count();
    auto* module = valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_create();
    ASSERT_NE(module, nullptr);

    {
        const uint8_t source[] = {9, 10, 11, 12};
        auto buffer = valdi_rust_buffer_clone({source, sizeof(source)});
        const auto* originalData = buffer.data;
        auto input = ownedBytes(buffer);
        auto output = invoke(
            module,
            valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_echo_bytes,
            {&input});
        input = ValueHandle();

        ValdiRustSlice returned{};
        expectOk(valdi_rust_value_ref_get_bytes(output.borrow(), &returned));
        EXPECT_EQ(returned.data, originalData);
        ASSERT_EQ(returned.len, sizeof(source));
        EXPECT_EQ(std::memcmp(returned.data, source, sizeof(source)), 0);
    }

    valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_drop(module);
    EXPECT_EQ(valdi_rust_handle_live_count(), baseline);
}

TEST(RustCodegenFfiTest, ReturnsStatusesForValueErrors) {
    auto scalar = number(42);
    ValdiRustString stringResult{};
    expectError(valdi_rust_value_ref_get_string(scalar.borrow(), &stringResult), "Valdi value is not a string");

    auto values = array({&scalar});
    const void* arrayValue = nullptr;
    expectError(valdi_rust_value_ref_array_get(values.borrow(), 1, &arrayValue), "Valdi array index out of bounds");

    void* schema = nullptr;
    expectError(valdi_rust_register_schema(slice(""), static_cast<ValdiRustSchemaKind>(-1), {nullptr, 0}, &schema),
                "Invalid Rust schema kind");
    EXPECT_EQ(schema, nullptr);
}

TEST(RustCodegenFfiTest, PrimitiveAndUndefinedModuleCallsDoNotAllocateBridgeHandles) {
    EXPECT_TRUE(rust_codegen_test_inline_values_do_not_allocate_handles());
#if defined(VALDI_RUST_TRACK_HANDLES)
    constexpr size_t expectedTrackedHandles = 7;
#else
    constexpr size_t expectedTrackedHandles = 0;
#endif
    EXPECT_TRUE(rust_codegen_test_inline_values_materialize_through_marshaller(expectedTrackedHandles));
    const auto baseline = valdi_rust_handle_live_count();
    auto* module = valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_create();
    ASSERT_NE(module, nullptr);

    {
        Valdi::SimpleExceptionTracker exceptionTracker;
        Valdi::Marshaller marshaller(exceptionTracker);
        marshaller.pushDouble(20);
        marshaller.pushDouble(22);
        expectOk(valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_invoke(
            module,
            valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_add,
            {&marshaller, 2}));
        EXPECT_DOUBLE_EQ(marshaller.getDouble(2), 42);
        EXPECT_EQ(valdi_rust_handle_live_count(), baseline);
    }

    {
        Valdi::SimpleExceptionTracker exceptionTracker;
        Valdi::Marshaller marshaller(exceptionTracker);
        marshaller.pushUndefined();
        expectOk(valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_invoke(
            module,
            valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_echo_optional_any,
            {&marshaller, 1}));
        EXPECT_TRUE(marshaller.isNullOrUndefined(1));
        EXPECT_EQ(valdi_rust_handle_live_count(), baseline);
    }

    valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_drop(module);
    EXPECT_EQ(valdi_rust_handle_live_count(), baseline);
}

TEST(RustCodegenFfiTest, CallsRustThroughMarshallerAbiAndBalancesOwnedHandles) {
    const auto baseline = valdi_rust_handle_live_count();

    for (size_t iteration = 0; iteration < 1000; iteration++) {
        auto* module = valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_create();
        ASSERT_NE(module, nullptr);

        {
            auto left = number(20);
            auto right = number(22);
            auto result = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_add,
                {&left, &right});
            EXPECT_DOUBLE_EQ(asNumber(result), 42);

            auto name = string("Rust");
            auto greeting = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_greet,
                {&name});
            EXPECT_EQ(asString(greeting), "Hello, Rust");

            auto label = string("origin");
            auto enabled = boolean(true);
            auto x = number(3);
            auto point = object({{"x", &x}, {"label", &label}, {"enabled", &enabled}});
            auto pointCopy = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_copy_point,
                {&point});
            EXPECT_DOUBLE_EQ(asNumber(property(pointCopy, "x")), 3);
            EXPECT_EQ(asString(property(pointCopy, "label")), "origin");

            auto one = number(1);
            auto two = number(2);
            auto thirtyNine = number(39);
            auto numbers = array({&one, &two, &thirtyNine});
            auto returnedNumbers = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_echo_numbers,
                {&numbers});
            size_t length = 0;
            expectOk(valdi_rust_value_ref_array_len(returnedNumbers.borrow(), &length));
            EXPECT_EQ(length, 3);
            const void* last = nullptr;
            expectOk(valdi_rust_value_ref_array_get(returnedNumbers.borrow(), 2, &last));
            double lastNumber = 0;
            expectOk(valdi_rust_value_ref_get_number(last, &lastNumber));
            EXPECT_DOUBLE_EQ(lastNumber, 39);

            auto callback = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_make_callback,
                {&left});
            auto callbackResult = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_apply_callback,
                {&right, &callback});
            EXPECT_DOUBLE_EQ(asNumber(callbackResult), 42);

            auto future = invoke(
                module,
                valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_make_resolved_future,
                {&right});
            FutureObservation observation;
            expectOk(valdi_rust_value_observe_promise(
                future.borrow(), &observation, observeFutureSuccess, observeFutureError, dropFutureObservation));
            EXPECT_EQ(observation.successes, 1);
            EXPECT_EQ(observation.errors, 0);
            EXPECT_EQ(observation.drops, 1);
            EXPECT_DOUBLE_EQ(observation.value, 22);

            if (iteration == 0) {
                ValdiRustHandle* ignored = nullptr;
                expectError(
                    invokeStatus(
                        module,
                        valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_fail,
                        {},
                        &ignored),
                    "expected Rust failure");
                EXPECT_EQ(ignored, nullptr);
                expectError(
                    invokeStatus(
                        module,
                        valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_method_panic_call,
                        {},
                        &ignored),
                    "expected Rust panic");
                EXPECT_EQ(ignored, nullptr);
            }
        }

        valdi_rust_codegen_test_snap__valdi_modules__rust_codegen_test__calculatormodule_module_drop(module);
        EXPECT_EQ(valdi_rust_handle_live_count(), baseline) << "iteration " << iteration;
    }
}

} // namespace
