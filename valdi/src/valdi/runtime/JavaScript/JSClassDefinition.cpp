//
//  JSClassDefinition.cpp
//  ValdiRuntime
//

#include "valdi/runtime/JavaScript/JSClassDefinition.hpp"

#include <utility>

namespace Valdi {

JSClassEntry::JSClassEntry() = default;

JSClassEntry::~JSClassEntry() = default;

JSClassEntry JSClassEntry::method(const StringBox& name, JSClassCallback callback) {
    return method(name, callback, true, false, true);
}

JSClassEntry JSClassEntry::method(
    const StringBox& name, JSClassCallback callback, bool writable, bool enumerable, bool configurable) {
    JSClassEntry entry;
    entry._name = name;
    entry._kind = JSClassEntryKind::Method;
    entry._methodCallback = callback;
    entry._writable = writable;
    entry._enumerable = enumerable;
    entry._configurable = configurable;
    return entry;
}

JSClassEntry JSClassEntry::constant(const StringBox& name, JSValueRef value) {
    return constant(name, std::move(value), false, false, false);
}

JSClassEntry JSClassEntry::constant(
    const StringBox& name, JSValueRef value, bool writable, bool enumerable, bool configurable) {
    JSClassEntry entry;
    entry._name = name;
    entry._kind = JSClassEntryKind::Constant;
    entry._value = std::move(value);
    entry._writable = writable;
    entry._enumerable = enumerable;
    entry._configurable = configurable;
    return entry;
}

JSClassEntry JSClassEntry::accessor(const StringBox& name, JSClassCallback getter, JSClassCallback setter) {
    return accessor(name, getter, setter, false, true);
}

JSClassEntry JSClassEntry::accessor(
    const StringBox& name, JSClassCallback getter, JSClassCallback setter, bool enumerable, bool configurable) {
    JSClassEntry entry;
    entry._name = name;
    entry._kind = JSClassEntryKind::Accessor;
    entry._getterCallback = getter;
    entry._setterCallback = setter;
    entry._writable = false;
    entry._enumerable = enumerable;
    entry._configurable = configurable;
    return entry;
}

const StringBox& JSClassEntry::getName() const noexcept {
    return _name;
}

JSClassEntryKind JSClassEntry::getKind() const noexcept {
    return _kind;
}

const JSValueRef& JSClassEntry::getValue() const noexcept {
    return _value;
}

JSClassCallback JSClassEntry::getMethodCallback() const noexcept {
    return _methodCallback;
}

JSClassCallback JSClassEntry::getGetterCallback() const noexcept {
    return _getterCallback;
}

JSClassCallback JSClassEntry::getSetterCallback() const noexcept {
    return _setterCallback;
}

bool JSClassEntry::isWritable() const noexcept {
    return _writable;
}

bool JSClassEntry::isEnumerable() const noexcept {
    return _enumerable;
}

bool JSClassEntry::isConfigurable() const noexcept {
    return _configurable;
}

bool JSClassEntry::isClassMember() const noexcept {
    return _classMember;
}

void JSClassEntry::setClassMember(bool classMember) noexcept {
    _classMember = classMember;
}

JSClassDefinition::JSClassDefinition(const StringBox& name, JSClassConstructorCallback constructor)
    : _name(name), _constructor(constructor) {}

JSClassDefinition::~JSClassDefinition() = default;

const StringBox& JSClassDefinition::getName() const noexcept {
    return _name;
}

JSClassConstructorCallback JSClassDefinition::getConstructor() const noexcept {
    return _constructor;
}

void JSClassDefinition::setConstructor(JSClassConstructorCallback constructor) noexcept {
    _constructor = constructor;
}

const std::vector<JSClassEntry>& JSClassDefinition::getEntries() const noexcept {
    return _entries;
}

JSClassDefinition& JSClassDefinition::appendInstanceEntry(JSClassEntry entry) {
    entry.setClassMember(false);
    _entries.emplace_back(std::move(entry));
    return *this;
}

JSClassDefinition& JSClassDefinition::appendClassEntry(JSClassEntry entry) {
    entry.setClassMember(true);
    _entries.emplace_back(std::move(entry));
    return *this;
}

JSClassDefinition& JSClassDefinition::appendMethod(const StringBox& name, JSClassCallback callback) {
    return appendMethod(name, callback, true, false, true);
}

JSClassDefinition& JSClassDefinition::appendMethod(
    const StringBox& name, JSClassCallback callback, bool writable, bool enumerable, bool configurable) {
    return appendInstanceEntry(JSClassEntry::method(name, callback, writable, enumerable, configurable));
}

JSClassDefinition& JSClassDefinition::appendConstant(const StringBox& name, JSValueRef value) {
    return appendConstant(name, std::move(value), false, false, false);
}

JSClassDefinition& JSClassDefinition::appendConstant(
    const StringBox& name, JSValueRef value, bool writable, bool enumerable, bool configurable) {
    return appendInstanceEntry(JSClassEntry::constant(name, std::move(value), writable, enumerable, configurable));
}

JSClassDefinition& JSClassDefinition::appendAccessor(const StringBox& name,
                                                     JSClassCallback getter,
                                                     JSClassCallback setter) {
    return appendAccessor(name, getter, setter, false, true);
}

JSClassDefinition& JSClassDefinition::appendAccessor(
    const StringBox& name, JSClassCallback getter, JSClassCallback setter, bool enumerable, bool configurable) {
    return appendInstanceEntry(JSClassEntry::accessor(name, getter, setter, enumerable, configurable));
}

JSClassDefinition& JSClassDefinition::appendClassMethod(const StringBox& name, JSClassCallback callback) {
    return appendClassMethod(name, callback, true, false, true);
}

JSClassDefinition& JSClassDefinition::appendClassMethod(
    const StringBox& name, JSClassCallback callback, bool writable, bool enumerable, bool configurable) {
    return appendClassEntry(JSClassEntry::method(name, callback, writable, enumerable, configurable));
}

JSClassDefinition& JSClassDefinition::appendClassConstant(const StringBox& name, JSValueRef value) {
    return appendClassConstant(name, std::move(value), false, false, false);
}

JSClassDefinition& JSClassDefinition::appendClassConstant(
    const StringBox& name, JSValueRef value, bool writable, bool enumerable, bool configurable) {
    return appendClassEntry(JSClassEntry::constant(name, std::move(value), writable, enumerable, configurable));
}

JSClassDefinition& JSClassDefinition::appendClassAccessor(const StringBox& name,
                                                          JSClassCallback getter,
                                                          JSClassCallback setter) {
    return appendClassAccessor(name, getter, setter, false, true);
}

JSClassDefinition& JSClassDefinition::appendClassAccessor(
    const StringBox& name, JSClassCallback getter, JSClassCallback setter, bool enumerable, bool configurable) {
    return appendClassEntry(JSClassEntry::accessor(name, getter, setter, enumerable, configurable));
}

} // namespace Valdi
