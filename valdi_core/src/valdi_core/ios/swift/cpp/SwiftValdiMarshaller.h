//
//  SwiftValdiMarshaller.h
//  valdi_core
//
//  Created by Edward Lee on 4/30/24.
//
#pragma once

#include <valdi_core/ios/swift/cpp/SwiftValdiMarshallableObjectRegistry.h>
#include <valdi_core/ios/swift/cpp/SwiftValdiTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* SwiftValdiMarshallerRef;
typedef void* SwiftValueFunctionPtr;
typedef void* SwiftValdiFunction;
typedef void* SwiftValuePromisePtr;

SwiftValdiMarshallerRef SwiftValdiMarshaller_Create();
void SwiftValdiMarshaller_Destroy(SwiftValdiMarshallerRef marshaller);

SwiftInt SwiftValdiMarshaller_Size(SwiftValdiMarshallerRef marshaller);
void SwiftValdiMarshaller_Pop(SwiftValdiMarshallerRef marshaller);
void SwiftValdiMarshaller_PopCount(SwiftValdiMarshallerRef marshaller, SwiftInt count);
SwiftInt SwiftValdiMarshaller_GetCount(SwiftValdiMarshallerRef marshaller);
SwiftBool SwiftValdiMarshaller_CheckError(SwiftValdiMarshallerRef marshaller);
SwiftString SwiftValdiMarshaller_GetError(SwiftValdiMarshallerRef marshaller);
void SwiftValdiMarshaller_SetError(SwiftValdiMarshallerRef marshaller, const char* errorMessage);
SwiftString SwiftValdiMarshaller_ToString(SwiftValdiMarshallerRef marshaller, SwiftInt index, SwiftBool indent);

SwiftInt SwiftValdiMarshaller_PushMap(SwiftValdiMarshallerRef marshaller, SwiftInt initialCapacity);
SwiftInt SwiftValdiMarshaller_PushArray(SwiftValdiMarshallerRef marshaller, SwiftInt capacity);
SwiftInt SwiftValdiMarshaller_PushDouble(SwiftValdiMarshallerRef marshaller, SwiftDouble d);
SwiftInt SwiftValdiMarshaller_PushLong(SwiftValdiMarshallerRef marshaller, SwiftInt64 i);
SwiftInt SwiftValdiMarshaller_PushBool(SwiftValdiMarshallerRef marshaller, SwiftBool boolean);
SwiftInt SwiftValdiMarshaller_PushInt(SwiftValdiMarshallerRef marshaller, SwiftInt32 i);
SwiftInt SwiftValdiMarshaller_PushString(SwiftValdiMarshallerRef marshaller, SwiftStringView str);
SwiftInt SwiftValdiMarshaller_PushUndefined(SwiftValdiMarshallerRef marshaller);
SwiftInt SwiftValdiMarshaller_PushNull(SwiftValdiMarshallerRef marshaller);
SwiftInt SwiftValdiMarshaller_PushData(SwiftValdiMarshallerRef marshaller, const unsigned char* data, SwiftInt length);
SwiftInt SwiftValdiMarshaller_PushObject(SwiftValdiMarshallerRef marshaller,
                                         SwiftValdiMarshallableObjectRegistryRef marshallerRegistry,
                                         const char* className);
SwiftInt SwiftValdiMarshaller_PushProxyObject(SwiftValdiMarshallerRef marshaller, const void* swiftObject);
const void* SwiftValdiMarshaller_GetProxyObject(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftInt SwiftValdiMarshaller_UnwrapProxyObject(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftInt SwiftValdiMarshaller_PushMapIterator(SwiftValdiMarshallerRef marshaller, SwiftInt mapIndex);
SwiftBool SwiftValdiMarshaller_PushMapIteratorNext(SwiftValdiMarshallerRef marshaller, SwiftInt iteratorIndex);

SwiftBool SwiftValdiMarshaller_GetBool(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftInt32 SwiftValdiMarshaller_GetInt(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftInt64 SwiftValdiMarshaller_GetLong(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftDouble SwiftValdiMarshaller_GetDouble(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftStringBox SwiftValdiMarshaller_GetString(SwiftValdiMarshallerRef marshaller, SwiftInt index);
const unsigned char* SwiftValdiMarshaller_GetData(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftInt SwiftValdiMarshaller_GetDataSize(SwiftValdiMarshallerRef marshaller, SwiftInt index);

SwiftBool SwiftValdiMarshaller_IsNullOrUndefined(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsBool(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsInt(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsBigInt(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsLong(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsDouble(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsString(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsMap(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsArray(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsTypedObject(SwiftValdiMarshallerRef marshaller, SwiftInt index);
SwiftBool SwiftValdiMarshaller_IsError(SwiftValdiMarshallerRef marshaller, SwiftInt index);

SwiftInt SwiftValdiMarshaller_GetTypedObjectProperty(SwiftValdiMarshallerRef marshaller,
                                                     SwiftInt objectIndex,
                                                     SwiftInt propertyIndex);
void SwiftValdiMarshaller_SetTypedObjectProperty(SwiftValdiMarshallerRef marshaller,
                                                 SwiftInt objectIndex,
                                                 SwiftInt propertyIndex);

bool SwiftValdiMarshaller_GetMapProperty(SwiftValdiMarshallerRef marshaller, const char* propertyName, SwiftInt index);
void SwiftValdiMarshaller_PutMapProperty(SwiftValdiMarshallerRef marshaller, const char* propertyName, SwiftInt index);

void SwiftValdiMarshaller_SetArrayItem(SwiftValdiMarshallerRef marshaller, SwiftInt index, SwiftInt arrayIndex);
SwiftInt SwiftValdiMarshaller_GetArrayItem(SwiftValdiMarshallerRef marshaller, SwiftInt index, SwiftInt arrayIndex);
SwiftInt SwiftValdiMarshaller_GetArraySize(SwiftValdiMarshallerRef marshaller, SwiftInt index);

SwiftBool SwiftValdiMarshaller_CallFunction(SwiftValueFunctionPtr function,
                                            SwiftValdiMarshallerRef marshaller,
                                            SwiftBool sync);
SwiftInt SwiftValdiMarshaller_PushFunction(SwiftValdiMarshallerRef marshaller, SwiftValdiFunction valdiFunction);

// Returns a SwiftValueFunctionPtr that can be used to call the function
// Acquires a reference to the function that must be released with SwiftValdiMarshaller_ReleaseFunction
SwiftValueFunctionPtr SwiftValdiMarshaller_GetFunction(SwiftValdiMarshallerRef marshaller, SwiftInt index);
void SwiftValdiMarshaller_ReleaseFunction(SwiftValueFunctionPtr function);

SwiftValuePromisePtr SwiftValdiMarshaller_GetPromise(SwiftValdiMarshallerRef marshaller, SwiftInt index);
void SwiftValdiMarshaller_ReleasePromise(SwiftValuePromisePtr promisePtr);
SwiftInt SwiftValdiMarshaller_PushNewPromise(SwiftValdiMarshallerRef marshaller);
SwiftInt SwiftValdiMarshaller_PushPromise(SwiftValdiMarshallerRef marshaller, SwiftValuePromisePtr promisePtr);
void SwiftValdiMarshaller_OnPromiseComplete(SwiftValdiMarshallerRef marshaller,
                                            SwiftValuePromisePtr promisePtr,
                                            SwiftInt iFunction);
void SwiftValdiMarshaller_ResolvePromise(SwiftValdiMarshallerRef marshaller,
                                         SwiftValuePromisePtr promisePtr,
                                         SwiftInt iResult);
void SwiftValdiMarshaller_RejectPromise(SwiftValuePromisePtr promisePtr, const char* errorMessage);
void SwiftValdiMarshaller_CancelPromise(SwiftValuePromisePtr promisePtr);
void SwiftValdiMarshaller_SetCancelCallback(SwiftValdiMarshallerRef marshaller,
                                            SwiftValuePromisePtr promisePtr,
                                            SwiftInt iFunction);

SwiftStringView SwiftValdiMarshaller_StringViewFromStringBox(SwiftStringBox strbox);
void SwiftValdiMarshaller_ReleaseStringBox(SwiftStringBox strbox);
#ifdef __cplusplus
}
#endif
