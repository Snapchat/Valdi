//
//  JSNativeClassData.cpp
//  ValdiRuntime
//

#include "valdi/runtime/JavaScript/JSNativeClassData.hpp"

#include <utility>

namespace Valdi {

JSNativeClassData::JSNativeClassData(StringBox name,
                                     const Ref<RefCountable>& opaque,
                                     JSClassConstructorCallback constructor)
    : _name(std::move(name)),
      _opaque(opaque),
      _constructor(constructor),
      _constructorReferenceInfo(ReferenceInfoBuilder().withObject(_name).asFunction().build()) {}

JSNativeClassData::~JSNativeClassData() = default;

const StringBox& JSNativeClassData::getName() const noexcept {
    return _name;
}

const Ref<RefCountable>& JSNativeClassData::getOpaque() const noexcept {
    return _opaque;
}

JSClassConstructorCallback JSNativeClassData::getConstructor() const noexcept {
    return _constructor;
}

const ReferenceInfo& JSNativeClassData::getConstructorReferenceInfo() const noexcept {
    return _constructorReferenceInfo;
}

ReferenceInfo JSNativeClassData::makeMemberReferenceInfo(const StringBox& propertyName) const {
    return ReferenceInfoBuilder().withObject(_name).withProperty(propertyName).asFunction().build();
}

JSNativeClassInstanceData::JSNativeClassInstanceData(const Ref<JSNativeClassData>& nativeClass,
                                                     const Ref<RefCountable>& opaque)
    : _nativeClass(nativeClass), _opaque(opaque) {}

JSNativeClassInstanceData::~JSNativeClassInstanceData() = default;

const Ref<JSNativeClassData>& JSNativeClassInstanceData::getNativeClass() const noexcept {
    return _nativeClass;
}

const Ref<RefCountable>& JSNativeClassInstanceData::getOpaque() const noexcept {
    return _opaque;
}

Ref<RefCountable> unwrapNativeClassInstanceData(const Ref<RefCountable>& wrappedObject) {
    auto* instanceData = dynamic_cast<JSNativeClassInstanceData*>(wrappedObject.get());
    return instanceData == nullptr ? wrappedObject : instanceData->getOpaque();
}

} // namespace Valdi
