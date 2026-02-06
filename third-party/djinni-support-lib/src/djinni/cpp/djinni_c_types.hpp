#pragma once

#include <atomic>
#include <memory>

#ifndef DJINNI_CHECKED_C_API
#if DEBUG
#define DJINNI_CHECKED_C_API 1
#else
#define DJINNI_CHECKED_C_API 0
#endif
#endif

namespace djinni {

class Object {
public:
  Object();
  virtual ~Object();

  static void retain(Object *object);
  static void release(Object *object);

private:
  std::atomic_int _ref;

  void onRetain();
  void onRelease();
};

class String : public Object {
public:
  String(size_t length);
  ~String() override;

  const char *data() const;
  size_t length() const;

  static String *make(const char *str, size_t length);

private:
  size_t _length;
};

class ObjectArray : public Object {
public:
  ObjectArray(size_t length);
  ~ObjectArray() override;

  size_t length() const;

  Object *getObjectAtIndex(size_t index) const;
  void setObjectAtIndex(size_t index, Object *object);

  static ObjectArray *make(size_t length);

private:
  size_t _length;
};

class Binary : public Object {
public:
  Binary(uint8_t *data, size_t length);
  ~Binary() override;

  uint8_t *data() const;
  size_t length() const;

private:
  uint8_t *_data;
  size_t _length;
};

class BinaryWithDeallocator : public Binary {
public:
  using Deallocator = void (*)(uint8_t *, size_t, void *);
  BinaryWithDeallocator(uint8_t *data, size_t length, void *opaque,
                        BinaryWithDeallocator::Deallocator deallocator);
  ~BinaryWithDeallocator() override;

  static BinaryWithDeallocator *
  make(uint8_t *data, size_t length, void *opaque,
       BinaryWithDeallocator::Deallocator deallocator);

private:
  void *_opaque;
  BinaryWithDeallocator::Deallocator _deallocator;
};

class Number : public Object {
public:
  union Value {
    uint64_t u;
    int64_t i;
    double d;
  };

  enum class ValueType { UNSIGNED_INT, SIGNED_INT, DOUBLE };

  Number(Value value, ValueType type);
  ~Number() override;

  uint64_t toUnsignedInt() const;
  int64_t toSignedInt() const;
  double toDouble() const;

  static Number *make(Value value, ValueType type);

private:
  Value _value;
  ValueType _type;
};

template <typename T> class RecordHolder : public Object {
public:
  RecordHolder(T data) : _data(std::move(data)) {}
  ~RecordHolder() override = default;

  const T &data() const { return _data; }

  T &data() { return _data; }

private:
  T _data;
};

template <typename T> class InterfaceHolder : public Object {
public:
  InterfaceHolder(std::shared_ptr<T> data) : _data(std::move(data)) {}
  ~InterfaceHolder() override = default;

  const std::shared_ptr<T> &data() const { return _data; }

private:
  std::shared_ptr<T> _data;
};

template <typename T> class ProxyClass : public Object {
public:
  using OpaqueDeallocator = void (*)(void *);
  ProxyClass(const T &methodDefs, OpaqueDeallocator opaqueDeallocator)
      : _methodDefs(methodDefs), _opaqueDeallocator(opaqueDeallocator) {}
  ~ProxyClass() override = default;

  const T &methodDefs() const { return _methodDefs; }
  const OpaqueDeallocator &getOpaqueDeallocator() const {
    return _opaqueDeallocator;
  }

private:
  T _methodDefs;
  OpaqueDeallocator _opaqueDeallocator;
};

template <typename T> class Proxy {
public:
  Proxy(ProxyClass<T> *proxyClass, void *opaque)
      : _proxyClass(proxyClass), _opaque(opaque) {
    Object::retain(_proxyClass);
  }

  ~Proxy() {
    if (_proxyClass->getOpaqueDeallocator() != nullptr) {
      _proxyClass->getOpaqueDeallocator()(_opaque);
    }

    Object::release(_proxyClass);
  }

  const ProxyClass<T> &getProxyClass() const { return *_proxyClass; }

  void *getOpaque() const { return _opaque; }

private:
  ProxyClass<T> *_proxyClass;
  void *_opaque;
};

inline void *toC(Object *obj) { return reinterpret_cast<void *>(obj); }

void crashForInvalidCast(const char *str);

template <typename T>
static void onInvalidCast() {
  crashForInvalidCast(typeid(T).name());
}

#if DJINNI_CHECKED_C_API
template <typename T> static T *fromC(void *ref) {
  if (ref == nullptr) {
    return nullptr;
  }
  auto *ptr = dynamic_cast<T *>(reinterpret_cast<Object *>(ref));
  if (ptr == nullptr) {
      onInvalidCast<T>();
  }

  return ptr;
}
#else
template <typename T> static T *fromC(void *ref) {
  return static_cast<T *>(reinterpret_cast<Object *>(ref));
}
#endif

} // namespace djinni