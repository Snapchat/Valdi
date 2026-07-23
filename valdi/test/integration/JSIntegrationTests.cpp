#include "JSBridgeTestFixture.hpp"
#include "JSIntegrationTestsUtils.hpp"
#include "utils/platform/TargetPlatform.hpp"
#include "valdi/runtime/Interfaces/IJavaScriptBridge.hpp"
#include "valdi/runtime/JavaScript/JSFunctionWithCallable.hpp"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include <future>
#include <gtest/gtest.h>

using namespace Valdi;
using namespace snap::valdi_core;

namespace ValdiTest {

#define SKIP_IF_JSCORE(msg)                                                                                            \
    do {                                                                                                               \
        if (isJSCore()) {                                                                                              \
            GTEST_SKIP() << msg;                                                                                       \
        }                                                                                                              \
    } while (false)

#define SKIP_IF_HERMES(msg)                                                                                            \
    do {                                                                                                               \
        if (isHermes()) {                                                                                              \
            GTEST_SKIP() << msg;                                                                                       \
        }                                                                                                              \
    } while (false)

#define SKIP_IF_V8(msg)                                                                                                \
    do {                                                                                                               \
        if (isV8()) {                                                                                                  \
            GTEST_SKIP() << msg;                                                                                       \
        }                                                                                                              \
    } while (false)

struct MockJavaScriptContextListener : public IJavaScriptContextListener {
    std::vector<Value> unhandledPromiseResults;
    size_t interruptCount = 0;

    JSValueRef symbolicateError(const JSValueRef& jsError) override {
        return jsError;
    }

    Ref<JSStackTraceProvider> captureCurrentStackTrace() override {
        return nullptr;
    }

    // This method can be called in an arbitrary and must be thread safe.
    MainThreadManager* getMainThreadManager() const override {
        return nullptr;
    }

    void onUnhandledRejectedPromise(IJavaScriptContext& jsContext, const JSValue& promiseResult) override {
        Valdi::JSExceptionTracker exceptionTracker(jsContext);
        unhandledPromiseResults.emplace_back(
            jsValueToValue(jsContext, promiseResult, ReferenceInfoBuilder(), exceptionTracker));
        exceptionTracker.clearError();
    }

    void onInterrupt(IJavaScriptContext& jsContext) override {
        interruptCount++;
    }
};

struct PropertyNamesVisitor : public IJavaScriptPropertyNamesVisitor {
    std::vector<StringBox> propertyNames;

    bool visitPropertyName(IJavaScriptContext& context,
                           JSValue object,
                           const JSPropertyName& propertyName,
                           JSExceptionTracker& exceptionTracker) override {
        propertyNames.emplace_back(context.propertyNameToString(propertyName));
        return true;
    }
};

class JSContextFixture : public JSBridgeTestFixture {
protected:
    JSContextWrapper createWrapper(Valdi::DispatchQueue* dispatchQueue = nullptr) {
        return JSContextWrapper(getJsBridge(), dispatchQueue);
    }
};

TEST_P(JSContextFixture, canRetrieveNumbers) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("42");
    auto value2 = wrapper.evaluateScript("120.5");
    auto value3 = wrapper.evaluateScript("-999999");

    ASSERT_EQ(ValueType::Double, value1.getType());
    ASSERT_EQ(ValueType::Double, value2.getType());
    ASSERT_EQ(ValueType::Double, value3.getType());

    ASSERT_DOUBLE_EQ(42.0, value1.toDouble());
    ASSERT_DOUBLE_EQ(120.5, value2.toDouble());
    ASSERT_DOUBLE_EQ(-999999, value3.toDouble());
}

TEST_P(JSContextFixture, canRetrieveError) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("new Error('this went bad')");

    ASSERT_EQ(ValueType::Error, value1.getType());

    ASSERT_TRUE(value1.getError().toStringBox().contains("this went bad"));
}

TEST_P(JSContextFixture, canRetrieveStrings) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("'hello'");
    auto value2 = wrapper.evaluateScript("'I like this'");

    ASSERT_EQ(ValueType::InternedString, value1.getType());
    ASSERT_EQ(ValueType::InternedString, value2.getType());

    ASSERT_EQ(STRING_LITERAL("hello"), value1.toStringBox());
    ASSERT_EQ(STRING_LITERAL("I like this"), value2.toStringBox());
}

TEST_P(JSContextFixture, canRetrieveBool) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("false");
    auto value2 = wrapper.evaluateScript("true");

    ASSERT_EQ(ValueType::Bool, value1.getType());
    ASSERT_EQ(ValueType::Bool, value2.getType());

    ASSERT_EQ(false, value1.toBool());
    ASSERT_EQ(true, value2.toBool());
}

TEST_P(JSContextFixture, canRetrieveNull) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("null");

    ASSERT_EQ(ValueType::Null, value1.getType());
}

TEST_P(JSContextFixture, canRetrieveUndefined) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("undefined");

    ASSERT_EQ(ValueType::Undefined, value1.getType());
}

TEST_P(JSContextFixture, canRetrieveObject) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("{hello: 'world', nice: 42, cool: true}");

    ASSERT_EQ(ValueType::Map, value1.getType());

    auto expectedValue = makeShared<ValueMap>();
    (*expectedValue)[STRING_LITERAL("hello")] = Value(STRING_LITERAL("world"));
    (*expectedValue)[STRING_LITERAL("nice")] = Value(42.0);
    (*expectedValue)[STRING_LITERAL("cool")] = Value(true);

    ASSERT_EQ(Value(expectedValue), value1);
}

TEST_P(JSContextFixture, canRetrieveArray) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("['world', 42, true]");

    ASSERT_EQ(ValueType::Array, value1.getType());

    auto expectedValue = ValueArrayBuilder();
    expectedValue.emplace(STRING_LITERAL("world"));
    expectedValue.emplace(42.0);
    expectedValue.emplace(true);

    ASSERT_EQ(Value(expectedValue.build()), value1);
}

TEST_P(JSContextFixture, canRetrieveFunction) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.evaluateScript("function() { return 42; }");

    ASSERT_EQ(ValueType::Function, value1.getType());
    ASSERT_TRUE(value1.getFunctionRef() != nullptr);
}

TEST_P(JSContextFixture, canRetrieveTypedArray) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto dataVec = makeShared<ByteBuffer>();
    dataVec->set({0, 1, 2, 3});
    auto data = Value(makeShared<ValueTypedArray>(Uint8Array, dataVec->toBytesView()));
    std::vector<double> expectedFloatArray = {0, 1, 2, 3};
    auto buffer =
        makeShared<ByteBuffer>(reinterpret_cast<const Byte*>(expectedFloatArray.data()),
                               reinterpret_cast<const Byte*>(expectedFloatArray.data() + expectedFloatArray.size()));
    auto float64Data = Value(makeShared<ValueTypedArray>(Float64Array, buffer->toBytesView()));

    auto value1 = wrapper.evaluateScript("new Uint8Array([0, 1, 2, 3])");
    auto value2 = wrapper.evaluateScript("new Float64Array([0, 1, 2, 3])");

    ASSERT_EQ(ValueType::TypedArray, value1.getType());
    ASSERT_EQ(ValueType::TypedArray, value2.getType());

    ASSERT_EQ(data, value1);

    ASSERT_EQ(float64Data, value2);
}

TEST_P(JSContextFixture, canCreateNumber) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 =
        wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newNumber(42.0); });
    auto value2 =
        wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newNumber(120.5); });
    auto value3 =
        wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newNumber(-999999.0); });

    ASSERT_EQ(ValueType::Double, value1.getType());
    ASSERT_EQ(ValueType::Double, value2.getType());
    ASSERT_EQ(ValueType::Double, value3.getType());

    ASSERT_DOUBLE_EQ(42.0, value1.toDouble());
    ASSERT_DOUBLE_EQ(120.5, value2.toDouble());
    ASSERT_DOUBLE_EQ(-999999, value3.toDouble());
}

TEST_P(JSContextFixture, canCreateUTF8String) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.withContextRet(
        [&](auto& context, auto& exceptionTracker) { return context.newStringUTF8("hello", exceptionTracker); });
    auto value2 = wrapper.withContextRet(
        [&](auto& context, auto& exceptionTracker) { return context.newStringUTF8("I like this", exceptionTracker); });

    ASSERT_EQ(ValueType::InternedString, value1.getType());
    ASSERT_EQ(ValueType::InternedString, value2.getType());

    ASSERT_EQ(STRING_LITERAL("hello"), value1.toStringBox());
    ASSERT_EQ(STRING_LITERAL("I like this"), value2.toStringBox());
}

TEST_P(JSContextFixture, canCreateUTF16String) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    JSEntry jsEntry = wrapper.makeJsEntry();
    IJavaScriptContext& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    std::u16string_view str = u"Hello World";

    auto jsString = context.newStringUTF16(str, exceptionTracker);

    jsEntry.checkException();

    ASSERT_EQ(ValueType::StaticString, context.getValueType(jsString.get()));

    auto toStringResult = context.valueToString(jsString.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(STRING_LITERAL("Hello World"), toStringResult);

    auto staticStringResult = context.valueToStaticString(jsString.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(std::string("Hello World"), staticStringResult->toStdString());
}

TEST_P(JSContextFixture, canCreateStringFromStaticString) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto utf8 = StaticString::makeUTF8("UTF8");
    auto utf8Value = context.newString(*utf8, exceptionTracker);
    ASSERT_EQ(STRING_LITERAL("UTF8"), context.valueToString(utf8Value.get(), exceptionTracker));

    std::u16string utf16 = u"UTF16";
    auto utf16Value =
        context.newString(*StaticString::makeUTF16(utf16.data(), utf16.size()), exceptionTracker);
    ASSERT_EQ(STRING_LITERAL("UTF16"), context.valueToString(utf16Value.get(), exceptionTracker));

    std::u32string utf32 = U"UTF32";
    auto utf32Value =
        context.newString(*StaticString::makeUTF32(utf32.data(), utf32.size()), exceptionTracker);
    ASSERT_EQ(STRING_LITERAL("UTF32"), context.valueToString(utf32Value.get(), exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSContextFixture, canCreateBools) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newBool(false); });
    auto value2 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newBool(true); });

    ASSERT_EQ(ValueType::Bool, value1.getType());
    ASSERT_EQ(ValueType::Bool, value2.getType());

    ASSERT_EQ(false, value1.toBool());
    ASSERT_EQ(true, value2.toBool());
}

TEST_P(JSContextFixture, canCreateNull) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newNull(); });

    ASSERT_EQ(ValueType::Null, value1.getType());
}

TEST_P(JSContextFixture, canCreateUndefined) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) { return context.newUndefined(); });

    ASSERT_EQ(ValueType::Undefined, value1.getType());
}

TEST_P(JSContextFixture, canCreateObject) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) {
        auto object = context.newObject(exceptionTracker);
        if (!exceptionTracker) {
            return context.newUndefined();
        }

        auto hello = context.newStringUTF8("world", exceptionTracker);
        if (!exceptionTracker) {
            return context.newUndefined();
        }

        context.setObjectProperty(object.get(), "hello", hello.get(), exceptionTracker);
        if (!exceptionTracker) {
            return context.newUndefined();
        }

        auto number = context.newNumber(42.0);

        context.setObjectProperty(object.get(), "nice", number.get(), exceptionTracker);
        if (!exceptionTracker) {
            return context.newUndefined();
        }

        auto boolean = context.newBool(true);

        context.setObjectProperty(object.get(), "cool", boolean.get(), exceptionTracker);
        if (!exceptionTracker) {
            return context.newUndefined();
        }

        return object;
    });

    ASSERT_EQ(ValueType::Map, value1.getType());

    auto expectedValue = makeShared<ValueMap>();
    (*expectedValue)[STRING_LITERAL("hello")] = Value(STRING_LITERAL("world"));
    (*expectedValue)[STRING_LITERAL("nice")] = Value(42.0);
    (*expectedValue)[STRING_LITERAL("cool")] = Value(true);

    ASSERT_EQ(Value(expectedValue), value1);
}

TEST_P(JSContextFixture, canCreateArray) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto value1 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) {
        auto firstItem = context.newStringUTF8("world", exceptionTracker);
        if (!exceptionTracker) {
            return context.newUndefined();
        }

        auto secondItem = context.newNumber(42.0);

        auto thirdItem = context.newBool(true);

        Valdi::JSValue values[] = {
            firstItem.get(),
            secondItem.get(),
            thirdItem.get(),
        };

        return context.newArrayWithValues(values, 3, exceptionTracker);
    });

    ASSERT_EQ(ValueType::Array, value1.getType());

    auto expectedValue = ValueArrayBuilder();
    expectedValue.append(Value(STRING_LITERAL("world")));
    expectedValue.append(Value(42.0));
    expectedValue.append(Value(true));

    ASSERT_EQ(Value(expectedValue.build()), value1);
}

TEST_P(JSContextFixture, canCreateAndCallFunction) {
    SKIP_IF_V8("Ticket: 2249");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    Value value1;
    {
        auto jsEntry = wrapper.makeJsEntry();
        auto myFunctionKey = STRING_LITERAL("myFunction");
        auto jsFunction = jsEntry.context.newFunction(
            makeShared<JSFunctionWithCallable>(ReferenceInfoBuilder().withObject(myFunctionKey),
                                               [](auto& callContext) -> Valdi::JSValueRef {
                                                   auto value = callContext.getParameterAsDouble(0);
                                                   CHECK_CALL_CONTEXT(callContext);
                                                   return callContext.getContext().newNumber(value * 2.0);
                                               }),
            jsEntry.exceptionTracker);
        jsEntry.checkException();

        value1 = jsValueToValue(jsEntry.context, jsFunction.get(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);
        jsEntry.checkException();
    }

    ASSERT_EQ(ValueType::Function, value1.getType());

    auto result = callFunction(toFunction(value1), {Value(21.0)});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Value(42.0), result.value());
}

TEST_P(JSContextFixture, canCreateFunctionWithName) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto functionName = STRING_LITERAL("MyFunctionName");
    auto function =
        context.newFunction(makeShared<JSFunctionWithCallable>(ReferenceInfoBuilder().withObject(functionName),
                                                               [](auto& callContext) -> Valdi::JSValueRef {
                                                                   return callContext.getContext().newUndefined();
                                                               }),
                            exceptionTracker);
    jsEntry.checkException();

    auto property = context.getObjectProperty(function.get(), "name", exceptionTracker);
    jsEntry.checkException();
    auto resolvedFunctionName = context.valueToString(property.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(functionName, resolvedFunctionName);
}

TEST_P(JSContextFixture, canCreateTypedArray) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto dataVec = makeShared<ByteBuffer>();
    dataVec->set({0, 1, 2, 3});
    auto data = Value(makeShared<ValueTypedArray>(Uint8Array, dataVec->toBytesView()));
    std::vector<double> expectedFloatArray = {0, 1, 2, 3};
    auto buffer =
        makeShared<ByteBuffer>(reinterpret_cast<const Byte*>(expectedFloatArray.data()),
                               reinterpret_cast<const Byte*>(expectedFloatArray.data() + expectedFloatArray.size()));
    auto float64Data = Value(makeShared<ValueTypedArray>(Float64Array, buffer->toBytesView()));

    auto value1 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) -> Valdi::JSValueRef {
        return newTypedArrayFromBytesView(context, Uint8Array, data.getTypedArray()->getBuffer(), exceptionTracker);
    });
    auto value2 = wrapper.withContextRet([&](auto& context, auto& exceptionTracker) -> Valdi::JSValueRef {
        return newTypedArrayFromBytesView(
            context, Float64Array, float64Data.getTypedArray()->getBuffer(), exceptionTracker);
    });

    ASSERT_EQ(ValueType::TypedArray, value1.getType());
    ASSERT_EQ(ValueType::TypedArray, value2.getType());

    ASSERT_EQ(data, value1);

    ASSERT_EQ(float64Data, value2);
}

TEST_P(JSContextFixture, canCreateTypedArrayWithNoCopy) {
    GTEST_SKIP() << "Test fails for QuickJS Only";
    SKIP_IF_V8("Ticket: 2250");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto dataVec = makeShared<ByteBuffer>();
    dataVec->set({0, 1, 2, 3, 4, 5, 6, 7});

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    ASSERT_EQ(1, dataVec.use_count());

    std::initializer_list<TypedArrayType> types = {
        Int8Array,
        Int16Array,
        Int32Array,
        Uint8Array,
        Uint8ClampedArray,
        Uint16Array,
        Uint32Array,
        Float32Array,
        Float64Array,
        ArrayBuffer,
    };

    {
        RefCountableAutoreleasePool autoreleasePool;
        {
            std::vector<JSValueRef> refs;

            for (auto type : types) {
                auto ref = newTypedArrayFromBytesView(context, type, dataVec->toBytesView(), exceptionTracker);
                jsEntry.checkException();

                refs.emplace_back(JSValueRef::makeRetained(context, ref.get()));

                // It should have retained our vec
                ASSERT_EQ(static_cast<long>(1 + (refs.size() * 2)), dataVec.use_count());

                auto typedArray = context.valueToTypedArray(ref.get(), exceptionTracker);
                jsEntry.checkException();

                ASSERT_EQ(dataVec->data(), typedArray.data);
                ASSERT_EQ(dataVec->size(), typedArray.length);
                ASSERT_EQ(type, typedArray.type);
            }
        }
        context.garbageCollect();
    }

    // References should have been released after GC
    ASSERT_EQ(1, dataVec.use_count());
}

TEST_P(JSContextFixture, unwrapsNativeTypedArray) {
    SKIP_IF_V8("Ticket: 2252");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto dataVec = makeShared<ByteBuffer>();
    dataVec->set({0, 1, 2, 3, 4, 5, 6, 7});

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto uint8ArrayRef = newTypedArrayFromBytesView(context, Uint8Array, dataVec->toBytesView(), exceptionTracker);
    jsEntry.checkException();

    auto cppTypedArray =
        jsTypedArrayToValueTypedArray(context, uint8ArrayRef.get(), ReferenceInfoBuilder(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(Uint8Array, cppTypedArray->getType());
    ASSERT_EQ(dataVec->data(), cppTypedArray->getBuffer().data());
    ASSERT_EQ(dataVec->size(), cppTypedArray->getBuffer().size());
    ASSERT_EQ(dataVec.get(), cppTypedArray->getBuffer().getSource().get());
}

TEST_P(JSContextFixture, unwrapsNativeArrayBuffer) {
    SKIP_IF_V8("Ticket: 2253");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto dataVec = makeShared<ByteBuffer>();
    dataVec->set({0, 1, 2, 3, 4, 5, 6, 7});

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto arrayBufferRef = newTypedArrayFromBytesView(context, ArrayBuffer, dataVec->toBytesView(), exceptionTracker);
    jsEntry.checkException();

    auto cppTypedArray =
        jsTypedArrayToValueTypedArray(context, arrayBufferRef.get(), ReferenceInfoBuilder(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(ArrayBuffer, cppTypedArray->getType());
    ASSERT_EQ(dataVec->data(), cppTypedArray->getBuffer().data());
    ASSERT_EQ(dataVec->size(), cppTypedArray->getBuffer().size());
    ASSERT_EQ(dataVec.get(), cppTypedArray->getBuffer().getSource().get());
}

TEST_P(JSContextFixture, unwrapsJsTypedArray) {
    SKIP_IF_V8("Ticket: 2254");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto globalObject = context.getGlobalObject(exceptionTracker);
    jsEntry.checkException();
    auto uint8ArrayCtor = context.getObjectProperty(globalObject.get(), "Uint8Array", exceptionTracker);
    jsEntry.checkException();

    std::initializer_list<JSValueRef> uint8Params = {context.newNumber(2)};

    JSFunctionCallContext uint8CallContext(context, uint8Params.begin(), uint8Params.size(), exceptionTracker);

    auto uint8ArrayRef = context.callObjectAsConstructor(uint8ArrayCtor.get(), uint8CallContext);
    jsEntry.checkException();

    {
        auto value = context.newNumber(42);
        context.setObjectPropertyIndex(uint8ArrayRef.get(), 0, value.get(), exceptionTracker);
        jsEntry.checkException();
        value = context.newNumber(100);
        context.setObjectPropertyIndex(uint8ArrayRef.get(), 1, value.get(), exceptionTracker);
        jsEntry.checkException();
    }

    auto cppTypedArray =
        jsTypedArrayToValueTypedArray(context, uint8ArrayRef.get(), ReferenceInfoBuilder(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(Uint8Array, cppTypedArray->getType());
    ASSERT_EQ(static_cast<size_t>(2), cppTypedArray->getBuffer().size());
    ASSERT_EQ(static_cast<Byte>(42), cppTypedArray->getBuffer().data()[0]);
    ASSERT_EQ(static_cast<Byte>(100), cppTypedArray->getBuffer().data()[1]);

    // Wrap the ValueTypedArray into another Uint8Array
    auto newJsTypedArray =
        newTypedArrayFromBytesView(context, Uint8Array, cppTypedArray->getBuffer(), exceptionTracker);
    jsEntry.checkException();

    {
        // Modify it
        auto value = context.newNumber(7);
        context.setObjectPropertyIndex(newJsTypedArray.get(), 0, value.get(), exceptionTracker);
    }

    // Modification should have applied to our ArrayBuffer
    ASSERT_EQ(static_cast<Byte>(7), cppTypedArray->getBuffer().data()[0]);
}

struct DummyObject : public Valdi::ValdiObject {
    VALDI_CLASS_HEADER_IMPL(DummyObject);
};

class NativeCounter final : public Valdi::SimpleRefCountable {
public:
    explicit NativeCounter(int32_t value) : value(value) {}

    int32_t value;
};

class NativeCounterClass final : public Valdi::SimpleRefCountable {
public:
    int32_t constructorCalls = 0;
    int32_t staticValue = 0;
    int32_t multiplier = 2;
};

static Ref<RefCountable> constructNativeCounter(RefCountable* classOpaque,
                                                JSFunctionNativeCallContext& callContext) noexcept {
    auto value = callContext.getParameterAsInt(0);
    if (!callContext.getExceptionTracker()) {
        return nullptr;
    }
    static_cast<NativeCounterClass*>(classOpaque)->constructorCalls++;
    return makeShared<NativeCounter>(value);
}

static JSValueRef addToNativeCounter(RefCountable* opaque, JSFunctionNativeCallContext& callContext) noexcept {
    auto* counter = static_cast<NativeCounter*>(opaque);
    counter->value += callContext.getParameterAsInt(0);
    if (!callContext.getExceptionTracker()) {
        return callContext.getContext().newUndefined();
    }
    return callContext.getContext().newNumber(counter->value);
}

static JSValueRef getNativeCounterValue(RefCountable* opaque, JSFunctionNativeCallContext& callContext) noexcept {
    return callContext.getContext().newNumber(static_cast<NativeCounter*>(opaque)->value);
}

static JSValueRef setNativeCounterValue(RefCountable* opaque,
                                        JSFunctionNativeCallContext& callContext) noexcept {
    static_cast<NativeCounter*>(opaque)->value = callContext.getParameterAsInt(0);
    return callContext.getContext().newUndefined();
}

static JSValueRef doubleNativeCounterValue(RefCountable* classOpaque,
                                           JSFunctionNativeCallContext& callContext) noexcept {
    auto value = callContext.getParameterAsInt(0);
    if (!callContext.getExceptionTracker()) {
        return callContext.getContext().newUndefined();
    }
    return callContext.getContext().newNumber(value * static_cast<NativeCounterClass*>(classOpaque)->multiplier);
}

static JSValueRef getNativeCounterStaticValue(RefCountable* classOpaque,
                                              JSFunctionNativeCallContext& callContext) noexcept {
    return callContext.getContext().newNumber(static_cast<NativeCounterClass*>(classOpaque)->staticValue);
}

static JSValueRef setNativeCounterStaticValue(RefCountable* classOpaque,
                                              JSFunctionNativeCallContext& callContext) noexcept {
    static_cast<NativeCounterClass*>(classOpaque)->staticValue = callContext.getParameterAsInt(0);
    return callContext.getContext().newUndefined();
}

static JSClassDefinition makeNativeCounterClassDefinition(IJavaScriptContext& context,
                                                          JSExceptionTracker& exceptionTracker,
                                                          const StringBox& name,
                                                          JSClassConstructorCallback constructor) {
    JSClassDefinition definition(name, constructor);
    definition
        .appendInstanceEntry(JSClassEntry::method(STRING_LITERAL("add"), &addToNativeCounter))
        .appendConstant(STRING_LITERAL("kind"), context.newStringUTF8("native-counter", exceptionTracker))
        .appendAccessor(STRING_LITERAL("value"), &getNativeCounterValue, &setNativeCounterValue)
        .appendClassEntry(JSClassEntry::method(STRING_LITERAL("twice"), &doubleNativeCounterValue))
        .appendClassConstant(STRING_LITERAL("category"),
                             context.newStringUTF8("counter-class", exceptionTracker))
        .appendClassAccessor(
            STRING_LITERAL("sharedValue"), &getNativeCounterStaticValue, &setNativeCounterStaticValue);
    return definition;
}

struct NativeCounterTestValues {
    Ref<NativeCounterClass> classOpaque;
    Ref<NativeCounter> instanceOpaque;
    JSValueRef cls;
    JSValueRef instance;
};

static NativeCounterTestValues setUpNativeCounterTest(JSEntry& jsEntry,
                                                      JSClassConstructorCallback constructor) {
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto classOpaque = makeShared<NativeCounterClass>();
    auto classDefinition =
        makeNativeCounterClassDefinition(context, exceptionTracker, STRING_LITERAL("NativeCounter"), constructor);
    jsEntry.checkException();

    auto cls = context.newNativeClass(classOpaque, classDefinition, exceptionTracker);
    jsEntry.checkException();
    auto instanceOpaque = makeShared<NativeCounter>(10);
    auto instance = context.newObjectFromNativeClass(instanceOpaque, cls.get(), exceptionTracker);
    jsEntry.checkException();

    auto global = context.getGlobalObject(exceptionTracker);
    context.setObjectProperty(global.get(), "NativeCounter", cls.get(), exceptionTracker);
    context.setObjectProperty(global.get(), "nativeCounter", instance.get(), exceptionTracker);
    jsEntry.checkException();

    return NativeCounterTestValues{
        std::move(classOpaque),
        std::move(instanceOpaque),
        std::move(cls),
        std::move(instance),
    };
}

static JSValueRef evaluateNativeCounterExpression(JSEntry& jsEntry, const char* source) {
    auto value = jsEntry.context.evaluate(source, "NativeClassEntries.js", jsEntry.exceptionTracker);
    jsEntry.checkException();
    return value;
}

TEST_P(JSContextFixture, canCreateAndUseNativeClass) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto values = setUpNativeCounterTest(jsEntry, &constructNativeCounter);
    ASSERT_GT(values.classOpaque.use_count(), 1);
    ASSERT_EQ(0, values.classOpaque->constructorCalls);
    ASSERT_EQ(values.instanceOpaque,
              castOrNull<NativeCounter>(context.valueToWrappedObject(values.instance.get(), exceptionTracker)));
    jsEntry.checkException();

    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(jsEntry, "nativeCounter instanceof NativeCounter").get(), exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(jsEntry, "nativeCounter.add === NativeCounter.prototype.add").get(),
        exceptionTracker));
    ASSERT_EQ(10,
              context.valueToInt(
                  evaluateNativeCounterExpression(jsEntry, "nativeCounter.value").get(), exceptionTracker));
    ASSERT_EQ(15,
              context.valueToInt(
                  evaluateNativeCounterExpression(jsEntry, "nativeCounter.add(5)").get(), exceptionTracker));

    evaluateNativeCounterExpression(jsEntry, "nativeCounter.value = 20");
    ASSERT_EQ(20, values.instanceOpaque->value);
    ASSERT_EQ(20,
              context.valueToInt(
                  evaluateNativeCounterExpression(jsEntry, "nativeCounter.value").get(), exceptionTracker));
    ASSERT_EQ(STRING_LITERAL("native-counter"),
              context.valueToString(
                  evaluateNativeCounterExpression(jsEntry, "nativeCounter.kind").get(), exceptionTracker));

    ASSERT_EQ(12,
              context.valueToInt(
                  evaluateNativeCounterExpression(jsEntry, "NativeCounter.twice(6)").get(), exceptionTracker));
    ASSERT_EQ(STRING_LITERAL("counter-class"),
              context.valueToString(
                  evaluateNativeCounterExpression(jsEntry, "NativeCounter.category").get(), exceptionTracker));

    evaluateNativeCounterExpression(jsEntry, "NativeCounter.sharedValue = 9");
    ASSERT_EQ(9, values.classOpaque->staticValue);
    ASSERT_EQ(9,
              context.valueToInt(
                  evaluateNativeCounterExpression(jsEntry, "NativeCounter.sharedValue").get(), exceptionTracker));

    auto constructed =
        context.evaluate("globalThis.constructedNativeCounter = new NativeCounter(4); constructedNativeCounter",
                         "NativeClassConstructor.js",
                         exceptionTracker);
    jsEntry.checkException();
    ASSERT_EQ(1, values.classOpaque->constructorCalls);
    auto constructedCounter =
        castOrNull<NativeCounter>(context.valueToWrappedObject(constructed.get(), exceptionTracker));
    jsEntry.checkException();
    ASSERT_NE(nullptr, constructedCounter);
    ASSERT_EQ(4, constructedCounter->value);

    RefCountableAutoreleasePool autoreleasePool;
    auto lifetimeCounter = makeShared<NativeCounter>(1);
    {
        auto lifetimeObject = context.ensureRetainedValue(
            context.newObjectFromNativeClass(lifetimeCounter, values.cls.get(), exceptionTracker));
        jsEntry.checkException();
        autoreleasePool.releaseAll();
        ASSERT_EQ(2, lifetimeCounter.use_count());
    }
    context.garbageCollect();
    autoreleasePool.releaseAll();
    ASSERT_EQ(1, lifetimeCounter.use_count());
}

TEST_P(JSContextFixture, nativeClassEntriesHaveExpectedDescriptors) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    setUpNativeCounterTest(jsEntry, nullptr);

    ASSERT_FALSE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'add').enumerable")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'add').writable")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'add').configurable")
            .get(),
        exceptionTracker));
    ASSERT_FALSE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'kind').writable")
            .get(),
        exceptionTracker));
    ASSERT_FALSE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'kind').configurable")
            .get(),
        exceptionTracker));
    ASSERT_FALSE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'value').enumerable")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry, "Object.getOwnPropertyDescriptor(NativeCounter.prototype, 'value').configurable")
            .get(),
        exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSContextFixture, nativeClassMethodsCannotBeExportedAsNativeFunctions) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    setUpNativeCounterTest(jsEntry, nullptr);

    auto method = evaluateNativeCounterExpression(jsEntry, "NativeCounter.prototype.add");
    ASSERT_EQ(nullptr, context.valueToFunction(method.get(), exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSContextFixture, nativeClassCallbacksDeliverInterrupts) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto values = setUpNativeCounterTest(jsEntry, &constructNativeCounter);
    MockJavaScriptContextListener listener;
    context.setListener(&listener);

    auto argument = context.newNumber(1);

    auto instanceMethod = evaluateNativeCounterExpression(jsEntry, "NativeCounter.prototype.add");
    JSFunctionCallContext instanceCallContext(context, &argument, 1, exceptionTracker);
    instanceCallContext.setThisValue(values.instance.get());
    context.requestInterrupt();
    context.callObjectAsFunction(instanceMethod.get(), instanceCallContext);
    jsEntry.checkException();
    ASSERT_EQ(1, listener.interruptCount);

    auto classMethod = evaluateNativeCounterExpression(jsEntry, "NativeCounter.twice");
    JSFunctionCallContext classCallContext(context, &argument, 1, exceptionTracker);
    classCallContext.setThisValue(values.cls.get());
    context.requestInterrupt();
    context.callObjectAsFunction(classMethod.get(), classCallContext);
    jsEntry.checkException();
    ASSERT_EQ(2, listener.interruptCount);

    JSFunctionCallContext constructorCallContext(context, &argument, 1, exceptionTracker);
    context.requestInterrupt();
    context.callObjectAsConstructor(values.cls.get(), constructorCallContext);
    jsEntry.checkException();
    ASSERT_EQ(3, listener.interruptCount);

    context.setListener(nullptr);
}

TEST_P(JSContextFixture, nativeClassRejectsInvalidUsage) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    setUpNativeCounterTest(jsEntry, &constructNativeCounter);

    auto otherDefinition = makeNativeCounterClassDefinition(
        context, exceptionTracker, STRING_LITERAL("OtherNativeCounter"), nullptr);
    auto otherClassOpaque = makeShared<NativeCounterClass>();
    auto otherClass = context.newNativeClass(otherClassOpaque, otherDefinition, exceptionTracker);
    auto otherCounter = makeShared<NativeCounter>(3);
    auto otherObject = context.newObjectFromNativeClass(otherCounter, otherClass.get(), exceptionTracker);
    auto global = context.getGlobalObject(exceptionTracker);
    context.setObjectProperty(global.get(), "OtherNativeCounter", otherClass.get(), exceptionTracker);
    context.setObjectProperty(global.get(), "otherNativeCounter", otherObject.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry,
            "(() => { try { NativeCounter(); return false; } "
            "catch (error) { return error instanceof Error; } })()")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry,
            "(() => { try { class Child extends NativeCounter {}; new Child(1); return false; } "
            "catch (error) { return error instanceof Error; } })()")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry,
            "(() => { try { NativeCounter.prototype.add.call(otherNativeCounter, 1); return false; } "
            "catch (error) { return error instanceof Error; } })()")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateNativeCounterExpression(
            jsEntry,
            "(() => { try { new OtherNativeCounter(); return false; } "
            "catch (error) { return error instanceof Error; } })()")
            .get(),
        exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSContextFixture, canCreateWrappedObject) {
    SKIP_IF_V8("Ticket: 2255");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto object = Valdi::makeShared<DummyObject>();
    auto objectValue = Valdi::Value(object);

    auto value1 = wrapper.withContextRet(
        [&](auto& context, auto& exceptionTracker) { return context.newWrappedObject(object, exceptionTracker); });

    ASSERT_EQ(ValueType::ValdiObject, value1.getType());

    ASSERT_EQ(objectValue, value1);
}

TEST_P(JSContextFixture, canUseToPrimitiveOnWrappedObject) {
    SKIP_IF_V8("Ticket: 2255");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();

    auto object = Valdi::makeShared<DummyObject>();

    auto wrappedObject = jsEntry.context.newWrappedObject(object, jsEntry.exceptionTracker);
    jsEntry.checkException();

    auto toPrimitive = jsEntry.context.evaluate(
        "(function(wrappedObject) { return 'WrappedObject: ' + wrappedObject })", "", jsEntry.exceptionTracker);
    jsEntry.checkException();

    JSFunctionCallContext callContext(jsEntry.context, &wrappedObject, 1, jsEntry.exceptionTracker);
    auto result = jsEntry.context.callObjectAsFunction(toPrimitive.get(), callContext);
    jsEntry.checkException();

    auto toStringResult = jsEntry.context.valueToString(result.get(), jsEntry.exceptionTracker);
    jsEntry.checkException();

    ASSERT_TRUE(toStringResult.hasPrefix("WrappedObject: [object "));
}

TEST_P(JSContextFixture, canCallFunction) {
    SKIP_IF_V8("Ticket: 2256");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto trueParams = ValueArray::make(1);
    trueParams->emplace(0, Value(true));

    auto function = wrapper.evaluateScript("function(param1, param2, param3) { return [param3, param2, param1]; }");
    auto result = callFunction(toFunction(function), {Value(STRING_LITERAL("I like")), Value(42.0), Value(trueParams)});

    ASSERT_TRUE(result) << result.description();
    ;
    ASSERT_EQ(ValueType::Array, result.value().getType());

    auto expectedOutput = ValueArray::make(3);
    expectedOutput->emplace(0, Value(trueParams));
    expectedOutput->emplace(1, Value(42.0));
    expectedOutput->emplace(2, Value(STRING_LITERAL("I like")));

    ASSERT_EQ(Value(expectedOutput), result.value());
}

TEST_P(JSContextFixture, canCallFunctionAsync) {
    SKIP_IF_V8("Ticket: 2257");
    auto dispatchQueue = Valdi::DispatchQueue::createThreaded(STRING_LITERAL("JS Thread"), Valdi::ThreadQoSClassMax);

    auto wrapper = createWrapper(dispatchQueue.get());

    auto startCallingPromise = Valdi::makeShared<std::promise<void>>();
    auto finishCallingPromise = Valdi::makeShared<std::promise<void>>();
    auto finishCalling = Valdi::makeShared<std::atomic_bool>();
    (*finishCalling) = false;

    Ref<ValueFunction> function;
    {
        auto jsEntry = wrapper.makeJsEntry();
        auto myAsyncFunctionKey = STRING_LITERAL("myAsyncFunction");

        auto jsFunction = jsEntry.context.newFunction(
            makeShared<JSFunctionWithCallable>(
                ReferenceInfoBuilder().withObject(myAsyncFunctionKey),
                [=](auto& callContext) -> Valdi::JSValueRef {
                    auto result = startCallingPromise->get_future().wait_for(std::chrono::seconds(5));

                    if (result == std::future_status::timeout) {
                        return callContext.throwError(
                            Error("timed out waiting for the main thread to tell us to set the result"));
                    }

                    (*finishCalling) = true;

                    finishCallingPromise->set_value();

                    return Valdi::JSValueRef();
                }),
            jsEntry.exceptionTracker);
        jsEntry.checkException();
        function =
            jsValueToFunction(jsEntry.context, jsFunction.get(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);
        jsEntry.checkException();
    }

    (*function)(ValueFunctionFlagsNeverCallSync, {});

    ASSERT_FALSE(finishCalling->load());

    // Now allow the JS thread to start its logic
    startCallingPromise->set_value();

    // Wait until the JS thread finishes
    auto result = finishCallingPromise->get_future().wait_for(std::chrono::seconds(5));
    ASSERT_NE(std::future_status::timeout, result);

    // The JS thread should have been called and set that flag
    ASSERT_TRUE(finishCalling->load());
}

TEST_P(JSContextFixture, canCallFunctionSync) {
    SKIP_IF_V8("Ticket: 2258");
    auto dispatchQueue = Valdi::DispatchQueue::createThreaded(STRING_LITERAL("JS Thread"), Valdi::ThreadQoSClassMax);

    auto wrapper = createWrapper(dispatchQueue.get());

    auto finishCalling = Valdi::makeShared<std::atomic_bool>();
    (*finishCalling) = false;

    Ref<ValueFunction> function;
    {
        auto jsEntry = wrapper.makeJsEntry();
        auto mySyncFunctionKey = STRING_LITERAL("mySyncFunction");

        auto jsFunction = jsEntry.context.newFunction(
            makeShared<JSFunctionWithCallable>(ReferenceInfoBuilder().withObject(mySyncFunctionKey),
                                               [=](auto& callContext) -> Valdi::JSValueRef {
                                                   (*finishCalling) = true;
                                                   return Valdi::JSValueRef();
                                               }),
            jsEntry.exceptionTracker);
        jsEntry.checkException();
        function =
            jsValueToFunction(jsEntry.context, jsFunction.get(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);
        jsEntry.checkException();
    }

    ASSERT_FALSE(finishCalling->load());

    (*function)(ValueFunctionFlagsCallSync, {});

    ASSERT_TRUE(finishCalling->load());
}

TEST_P(JSContextFixture, canCallAsConstructor) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto func = context.evaluate(R""""(
        (function MyClass(value) {
            this.storedValue = value * 2;
        });
    )"""",
                                 "",
                                 exceptionTracker);
    jsEntry.checkException();

    auto value = context.newNumber(42.0);

    Valdi::JSFunctionCallContext params(context, &value, 1, exceptionTracker);

    auto instance = context.callObjectAsConstructor(func.get(), params);

    jsEntry.checkException();

    auto retValue = context.getObjectProperty(instance.get(), "storedValue", exceptionTracker);
    jsEntry.checkException();

    auto number = context.valueToDouble(retValue.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(84, number);
}

TEST_P(JSContextFixture, canRetrieveErrorFromNativeFunction) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto func = context.evaluate(R""""(
        (function (cb) {
            try {
                cb();
                return undefined;
            } catch (err) {
                return err.message;
            }
        });
    )"""",
                                 "",
                                 exceptionTracker);
    jsEntry.checkException();

    auto myCrashingFunctionKey = STRING_LITERAL("myCrashingFunction");

    auto nativeFunc = context.newFunction(
        makeShared<JSFunctionWithCallable>(ReferenceInfoBuilder().withObject(myCrashingFunctionKey),
                                           [=](auto& callContext) -> Valdi::JSValueRef {
                                               return callContext.throwError(Valdi::Error("My Native Error"));
                                           }),
        exceptionTracker);
    jsEntry.checkException();

    Valdi::JSFunctionCallContext callContext(context, &nativeFunc, 1, exceptionTracker);
    auto callReturnValue = context.callObjectAsFunction(func.get(), callContext);

    jsEntry.checkException();

    auto retValueString = context.valueToString(callReturnValue.get(), exceptionTracker);

    jsEntry.checkException();

    ASSERT_EQ(STRING_LITERAL("My Native Error"), retValueString);
}

template<typename T>
class TestSemaphore {
public:
    TestSemaphore(T initialValue) : _lastValue(std::move(initialValue)) {}
    void waitForValue(T expectedValue) {
        // Flush the queue first before trying to compare the values
        while (_taskQueue.runNextTask()) {
        }

        while (_lastValue != expectedValue) {
            auto success = _taskQueue.runNextTask(std::chrono::steady_clock::now() + std::chrono::seconds(5));
            if (!success) {
                throw Exception(STRING_FORMAT("Failed to wait until value becomes {}", expectedValue));
            }
        }
    }

    void setValue(T value) {
        _taskQueue.enqueue([this, value]() {
            _lastValue = value;
            _valueHistory.emplace_back(value);
        });
    }

    void clearAndSetValue(T value) {
        _valueHistory.clear();
        _lastValue = value;
    }

    const std::vector<T>& getValueHistory() const {
        return _valueHistory;
    }

private:
    // Use a TaskQueue as a semaphore
    TaskQueue _taskQueue;
    T _lastValue = T();
    std::vector<T> _valueHistory;
};

void setJsQueuePaused(JSContextWrapper& wrapper, const Shared<TestSemaphore<bool>>& semaphore, bool paused) {
    semaphore->setValue(paused);

    if (paused) {
        // Enqueue a block that will lock the thread
        wrapper.getDispatchQueue()->async([=]() { semaphore->waitForValue(false); });
    }
}

TEST_P(JSContextFixture, canCallFunctionThrottled) {
    SKIP_IF_V8("Ticket: 2259");
    auto dispatchQueue = Valdi::DispatchQueue::createThreaded(STRING_LITERAL("JS Thread"), Valdi::ThreadQoSClassMax);

    auto wrapper = createWrapper(dispatchQueue.get());

    auto callSemaphore = Valdi::makeShared<TestSemaphore<double>>(0.0);
    auto allowCallSemaphore = Valdi::makeShared<TestSemaphore<bool>>(false);

    Ref<ValueFunction> function;

    {
        auto jsEntry = wrapper.makeJsEntry();
        auto myThrottledFunctionKey = STRING_LITERAL("myThrottledFunction");

        auto jsFunction = jsEntry.context.newFunction(
            makeShared<JSFunctionWithCallable>(ReferenceInfoBuilder().withObject(myThrottledFunctionKey),
                                               [=](auto& callContext) -> Valdi::JSValueRef {
                                                   callSemaphore->setValue(callContext.getParameterAsDouble(0));
                                                   return Valdi::JSValueRef();
                                               }),
            jsEntry.exceptionTracker);
        jsEntry.checkException();

        function =
            jsValueToFunction(jsEntry.context, jsFunction.get(), ReferenceInfoBuilder(), jsEntry.exceptionTracker);
        jsEntry.checkException();
    }

    setJsQueuePaused(wrapper, allowCallSemaphore, false);

    // First try without throttling
    (*function)(ValueFunctionFlagsNone, {Value(1.0)});
    (*function)(ValueFunctionFlagsNone, {Value(2.0)});
    (*function)(ValueFunctionFlagsNone, {Value(3.0)});

    // Wait until the last call submits 3.0
    callSemaphore->waitForValue(3.0);
    // We should have been called 3 times
    ASSERT_EQ(std::vector<double>({1.0, 2.0, 3.0}), callSemaphore->getValueHistory());

    callSemaphore->clearAndSetValue(0.0);
    setJsQueuePaused(wrapper, allowCallSemaphore, true);

    // Now call with throttling
    (*function)(ValueFunctionFlagsAllowThrottling, {Value(1.0)});
    (*function)(ValueFunctionFlagsAllowThrottling, {Value(2.0)});
    (*function)(ValueFunctionFlagsAllowThrottling, {Value(3.0)});

    setJsQueuePaused(wrapper, allowCallSemaphore, false);

    // Wait until the last call submits 3.0
    callSemaphore->waitForValue(3.0);

    // We should have just 1 call, since the other calls should have been throttled
    ASSERT_EQ(std::vector<double>({3.0}), callSemaphore->getValueHistory());

    callSemaphore->clearAndSetValue(0.0);
    setJsQueuePaused(wrapper, allowCallSemaphore, true);

    // Now call with mix of throttling and regular calls

    (*function)(ValueFunctionFlagsNone, {Value(1.0)});
    (*function)(ValueFunctionFlagsAllowThrottling, {Value(2.0)});
    (*function)(ValueFunctionFlagsAllowThrottling, {Value(3.0)});
    (*function)(ValueFunctionFlagsAllowThrottling, {Value(4.0)});
    (*function)(ValueFunctionFlagsNone, {Value(5.0)});

    setJsQueuePaused(wrapper, allowCallSemaphore, false);

    // Wait until the last call submits 5.0
    callSemaphore->waitForValue(5.0);

    // We should have just 3 calls:
    // The first call, because it has no flag
    // The 4th call, because 2nd and 3rd call should have been throttled
    // The 5th call, because it has no flag
    ASSERT_EQ(std::vector<double>({1.0, 4.0, 5.0}), callSemaphore->getValueHistory());
}

TEST_P(JSContextFixture, canSetAndRetrieveObjectProperties) {
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto fetchedValueConverted =
        wrapper.withContextRet([&](auto& context, auto& exceptionTracker) -> Valdi::JSValueRef {
            auto object = context.newObject(exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::JSValueRef();
            }

            auto value = context.newStringUTF8("Simon", exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::JSValueRef();
            }

            std::string_view propertyName = "myName";

            context.setObjectProperty(object.get(), propertyName, value.get(), exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::JSValueRef();
            }

            return context.getObjectProperty(object.get(), propertyName, exceptionTracker);
        });

    ASSERT_EQ(STRING_LITERAL("Simon"), fetchedValueConverted.toStringBox());
}

TEST_P(JSContextFixture, canVisitObjectProperties) {
    MAIN_THREAD_INIT();

    // Eval the precompiled code
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto jsCode = R"""(
        (function() {
            function MyClass(prop1, prop2) {
                this.prop1 = prop1;
                this.prop2 = prop2;
            }

            Object.defineProperty(MyClass.prototype, 'combined', {
                enumerable: true,
                get() {
                    return this.prop1 + this.prop2;
                },
            });

            Object.defineProperty(MyClass.prototype, 'combinedUpper', {
                enumerable: false,
                get() {
                    return (this.prop1 + this.prop2).toUpperCase();
                },
            });

            return new MyClass('Hello', 'World');
        })()
    )""";

    auto object = context.evaluate(jsCode, "", exceptionTracker);
    jsEntry.checkException();

    PropertyNamesVisitor visitor;

    context.visitObjectPropertyNames(object.get(), exceptionTracker, visitor);
    jsEntry.checkException();

    std::vector<StringBox> expectedPropertyNames = {
        STRING_LITERAL("prop1"), STRING_LITERAL("prop2"), STRING_LITERAL("combined")};

    ASSERT_EQ(expectedPropertyNames, visitor.propertyNames);

    auto getProperty = [&](std::string_view propertyName) -> Valdi::StringBox {
        auto prop = context.getObjectProperty(object.get(), propertyName, exceptionTracker);
        jsEntry.checkException();

        auto propStr = context.valueToString(prop.get(), exceptionTracker);
        jsEntry.checkException();

        return propStr;
    };

    auto prop1 = getProperty("prop1");

    ASSERT_EQ(STRING_LITERAL("Hello"), prop1);

    auto prop2 = getProperty("prop2");

    ASSERT_EQ(STRING_LITERAL("World"), prop2);

    auto combined = getProperty("combined");

    ASSERT_EQ(STRING_LITERAL("HelloWorld"), combined);

    auto combinedUpper = getProperty("combinedUpper");

    ASSERT_EQ(STRING_LITERAL("HELLOWORLD"), combinedUpper);
}

TEST_P(JSContextFixture, canSetNonEnumerableProperties) {
    MAIN_THREAD_INIT();

    // Eval the precompiled code
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto object = context.newObject(exceptionTracker);
    jsEntry.checkException();

    auto value1 = context.newNumber(1);
    auto value2 = context.newNumber(2);
    auto value3 = context.newNumber(3);

    context.setObjectProperty(object.get(), "visible", value1.get(), true, exceptionTracker);
    jsEntry.checkException();

    context.setObjectProperty(object.get(), "invisible", value2.get(), false, exceptionTracker);
    jsEntry.checkException();

    context.setObjectProperty(object.get(), "alsoVisible", value3.get(), true, exceptionTracker);
    jsEntry.checkException();

    PropertyNamesVisitor visitor;

    context.visitObjectPropertyNames(object.get(), exceptionTracker, visitor);
    jsEntry.checkException();

    std::vector<StringBox> expectedPropertyNames = {STRING_LITERAL("visible"), STRING_LITERAL("alsoVisible")};

    ASSERT_EQ(expectedPropertyNames, visitor.propertyNames);

    auto value = context.getObjectProperty(object.get(), "visible", exceptionTracker);
    jsEntry.checkException();

    auto intValue = context.valueToInt(value.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(1, intValue);

    value = context.getObjectProperty(object.get(), "invisible", exceptionTracker);
    jsEntry.checkException();

    intValue = context.valueToInt(value.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(2, intValue);

    value = context.getObjectProperty(object.get(), "alsoVisible", exceptionTracker);
    jsEntry.checkException();

    intValue = context.valueToInt(value.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(3, intValue);

    // We can set non enumerble with new values

    auto value4 = context.newNumber(4);

    context.setObjectProperty(object.get(), "invisible", value4.get(), false, exceptionTracker);
    jsEntry.checkException();

    value = context.getObjectProperty(object.get(), "invisible", exceptionTracker);
    jsEntry.checkException();

    intValue = context.valueToInt(value.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(4, intValue);
}

TEST_P(JSContextFixture, canSetAndRetrieveObjectPropertiesWithCachedPropertyName) {
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto fetchedValueConverted =
        wrapper.withContextRet([&](auto& context, auto& exceptionTracker) -> Valdi::JSValueRef {
            auto object = context.newObject(exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::JSValueRef();
            }

            auto value = context.newStringUTF8("Simon", exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::JSValueRef();
            }

            auto propertyName = context.newPropertyName("myName");

            context.setObjectProperty(object.get(), propertyName.get(), value.get(), exceptionTracker);
            if (!exceptionTracker) {
                return Valdi::JSValueRef();
            }

            return context.getObjectProperty(object.get(), propertyName.get(), exceptionTracker);
        });

    ASSERT_EQ(STRING_LITERAL("Simon"), fetchedValueConverted.toStringBox());
}

TEST_P(JSContextFixture, canCallFunctionWithThisBinding) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto object = context.newObject(exceptionTracker);
    jsEntry.checkException();

    auto number = context.newNumber(42.0);

    context.setObjectProperty(object.get(), "storedValue", number.get(), exceptionTracker);
    jsEntry.checkException();

    auto func = context.evaluate(R""""(
        (function () {
            return this.storedValue;
        });
    )"""",
                                 "",
                                 exceptionTracker);
    jsEntry.checkException();

    // Call without "this"

    JSFunctionCallContext callContext(context, nullptr, 0, exceptionTracker);
    auto result = context.callObjectAsFunction(func.get(), callContext);
    jsEntry.checkException();

    auto firstCallRetType = context.getValueType(result.get());

    ASSERT_EQ(Valdi::ValueType::Undefined, firstCallRetType);

    callContext.setThisValue(object.get());
    result = context.callObjectAsFunction(func.get(), callContext);

    jsEntry.checkException();

    auto secondCallRetType = context.getValueType(result.get());

    ASSERT_EQ(Valdi::ValueType::Double, secondCallRetType);

    auto retValue = context.valueToDouble(result.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(42.0, retValue);
}

TEST_P(JSContextFixture, canThrowException) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();

    auto func = jsEntry.context.evaluate(R""""(
        (function () {
            throw new Error('This is an error!');
        })()
    )"""",
                                         "",
                                         jsEntry.exceptionTracker);

    ASSERT_FALSE(jsEntry.exceptionTracker);

    auto error = jsEntry.exceptionTracker.extractError();

    ASSERT_TRUE(error.toStringBox().hasPrefix("This is an error!"));
}

TEST_P(JSContextFixture, supportsLongObject) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();

    auto func = jsEntry.context.evaluate(R""""(
            (function Long(low, high) {
                this.low = low;
                this.high = high;
            })
    )"""",
                                         "",
                                         jsEntry.exceptionTracker);
    jsEntry.checkException();

    std::initializer_list<JSValueRef> parameters = {jsEntry.context.newNumber(static_cast<int32_t>(-2147483648)),
                                                    jsEntry.context.newNumber(4200000)};
    JSFunctionCallContext callContext(jsEntry.context, parameters.begin(), parameters.size(), jsEntry.exceptionTracker);

    auto longValue = jsEntry.context.callObjectAsConstructor(func.get(), callContext);
    jsEntry.checkException();

    // Without the Long constructor set, it should be recognized as a map at the beginning
    ASSERT_EQ(ValueType::Map, jsEntry.context.getValueType(longValue.get()));

    jsEntry.context.setLongConstructor(func);

    ASSERT_EQ(ValueType::Long, jsEntry.context.getValueType(longValue.get()));

    auto convertedValue = jsEntry.context.valueToLong(longValue.get(), jsEntry.exceptionTracker).toInt64();
    jsEntry.checkException();

    ASSERT_EQ(18038864790683648, convertedValue);

    // Test creation from C++
    auto createdValue = jsEntry.context.newLong(static_cast<int64_t>(133700000000), jsEntry.exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(ValueType::Long, jsEntry.context.getValueType(createdValue.get()));

    auto convertedValue2 = jsEntry.context.valueToLong(createdValue.get(), jsEntry.exceptionTracker).toInt64();
    jsEntry.checkException();

    ASSERT_EQ(133700000000, convertedValue2);
}

TEST_P(JSContextFixture, cachesSignedLongObject) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();

    auto func = jsEntry.context.evaluate(R""""(
            (function Long(low, high) {
                this.low = low;
                this.high = high;
            })
    )"""",
                                         "",
                                         jsEntry.exceptionTracker);
    jsEntry.checkException();

    jsEntry.context.setLongConstructor(func);

    auto value0 = jsEntry.context.newLong(static_cast<int64_t>(0), jsEntry.exceptionTracker);
    jsEntry.checkException();

    auto valueMinus7 = jsEntry.context.newLong(static_cast<int64_t>(-7), jsEntry.exceptionTracker);
    jsEntry.checkException();

    ASSERT_FALSE(jsEntry.context.isValueEqual(value0.get(), valueMinus7.get()));

    auto value0Bis = jsEntry.context.newLong(static_cast<int64_t>(0), jsEntry.exceptionTracker);

    jsEntry.checkException();
    ASSERT_TRUE(jsEntry.context.isValueEqual(value0.get(), value0Bis.get()));

    auto valueMinus7Bis = jsEntry.context.newLong(static_cast<int64_t>(-7), jsEntry.exceptionTracker);
    jsEntry.checkException();
    ASSERT_TRUE(jsEntry.context.isValueEqual(valueMinus7.get(), valueMinus7Bis.get()));

    auto valueLarge = jsEntry.context.newLong(static_cast<int64_t>(128), jsEntry.exceptionTracker);
    jsEntry.checkException();

    auto valueLargeBis = jsEntry.context.newLong(static_cast<int64_t>(128), jsEntry.exceptionTracker);
    jsEntry.checkException();

    ASSERT_FALSE(jsEntry.context.isValueEqual(valueLarge.get(), valueLargeBis.get()));
}

TEST_P(JSContextFixture, cachesUnsignedLongObject) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();

    auto func = jsEntry.context.evaluate(R""""(
            (function Long(low, high) {
                this.low = low;
                this.high = high;
            })
    )"""",
                                         "",
                                         jsEntry.exceptionTracker);
    jsEntry.checkException();

    jsEntry.context.setLongConstructor(func);

    auto value0 = jsEntry.context.newUnsignedLong(0, jsEntry.exceptionTracker);
    jsEntry.checkException();

    auto value1 = jsEntry.context.newUnsignedLong(1, jsEntry.exceptionTracker);
    jsEntry.checkException();

    ASSERT_FALSE(jsEntry.context.isValueEqual(value0.get(), value1.get()));

    auto value0Bis = jsEntry.context.newUnsignedLong(0, jsEntry.exceptionTracker);

    jsEntry.checkException();
    ASSERT_TRUE(jsEntry.context.isValueEqual(value0.get(), value0Bis.get()));

    auto valueLarge = jsEntry.context.newUnsignedLong(257, jsEntry.exceptionTracker);
    jsEntry.checkException();

    auto valueLargeBis = jsEntry.context.newUnsignedLong(257, jsEntry.exceptionTracker);
    jsEntry.checkException();

    ASSERT_FALSE(jsEntry.context.isValueEqual(valueLarge.get(), valueLargeBis.get()));
}

TEST_P(JSContextFixture, canDeallocateBridgedObjectsAfterJsContextIsDestroyed) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();

    Valdi::Ref<Valdi::ValueFunction> functionObject;
    {
        auto wrapper = createWrapper();

        functionObject = wrapper.evaluateScript("function() { return 42; }").getFunctionRef();
        ASSERT_TRUE(functionObject != nullptr);

        auto result = functionObject->call(ValueFunctionFlagsNone, {});

        ASSERT_TRUE(result) << result.description();
        ASSERT_EQ(Valdi::Value(42.0), result.value());
    }

    // After destruction of the context, it should return undefined
    auto result = functionObject->call(ValueFunctionFlagsNone, {});

    ASSERT_TRUE(result) << result.description();
    ASSERT_EQ(Valdi::Value::undefined(), result.value());

    functionObject = nullptr;
}

TEST_P(JSContextFixture, supportsPromise) {
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    JSValueRef returnValue;
    {
        auto jsEntry = wrapper.makeJsEntry();
        auto& context = jsEntry.context;
        auto& exceptionTracker = jsEntry.exceptionTracker;

        returnValue = context.evaluate(R""""(
            (() => {
                let returnValue = {value: 1};
                new Promise(resolve => {
                    resolve();
                }).then(() => {
                    returnValue.value = 2;
                });
                return returnValue;
            })()
        )"""",
                                       "unnamed.js",
                                       exceptionTracker);
        jsEntry.checkException();
    }

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto result = Valdi::jsValueToValue(context, returnValue.get(), ReferenceInfoBuilder(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(Value().setMapValue("value", Value(2.0)), result);
}

TEST_P(JSContextFixture, callsUnhandledPromiseCallbackWhenExceptionIsThrown) {
    SKIP_IF_V8("Ticket: 2259");
    SKIP_IF_HERMES("Unhandled Promise Callback is not yet supported in Hermes");

    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    MockJavaScriptContextListener listener;

    {
        auto jsEntry = wrapper.makeJsEntry();
        auto& context = jsEntry.context;
        auto& exceptionTracker = jsEntry.exceptionTracker;

        context.setListener(&listener);

        ASSERT_EQ(static_cast<size_t>(0), listener.unhandledPromiseResults.size());

        context.evaluate(R""""(
            (() => {
                new Promise(resolve => {
                    resolve();
                }).then(() => {
                    throw new Error('This is not good');
                });
            })()
        )"""",
                         "unnamed.js",
                         exceptionTracker);
        jsEntry.checkException();
        jsEntry.willExitVM();
        jsEntry.checkException();
    }

    ASSERT_EQ(static_cast<size_t>(1), listener.unhandledPromiseResults.size());

    ASSERT_EQ(ValueType::Error, listener.unhandledPromiseResults[0].getType());

    ASSERT_TRUE(listener.unhandledPromiseResults[0].getError().toStringBox().contains("This is not good"));
}

TEST_P(JSContextFixture, callsUnhandledPromiseCallbackWhenRejectedPromiseIsUnhandled) {
    SKIP_IF_V8("Ticket: 2259");
    SKIP_IF_HERMES("Unhandled Promise Callback is not yet supported in Hermes");

    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();
    MockJavaScriptContextListener listener;

    {
        auto jsEntry = wrapper.makeJsEntry();
        auto& context = jsEntry.context;
        auto& exceptionTracker = jsEntry.exceptionTracker;

        context.setListener(&listener);

        ASSERT_EQ(static_cast<size_t>(0), listener.unhandledPromiseResults.size());

        context.evaluate(R""""(
            (() => {
                new Promise((resolve, reject) => {
                    reject(42);
                });
            })()
        )"""",
                         "unnamed.js",
                         exceptionTracker);
        jsEntry.checkException();
    }

    ASSERT_EQ(static_cast<size_t>(1), listener.unhandledPromiseResults.size());

    ASSERT_EQ(ValueType::Double, listener.unhandledPromiseResults[0].getType());

    ASSERT_EQ(Value(42.0), listener.unhandledPromiseResults[0]);
}

TEST_P(JSContextFixture, doesntCallUnhandledPromiseCallbackForHandledPromises) {
    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();

    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    MockJavaScriptContextListener listener;
    context.setListener(&listener);

    ASSERT_EQ(static_cast<size_t>(0), listener.unhandledPromiseResults.size());

    context.evaluate(R""""(
        (() => {
            new Promise((resolve, reject) => {
                reject(42);
            }).catch(() => {
            });
        })()
    )"""",
                     "unnamed.js",
                     exceptionTracker);
    jsEntry.checkException();

    // Since the promise is handled with a catch, it should call the unhandled promise callback
    ASSERT_EQ(static_cast<size_t>(0), listener.unhandledPromiseResults.size());
}

void doCreateWrappedObject(JSEntry& jsEntry, const Ref<DummyObject>& object) {
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto wrappedObject = context.newWrappedObject(object, exceptionTracker);
    jsEntry.checkException();

    // Make a circular reference to force the engine to use a proper GC to collect the object
    context.setObjectProperty(wrappedObject.get(), "self", wrappedObject.get(), exceptionTracker);
    jsEntry.checkException();

    auto globalObject = context.getGlobalObject(exceptionTracker);
    jsEntry.checkException();

    context.setObjectProperty(globalObject.get(), "wrappedObject", wrappedObject.get(), exceptionTracker);
    jsEntry.checkException();
}

TEST_P(JSContextFixture, canGCWrappedObject) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();

    auto object = Valdi::makeShared<DummyObject>();

    {
        auto jsEntry = wrapper.makeJsEntry();
        auto& context = jsEntry.context;
        auto& exceptionTracker = jsEntry.exceptionTracker;

        doCreateWrappedObject(jsEntry, object);

        ASSERT_EQ(2, object.use_count());

        {
            auto globalObject = context.getGlobalObject(exceptionTracker);
            jsEntry.checkException();

            context.setObjectProperty(
                globalObject.get(), "wrappedObject", context.newUndefined().get(), exceptionTracker);
            jsEntry.checkException();
        }

        context.garbageCollect();
    }

    ASSERT_EQ(1, object.use_count());
}

TEST_P(JSContextFixture, canEvaluatePrecompiled) {
    MAIN_THREAD_INIT();

    BytesView moduleData;

    {
        // Precompile some code
        auto wrapper = createWrapper();
        auto jsEntry = wrapper.makeJsEntry();
        if (!jsEntry.context.supportsPreCompilation()) {
            GTEST_SKIP() << "JS entry does not support pre-compilation";
        }

        auto script = "return 42;";

        moduleData = jsEntry.context.preCompile(script, "file.js", jsEntry.exceptionTracker);
        jsEntry.checkException();
    }

    // Eval the precompiled code
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto bytecode = getPreCompiledJsModuleData(moduleData);

    ASSERT_TRUE(bytecode);

    auto value = context.evaluatePreCompiled(bytecode.value(), "file.js", exceptionTracker);
    jsEntry.checkException();

    Valdi::JSFunctionCallContext callContext(context, nullptr, 0, exceptionTracker);
    auto callResult = context.callObjectAsFunction(value.get(), callContext);
    jsEntry.checkException();

    auto result = context.valueToDouble(callResult.get(), exceptionTracker);
    jsEntry.checkException();

    ASSERT_EQ(42.0, result);
}

TEST_P(JSContextFixture, canCreateWeakReferences) {
    SKIP_IF_V8("Ticket: 2259");
#if SC_DESKTOP_LINUX
    SKIP_IF_JSCORE("Skipped since the version we use does not support weak references");
#endif

    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto object = Valdi::makeShared<DummyObject>();

    RefCountableAutoreleasePool autoreleasePool;

    JSValueRef wrappedObject;
    JSValueRef weakRef;
    {
        wrappedObject = context.newWrappedObject(object, exceptionTracker);
        jsEntry.checkException();

        weakRef = context.newWeakRef(wrappedObject.get(), exceptionTracker);
    }

    autoreleasePool.releaseAll();
    ASSERT_EQ(2, object.use_count());

    {
        auto ref = context.derefWeakRef(weakRef.get(), exceptionTracker);
        jsEntry.checkException();
        ASSERT_FALSE(context.isValueUndefined(ref.get()));

        auto unwrapped = context.valueToWrappedObject(ref.get(), exceptionTracker);
        jsEntry.checkException();

        ASSERT_EQ(object, castOrNull<DummyObject>(unwrapped));
    }

    wrappedObject = JSValueRef();
    context.garbageCollect();
    autoreleasePool.releaseAll();

    // Object should have been released by the JS engine
    ASSERT_EQ(1, object.use_count());

    {
        auto ref = context.derefWeakRef(weakRef.get(), exceptionTracker);
        jsEntry.checkException();

        ASSERT_TRUE(context.isValueUndefined(ref.get()));

        auto unwrapped = context.valueToWrappedObject(ref.get(), exceptionTracker);
        jsEntry.checkException();

        ASSERT_NE(object, castOrNull<DummyObject>(unwrapped));
    }
}

TEST_P(JSContextFixture, canDumpMemoryStatistics) {
#if SC_DESKTOP_LINUX
    SKIP_IF_JSCORE("Skipped since we don't compile a version of JSCore that supports dumping memory statistics.");
#endif

    MAIN_THREAD_INIT();

    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    context.garbageCollect();

    auto initialMemoryStatistics = context.dumpMemoryStatistics();

    ASSERT_TRUE(initialMemoryStatistics.memoryUsageBytes > 0);

    auto object = context.newObject(exceptionTracker);
    jsEntry.checkException();

    // Populate the object with some props to get the alloc size to grow
    for (size_t i = 0; i < 100; i++) {
        auto nb = context.newStringUTF8(fmt::format("What is up {}", i), exceptionTracker);
        jsEntry.checkException();

        auto propertyName = fmt::format("someProperty{}", i);
        context.setObjectProperty(object.get(), propertyName, nb.get(), exceptionTracker);

        jsEntry.checkException();
    }

    jsEntry.checkException();

    auto memoryStatisticsAfterNewObject = context.dumpMemoryStatistics();

    ASSERT_TRUE(initialMemoryStatistics.memoryUsageBytes < memoryStatisticsAfterNewObject.memoryUsageBytes);
    // TODO(simon): objectsCount does not seem to increase on JSCore for some reason
    ASSERT_TRUE(initialMemoryStatistics.objectsCount <= memoryStatisticsAfterNewObject.objectsCount);

    object = JSValueRef();

    context.garbageCollect();

    auto memoryStatisticsAfterGC = context.dumpMemoryStatistics();

    ASSERT_TRUE(memoryStatisticsAfterGC.memoryUsageBytes <= memoryStatisticsAfterNewObject.memoryUsageBytes);
    ASSERT_EQ(initialMemoryStatistics.objectsCount, memoryStatisticsAfterGC.objectsCount);
}

TEST_P(JSContextFixture, stackOverflowGuard) {
    SKIP_IF_V8("Ticket: 2259");
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;

    auto result =
        context.evaluate("var endless = () => {endless ()}; endless()", "stackOverflowGuardTest", exceptionTracker);
    ASSERT_TRUE(!jsEntry.exceptionTracker);

    if (!jsEntry.exceptionTracker) {
        auto error = exceptionTracker.extractError();
        auto errorString = error.toString();

        bool containsJSCErrorString = errorString.find("Maximum call stack size exceeded") != std::string::npos;
        bool containsQJSErrorString = errorString.find("stack overflow") != std::string::npos;

        ASSERT_TRUE(containsJSCErrorString ^ containsQJSErrorString);
    }
}

INSTANTIATE_TEST_SUITE_P(JSIntegrationTests,
                         JSContextFixture,
                         ::testing::Values(JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore,
                                           JavaScriptEngineTestCase::Hermes),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
