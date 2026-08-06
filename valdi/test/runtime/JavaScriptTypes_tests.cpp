#include <gtest/gtest.h>

#include "valdi/runtime/Interfaces/IJavaScriptContext.hpp"
#include "valdi/runtime/JavaScript/JSPropertyNameIndex.hpp"
#include "valdi/runtime/JavaScript/JSValueRefHolder.hpp"
#include "valdi/runtime/JavaScript/JavaScriptTaskScheduler.hpp"
#include "valdi/runtime/JavaScript/WrappedJSValueRef.hpp"
#include "valdi/runtime/Utils/HexUtils.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "valdi_core/cpp/Utils/StaticString.hpp"
#include <array>
#include <deque>

using namespace Valdi;

namespace ValdiTest {

struct MyJSValue {
    int retainCount = 0;
};

struct DispatchJsThreadTask {
    Ref<Context> context;
    bool synchronous;
    JavaScriptThreadTask function;

    DispatchJsThreadTask() = default;
};

struct MockJavaScriptTaskScheduler : public JavaScriptTaskScheduler {
    std::deque<DispatchJsThreadTask> dispatchedTasks;

    bool isInJsThreadValue = true;
    bool ignoreTasks = false;

    void dispatchOnJsThread(Ref<Context> ownerContext,
                            JavaScriptTaskScheduleType scheduleType,
                            uint32_t delayMs,
                            JavaScriptThreadTask&& function) override {
        if (ignoreTasks) {
            return;
        }

        auto& task = dispatchedTasks.emplace_back();
        task.context = std::move(ownerContext);
        task.synchronous = scheduleType == JavaScriptTaskScheduleTypeAlwaysSync;
        task.function = std::move(function);
    }

    bool isInJsThread() override {
        return isInJsThreadValue;
    }

    Valdi::Ref<Valdi::Context> getLastDispatchedContext() const override {
        return nullptr;
    }

    size_t flushTasks(IJavaScriptContext& jsContext) {
        size_t ranTasks = 0;
        while (dispatchedTasks.size() > 0) {
            auto task = std::move(dispatchedTasks[0]);
            dispatchedTasks.pop_front();

            JSExceptionTracker exceptionTracker(jsContext);
            JavaScriptEntryParameters jsEntry(jsContext, exceptionTracker, task.context);

            task.function(jsEntry);
            ranTasks++;
        }

        return ranTasks;
    }

    std::vector<JavaScriptCapturedStacktrace> captureStackTraces(std::chrono::steady_clock::duration timeout) override {
        return {};
    }
};

struct MockJavaScriptContext : public IJavaScriptContext {
public:
    MockJavaScriptContext(JavaScriptTaskScheduler* taskScheduler) : IJavaScriptContext(taskScheduler) {}

    void onInitialize(JSExceptionTracker& exceptionTracker) override {}

    JSValueRef getGlobalObject(JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef evaluate(const std::string& script,
                        const std::string_view& sourceFilename,
                        JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef evaluatePreCompiled(const BytesView& /*bytes*/,
                                   const std::string_view& /*sourceFilename*/,
                                   JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    BytesView preCompile(const std::string_view& /*script*/,
                         const std::string_view& /*sourceFilename*/,
                         JSExceptionTracker& exceptionTracker) override {
        return BytesView();
    }

    JSPropertyNameRef newPropertyName(const std::string_view& str) override {
        auto internedString = StringCache::getGlobal().makeString(str);
        return JSPropertyNameRef(
            this, JSPropertyName(unsafeBridgeRetain(internedString.getInternedString().get())), true);
    }

    StringBox propertyNameToString(const JSPropertyName& propertyName) override {
        auto ptr = propertyName.bridge<void*>();
        return StringBox(unsafeBridge<InternedStringImpl>(ptr));
    }

    JSValueRef newObject(JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newFunction(const Ref<JSFunction>& callable, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef onNewBool(bool boolean) override {
        return JSValueRef();
    }

    JSValueRef onNewNumber(int32_t number) override {
        return JSValueRef();
    }

    JSValueRef onNewNumber(double number) override {
        return JSValueRef();
    }

    JSValueRef newStringUTF8(const std::string_view& str, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newStringUTF16(const std::u16string_view& str, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef onNewNull() override {
        return JSValueRef();
    }

    JSValueRef onNewUndefined() override {
        return JSValueRef();
    }

    JSValueRef newArray(size_t /*initialSize*/, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newArrayBuffer(const BytesView& buffer, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newTypedArrayFromArrayBuffer(const TypedArrayType& type,
                                            const JSValue& arrayBuffer,
                                            JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newWrappedObject(const Ref<RefCountable>& wrappedObject, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newNativeClass(const Ref<RefCountable>& classOpaque,
                              const JSClassDefinition& classDefinition,
                              JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newObjectFromNativeClass(const Ref<RefCountable>& opaque,
                                        const JSValue& cls,
                                        JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef newWeakRef(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef getObjectProperty(const JSValue& object,
                                 const std::string_view& propertyName,
                                 JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef getObjectProperty(const JSValue& object,
                                 const JSPropertyName& propertyName,
                                 JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    JSValueRef getObjectPropertyForIndex(const JSValue& object,
                                         size_t index,
                                         JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    bool hasObjectProperty(const JSValue& object, const JSPropertyName& propertyName) override {
        return false;
    }

    void setObjectProperty(const JSValue& object,
                           const std::string_view& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) override {}

    void setObjectProperty(const JSValue& object,
                           const JSPropertyName& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) override {}

    void setObjectProperty(const JSValue& object,
                           const JSValue& propertyName,
                           const JSValue& propertyValue,
                           bool enumerable,
                           JSExceptionTracker& exceptionTracker) override {}

    void setObjectPropertyIndex(const JSValue& object,
                                size_t index,
                                const JSValue& value,
                                JSExceptionTracker& exceptionTracker) override {}

    JSValueRef callObjectAsFunction(const JSValue& object, JSFunctionCallContext& callContext) override {
        return JSValueRef();
    }

    JSValueRef callObjectAsConstructor(const JSValue& object, JSFunctionCallContext& callContext) override {
        return JSValueRef();
    }

    void visitObjectPropertyNames(const JSValue& object,
                                  JSExceptionTracker& exceptionTracker,
                                  IJavaScriptPropertyNamesVisitor& visitor) override {}

    JSValueRef derefWeakRef(const JSValue& weakRef, JSExceptionTracker& exceptionTracker) override {
        return JSValueRef();
    }

    StringBox valueToString(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return StringBox();
    }

    Ref<StaticString> valueToStaticString(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return nullptr;
    }

    bool valueToBool(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return false;
    }

    double valueToDouble(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return 0;
    }

    int32_t valueToInt(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return 0;
    }

    Ref<RefCountable> valueToWrappedObject(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return nullptr;
    }

    JSTypedArray valueToTypedArray(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return JSTypedArray();
    }

    Ref<JSFunction> valueToFunction(const JSValue& value, JSExceptionTracker& exceptionTracker) override {
        return nullptr;
    }

    ValueType getValueType(const JSValue& value) override {
        return ValueType::Undefined;
    }

    bool isValueUndefined(const JSValue& value) override {
        return false;
    }

    bool isValueNull(const JSValue& value) override {
        return false;
    }

    bool isValueFunction(const JSValue& value) override {
        return false;
    }

    bool isValueNumber(const JSValue& value) override {
        return false;
    }

    bool isValueLong(const JSValue& value) override {
        return false;
    }

    bool isValueObject(const JSValue& value) override {
        return false;
    }

    bool isValueEqual(const JSValue& left, const JSValue& right) override {
        return false;
    }

    void retainValue(const JSValue& value) override {
        value.bridge<MyJSValue*>()->retainCount++;
    }

    void releaseValue(const JSValue& value) override {
        value.bridge<MyJSValue*>()->retainCount--;
    }

    void retainPropertyName(const JSPropertyName& value) override {
        auto ptr = value.bridge<void*>();
        unsafeBridge<InternedStringImpl>(ptr)->unsafeRetainInner();
    }

    void releasePropertyName(const JSPropertyName& value) override {
        auto ptr = value.bridge<void*>();
        unsafeBridge<InternedStringImpl>(ptr)->unsafeReleaseInner();
    }

    void garbageCollect() override {}

    void enqueueMicrotask(const JSValue& value, JSExceptionTracker& exceptionTracker) override {}

    JavaScriptContextMemoryStatistics dumpMemoryStatistics() override {
        return JavaScriptContextMemoryStatistics();
    }
};

TEST(JSValue, canBeConstructed) {
    MyJSValue myValue;
    myValue.retainCount = 42;

    auto jsValue = JSValue(&myValue);
    ASSERT_EQ(42, (*reinterpret_cast<MyJSValue**>(&jsValue))->retainCount);
}

TEST(JSValue, canBeCompared) {
    MyJSValue value1;
    value1.retainCount = 13;

    MyJSValue value2;
    value2.retainCount = 42;

    ASSERT_EQ(JSValue(&value1), JSValue(&value1));
    ASSERT_EQ(JSValue(&value2), JSValue(&value2));
    ASSERT_EQ(JSValue(), JSValue());

    ASSERT_NE(JSValue(&value1), JSValue(&value2));
    ASSERT_NE(JSValue(&value2), JSValue(&value1));
}

TEST(JSValue, canBridge) {
    MyJSValue myValue;
    myValue.retainCount = 42;

    auto jsValue = JSValue(&myValue);
    auto bridged = jsValue.bridge<MyJSValue*>();
    ASSERT_EQ(bridged, &myValue);
}

TEST(JSValueRef, retainOnCopy) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    JSValueRef ref(&jsContext, JSValue(&myValue), true);

    ASSERT_EQ(1, myValue.retainCount);

    auto refCopy = ref;

    ASSERT_EQ(2, myValue.retainCount);

    JSValueRef refCopy2(refCopy);

    ASSERT_EQ(3, myValue.retainCount);
}

TEST(JSValueRef, releaseOnDestruct) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 2;

    {
        JSValueRef ref(&jsContext, JSValue(&myValue), true);
    }

    ASSERT_EQ(1, myValue.retainCount);

    JSValueRef ref(&jsContext, JSValue(&myValue), true);
    ref = JSValueRef();

    ASSERT_EQ(0, myValue.retainCount);
}

TEST(JSValueRef, canBeCheckedForEmpty) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    JSValueRef ref(&jsContext, JSValue(&myValue), true);
    JSValueRef ref2;

    ASSERT_FALSE(ref.empty());
    ASSERT_TRUE(ref2.empty());

    ref = JSValueRef();
    ASSERT_TRUE(ref.empty());
}

TEST(JSValueRef, canBeMoved) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    {
        JSValueRef ref(&jsContext, JSValue(&myValue), true);
        ASSERT_FALSE(ref.empty());
        ASSERT_EQ(1, myValue.retainCount);

        JSValueRef ref2(std::move(ref));
        ASSERT_EQ(1, myValue.retainCount);
        ASSERT_TRUE(ref.empty());
        ASSERT_FALSE(ref2.empty());

        JSValueRef ref3;
        ref3 = std::move(ref2);
        ASSERT_EQ(1, myValue.retainCount);
        ASSERT_TRUE(ref2.empty());
        ASSERT_FALSE(ref3.empty());
    }
    ASSERT_EQ(0, myValue.retainCount);
}

TEST(JSValueRef, canRetainExplicitly) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    auto ref = JSValueRef::makeRetained(jsContext, JSValue(&myValue));
    ASSERT_EQ(2, myValue.retainCount);
}

TEST(JSValueRef, doesntRetainOnCopyIfItWasNotRetained) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    {
        JSValueRef ref(&jsContext, JSValue(&myValue), false);

        ASSERT_EQ(1, myValue.retainCount);

        auto refCopy = ref;

        ASSERT_EQ(1, myValue.retainCount);

        JSValueRef refCopy2(refCopy);

        ASSERT_EQ(1, myValue.retainCount);
    }

    ASSERT_EQ(1, myValue.retainCount);
}

TEST(JSValueRefHolder, dispatchWhenDestroyedOutOfTheJsThread) {
    auto taskScheduler = makeShared<MockJavaScriptTaskScheduler>();
    MockJavaScriptContext jsContext(taskScheduler.get());

    MyJSValue myValue;
    myValue.retainCount = 1;

    taskScheduler->isInJsThreadValue = false;

    auto context = makeShared<Context>(1, Valdi::strongSmallRef(&ConsoleLogger::getLogger()));
    Context::setCurrent(context);
    {
        JSExceptionTracker exceptionTracker(jsContext);
        JSValueRefHolder refHolder(jsContext, JSValue(&myValue), ReferenceInfo(), exceptionTracker, false);

        ASSERT_EQ(static_cast<size_t>(0), taskScheduler->dispatchedTasks.size());
        ASSERT_EQ(2, myValue.retainCount);
    }

    ASSERT_EQ(static_cast<size_t>(1), taskScheduler->dispatchedTasks.size());
    ASSERT_EQ(2, myValue.retainCount);

    JSExceptionTracker exceptionTracker(jsContext);
    JavaScriptEntryParameters jsEntry(jsContext, exceptionTracker, nullptr);
    taskScheduler->dispatchedTasks[0].function(jsEntry);

    ASSERT_EQ(1, myValue.retainCount);
}

TEST(JSValueRefHolder, insertAndRemoveItselfAsDisposable) {
    auto taskScheduler = makeShared<MockJavaScriptTaskScheduler>();
    MockJavaScriptContext jsContext(taskScheduler.get());

    MyJSValue myValue;
    myValue.retainCount = 1;

    taskScheduler->isInJsThreadValue = true;

    auto context = makeShared<Context>(1, Valdi::strongSmallRef(&ConsoleLogger::getLogger()));
    Context::setCurrent(context);

    {
        JSExceptionTracker exceptionTracker(jsContext);
        JSValueRefHolder refHolder(jsContext, JSValue(&myValue), ReferenceInfo(), exceptionTracker, false);

        auto disposables = context->getAllDisposables();

        ASSERT_EQ(static_cast<size_t>(1), disposables.size());

        ASSERT_EQ(&refHolder, disposables[0]);
    }
    ASSERT_EQ(static_cast<size_t>(1), taskScheduler->flushTasks(jsContext));
    auto disposables = context->getAllDisposables();

    ASSERT_EQ(static_cast<size_t>(0), disposables.size());
}

TEST(JSValueRefHolder, releasesWhenDisposed) {
    auto taskScheduler = makeShared<MockJavaScriptTaskScheduler>();
    MockJavaScriptContext jsContext(taskScheduler.get());

    MyJSValue myValue;
    myValue.retainCount = 1;

    taskScheduler->isInJsThreadValue = true;

    auto context = makeShared<Context>(1, Valdi::strongSmallRef(&ConsoleLogger::getLogger()));
    Context::setCurrent(context);
    context->retainDisposables();

    {
        JSExceptionTracker exceptionTracker(jsContext);
        JSValueRefHolder refHolder(jsContext, JSValue(&myValue), ReferenceInfo(), exceptionTracker, false);
        ASSERT_EQ(2, myValue.retainCount);

        context->releaseDisposables();
    }

    ASSERT_EQ(static_cast<size_t>(1), taskScheduler->flushTasks(jsContext));

    ASSERT_EQ(1, myValue.retainCount);
    ASSERT_EQ(static_cast<size_t>(0), taskScheduler->dispatchedTasks.size());
}

TEST(JSValueRefHolder, canDeallocateConcurrently) {
    auto queue1 = DispatchQueue::create(STRING_LITERAL("Thread1"), ThreadQoSClassMax);
    auto queue2 = DispatchQueue::create(STRING_LITERAL("Thread2"), ThreadQoSClassMax);

    auto taskScheduler = makeShared<MockJavaScriptTaskScheduler>();
    taskScheduler->ignoreTasks = true;
    MockJavaScriptContext jsContext(taskScheduler.get());
    MyJSValue myValue;
    myValue.retainCount = 1;

    std::vector<Shared<JSValueRefHolder>> refs;

    for (size_t i = 0; i < 10; i++) {
        auto context = makeShared<Context>(1, Valdi::strongSmallRef(&ConsoleLogger::getLogger()));
        context->retainDisposables();

        Context::setCurrent(context);

        for (size_t j = 0; j < 5; j++) {
            JSExceptionTracker exceptionTracker(jsContext);
            auto ref = makeShared<WrappedJSValueRef>(jsContext, JSValue(&myValue), ReferenceInfo(), exceptionTracker);
            refs.emplace_back(ref.toShared());
        }

        Context::setCurrent(nullptr);

        queue1->async([&]() {
            while (!refs.empty()) {
                refs.pop_back();
            }
        });

        queue2->async([&]() { context->releaseDisposables(); });

        queue1->sync([]() {});
        queue2->sync([]() {});

        taskScheduler->dispatchedTasks.clear();
    }
}

TEST(JSExceptionTracker, isTruthyWithoutException) {
    MockJavaScriptContext jsContext(nullptr);
    JSExceptionTracker exceptionTracker(jsContext);

    ASSERT_TRUE(exceptionTracker);
}

TEST(JSExceptionTracker, isFalsyWithException) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    JSExceptionTracker exceptionTracker(jsContext);

    ASSERT_TRUE(exceptionTracker);

    exceptionTracker.storeException(JSValue(&myValue));

    ASSERT_FALSE(exceptionTracker);

    exceptionTracker.clearError();

    ASSERT_TRUE(exceptionTracker);
}

TEST(JSExceptionTracker, canClearAndGetException) {
    MockJavaScriptContext jsContext(nullptr);

    MyJSValue myValue;
    myValue.retainCount = 1;

    JSExceptionTracker exceptionTracker(jsContext);

    exceptionTracker.storeException(JSValue(&myValue));

    ASSERT_FALSE(exceptionTracker);

    auto exception = exceptionTracker.getExceptionAndClear();

    ASSERT_TRUE(exceptionTracker);

    ASSERT_EQ(JSValue(&myValue), exception.get());
}

TEST(PropertyNameIndex, canLoadAndFetchProperties) {
    MockJavaScriptContext jsContext(nullptr);

    JSPropertyNameIndex<2> propertyNames;
    propertyNames.set(0, "ThisIsFirst");
    propertyNames.set(1, "ThisIsSecond");

    propertyNames.setContext(&jsContext);

    auto firstProperty = propertyNames.getJsName(0);
    auto secondProperty = propertyNames.getJsName(1);

    auto firstPropertyCpp = jsContext.propertyNameToString(firstProperty);
    auto secondPropertyCpp = jsContext.propertyNameToString(secondProperty);

    ASSERT_EQ(STRING_LITERAL("ThisIsFirst"), firstPropertyCpp);
    ASSERT_EQ(STRING_LITERAL("ThisIsSecond"), secondPropertyCpp);
}

TEST(PropertyNameIndex, canUnloadProperties) {
    MockJavaScriptContext jsContext(nullptr);

    auto firstPropertyCpp = STRING_LITERAL("THIS_IS_MY_STRING");
    auto secondPropertyCpp = STRING_LITERAL("THIS_IS_MY_SECOND_STRING");

    JSPropertyNameIndex<2> propertyNames;
    propertyNames.set(0, firstPropertyCpp.toStringView());
    propertyNames.set(1, secondPropertyCpp.toStringView());

    ASSERT_EQ(1, firstPropertyCpp.getInternedString()->retainCount());
    ASSERT_EQ(1, secondPropertyCpp.getInternedString()->retainCount());

    propertyNames.setContext(&jsContext);

    ASSERT_EQ(2, firstPropertyCpp.getInternedString()->retainCount());
    ASSERT_EQ(2, secondPropertyCpp.getInternedString()->retainCount());

    propertyNames.setContext(nullptr);

    ASSERT_EQ(1, firstPropertyCpp.getInternedString()->retainCount());
    ASSERT_EQ(1, secondPropertyCpp.getInternedString()->retainCount());
}

} // namespace ValdiTest
