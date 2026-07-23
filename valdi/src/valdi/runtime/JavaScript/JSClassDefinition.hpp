//
//  JSClassDefinition.hpp
//  ValdiRuntime
//

#pragma once

#include "valdi/runtime/JavaScript/JavaScriptTypes.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"

#include <vector>

namespace Valdi {

class JSFunctionNativeCallContext;

using JSClassConstructorCallback = Ref<RefCountable> (*)(RefCountable* classOpaque,
                                                         JSFunctionNativeCallContext&) noexcept;
using JSClassCallback = JSValueRef (*)(RefCountable* opaque, JSFunctionNativeCallContext&) noexcept;

enum class JSClassEntryKind {
    Method,
    Constant,
    Accessor,
};

class JSClassEntry {
public:
    JSClassEntry();
    ~JSClassEntry();

    static JSClassEntry method(const StringBox& name, JSClassCallback callback);
    static JSClassEntry method(
        const StringBox& name, JSClassCallback callback, bool writable, bool enumerable, bool configurable);
    static JSClassEntry constant(const StringBox& name, JSValueRef value);
    static JSClassEntry constant(
        const StringBox& name, JSValueRef value, bool writable, bool enumerable, bool configurable);
    static JSClassEntry accessor(const StringBox& name, JSClassCallback getter, JSClassCallback setter);
    static JSClassEntry accessor(
        const StringBox& name, JSClassCallback getter, JSClassCallback setter, bool enumerable, bool configurable);

    const StringBox& getName() const noexcept;
    JSClassEntryKind getKind() const noexcept;
    const JSValueRef& getValue() const noexcept;
    JSClassCallback getMethodCallback() const noexcept;
    JSClassCallback getGetterCallback() const noexcept;
    JSClassCallback getSetterCallback() const noexcept;
    bool isWritable() const noexcept;
    bool isEnumerable() const noexcept;
    bool isConfigurable() const noexcept;
    bool isClassMember() const noexcept;
    void setClassMember(bool classMember) noexcept;

private:
    StringBox _name;
    JSClassEntryKind _kind = JSClassEntryKind::Method;
    JSValueRef _value;
    JSClassCallback _methodCallback = nullptr;
    JSClassCallback _getterCallback = nullptr;
    JSClassCallback _setterCallback = nullptr;
    bool _writable = true;
    bool _enumerable = false;
    bool _configurable = true;
    bool _classMember = false;
};

class JSClassDefinition {
public:
    JSClassDefinition(const StringBox& name, JSClassConstructorCallback constructor);
    ~JSClassDefinition();

    const StringBox& getName() const noexcept;
    JSClassConstructorCallback getConstructor() const noexcept;
    void setConstructor(JSClassConstructorCallback constructor) noexcept;
    const std::vector<JSClassEntry>& getEntries() const noexcept;

    JSClassDefinition& appendInstanceEntry(JSClassEntry entry);
    JSClassDefinition& appendClassEntry(JSClassEntry entry);

    JSClassDefinition& appendMethod(const StringBox& name, JSClassCallback callback);
    JSClassDefinition& appendMethod(
        const StringBox& name, JSClassCallback callback, bool writable, bool enumerable, bool configurable);
    JSClassDefinition& appendConstant(const StringBox& name, JSValueRef value);
    JSClassDefinition& appendConstant(
        const StringBox& name, JSValueRef value, bool writable, bool enumerable, bool configurable);
    JSClassDefinition& appendAccessor(const StringBox& name, JSClassCallback getter, JSClassCallback setter);
    JSClassDefinition& appendAccessor(
        const StringBox& name, JSClassCallback getter, JSClassCallback setter, bool enumerable, bool configurable);

    JSClassDefinition& appendClassMethod(const StringBox& name, JSClassCallback callback);
    JSClassDefinition& appendClassMethod(
        const StringBox& name, JSClassCallback callback, bool writable, bool enumerable, bool configurable);
    JSClassDefinition& appendClassConstant(const StringBox& name, JSValueRef value);
    JSClassDefinition& appendClassConstant(
        const StringBox& name, JSValueRef value, bool writable, bool enumerable, bool configurable);
    JSClassDefinition& appendClassAccessor(const StringBox& name, JSClassCallback getter, JSClassCallback setter);
    JSClassDefinition& appendClassAccessor(
        const StringBox& name, JSClassCallback getter, JSClassCallback setter, bool enumerable, bool configurable);

private:
    StringBox _name;
    JSClassConstructorCallback _constructor;
    std::vector<JSClassEntry> _entries;
};

} // namespace Valdi
