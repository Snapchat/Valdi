//
//  CppMarshaller.cpp
//  valdi_core-pc
//
//  Created by Simon Corsin on 4/11/23.
//

#include "valdi_core/cpp/Marshalling/CppMarshaller.hpp"
#include "fmt/format.h"
#include "valdi_core/cpp/Marshalling/RegisteredCppGeneratedClass.hpp"
#include "valdi_core/cpp/Schema/ValueSchema.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/DefaultValueMarshallers.hpp"
#include "valdi_core/cpp/Utils/PlatformObjectAttachments.hpp"
#include "valdi_core/cpp/Utils/ValueMap.hpp"
#include "valdi_core/cpp/Utils/ValueTypedArray.hpp"
#include "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#include <fmt/ostream.h>

namespace Valdi {

std::unique_lock<std::recursive_mutex> CppObjectStore::lock() const {
    return std::unique_lock<std::recursive_mutex>(_mutex);
}

void CppObjectStore::setObjectProxyForId(uint32_t id, const Ref<CppGeneratedInterface>& object) {
    _objectProxyById[id] = weakRef(object.get());
}

Ref<CppGeneratedInterface> CppObjectStore::getObjectProxyForId(uint32_t id) {
    const auto& it = _objectProxyById.find(id);
    if (it == _objectProxyById.end()) {
        return nullptr;
    }

    auto ref = it->second.lock();
    if (ref == nullptr) {
        _objectProxyById.erase(it);
    }

    return ref;
}

CppObjectStore* CppObjectStore::sharedInstance() {
    static auto* kInstance = new CppObjectStore();
    return kInstance;
}

CppProxyMarshallerBase::CppProxyMarshallerBase(CppObjectStore* objectStore,
                                               std::unique_lock<std::recursive_mutex>&& lock)
    : _objectStore(objectStore), _lock(std::move(lock)) {}
CppProxyMarshallerBase::~CppProxyMarshallerBase() = default;

CppObjectStore* CppProxyMarshallerBase::getObjectStore() const {
    return _objectStore;
}

CppProxyMarshaller::CppProxyMarshaller(CppObjectStore* objectStore,
                                       std::unique_lock<std::recursive_mutex>&& lock,
                                       bool finishedMarshalling)
    : CppProxyMarshallerBase(objectStore, std::move(lock)), _finishedMarshalling(finishedMarshalling) {}

CppProxyMarshaller::~CppProxyMarshaller() = default;

bool CppProxyMarshaller::finishedMarshalling() const {
    return _finishedMarshalling;
}

CppProxyUnmarshaller::CppProxyUnmarshaller(CppObjectStore* objectStore,
                                           std::unique_lock<std::recursive_mutex>&& lock,
                                           Ref<ValueTypedProxyObject>&& proxyObject,
                                           Ref<CppGeneratedInterface>&& object)
    : CppProxyMarshallerBase(objectStore, std::move(lock)),
      _proxyObject(std::move(proxyObject)),
      _object(std::move(object)) {}

CppProxyUnmarshaller::~CppProxyUnmarshaller() = default;

const Ref<ValueTypedProxyObject>& CppProxyUnmarshaller::getProxyObject() const {
    return _proxyObject;
    ;
}

const Ref<CppGeneratedInterface>& CppProxyUnmarshaller::getObject() const {
    return _object;
}

const Ref<ValueTypedObject>& CppProxyUnmarshaller::getTypedObject() const {
    return _proxyObject->getTypedObject();
}

class CppProxyObject : public ValueTypedProxyObject {
public:
    CppProxyObject(const Ref<ValueTypedObject>& typedObject, CppGeneratedInterface& ref)
        : ValueTypedProxyObject(typedObject), _ref(&ref) {}
    ~CppProxyObject() override = default;

    std::string_view getType() const final {
        return "C++ Proxy";
    }

private:
    Ref<CppGeneratedInterface> _ref;
};

void CppMarshaller::marshall(ExceptionTracker& exceptionTracker, const BytesView& value, Value& out) {
    auto existingTypedArray = castOrNull<ValueTypedArray>(value.getSource());
    if (existingTypedArray != nullptr && existingTypedArray->getBuffer().isStrictlyIdenticalTo(value)) {
        out = Value(existingTypedArray);
        return;
    }

    out = Value(makeShared<ValueTypedArray>(TypedArrayType::Uint8Array, value));
}

void CppMarshaller::unmarshall(ExceptionTracker& exceptionTracker, const Value& value, BytesView& out) {
    auto typedArray = value.checkedTo<Ref<ValueTypedArray>>(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    out = typedArray->getBuffer();
}

void CppMarshaller::unmarshall(ExceptionTracker& exceptionTracker, const Value& value, Value& out) {
    out = value;
}

Value* CppMarshaller::marshallTypedObjectPrologue(ExceptionTracker& exceptionTracker,
                                                  RegisteredCppGeneratedClass& registeredClass,
                                                  Value& out,
                                                  size_t inputPropertiesSize) {
    auto cls = registeredClass.getResolvedClassSchema(exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    SC_ASSERT(cls->getPropertiesSize() == inputPropertiesSize);
    auto output = ValueTypedObject::make(cls);
    out = Value(output);
    return output->getProperties();
}

void CppMarshaller::onMarshallTypedObjectFailure(ExceptionTracker& exceptionTracker,
                                                 RegisteredCppGeneratedClass& registeredClass,
                                                 size_t propertyIndex) {
    auto classSchema = registeredClass.getResolvedClassSchema();
    if (!classSchema) {
        return;
    }

    exceptionTracker.onError(ObjectValueMarshaller<Value>::getMarshallPropertyErrorMessage(
        *classSchema.value(), classSchema.value()->getProperty(propertyIndex)));
}

Ref<ValueTypedObject> CppMarshaller::unmarshallTypedObjectPrologue(ExceptionTracker& exceptionTracker,
                                                                   RegisteredCppGeneratedClass& registeredClass,
                                                                   const Value& value,
                                                                   size_t outputPropertiesSize) {
    auto cls = registeredClass.getResolvedClassSchema(exceptionTracker);
    if (!exceptionTracker) {
        return nullptr;
    }

    SC_ASSERT(cls->getPropertiesSize() == outputPropertiesSize);

    if (value.isMap()) {
        // Convert from Map to typed object
        auto typedObject = ValueTypedObject::make(cls);

        for (const auto& it : *value.getMap()) {
            typedObject->setPropertyForName(it.first, it.second);
        }

        return typedObject;
    }

    auto typedObject = value.checkedTo<Ref<ValueTypedObject>>(exceptionTracker);

    if (typedObject != nullptr) {
        SC_ASSERT(typedObject->getPropertiesSize() == outputPropertiesSize);
    }

    return typedObject;
}

void CppMarshaller::onUnmarshallTypedObjectFailure(ExceptionTracker& exceptionTracker,
                                                   RegisteredCppGeneratedClass& registeredClass,
                                                   size_t propertyIndex) {
    auto classSchema = registeredClass.getResolvedClassSchema();
    if (!classSchema) {
        return;
    }
    exceptionTracker.onError(ObjectValueMarshaller<Value>::getUnmarshallPropertyErrorMessage(
        *classSchema.value(), classSchema.value()->getProperty(propertyIndex)));
}

CppProxyMarshaller CppMarshaller::marshallProxyObjectPrologue(CppObjectStore* objectStore,
                                                              CppGeneratedInterface& value,
                                                              Value& out) {
    auto lock = objectStore->lock();

    const auto& objectAttachments = value.getObjectAttachments();
    auto proxy = objectAttachments->getProxyForSource(nullptr);
    if (proxy != nullptr) {
        out = Value(proxy);
        return CppProxyMarshaller(objectStore, std::move(lock), true);
    } else {
        return CppProxyMarshaller(objectStore, std::move(lock), false);
    }
}

void CppMarshaller::marshallProxyObjectEpilogue(ExceptionTracker& exceptionTracker,
                                                CppProxyMarshaller& marshaller,
                                                CppGeneratedInterface& value,
                                                Value& out) {
    if (!exceptionTracker) {
        return;
    }

    auto typedObject = out.checkedTo<Ref<ValueTypedObject>>(exceptionTracker);
    if (!exceptionTracker) {
        return;
    }

    auto proxy = makeShared<CppProxyObject>(typedObject, value);

    value.getObjectAttachments()->setProxyForSource(nullptr, proxy, false);
    marshaller.getObjectStore()->setObjectProxyForId(proxy->getId(), strongSmallRef(&value));

    out = Value(proxy);
}

CppProxyUnmarshaller CppMarshaller::unmarshallProxyObjectPrologue(ExceptionTracker& exceptionTracker,
                                                                  CppObjectStore* objectStore,
                                                                  const Value& value) {
    auto lock = objectStore->lock();
    auto proxyObject = value.checkedTo<Ref<ValueTypedProxyObject>>(exceptionTracker);
    if (!exceptionTracker) {
        return CppProxyUnmarshaller(objectStore, std::move(lock), nullptr, nullptr);
    }

    auto objectProxy = objectStore->getObjectProxyForId(proxyObject->getId());
    return CppProxyUnmarshaller(objectStore, std::move(lock), std::move(proxyObject), std::move(objectProxy));
}

void CppMarshaller::unmarshallProxyObjectEpilogue(CppProxyUnmarshaller& unmarshaller,
                                                  CppGeneratedInterface& generatedInterface) {
    const auto& proxyObject = unmarshaller.getProxyObject();
    const auto& objectAttachments = generatedInterface.getObjectAttachments();
    objectAttachments->setProxyForSource(nullptr, proxyObject, true);
    unmarshaller.getObjectStore()->setObjectProxyForId(proxyObject->getId(), strongSmallRef(&generatedInterface));
}

void CppMarshaller::throwUnimplementedMethod() {
    throw Exception("Unimplemented method");
}

} // namespace Valdi
