//
//  SwiftValdiMarshaller.cpp
//  valdi_core
//
//  Created by Edward Lee on 4/30/24.
//

#include "valdi_core/ios/swift/cpp/SwiftValdiMarshaller.h"
#include "valdi_core/cpp/Utils/ByteBuffer.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/Marshaller.hpp"
#include "valdi_core/cpp/Utils/ResolvablePromise.hpp"
#include "valdi_core/cpp/Utils/ValueFunction.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#include "valdi_core/ios/swift/cpp/SwiftProxyTypedObject.hpp"
#include "valdi_core/ios/swift/cpp/SwiftValdiMarshallableObjectRegistryImpl.hpp"
#include "valdi_core/ios/swift/cpp/SwiftValdiMarshallerPrivate.hpp"
#include "valdi_core/ios/swift/cpp/SwiftValueFunction.hpp"

class IosSwiftMarshaller : public Valdi::Marshaller {
public:
    IosSwiftMarshaller() : Valdi::Marshaller(_exceptionTracker) {}
    ~IosSwiftMarshaller() override = default;

private:
    Valdi::SimpleExceptionTracker _exceptionTracker;
};

static SwiftString SwiftStringFromString(const std::string& str) {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

static SwiftValdiMarshallerRef SwiftValdiMarshaller_Wrap(Valdi::Marshaller* marshaller) {
    return reinterpret_cast<SwiftValdiMarshallerRef>(marshaller);
}

Valdi::Marshaller* SwiftValdiMarshaller_Unwrap(SwiftValdiMarshallerRef marshallerRef) {
    return reinterpret_cast<Valdi::Marshaller*>(marshallerRef);
}

static Valdi::Value SwiftValdiMarshaller_GetValue(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto value = marshallerCpp->get(index);
    return value;
}

static Valdi::Value SwiftValdiMarshaller_GetValueOrUndefined(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto value = marshallerCpp->getOrUndefined(index);
    return value;
}

SwiftValdiMarshallerRef SwiftValdiMarshaller_Create() {
    Valdi::Marshaller* marshaller = new IosSwiftMarshaller();
    return SwiftValdiMarshaller_Wrap(marshaller);
}

void SwiftValdiMarshaller_Destroy(SwiftValdiMarshallerRef marshaller) {
    delete SwiftValdiMarshaller_Unwrap(marshaller);
}

SwiftInt SwiftValdiMarshaller_Size(SwiftValdiMarshallerRef marshaller) {
    return (SwiftInt)SwiftValdiMarshaller_Unwrap(marshaller)->size();
}

void SwiftValdiMarshaller_Pop(SwiftValdiMarshallerRef marshaller) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pop();
}

void SwiftValdiMarshaller_PopCount(SwiftValdiMarshallerRef marshaller, SwiftInt count) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->popCount(count);
}

SwiftInt SwiftValdiMarshaller_GetCount(SwiftValdiMarshallerRef marshaller) {
    return (SwiftInt)SwiftValdiMarshaller_Unwrap(marshaller)->size();
}

SwiftBool SwiftValdiMarshaller_CheckError(SwiftValdiMarshallerRef marshaller) {
    return !(SwiftValdiMarshaller_Unwrap(marshaller)->getExceptionTracker());
}

SwiftString SwiftValdiMarshaller_GetError(SwiftValdiMarshallerRef marshallerRef) {
    auto marshaller = SwiftValdiMarshaller_Unwrap(marshallerRef);
    SC_ASSERT(!marshaller->getExceptionTracker());
    auto error = marshaller->getExceptionTracker().extractError().toString();
    return SwiftStringFromString(error);
}

void SwiftValdiMarshaller_SetError(SwiftValdiMarshallerRef marshaller, const char* errorMessage) {
    SwiftValdiMarshaller_Unwrap(marshaller)->getExceptionTracker().onError(errorMessage);
}

SwiftString SwiftValdiMarshaller_ToString(SwiftValdiMarshallerRef marshaller, SwiftInt index, SwiftBool indent) {
    auto str = SwiftValdiMarshaller_GetValue(marshaller, index).toString(indent);
    return SwiftStringFromString(str);
}

SwiftInt SwiftValdiMarshaller_PushMap(SwiftValdiMarshallerRef marshaller, SwiftInt initialCapacity) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushMap(initialCapacity);
}

SwiftInt SwiftValdiMarshaller_PushArray(SwiftValdiMarshallerRef marshaller, SwiftInt capacity) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushArray(capacity);
}

SwiftInt SwiftValdiMarshaller_PushDouble(SwiftValdiMarshallerRef marshaller, SwiftDouble d) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushDouble(d);
}

SwiftInt SwiftValdiMarshaller_PushLong(SwiftValdiMarshallerRef marshaller, SwiftInt64 i) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushLong(i);
}

SwiftInt SwiftValdiMarshaller_PushBool(SwiftValdiMarshallerRef marshaller, SwiftBool boolean) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushBool(boolean);
}

SwiftInt SwiftValdiMarshaller_PushInt(SwiftValdiMarshallerRef marshaller, SwiftInt32 i) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushInt(i);
}

SwiftInt SwiftValdiMarshaller_PushString(SwiftValdiMarshallerRef marshaller, SwiftStringView str) {
    return SwiftValdiMarshaller_Unwrap(marshaller)
        ->pushString(Valdi::StringCache::getGlobal().makeString(std::string_view(str.buf, str.size)));
}

SwiftInt SwiftValdiMarshaller_PushUndefined(SwiftValdiMarshallerRef marshaller) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushUndefined();
}

SwiftInt SwiftValdiMarshaller_PushNull(SwiftValdiMarshallerRef marshaller) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushNull();
}

SwiftInt SwiftValdiMarshaller_PushData(SwiftValdiMarshallerRef marshaller, const unsigned char* data, SwiftInt length) {
    const auto* dataBegin = reinterpret_cast<const Valdi::Byte*>(data);
    const auto* dataEnd = dataBegin + static_cast<size_t>(length);
    auto bytes = Valdi::makeShared<Valdi::ByteBuffer>(dataBegin, dataEnd)->toBytesView();
    auto value =
        Valdi::Value(Valdi::makeShared<Valdi::ValueTypedArray>(Valdi::kDefaultTypedArrayType, Valdi::BytesView(bytes)));
    return SwiftValdiMarshaller_Unwrap(marshaller)->push(value);
}

SwiftInt SwiftValdiMarshaller_PushObject(SwiftValdiMarshallerRef marshaller,
                                         SwiftValdiMarshallableObjectRegistryRef marshallerRegistry,
                                         const char* className) {
    ValdiSwift::SwiftValdiMarshallableObjectRegistry* registry =
        reinterpret_cast<ValdiSwift::SwiftValdiMarshallableObjectRegistry*>(marshallerRegistry);
    auto cppMarshaller = SwiftValdiMarshaller_Unwrap(marshaller);
    auto classSchemaRef = registry->getClassSchema(STRING_LITERAL(className));
    if (!classSchemaRef) {
        cppMarshaller->getExceptionTracker().onError(classSchemaRef.error());
        return -1;
    }
    auto typedObject = Valdi::ValueTypedObject::make(classSchemaRef.value());
    return cppMarshaller->push(Valdi::Value(typedObject));
}

SwiftInt SwiftValdiMarshaller_PushProxyObject(SwiftValdiMarshallerRef marshaller, const void* swiftObject) {
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto typedObject = marshallerCpp->getTypedObject(-1);
    if (typedObject == nullptr) {
        return -1;
    }
    marshallerCpp->pop();
    Valdi::Ref<Valdi::ValueTypedProxyObject> proxyObject =
        Valdi::makeShared<ValdiSwift::SwiftProxyObject>(typedObject, swiftObject);
    return marshallerCpp->push(Valdi::Value(proxyObject));
}

static const void* SwiftObjectFromProxyObject(const Valdi::ValueTypedProxyObject& proxyObject) {
    auto swiftProxy = dynamic_cast<const ValdiSwift::SwiftProxyObject*>(&proxyObject);
    if (swiftProxy == nullptr) {
        return nil;
    }
    return swiftProxy->getValue();
}

const void* SwiftValdiMarshaller_GetProxyObject(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto proxyObject = marshallerCpp->getProxyObject(index);
    if (proxyObject == nullptr) {
        return nil;
    }
    return SwiftObjectFromProxyObject(*proxyObject);
}

SwiftInt SwiftValdiMarshaller_UnwrapProxyObject(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto proxyObject = marshallerCpp->getProxyObject(index);
    if (proxyObject == nullptr) {
        return -1;
    }
    return marshallerCpp->push(Valdi::Value(proxyObject->getTypedObject()));
}

SwiftInt SwiftValdiMarshaller_PushMapIterator(SwiftValdiMarshallerRef marshaller, SwiftInt mapIndex) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushMapIterator(mapIndex);
}

SwiftBool SwiftValdiMarshaller_PushMapIteratorNext(SwiftValdiMarshallerRef marshaller, SwiftInt iteratorIndex) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushMapIteratorNextEntry(iteratorIndex);
}

SwiftBool SwiftValdiMarshaller_GetBool(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValue(marshaller, index).toBool();
}

SwiftInt32 SwiftValdiMarshaller_GetInt(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValue(marshaller, index).toInt();
}

SwiftInt64 SwiftValdiMarshaller_GetLong(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValue(marshaller, index).toLong();
}

double SwiftValdiMarshaller_GetDouble(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValue(marshaller, index).toDouble();
}

SwiftStringBox SwiftValdiMarshaller_GetString(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto str = SwiftValdiMarshaller_GetValue(marshaller, index).toStringBox();
    return Valdi::unsafeBridgeRetain(str.getInternedString().get());
}

const unsigned char* SwiftValdiMarshaller_GetData(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto array = SwiftValdiMarshaller_GetValue(marshaller, index).getTypedArrayRef();
    if (array == nullptr) {
        return nullptr;
    }
    return array->getBuffer().data();
}

SwiftInt SwiftValdiMarshaller_GetDataSize(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto array = SwiftValdiMarshaller_GetValue(marshaller, index).getTypedArrayRef();
    if (array == nullptr) {
        return 0;
    }
    return array->getBuffer().size();
}

SwiftBool SwiftValdiMarshaller_IsNullOrUndefined(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->getOrUndefined(static_cast<int>(index)).isNullOrUndefined();
}

SwiftBool SwiftValdiMarshaller_IsBool(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isBool();
}

SwiftBool SwiftValdiMarshaller_IsInt(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isInt();
}

SwiftBool SwiftValdiMarshaller_IsBigInt(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isBigInt();
}

SwiftBool SwiftValdiMarshaller_IsLong(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isLong();
}

SwiftBool SwiftValdiMarshaller_IsDouble(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isDouble();
}

SwiftBool SwiftValdiMarshaller_IsString(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isString();
}

SwiftBool SwiftValdiMarshaller_IsMap(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isMap();
}

SwiftBool SwiftValdiMarshaller_IsArray(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isArray();
}

SwiftBool SwiftValdiMarshaller_IsTypedObject(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isTypedObject();
}

SwiftBool SwiftValdiMarshaller_IsError(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_GetValueOrUndefined(marshaller, index).isError();
}

void SwiftValdiMarshaller_SetTypedObjectProperty(SwiftValdiMarshallerRef marshaller,
                                                 SwiftInt objectIndex,
                                                 SwiftInt propertyIndex) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto typedObject = marshallerCpp->getTypedObject(objectIndex);
    auto propertyValue = marshallerCpp->get(-1);
    typedObject->setProperty(propertyIndex, propertyValue);
    return;
}

SwiftInt SwiftValdiMarshaller_GetTypedObjectProperty(SwiftValdiMarshallerRef marshaller,
                                                     SwiftInt objectIndex,
                                                     SwiftInt propertyIndex) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto typedObject = marshallerCpp->getTypedObject(objectIndex);
    return marshallerCpp->push(typedObject->getProperty(propertyIndex));
}

SwiftBool SwiftValdiMarshaller_GetMapProperty(SwiftValdiMarshallerRef marshaller,
                                              const char* propertyName,
                                              SwiftInt index) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto result = marshallerCpp->getMapProperty(
        STRING_LITERAL(propertyName), index, Valdi::Marshaller::GetMapPropertyFlags::None);
    return result;
}

void SwiftValdiMarshaller_PutMapProperty(SwiftValdiMarshallerRef marshaller, const char* propertyName, SwiftInt index) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    marshallerCpp->putMapProperty(STRING_LITERAL(propertyName), index);
}

void SwiftValdiMarshaller_SetArrayItem(SwiftValdiMarshallerRef marshaller, SwiftInt index, SwiftInt arrayIndex) {
    auto* marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    marshallerCpp->setArrayItem(index, arrayIndex);
}

SwiftInt SwiftValdiMarshaller_GetArrayItem(SwiftValdiMarshallerRef marshaller, SwiftInt index, SwiftInt arrayIndex) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->getArrayItem(index, arrayIndex);
}

SwiftInt SwiftValdiMarshaller_GetArraySize(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    return SwiftValdiMarshaller_Unwrap(marshaller)->getArrayLength(index);
}

SwiftInt SwiftValdiMarshaller_PushFunction(SwiftValdiMarshallerRef marshaller, SwiftValdiFunction valdiFunction) {
    Valdi::Ref<Valdi::ValueFunction> valueFunction = Valdi::makeShared<SwiftValueFunction>(valdiFunction);
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushFunction(valueFunction);
}

SwiftValueFunctionPtr SwiftValdiMarshaller_GetFunction(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto valueFunction = SwiftValdiMarshaller_Unwrap(marshaller)->getFunction(index);
    if (valueFunction == nullptr) {
        return nullptr;
    }
    return Valdi::unsafeBridgeRetain(valueFunction.get());
}

void SwiftValdiMarshaller_ReleaseFunction(SwiftValueFunctionPtr functionBridge) {
    Valdi::unsafeBridgeRelease(functionBridge);
    return;
}

SwiftBool SwiftValdiMarshaller_CallFunction(SwiftValueFunctionPtr functionBridge,
                                            SwiftValdiMarshallerRef wrappedMarshaller,
                                            SwiftBool sync) {
    auto valueFunction = Valdi::unsafeBridge<Valdi::ValueFunction>(functionBridge);
    auto* marshaller = SwiftValdiMarshaller_Unwrap(wrappedMarshaller);

    auto flags = Valdi::ValueFunctionFlagsPropagatesError;
    if (sync) {
        flags = static_cast<Valdi::ValueFunctionFlags>(Valdi::ValueFunctionFlagsCallSync | flags);
    }
    Valdi::ValueFunctionCallContext callContext(
        flags, marshaller->getValues(), marshaller->size(), marshaller->getExceptionTracker());
    auto result = (*valueFunction)(callContext);
    if (!marshaller->getExceptionTracker()) {
        return false;
    }
    marshaller->push(result);
    return true;
}

// -------- Promise helper functions
SwiftValuePromisePtr SwiftValdiMarshaller_GetPromise(SwiftValdiMarshallerRef marshaller, SwiftInt index) {
    auto valdiObject = SwiftValdiMarshaller_Unwrap(marshaller)->getValdiObject(index);
    auto valuePromise = castOrNull<Valdi::Promise>(valdiObject);
    return (valuePromise == nullptr) ? nullptr : Valdi::unsafeBridgeRetain(valuePromise.get());
}

void SwiftValdiMarshaller_ReleasePromise(SwiftValuePromisePtr promisePtr) {
    SC_ASSERT(promisePtr != nullptr);
    Valdi::unsafeBridgeRelease(promisePtr);
    return;
}

SwiftInt SwiftValdiMarshaller_PushNewPromise(SwiftValdiMarshallerRef marshaller) {
    auto promise = Valdi::makeShared<Valdi::ResolvablePromise>();
    return SwiftValdiMarshaller_Unwrap(marshaller)->pushValdiObject(promise);
}

SwiftInt SwiftValdiMarshaller_PushPromise(SwiftValdiMarshallerRef marshaller, SwiftValuePromisePtr promisePtr) {
    SC_ASSERT(promisePtr != nullptr);
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto promise = Valdi::unsafeBridge<Valdi::Promise>(promisePtr);
    return marshallerCpp->pushValdiObject(promise);
}

void SwiftValdiMarshaller_OnPromiseComplete(SwiftValdiMarshallerRef marshaller,
                                            SwiftValuePromisePtr promisePtr,
                                            SwiftInt iFunction) {
    SC_ASSERT(promisePtr != nullptr);
    SC_ASSERT(iFunction >= 0);
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto func = marshallerCpp->getFunction(iFunction);
    auto iPromise = marshallerCpp->pushValdiObject(Valdi::unsafeBridge<Valdi::Promise>(promisePtr));
    marshallerCpp->onPromiseComplete(iPromise, func);
}

void SwiftValdiMarshaller_ResolvePromise(SwiftValdiMarshallerRef marshaller,
                                         SwiftValuePromisePtr promisePtr,
                                         SwiftInt iResult) {
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto promise = Valdi::unsafeBridge<Valdi::Promise>(promisePtr);
    auto resolvablePromise = castOrNull<Valdi::ResolvablePromise>(promise);
    SC_ASSERT(resolvablePromise != nullptr);
    resolvablePromise->fulfill(marshallerCpp->get(iResult));
}

void SwiftValdiMarshaller_RejectPromise(SwiftValuePromisePtr promisePtr, const char* errorMessage) {
    auto promise = Valdi::unsafeBridge<Valdi::Promise>(promisePtr);
    auto resolvablePromise = castOrNull<Valdi::ResolvablePromise>(promise);
    SC_ASSERT(resolvablePromise != nullptr);
    resolvablePromise->fulfill(Valdi::Error(errorMessage));
}

void SwiftValdiMarshaller_CancelPromise(SwiftValuePromisePtr promisePtr) {
    SC_ASSERT(promisePtr != nullptr);
    auto promise = Valdi::unsafeBridge<Valdi::Promise>(promisePtr);
    promise->cancel();
}

void SwiftValdiMarshaller_SetCancelCallback(SwiftValdiMarshallerRef marshaller,
                                            SwiftValuePromisePtr promisePtr,
                                            SwiftInt iFunction) {
    SC_ASSERT(promisePtr != nullptr);
    SC_ASSERT(iFunction >= 0);
    auto marshallerCpp = SwiftValdiMarshaller_Unwrap(marshaller);
    auto func = marshallerCpp->getFunction(iFunction);
    auto promise = Valdi::unsafeBridge<Valdi::Promise>(promisePtr);
    auto resolvablePromise = castOrNull<Valdi::ResolvablePromise>(promise);
    SC_ASSERT(resolvablePromise != nullptr);
    resolvablePromise->setCancelCallback([func] { (*func)(); });
}

SwiftStringView SwiftValdiMarshaller_StringViewFromStringBox(SwiftStringBox strbox) {
    auto cppStrBox = Valdi::StringBox(Valdi::unsafeBridge<Valdi::InternedStringImpl>(strbox));
    auto cppStrView = cppStrBox.toStringView();
    return {cppStrView.data(), cppStrView.size()};
}

void SwiftValdiMarshaller_ReleaseStringBox(SwiftStringBox strbox) {
    // Transfer ownership to local Ref and auto release on function exit
    Valdi::unsafeBridgeTransfer<Valdi::InternedStringImpl>(strbox);
}
