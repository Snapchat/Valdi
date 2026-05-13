#include "djinni_c_types.hpp"
#include <cstdio>

namespace djinni {

constexpr size_t alignUp(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

template <typename T, typename ValueType> struct ArrayAllocator {
  static constexpr size_t alignedValueSize = alignUp(sizeof(ValueType), alignof(ValueType));

  template <typename... Args> static T *allocate(size_t size, Args &&...args) {
    auto allocSize =
        alignUp(sizeof(T), alignof(ValueType)) + (alignedValueSize * size);

    auto *arrayRegion = ::operator new(allocSize);

    new (arrayRegion)(T)(std::forward<Args>(args)...);

    return reinterpret_cast<T *>(arrayRegion);
  }

  constexpr inline static ValueType *getContainerStartPtr(T *object) {
    return reinterpret_cast<ValueType *>(
        reinterpret_cast<uint8_t *>(object) +
        alignUp(sizeof(T), alignof(ValueType)));
  }

  constexpr inline static const ValueType *
  getContainerStartPtr(const T *object) {
    return reinterpret_cast<const ValueType *>(
        reinterpret_cast<const uint8_t *>(object) +
        alignUp(sizeof(T), alignof(ValueType)));
  }
};

Object::Object() : _ref(1) {}

Object::~Object() = default;

void Object::onRetain() { _ref++; }

void Object::onRelease() {
  if (--_ref == 0) {
    delete this;
  }
}

void Object::retain(Object *obj) {
  if (obj != nullptr) {
    obj->onRetain();
  }
}

void Object::release(Object *obj) {
  if (obj != nullptr) {
    obj->onRelease();
  }
}

using StringAllocator = ArrayAllocator<String, char>;

String::String(size_t length) : _length(length) {}
String::~String() = default;

const char *String::data() const {
  return StringAllocator::getContainerStartPtr(this);
}

size_t String::String::length() const { return _length; }

String *String::make(const char *str, size_t length) {
  auto *output = StringAllocator::allocate(length + 1, length);
  auto *data = StringAllocator::getContainerStartPtr(output);
  for (size_t i = 0; i < length; i++) {
    data[i] = str[i];
  }
  data[length] = 0;

  return output;
}

using ObjectArrayAllocator = ArrayAllocator<ObjectArray, Object *>;

ObjectArray::ObjectArray(size_t length) : _length(length) {}

ObjectArray::~ObjectArray() {
  auto **data = ObjectArrayAllocator::getContainerStartPtr(this);
  for (size_t i = 0; i < _length; i++) {
    Object::release(data[i]);
  }
}

size_t ObjectArray::length() const { return _length; }

Object *ObjectArray::getObjectAtIndex(size_t index) const {
  Object *const *data = ObjectArrayAllocator::getContainerStartPtr(this);
  auto *object = data[index];
  Object::retain(object);
  return object;
}

void ObjectArray::setObjectAtIndex(size_t index, Object *object) {
  Object **data = ObjectArrayAllocator::getContainerStartPtr(this);

  Object::retain(object);
  Object::release(data[index]);
  data[index] = object;
}

ObjectArray *ObjectArray::make(size_t length) {
  auto *output = ObjectArrayAllocator::allocate(length, length);

  std::memset(ObjectArrayAllocator::getContainerStartPtr(output), 0,
    length * ObjectArrayAllocator::alignedValueSize);

  return output;
}

Binary::Binary(uint8_t *data, size_t length) : _data(data), _length(length) {}

Binary::~Binary() = default;

uint8_t *Binary::data() const { return _data; }

size_t Binary::length() const { return _length; }

BinaryWithDeallocator::BinaryWithDeallocator(
    uint8_t *data, size_t length, void *opaque,
    BinaryWithDeallocator::Deallocator deallocator)
    : Binary(data, length), _opaque(opaque), _deallocator(deallocator) {}

BinaryWithDeallocator::~BinaryWithDeallocator() {
  if (_deallocator) {
    _deallocator(data(), length(), _opaque);
  }
}

BinaryWithDeallocator *
BinaryWithDeallocator::make(uint8_t *data, size_t length, void *opaque,
                            BinaryWithDeallocator::Deallocator deallocator) {
  return new BinaryWithDeallocator(data, length, opaque, deallocator);
}

Number::Number(Number::Value value, Number::ValueType type)
    : _value(value), _type(type) {}
Number::~Number() = default;

uint64_t Number::toUnsignedInt() const {
  switch (_type) {
  case Number::ValueType::UNSIGNED_INT:
    return _value.u;
  case Number::ValueType::SIGNED_INT:
    return static_cast<uint64_t>(_value.i);
  case Number::ValueType::DOUBLE:
    return static_cast<uint64_t>(_value.d);
  }
}

int64_t Number::toSignedInt() const {
  switch (_type) {
  case Number::ValueType::UNSIGNED_INT:
    return static_cast<int64_t>(_value.u);
  case Number::ValueType::SIGNED_INT:
    return _value.i;
  case Number::ValueType::DOUBLE:
    return static_cast<int64_t>(_value.d);
  }
}

double Number::toDouble() const {
  switch (_type) {
  case Number::ValueType::UNSIGNED_INT:
    return static_cast<double>(_value.u);
  case Number::ValueType::SIGNED_INT:
    return static_cast<double>(_value.i);
  case Number::ValueType::DOUBLE:
    return _value.d;
  }
}

Number *Number::make(Number::Value value, Number::ValueType type) {
  return new Number(value, type);
}

void crashForInvalidCast(const char *str) {
  std::fprintf(stderr, "Attempting to cast an invalid pointer to type: %s!",
               str);
  std::abort();
}

} // namespace djinni