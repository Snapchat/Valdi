//
//  JSNativeClassData.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi/runtime/JavaScript/JSClassDefinition.hpp"
#include "valdi_core/cpp/Utils/ReferenceInfo.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

namespace Valdi {

class JSNativeClassData final : public SimpleRefCountable {
public:
    JSNativeClassData(StringBox name, const Ref<RefCountable>& opaque, JSClassConstructorCallback constructor);
    ~JSNativeClassData() override;

    const StringBox& getName() const noexcept;
    const Ref<RefCountable>& getOpaque() const noexcept;
    JSClassConstructorCallback getConstructor() const noexcept;
    const ReferenceInfo& getConstructorReferenceInfo() const noexcept;
    ReferenceInfo makeMemberReferenceInfo(const StringBox& propertyName) const;

private:
    StringBox _name;
    Ref<RefCountable> _opaque;
    JSClassConstructorCallback _constructor;
    ReferenceInfo _constructorReferenceInfo;
};

class JSNativeClassInstanceData final : public SimpleRefCountable {
public:
    JSNativeClassInstanceData(const Ref<JSNativeClassData>& nativeClass, const Ref<RefCountable>& opaque);
    ~JSNativeClassInstanceData() override;

    const Ref<JSNativeClassData>& getNativeClass() const noexcept;
    const Ref<RefCountable>& getOpaque() const noexcept;

private:
    Ref<JSNativeClassData> _nativeClass;
    Ref<RefCountable> _opaque;
};

Ref<RefCountable> unwrapNativeClassInstanceData(const Ref<RefCountable>& wrappedObject);

} // namespace Valdi
