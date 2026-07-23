#include "JSBridgeTestFixture.hpp"
#include "JSIntegrationTestsUtils.hpp"
#include "valdi/runtime/JavaScript/JSNativeClassBinder.hpp"

#include <gtest/gtest.h>

using namespace Valdi;

namespace ValdiTest {

class JSNativeClassBinderTest : public JSBridgeTestFixture {
protected:
    JSContextWrapper createWrapper() {
        return JSContextWrapper(getJsBridge(), nullptr);
    }
};

class BinderTestObject final : public SimpleRefCountable {
public:
    static constexpr int64_t kLargeLongValue = static_cast<int64_t>(3670116110564327421LL);

    explicit BinderTestObject(int32_t value) : _value(value) {}

    BinderTestObject(int32_t value, JSFunctionNativeCallContext& callContext)
        : _value(value), _constructorContext(&callContext.getContext()) {}

    int32_t add(int32_t amount, JSFunctionNativeCallContext& callContext) {
        _lastContext = &callContext.getContext();
        _value += amount;
        return _value;
    }

    void reset(JSFunctionNativeCallContext& callContext) {
        _lastContext = &callContext.getContext();
        _value = 0;
    }

    int32_t getValue(JSFunctionNativeCallContext& /*callContext*/) const {
        return _value;
    }

    void setValue(int32_t value, JSFunctionNativeCallContext& /*callContext*/) {
        _value = value;
    }

    int32_t getReadOnly(JSFunctionNativeCallContext& /*callContext*/) const {
        return _value;
    }

    void setWriteOnly(int32_t value, JSFunctionNativeCallContext& /*callContext*/) {
        _value = value;
    }

    int32_t requiredInt(int32_t value, JSFunctionNativeCallContext& /*callContext*/) {
        _requiredCallCount++;
        return value;
    }

    int64_t echoLong(int64_t value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    int64_t largeLong(JSFunctionNativeCallContext& /*callContext*/) {
        return kLargeLongValue;
    }

    std::optional<int32_t> optionalInt(std::optional<int32_t> value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    std::optional<double> optionalDouble(std::optional<double> value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    std::optional<bool> optionalBool(std::optional<bool> value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    StringBox echoString(const StringBox& value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    Ref<StaticString> echoStaticString(const Ref<StaticString>& value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    Value echoValue(Value value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    int32_t typedArrayLength(const JSTypedArray& value, JSFunctionNativeCallContext& /*callContext*/) {
        return static_cast<int32_t>(value.length);
    }

    BytesView echoBytes(const BytesView& value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    Ref<RefCountable> echoWrappedObject(const Ref<RefCountable>& value, JSFunctionNativeCallContext& /*callContext*/) {
        return value;
    }

    JSValueRef echoJSValue(JSValue value, JSFunctionNativeCallContext& callContext) {
        return JSValueRef::makeRetained(callContext.getContext(), value);
    }

    static int32_t twice(int32_t value, JSFunctionNativeCallContext& /*callContext*/) {
        return value * 2;
    }

    static int32_t getSharedValue(JSFunctionNativeCallContext& /*callContext*/) {
        return _sharedValue;
    }

    static void setSharedValue(int32_t value, JSFunctionNativeCallContext& /*callContext*/) {
        _sharedValue = value;
    }

    static int32_t getClassReadOnly(JSFunctionNativeCallContext& /*callContext*/) {
        return _sharedValue;
    }

    static void setClassWriteOnly(int32_t value, JSFunctionNativeCallContext& /*callContext*/) {
        _sharedValue = value;
    }

    int32_t getRequiredCallCount() const {
        return _requiredCallCount;
    }

    IJavaScriptContext* getConstructorContext() const {
        return _constructorContext;
    }

    IJavaScriptContext* getLastContext() const {
        return _lastContext;
    }

    static void resetSharedValue() {
        _sharedValue = 0;
    }

private:
    int32_t _value;
    int32_t _requiredCallCount = 0;
    IJavaScriptContext* _constructorContext = nullptr;
    IJavaScriptContext* _lastContext = nullptr;
    static int32_t _sharedValue;
};

int32_t BinderTestObject::_sharedValue = 0;

struct BinderTestValues {
    Ref<BinderTestObject> instanceOpaque;
    JSValueRef cls;
    JSValueRef instance;
};

static BinderTestValues setUpBinderTest(JSEntry& jsEntry) {
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    BinderTestObject::resetSharedValue();

    auto longConstructor = context.evaluate("(function Long(low, high, unsigned) {"
                                            "  this.low = low;"
                                            "  this.high = high;"
                                            "  this.unsigned = unsigned;"
                                            "})",
                                            "JSNativeClassBinderLong.js",
                                            exceptionTracker);
    jsEntry.checkException();
    context.setLongConstructor(longConstructor);

    auto definition =
        JSNativeClassBinder<BinderTestObject>("BinderTestObject")
            .bindConstructor<int32_t>()
            .bindMethod<&BinderTestObject::add>("add", false, true, false)
            .bindMethod<&BinderTestObject::reset>("reset")
            .bindMethod<&BinderTestObject::requiredInt>("requiredInt")
            .bindMethod<&BinderTestObject::echoLong>("echoLong")
            .bindMethod<&BinderTestObject::largeLong>("largeLong")
            .bindMethod<&BinderTestObject::optionalInt>("optionalInt")
            .bindMethod<&BinderTestObject::optionalDouble>("optionalDouble")
            .bindMethod<&BinderTestObject::optionalBool>("optionalBool")
            .bindMethod<&BinderTestObject::echoString>("echoString")
            .bindMethod<&BinderTestObject::echoStaticString>("echoStaticString")
            .bindMethod<&BinderTestObject::echoValue>("echoValue")
            .bindMethod<&BinderTestObject::typedArrayLength>("typedArrayLength")
            .bindMethod<&BinderTestObject::echoBytes>("echoBytes")
            .bindMethod<&BinderTestObject::echoWrappedObject>("echoWrappedObject")
            .bindMethod<&BinderTestObject::echoJSValue>("echoJSValue")
            .bindAccessor<&BinderTestObject::getValue, &BinderTestObject::setValue>("value")
            .bindGetter<&BinderTestObject::getReadOnly>("readOnly")
            .bindSetter<&BinderTestObject::setWriteOnly>("writeOnly")
            .bindConstant("kind", context.newStringUTF8("binder-instance", exceptionTracker))
            .bindClassMethod<&BinderTestObject::twice>("twice")
            .bindClassAccessor<&BinderTestObject::getSharedValue, &BinderTestObject::setSharedValue>("sharedValue")
            .bindClassGetter<&BinderTestObject::getClassReadOnly>("classReadOnly")
            .bindClassSetter<&BinderTestObject::setClassWriteOnly>("classWriteOnly")
            .bindClassConstant("category", context.newStringUTF8("binder-class", exceptionTracker))
            .extractClassDefinition();
    jsEntry.checkException();

    EXPECT_EQ(STRING_LITERAL("BinderTestObject"), definition.getName());
    for (const auto& entry : definition.getEntries()) {
        EXPECT_FALSE(entry.getName().isNull());
    }

    auto cls = context.newNativeClass(nullptr, definition, exceptionTracker);
    jsEntry.checkException();
    auto instanceOpaque = makeShared<BinderTestObject>(10);
    auto instance = context.newObjectFromNativeClass(instanceOpaque, cls.get(), exceptionTracker);
    jsEntry.checkException();

    auto global = context.getGlobalObject(exceptionTracker);
    context.setObjectProperty(global.get(), "BinderTestObject", cls.get(), exceptionTracker);
    context.setObjectProperty(global.get(), "binderTestObject", instance.get(), exceptionTracker);
    jsEntry.checkException();

    return BinderTestValues{
        std::move(instanceOpaque),
        std::move(cls),
        std::move(instance),
    };
}

static JSValueRef evaluateBinderExpression(JSEntry& jsEntry, const char* source) {
    auto value = jsEntry.context.evaluate(source, "JSNativeClassBinder_tests.js", jsEntry.exceptionTracker);
    jsEntry.checkException();
    return value;
}

TEST_P(JSNativeClassBinderTest, bindsConstructorAndInstanceMembers) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto values = setUpBinderTest(jsEntry);

    ASSERT_EQ(15,
              context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.add(5)").get(), exceptionTracker));
    ASSERT_EQ(&context, values.instanceOpaque->getLastContext());

    evaluateBinderExpression(jsEntry, "binderTestObject.value = 21");
    ASSERT_EQ(21,
              context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.value").get(), exceptionTracker));
    ASSERT_EQ(
        21, context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.readOnly").get(), exceptionTracker));

    evaluateBinderExpression(jsEntry, "binderTestObject.writeOnly = 8");
    ASSERT_EQ(8,
              context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.value").get(), exceptionTracker));

    auto resetResult = evaluateBinderExpression(jsEntry, "binderTestObject.reset()");
    ASSERT_TRUE(context.isValueUndefined(resetResult.get()));

    auto constructed = evaluateBinderExpression(
        jsEntry, "globalThis.constructedBinderObject = new BinderTestObject(7); constructedBinderObject");
    auto constructedOpaque =
        castOrNull<BinderTestObject>(context.valueToWrappedObject(constructed.get(), exceptionTracker));
    ASSERT_NE(nullptr, constructedOpaque);
    ASSERT_EQ(
        7,
        context.valueToInt(evaluateBinderExpression(jsEntry, "constructedBinderObject.value").get(), exceptionTracker));
    ASSERT_EQ(&context, constructedOpaque->getConstructorContext());

    ASSERT_EQ(
        STRING_LITERAL("binder-instance"),
        context.valueToString(evaluateBinderExpression(jsEntry, "binderTestObject.kind").get(), exceptionTracker));
    ASSERT_FALSE(context.valueToBool(
        evaluateBinderExpression(jsEntry, "Object.getOwnPropertyDescriptor(BinderTestObject.prototype, 'add').writable")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateBinderExpression(jsEntry,
                                 "Object.getOwnPropertyDescriptor(BinderTestObject.prototype, 'add').enumerable")
            .get(),
        exceptionTracker));
    ASSERT_FALSE(context.valueToBool(
        evaluateBinderExpression(jsEntry,
                                 "Object.getOwnPropertyDescriptor(BinderTestObject.prototype, 'add').configurable")
            .get(),
        exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSNativeClassBinderTest, bindsClassMembers) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    setUpBinderTest(jsEntry);

    ASSERT_EQ(
        12, context.valueToInt(evaluateBinderExpression(jsEntry, "BinderTestObject.twice(6)").get(), exceptionTracker));
    evaluateBinderExpression(jsEntry, "BinderTestObject.sharedValue = 9");
    ASSERT_EQ(
        9,
        context.valueToInt(evaluateBinderExpression(jsEntry, "BinderTestObject.sharedValue").get(), exceptionTracker));
    ASSERT_EQ(9,
              context.valueToInt(evaluateBinderExpression(jsEntry, "BinderTestObject.classReadOnly").get(),
                                 exceptionTracker));

    evaluateBinderExpression(jsEntry, "BinderTestObject.classWriteOnly = 14");
    ASSERT_EQ(
        14,
        context.valueToInt(evaluateBinderExpression(jsEntry, "BinderTestObject.sharedValue").get(), exceptionTracker));
    ASSERT_EQ(
        STRING_LITERAL("binder-class"),
        context.valueToString(evaluateBinderExpression(jsEntry, "BinderTestObject.category").get(), exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSNativeClassBinderTest, convertsRequiredAndOptionalPrimitives) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto values = setUpBinderTest(jsEntry);

    ASSERT_EQ(4,
              context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.requiredInt(4)").get(),
                                 exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateBinderExpression(
            jsEntry,
            "(() => { try { binderTestObject.requiredInt(null); return false; } catch (e) { return true; } })()")
            .get(),
        exceptionTracker));
    ASSERT_TRUE(context.valueToBool(
        evaluateBinderExpression(
            jsEntry, "(() => { try { binderTestObject.requiredInt(); return false; } catch (e) { return true; } })()")
            .get(),
        exceptionTracker));
    ASSERT_EQ(1, values.instanceOpaque->getRequiredCallCount());

    auto longResult = evaluateBinderExpression(jsEntry, "binderTestObject.echoLong(binderTestObject.largeLong())");
    ASSERT_EQ(BinderTestObject::kLargeLongValue, context.valueToLong(longResult.get(), exceptionTracker).toInt64());
    ASSERT_TRUE(context.valueToBool(
        evaluateBinderExpression(
            jsEntry, "(() => { try { binderTestObject.echoLong(null); return false; } catch (e) { return true; } })()")
            .get(),
        exceptionTracker));

    ASSERT_TRUE(
        context.isValueUndefined(evaluateBinderExpression(jsEntry, "binderTestObject.optionalInt(null)").get()));
    ASSERT_TRUE(context.isValueUndefined(evaluateBinderExpression(jsEntry, "binderTestObject.optionalInt()").get()));
    ASSERT_EQ(5,
              context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.optionalInt(5)").get(),
                                 exceptionTracker));
    ASSERT_DOUBLE_EQ(
        1.5,
        context.valueToDouble(evaluateBinderExpression(jsEntry, "binderTestObject.optionalDouble(1.5)").get(),
                              exceptionTracker));
    ASSERT_TRUE(context.valueToBool(evaluateBinderExpression(jsEntry, "binderTestObject.optionalBool(true)").get(),
                                    exceptionTracker));
    jsEntry.checkException();
}

TEST_P(JSNativeClassBinderTest, convertsObjectLikeValues) {
    MAIN_THREAD_INIT();
    auto wrapper = createWrapper();
    auto jsEntry = wrapper.makeJsEntry();
    auto& context = jsEntry.context;
    auto& exceptionTracker = jsEntry.exceptionTracker;
    auto values = setUpBinderTest(jsEntry);

    ASSERT_EQ(STRING_LITERAL("hello"),
              context.valueToString(evaluateBinderExpression(jsEntry, "binderTestObject.echoString('hello')").get(),
                                    exceptionTracker));
    ASSERT_TRUE(context.isValueUndefined(evaluateBinderExpression(jsEntry, "binderTestObject.echoString(null)").get()));
    ASSERT_EQ(
        STRING_LITERAL("static"),
        context.valueToString(evaluateBinderExpression(jsEntry, "binderTestObject.echoStaticString('static')").get(),
                              exceptionTracker));
    ASSERT_EQ(
        3,
        context.valueToInt(evaluateBinderExpression(jsEntry, "binderTestObject.echoValue({ count: 3 }).count").get(),
                           exceptionTracker));
    ASSERT_EQ(
        3,
        context.valueToInt(
            evaluateBinderExpression(jsEntry, "binderTestObject.typedArrayLength(new Uint8Array([1, 2, 3]))").get(),
            exceptionTracker));
    ASSERT_EQ(2,
              context.valueToInt(
                  evaluateBinderExpression(jsEntry, "binderTestObject.echoBytes(new Uint8Array([4, 5])).length").get(),
                  exceptionTracker));
    ASSERT_EQ(5,
              context.valueToInt(
                  evaluateBinderExpression(jsEntry, "binderTestObject.echoBytes(new Uint8Array([4, 5]))[1]").get(),
                  exceptionTracker));

    auto wrappedResult = evaluateBinderExpression(jsEntry, "binderTestObject.echoWrappedObject(binderTestObject)");
    ASSERT_EQ(values.instanceOpaque,
              castOrNull<BinderTestObject>(context.valueToWrappedObject(wrappedResult.get(), exceptionTracker)));

    ASSERT_TRUE(
        context.valueToBool(evaluateBinderExpression(jsEntry,
                                                     "globalThis.rawBinderValue = { marker: 1 }; "
                                                     "binderTestObject.echoJSValue(rawBinderValue) === rawBinderValue")
                                .get(),
                            exceptionTracker));
    jsEntry.checkException();
}

INSTANTIATE_TEST_SUITE_P(JSNativeClassBinderTests,
                         JSNativeClassBinderTest,
                         ::testing::Values(JavaScriptEngineTestCase::QuickJS,
                                           JavaScriptEngineTestCase::JSCore,
                                           JavaScriptEngineTestCase::Hermes),
                         PrintJavaScriptEngineType());

} // namespace ValdiTest
