//
//  SCValdiObjCValueDelegate.m
//  valdi_core-ios
//
//  Created by Simon Corsin on 1/31/23.
//

#import "SCValdiObjCValueDelegate.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiMarshallableObjectUtils.h"
#import "valdi_core/SCValdiBridgedPromise+CPP.h"
#import "valdi_core/cpp/Utils/PlatformFunctionTrampolineUtils.hpp"
#import "valdi_core/cpp/Utils/StaticString.hpp"
#import "valdi_core/cpp/Utils/Format.hpp"
#import "valdi_core/cpp/Utils/ReferenceInfo.hpp"

#import <objc/runtime.h>

namespace ValdiIOS {

template<typename T>
static void checkResult(const Valdi::Result<T> &result) {
    if (!result) {
        NSExceptionThrowFromError(result.error());
    }
}

static void checkExceptionTracker(Valdi::ExceptionTracker &exceptionTracker) {
    if (!exceptionTracker) {
        NSExceptionThrowFromError(exceptionTracker.extractError());
    }
}

static inline bool objectIsNull(__unsafe_unretained id object) {
    return !object || [object isKindOfClass:[NSNull class]];
}

ObjCObjectStore::ObjCObjectStore() = default;
ObjCObjectStore::~ObjCObjectStore() = default;

const void *ObjCObjectStore::getObjCKey() const {
    return this;
}

Valdi::Ref<Valdi::RefCountable> ObjCObjectStore::getValueForObjectKey(const ObjCValue& objectKey, Valdi::ExceptionTracker &exceptionTracker) {
    id object = objectKey.getObject();
    if (objectIsNull(object)) {
        return nullptr;
    }

    id associatedObject = objc_getAssociatedObject(objectKey.getObject(), getObjCKey());
    SCValdiCppObject *cppObject = ObjectAs(associatedObject, SCValdiCppObject);
    if (!cppObject) {
        return nullptr;
    }

    return [cppObject ref];
}

void ObjCObjectStore::setValueForObjectKey(const ObjCValue& objectKey, const Valdi::Ref<Valdi::RefCountable>& value, Valdi::ExceptionTracker &exceptionTracker) {
    id object = objectKey.getObject();
    if (objectIsNull(object)) {
        if constexpr (kStrictNullCheckingEnabled) {
            exceptionTracker.onError("Object is null");
        }
        return;
    }

    id associatedObject = [SCValdiCppObject objectWithRef:value];
    objc_setAssociatedObject(object, getObjCKey(), associatedObject, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

std::optional<ObjCValue> ObjCObjectStore::getObjectForId(uint32_t objectId, Valdi::ExceptionTracker &exceptionTracker) {
    const auto &it = _objectById.find(objectId);
    if (it == _objectById.end()) {
        return std::nullopt;
    }

    id value = it->second.getValue();
    if (!value) {
        _objectById.erase(it);
        return std::nullopt;
    }

    return ObjCValue::makeObject(value);
}

void ObjCObjectStore::setObjectForId(uint32_t objectId, const ObjCValue& object, Valdi::ExceptionTracker &exceptionTracker) {
    _objectById.try_emplace(objectId, object.getObject(), NO);
}

/**
 Implementation of ValueFunction that forwards to an Objective-C block when called.
 The ObjCBlockHelper is used to help passing the unmarshalled parameters to the block
 when calling it, and the trampoline instance is used to facilitate unmarshalling parameters
 and marshalling return values.
 */
class ObjCBlockValueFunction: public Valdi::ValueFunction, public ValdiIOS::ObjCObjectIndirectRef {
public:
    ObjCBlockValueFunction(const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>>& trampoline,
                           __unsafe_unretained id block,
                           const Valdi::Ref<ObjCBlockHelper> &blockHelper,
                           const Valdi::StringBox &functionName): ValdiIOS::ObjCObjectIndirectRef([block copy], YES), _trampoline(trampoline), _blockHelper(blockHelper), _functionName(functionName) {}

    ~ObjCBlockValueFunction() override = default;

    Valdi::Value operator()(const Valdi::ValueFunctionCallContext &callContext) noexcept final {
        return Valdi::handleBridgeCall(_trampoline->getCallQueue(),
                                          _trampoline->isPromiseReturnType(),
                                          this,
                                          callContext,
                                          [](auto *mySelf, const auto &callContext) {
            return mySelf->doCall(callContext);
        });
    }

    std::string_view getFunctionType() const final {
        return "Objective-C Block";
    }

    id getBlock() const {
        return getValue();
    }

    const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> &getTrampoline() const {
        return _trampoline;
    }

private:
    Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> _trampoline;
    Valdi::Ref<ObjCBlockHelper> _blockHelper;
    Valdi::StringBox _functionName;

    Valdi::Value doCall(const Valdi::ValueFunctionCallContext &callContext) {
        id block = getBlock();
        if (block == nullptr) {
            return Valdi::Value();
        }

        auto parametersSize = _blockHelper->getParametersSize();

        Valdi::SmallVector<ObjCValue, 16> parameters;
        parameters.resize(parametersSize);

        /**
         Here we lay out the parameters that we will send to the block as ObjCValue,
         which will be converted into SCValdiFieldValue by the block helper.
         Objective-C blocks always have at least one parameter, which is the receiver,
         for which we pass the block itself. The rest of the parameters are added
         by the C++ trampoline, which will take the values from the marshallers and
         add them into the parmaeters array.
         */
        parameters[0] = ObjCValue::makeUnretainedObject(block);

        if (!_trampoline->unmarshallParameters(callContext.getParameters(), callContext.getParametersSize(), &parameters.data()[1], parametersSize - 1, Valdi::ReferenceInfoBuilder().withObject(_functionName), callContext.getExceptionTracker())) {
            return Valdi::Value();
        }

        auto result = _blockHelper->invokeWithBlock(block, parameters.data());

        return _trampoline->marshallReturnValue(result, Valdi::ReferenceInfoBuilder().withObject(_functionName), callContext.getExceptionTracker());
    }
};

/**
 The ObjCBlockClassDelegate is responsible for creating ValueFunction instances that are backed
 by an Objective-C block, and for creating Objective-C blocks that are backed by a ValueFunction.
 */
class ObjCBlockClassDelegate: public Valdi::PlatformFunctionClassDelegate<ObjCValue> {
public:
    ObjCBlockClassDelegate(const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>>& trampoline,
                           const Valdi::Ref<ObjCBlockHelper> &blockHelper): _trampoline(trampoline), _blockHelper(blockHelper) {}
    ~ObjCBlockClassDelegate() override = default;

    ObjCValue newFunction(const Valdi::Ref<Valdi::ValueFunction>& valueFunction, const Valdi::ReferenceInfoBuilder &referenceInfoBuilder, Valdi::ExceptionTracker &exceptionTracker) final {
        auto blockValueFunction = Valdi::castOrNull<ObjCBlockValueFunction>(valueFunction);
        if (blockValueFunction != nullptr && blockValueFunction->getTrampoline() == _trampoline) {
            return ObjCValue::makeObject(blockValueFunction->getBlock());
        }

        return _blockHelper->makeBlock(ObjCBlockClassDelegate::makeBlockTrampoline(_trampoline, _blockHelper, valueFunction, referenceInfoBuilder.build().getName()));
    }

    Valdi::Ref<Valdi::ValueFunction> toValueFunction(const ObjCValue */*receiver*/, const ObjCValue& function, const Valdi::ReferenceInfoBuilder &referenceInfoBuilder, Valdi::ExceptionTracker &exceptionTracker) final {
        return Valdi::makeShared<ObjCBlockValueFunction>(_trampoline, function.getObject(), _blockHelper, referenceInfoBuilder.build().getName());
    }

private:
    Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> _trampoline;
    Valdi::Ref<ObjCBlockHelper> _blockHelper;

    /**
     Return a trampoline block that takes the Objective-C block arguments as variadic arguments,
     pass the arguments to the given C++ trampoline which will take care of marshalling them before
     calling the given ValueFunction. The C++ trampoline takes care of converting the return value
     back into Objective-C, wrapped as a SCValdiFieldValue.
     */
    static SCValdiBlockTrampoline makeBlockTrampoline(Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> cppTrampoline,
                                                         Valdi::Ref<ObjCBlockHelper> blockHelper,
                                                         Valdi::Ref<Valdi::ValueFunction> valueFunction,
                                                         Valdi::StringBox functionName) {
        return ^SCValdiFieldValue(const void *receiver, ...) {
            auto parametersSize = blockHelper->getParametersSize() - 1;

            SCValdiFieldValue *parameters = reinterpret_cast<SCValdiFieldValue *>(alloca(sizeof(SCValdiFieldValue) * parametersSize));

            va_list v;
            va_start(v, receiver);
            for (size_t i = 0; i < parametersSize; i++) {
                parameters[i] = va_arg(v, SCValdiFieldValue);
            }
            va_end(v);

            Valdi::SimpleExceptionTracker exceptionTracker;

            auto result = ObjCValue::toObjCValues(parametersSize, parameters, &blockHelper->getParameterTypes()[1], [&](const auto *parameters) {
                return cppTrampoline->forwardCall(valueFunction.get(), parameters, parametersSize, Valdi::ReferenceInfoBuilder().withObject(functionName), nullptr, exceptionTracker);
            });

            checkExceptionTracker(exceptionTracker);

            return result.value().getAsReturnValue(blockHelper->getReturnType());
        };
    }
};

/**
 Implementation of ValueFunction holding an Objective-C receiver instance wrapped in a ValdiObject,
 and a selector. When called, the underlying Objective-C instance inside the receiver is unwrapped,
 and an Objective-C message is passed with the selector to that instance. The arguments to that Objective-C
 message are unmarshalled from Value into ObjC value by using the given C++ trampoline object.
 */
class ObjCSelectorValueFunction: public Valdi::ValueFunction {
public:
    ObjCSelectorValueFunction(const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>>& trampoline,
                          id receiver,
                          SEL selector,
                          const Valdi::Ref<ObjCBlockHelper> &blockHelper,
                        const Valdi::StringBox &functionName):
    _trampoline(trampoline), _receiverClass([receiver class]), _receiver(receiver), _selector(selector), _blockHelper(blockHelper), _functionName(functionName) {}

    ~ObjCSelectorValueFunction() override = default;

    id getReceiver(Valdi::ExceptionTracker &exceptionTracker) const {
        id instance = _receiver;
        if (!instance) {
            exceptionTracker.onError(Valdi::Error(STRING_FORMAT("Could not unwrap instance of '{}' because it was deallocated", ValdiIOS::StringFromNSString(NSStringFromClass(_receiverClass)))));
        }

        return instance;
    }

    Valdi::Value operator()(const Valdi::ValueFunctionCallContext &callContext) noexcept final {
        id receiver = getReceiver(callContext.getExceptionTracker());
        if (!receiver) {
            return Valdi::Value();
        }

        auto parametersSize = _blockHelper->getParametersSize();

        Valdi::SmallVector<ObjCValue, 16> parameters;
        parameters.resize(parametersSize);

        /**
         Here we lay out the parameters that we will send to the Objective-C method as ObjCValues,
         which will be converted into SCValdiFieldValue by the block helper.
         Objective-C method calls always have at least two parameters, the first one is the receiver,
         which is the object that will receive the message, and the selector.
         The rest of the parameters are added by the C++ trampoline, which will take the values from
         the marshallers and add them into the parmaeters array.
         */

        // The first parameter needs to the receiver
        parameters[0] = ObjCValue::makeUnretainedObject(receiver);
        // The second parameter needs to be the selector
        parameters[1] = ObjCValue::makePtr(_selector);

        if (!_trampoline->unmarshallParameters(callContext.getParameters(), callContext.getParametersSize(), &parameters.data()[2], parametersSize - 2, Valdi::ReferenceInfoBuilder().withObject(_functionName), callContext.getExceptionTracker())) {
            return Valdi::Value();
        }

        IMP method = SCValdiResolveMethodImpl(receiver, _selector);

        auto result = _blockHelper->invokeWithImp(method, parameters.data());

        return _trampoline->marshallReturnValue(result, Valdi::ReferenceInfoBuilder().withObject(_functionName), callContext.getExceptionTracker());
    }

    std::string_view getFunctionType() const final {
        return "Objective-C Method";
    }

private:
    Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> _trampoline;
    Class _receiverClass;
    __weak id _receiver;
    SEL _selector;
    Valdi::Ref<ObjCBlockHelper> _blockHelper;
    Valdi::StringBox _functionName;
};


/**
 The ObjCMethodClassDelegate is responsible for ValueFunction implementations backed by an Objective-C
 receiver and a selector.
 */
class ObjCMethodClassDelegate: public Valdi::PlatformFunctionClassDelegate<ObjCValue> {
public:
    ObjCMethodClassDelegate(const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>>& trampoline,
                      const Valdi::Ref<ObjCBlockHelper> &blockHelper): _trampoline(trampoline), _blockHelper(blockHelper) {}
    ~ObjCMethodClassDelegate() override = default;

    ObjCValue newFunction(const Valdi::Ref<Valdi::ValueFunction>& valueFunction, const Valdi::ReferenceInfoBuilder &referenceInfoBuilder, Valdi::ExceptionTracker &exceptionTracker) final {
        return _blockHelper->makeBlock(ObjCMethodClassDelegate::makeBlockTrampoline(_trampoline, _blockHelper, valueFunction, referenceInfoBuilder.build().getName()));
    }

    Valdi::Ref<Valdi::ValueFunction> toValueFunction(const ObjCValue *receiver, const ObjCValue& function, const Valdi::ReferenceInfoBuilder &referenceInfoBuilder, Valdi::ExceptionTracker &exceptionTracker) final {
        auto functionName = referenceInfoBuilder.build().getName();
        if (receiver != nullptr) {
            return Valdi::makeShared<ObjCSelectorValueFunction>(_trampoline, receiver->getObject(), (SEL)(function.getPtr()), _blockHelper, functionName);
        } else {
            return Valdi::makeShared<ObjCBlockValueFunction>(_trampoline, function.getObject(), _blockHelper, functionName);
        }
    }

private:
    Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> _trampoline;
    Valdi::Ref<ObjCBlockHelper> _blockHelper;

    static SCValdiBlockTrampoline makeBlockTrampoline(Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>> cppTrampoline,
                                                         Valdi::Ref<ObjCBlockHelper> blockHelper,
                                                         Valdi::Ref<Valdi::ValueFunction> valueFunction,
                                                         Valdi::StringBox functionName) {
        return ^SCValdiFieldValue(const void *receiver, ...) {
            auto parametersSize = blockHelper->getParametersSize() - 2;

            SCValdiFieldValue parameters[parametersSize];

            va_list v;
            va_start(v, receiver);
            // Ignore CMD
            va_arg(v, SCValdiFieldValue);

            for (size_t i = 0; i < parametersSize; i++) {
                parameters[i] = va_arg(v, SCValdiFieldValue);
            }
            va_end(v);

            Valdi::SimpleExceptionTracker exceptionTracker;
            auto result = ObjCValue::toObjCValues(parametersSize, parameters, &blockHelper->getParameterTypes()[2], [&](const auto *parameters) {
                return cppTrampoline->forwardCall(valueFunction.get(), parameters, parametersSize, Valdi::ReferenceInfoBuilder().withObject(functionName), nullptr, exceptionTracker);
            });

            checkExceptionTracker(exceptionTracker);

            return result.value().getAsReturnValue(blockHelper->getReturnType());
        };
    }
};

static void onCastError(__unsafe_unretained id object, __unsafe_unretained Class cls, Valdi::ExceptionTracker &exceptionTracker) {
    if constexpr (!kStrictNullCheckingEnabled) {
        if (objectIsNull(object)) {
            return;
        }
    }

    auto className = ValdiIOS::StringFromNSString(NSStringFromClass(cls));
    if (!object) {
        exceptionTracker.onError(fmt::format("Got null instead of expected object of class {}", className));
    } else {
        auto receivedClassName = ValdiIOS::StringFromNSString(NSStringFromClass([object class]));
        exceptionTracker.onError(fmt::format("Got object of class {} instead of expected object of class {}", receivedClassName, className));
    }
}

inline static bool checkClass(__unsafe_unretained id object, __unsafe_unretained Class cls, Valdi::ExceptionTracker &exceptionTracker) {
    if (![object isKindOfClass:cls]) {
        onCastError(object, cls, exceptionTracker);
        return false;
    }
    return true;
}

ObjCValueDelegate::ObjCValueDelegate(const ObjCTypesResolver &typeResolver): _typeResolver(typeResolver) {}

ObjCValueDelegate::~ObjCValueDelegate() = default;

    ObjCValue ObjCValueDelegate::newVoid() {
        return ObjCValue();
    }

    ObjCValue ObjCValueDelegate::newNull() {
        return ObjCValue::makeNull();
    }

    ObjCValue ObjCValueDelegate::newInt(int32_t value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeInt(value);
    }

    ObjCValue ObjCValueDelegate::newIntObject(int32_t value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(@(value));
    }

    ObjCValue ObjCValueDelegate::newLong(int64_t value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeLong(value);
    }

    ObjCValue ObjCValueDelegate::newLongObject(int64_t value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(@(value));
    }

    ObjCValue ObjCValueDelegate::newDouble(double value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeDouble(value);
    }

    ObjCValue ObjCValueDelegate::newDoubleObject(double value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(@(value));
    }

    ObjCValue ObjCValueDelegate::newBool(bool value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeBool(value);
    }

    ObjCValue ObjCValueDelegate::newBoolObject(bool value, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(@(value));
    }

    ObjCValue ObjCValueDelegate::newStringUTF8(std::string_view str, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(ValdiIOS::NSStringFromStringView(str));
    }

    ObjCValue ObjCValueDelegate::newStringUTF16(std::u16string_view str, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(ValdiIOS::NSStringFromUTF16StringView(str));
    }

    ObjCValue ObjCValueDelegate::newByteArray(const Valdi::BytesView& bytes, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(ValdiIOS::NSDataFromBuffer(bytes));
    }

    ObjCValue ObjCValueDelegate::newUntyped(const Valdi::Value& value,  const Valdi::ReferenceInfoBuilder &/*referenceInfoBuilder*/, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject(NSObjectFromValue(value));
    }

    ObjCValue ObjCValueDelegate::newMap(size_t capacity, Valdi::ExceptionTracker &exceptionTracker) {
        return ObjCValue::makeObject([NSMutableDictionary dictionaryWithCapacity:capacity]);
    }

    size_t ObjCValueDelegate::getMapEstimatedLength(const ObjCValue& map, Valdi::ExceptionTracker &exceptionTracker) {
        return [((NSDictionary *) map.getObject()) count];
    }

    void ObjCValueDelegate::setMapEntry(const ObjCValue& map, const ObjCValue &key, const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) {
        id objcKey = key.getObject();
        id objcValue = value.getObject();
        if (!objcValue) {
            [(NSMutableDictionary *)map.getObject() removeObjectForKey:objcKey];
        } else {
            [(NSMutableDictionary *)map.getObject() setValue:objcValue forKey:objcKey];
        }
    }

    void ObjCValueDelegate::visitMapKeyValues(const ObjCValue& map, Valdi::PlatformMapValueVisitor<ObjCValue>& visitor, Valdi::ExceptionTracker &exceptionTracker) {
        NSDictionary *dictionary = map.getObject();
        NSEnumerator *enumerator = dictionary.keyEnumerator;
        for (;;) {
            id key = [enumerator nextObject];
            if (!key) {
                return;
            }
            id value = dictionary[key];

            if (!visitor.visit(ObjCValue::makeUnretainedObject(key), ObjCValue::makeUnretainedObject(value), exceptionTracker)) {
                return;
            }
        }
    }

    Valdi::PlatformArrayBuilder<ObjCValue> ObjCValueDelegate::newArrayBuilder(size_t capacity, Valdi::ExceptionTracker &exceptionTracker) {
        return Valdi::PlatformArrayBuilder<ObjCValue>(ObjCValue::makeObject([NSMutableArray arrayWithCapacity:capacity]));
    }

    void ObjCValueDelegate::setArrayEntry(const Valdi::PlatformArrayBuilder<ObjCValue>& arrayBuilder, size_t index, const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) {
        NSMutableArray *objcArray = (NSMutableArray *)arrayBuilder.getBuilder().getObject();

        [objcArray setObject:(value.getObject() ?: [NSNull null]) atIndexedSubscript:index];
    }

    ObjCValue ObjCValueDelegate::finalizeArray(Valdi::PlatformArrayBuilder<ObjCValue>& value,
                                    Valdi::ExceptionTracker& exceptionTracker) {
        return ObjCValue::makeObject([value.getBuilder().getObject() copy]);
    }

    Valdi::PlatformArrayIterator<ObjCValue> ObjCValueDelegate::newArrayIterator(const ObjCValue& array, Valdi::ExceptionTracker& exceptionTracker) {
        __unsafe_unretained NSArray *nsArray = array.getObject();
        if (!checkClass(nsArray, [NSArray class], exceptionTracker)) {
            return Valdi::PlatformArrayIterator<ObjCValue>(ObjCValue::makeNull(), 0);
        }
        return Valdi::PlatformArrayIterator<ObjCValue>(array, [nsArray count]);
    }

    ObjCValue ObjCValueDelegate::getArrayItem(const Valdi::PlatformArrayIterator<ObjCValue>& arrayIterator, size_t index, Valdi::ExceptionTracker &exceptionTracker) {
        id object = [(NSArray *)arrayIterator.getIterator().getObject() objectAtIndex:index];
        return ObjCValue::makeObject(object);
    }

    ObjCValue ObjCValueDelegate::newBridgedPromise(const Valdi::Ref<Valdi::Promise>& promise,
                                                    const Valdi::Ref<Valdi::ValueMarshaller<ObjCValue>>& valueMarshaller,
                                                    Valdi::ExceptionTracker& exceptionTracker) {
        return ObjCValue::makeObject([[SCValdiBridgedPromise alloc] initWithPromise:promise valueMarshaller:valueMarshaller]);
    }

    Valdi::Ref<Valdi::PlatformObjectClassDelegate<ObjCValue>> ObjCValueDelegate::newObjectClass(const Valdi::Ref<Valdi::ClassSchema>& schema, Valdi::ExceptionTracker &exceptionTracker) {
        return _typeResolver.getObjectClassDelegateForName(schema->getClassName());
    }

    Valdi::Ref<Valdi::PlatformEnumClassDelegate<ObjCValue>> ObjCValueDelegate::newEnumClass(const Valdi::Ref<Valdi::EnumSchema>& schema, Valdi::ExceptionTracker &exceptionTracker) {
        return _typeResolver.getEnumClassDelegateForName(schema->getName());
    }

    Valdi::Ref<Valdi::PlatformFunctionClassDelegate<ObjCValue>> ObjCValueDelegate::newFunctionClass(const Valdi::Ref<Valdi::PlatformFunctionTrampoline<ObjCValue>>& trampoline,
                                                                                                          const Valdi::Ref<Valdi::ValueFunctionSchema>& schema, Valdi::ExceptionTracker &exceptionTracker) {
        auto blockHelper = _typeResolver.getBlockHelperForSchema(*schema);
        if (blockHelper == nullptr) {
            return nullptr;
        }

        if (schema->getAttributes().isMethod()) {
            return Valdi::makeShared<ObjCMethodClassDelegate>(trampoline, blockHelper);
        } else {
            return Valdi::makeShared<ObjCBlockClassDelegate>(trampoline, blockHelper);
        }
    }

    bool ObjCValueDelegate::valueIsNull(const ObjCValue& value) const {
        if (value.isPtr()) {
            return value.getPtr() == nullptr;
        } else {
            __unsafe_unretained id object = value.getObject();
            return objectIsNull(object);
        }
    }

    int32_t ObjCValueDelegate::valueToInt(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        return value.getInt();
    }

    int64_t ObjCValueDelegate::valueToLong(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        return value.getLong();
    }

    double ObjCValueDelegate::valueToDouble(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        return value.getDouble();
    }

    bool ObjCValueDelegate::valueToBool(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        return value.getBool();
    }

    int32_t ObjCValueDelegate::valueObjectToInt(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained NSNumber *number = value.getObject();
        if (!checkClass(number, [NSNumber class], exceptionTracker)) {
            return 0;
        }
        return [number intValue];
    }

    int64_t ObjCValueDelegate::valueObjectToLong(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained NSNumber *number = value.getObject();
        if (!checkClass(number, [NSNumber class], exceptionTracker)) {
            return 0;
        }
        return [number longLongValue];
    }

    double ObjCValueDelegate::valueObjectToDouble(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained NSNumber *number = value.getObject();
        if (!checkClass(number, [NSNumber class], exceptionTracker)) {
            return 0.0;
        }
        return [number doubleValue];
    }

    bool ObjCValueDelegate::valueObjectToBool(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained NSNumber *number = value.getObject();
        if (!checkClass(number, [NSNumber class], exceptionTracker)) {
            return false;
        }
        return [number boolValue];
    }

    Valdi::Ref<Valdi::StaticString> ObjCValueDelegate::valueToString(const ObjCValue& value, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained NSString * str = value.getObject();
        if (!checkClass(str, [NSString class], exceptionTracker)) {
            return Valdi::StaticString::makeUTF8(0);
        }

        return ValdiIOS::StaticStringFromNSString(str);
    }

    Valdi::BytesView ObjCValueDelegate::valueToByteArray(const ObjCValue& value, const Valdi::ReferenceInfoBuilder &/*referenceInfoBuilder*/, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained NSData *data = value.getObject();
        if (!checkClass(data, [NSData class], exceptionTracker)) {
            return Valdi::BytesView();
        }

        return ValdiIOS::BufferFromNSData(data);
    }

    Valdi::Ref<Valdi::Promise> ObjCValueDelegate::valueToPromise(const ObjCValue& value,
                                                    const Valdi::Ref<Valdi::ValueMarshaller<ObjCValue>>& valueMarshaller,
                                                    const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                                    Valdi::ExceptionTracker& exceptionTracker) const {
        return ValdiIOS::PromiseFromSCValdiPromise(value.getObject(), valueMarshaller);
    }

    Valdi::Value ObjCValueDelegate::valueToUntyped(const ObjCValue& value, const Valdi::ReferenceInfoBuilder &/*referenceInfoBuilder*/, Valdi::ExceptionTracker &exceptionTracker) const {
        __unsafe_unretained id object = value.getObject();
        return ValdiIOS::ValueFromNSObject(object);
    }

    Valdi::PlatformObjectStore<ObjCValue> &ObjCValueDelegate::getObjectStore() {
        return _objectStore;
    }


    ObjCValue ObjCValueDelegate::newTypedArray(Valdi::TypedArrayType arrayType,
                                               const Valdi::BytesView& bytes,
                                               Valdi::ExceptionTracker& exceptionTracker) {
        // TODO: preserve type
        return newByteArray(bytes, exceptionTracker);
    }

    ObjCValue ObjCValueDelegate::newES6Collection(Valdi::CollectionType type,
                                                  Valdi::ExceptionTracker& exceptionTracker) {
        return {};
    }

    void ObjCValueDelegate::setES6CollectionEntry(const ObjCValue& collection,
                                                  Valdi::CollectionType type,
                                                  std::initializer_list<ObjCValue> items,
                                                  Valdi::ExceptionTracker& exceptionTracker) {
    }

    void ObjCValueDelegate::visitES6Collection(const ObjCValue& collection,
                                               Valdi::PlatformMapValueVisitor<ObjCValue>& visitor,
                                               Valdi::ExceptionTracker& exceptionTracker) {
    }

    ObjCValue ObjCValueDelegate::newDate(double millisecondsSinceEpoch,
                                         Valdi::ExceptionTracker& exceptionTracker) {
        return {};
    }

    Valdi::Ref<Valdi::ValueTypedArray> ObjCValueDelegate::valueToTypedArray(const ObjCValue& value,
                                                                                  const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                                                                  Valdi::ExceptionTracker& exceptionTracker) const {
        // TODO preserve type
        return Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType,
                                                               valueToByteArray(value, referenceInfoBuilder, exceptionTracker));
    }

    double ObjCValueDelegate::dateToDouble(const ObjCValue& value,
                                           Valdi::ExceptionTracker& exceptionTracker) const {
        return 0.0;
    }

    Valdi::BytesView ObjCValueDelegate::encodeProto(const ObjCValue& proto,
                                                       const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                                       Valdi::ExceptionTracker& exceptionTracker) const {
        return {};
    }

    ObjCValue ObjCValueDelegate::decodeProto(const Valdi::BytesView& bytes,
                                             const std::vector<std::string_view>& classNameParts,
                                             const Valdi::ReferenceInfoBuilder& referenceInfoBuilder,
                                             Valdi::ExceptionTracker& exceptionTracker) const {
        return {};
    }
}
