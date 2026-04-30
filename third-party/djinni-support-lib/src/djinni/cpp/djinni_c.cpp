#include "djinni_c.h"
#include "djinni_c_types.hpp"
#include <cstdio>

using namespace djinni;

void djinni_ref_retain(djinni_ref ref) { Object::retain(fromC<Object>(ref)); }

void djinni_ref_release(djinni_ref ref) { Object::release(fromC<Object>(ref)); }

djinni_string_ref djinni_string_new(const char *str, size_t length) {
  return toC(String::make(str, length));
}

const char *djinni_string_get_data(djinni_string_ref str) {
  return fromC<String>(str)->data();
}

size_t djinni_string_get_length(djinni_string_ref str) {
  return fromC<String>(str)->length();
}

djinni_binary_ref
djinni_binary_new_with_bytes(uint8_t *data, size_t length, void *opaque,
                             djinni_binary_deallocator deallocator) {
  return toC(BinaryWithDeallocator::make(data, length, opaque, deallocator));
}

static void djinni_binary_malloc_release(uint8_t *data, size_t length,
                                         void *opaque) {
  free(opaque);
}

djinni_binary_ref djinni_binary_new_with_bytes_copy(const uint8_t *data,
                                                    size_t length) {
  auto *mutableData = (uint8_t *)malloc(length);
  memcpy(mutableData, data, length);
  return djinni_binary_new_with_bytes(mutableData, length, mutableData,
                                      &djinni_binary_malloc_release);
}

djinni_binary_ref djinni_binary_new(size_t length) {
  auto *mutableData = (uint8_t *)malloc(length);
  return djinni_binary_new_with_bytes(mutableData, length, mutableData,
                                      &djinni_binary_malloc_release);
}

uint8_t *djinni_binary_get_data(djinni_binary_ref binary) {
  return fromC<Binary>(binary)->data();
}

size_t djinni_binary_get_length(djinni_binary_ref binary) {
  return fromC<Binary>(binary)->length();
}

djinni_number_ref djinni_number_int64_new(int64_t v) {
  Number::Value value;
  value.i = v;
  return toC(Number::make(value, Number::ValueType::SIGNED_INT));
}

djinni_number_ref djinni_number_uint64_new(uint64_t v) {
  Number::Value value;
  value.u = v;
  return toC(Number::make(value, Number::ValueType::UNSIGNED_INT));
}

djinni_number_ref djinni_number_double_new(double v) {
  Number::Value value;
  value.d = v;
  return toC(Number::make(value, Number::ValueType::DOUBLE));
}

int64_t djinni_number_get_int64(djinni_number_ref number) {
  return fromC<Number>(number)->toSignedInt();
}

uint64_t djinni_number_get_uint64(djinni_number_ref number) {
  return fromC<Number>(number)->toUnsignedInt();
}

double djinni_number_get_double(djinni_number_ref number) {
  return fromC<Number>(number)->toDouble();
}

djinni_keyval_array_ref djinni_keyval_array_new(size_t size) {
  auto *array = ObjectArray::make(size << 1);
  return toC(array);
}

size_t djinni_keyval_array_get_length(djinni_keyval_array_ref keyval_array) {
  return fromC<ObjectArray>(keyval_array)->length() >> 1;
}

djinni_ref djinni_keyval_array_get_key(djinni_keyval_array_ref keyval_array,
                                       size_t index) {
  return toC(fromC<ObjectArray>(keyval_array)->getObjectAtIndex(index << 1));
}

djinni_ref djinni_keyval_array_get_value(djinni_keyval_array_ref keyval_array,
                                         size_t index) {
  return toC(
      fromC<ObjectArray>(keyval_array)->getObjectAtIndex((index << 1) + 1));
}

void djinni_keyval_array_set_entry(djinni_keyval_array_ref keyval_array,
                                   size_t index, djinni_ref key,
                                   djinni_ref value) {
  auto *array = fromC<ObjectArray>(keyval_array);
  array->setObjectAtIndex(index << 1, fromC<Object>(key));
  array->setObjectAtIndex((index << 1) + 1, fromC<Object>(value));
}

djinni_array_ref djinni_array_new(size_t length) {
  if (length == 0) {
    static auto *kEmptyArray = ObjectArray::make(length);
    Object::retain(kEmptyArray);
    return toC(kEmptyArray);
  } else {
    return toC(ObjectArray::make(length));
  }
}

size_t djinni_array_get_length(djinni_array_ref array) {
  return fromC<ObjectArray>(array)->length();
}

djinni_ref djinni_array_get_value(djinni_array_ref array, size_t index) {
  return toC(fromC<ObjectArray>(array)->getObjectAtIndex(index));
}

void djinni_array_set_value(djinni_array_ref array, size_t index,
                            djinni_ref value) {
  fromC<ObjectArray>(array)->setObjectAtIndex(index, fromC<Object>(value));
}

djinni_date_ref djinni_date_new(uint64_t epoch_time_ms) {
  return djinni_number_uint64_new(epoch_time_ms);
}

uint64_t djinni_date_get_epoch(djinni_date_ref date) {
  return djinni_number_get_uint64(date);
}

namespace djinni::c_api {

struct ExceptionHandler {
  void *opaque;
  djinni_exception_handler handler;

  ExceptionHandler(void *opaque, djinni_exception_handler handler)
      : opaque(opaque), handler(handler) {}

  static void push(djinni_exception_handler *handler) {
    auto &handlerPtr = getHandlerPtr();
    handler->__reserved__ = handlerPtr;
    handlerPtr = handler;
  }

  static void pop() {
    auto &handlerPtr = getHandlerPtr();
    if (handlerPtr == nullptr) {
      std::fprintf(stderr, "Unbalanced djinni_exception_handler_push and "
                           "djinni_exception_handler_pop calls");
      std::abort();
    }

    handlerPtr =
        reinterpret_cast<djinni_exception_handler *>(handlerPtr->__reserved__);
  }

  static void notify(const char *error) {
    auto *handler = getHandlerPtr();
    if (handler == nullptr) {
      std::fprintf(stderr,
                   "No exception handler registered with "
                   "djinni_exception_handler_push to handle error: %s",
                   error);
      std::abort();
    }

    handler->callback(handler->opaque, error);
  }

private:
  static djinni_exception_handler *&getHandlerPtr() {
    thread_local djinni_exception_handler *kHandler = nullptr;
    return kHandler;
  }
};

} // namespace djinni::c_api

void djinni_exception_handler_push(djinni_exception_handler *handler) {
  djinni::c_api::ExceptionHandler::push(handler);
}
void djinni_exception_handler_pop() { djinni::c_api::ExceptionHandler::pop(); }

void djinni_exception_notify(const char *error) {
  djinni::c_api::ExceptionHandler::notify(error);
}