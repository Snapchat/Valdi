//
//  SCValdiMarshaller.m
//  valdi-ios
//
//  Created by Simon Corsin on 7/19/19.
//

#include "SCValdiMarshaller.h"
#import "valdi_core/SCValdiMarshaller+CPP.h"
#include <Foundation/Foundation.h>
#import "valdi_core/cpp/Utils/InterfaceMarshallDiagnostics.hpp"
#import "valdi_core/cpp/Utils/Marshaller.hpp"
#import "valdi_core/cpp/Utils/ValueTypedObject.hpp"
#import "valdi_core/cpp/Utils/ValueFunction.hpp"
#import "valdi_core/cpp/Utils/ValueTypedProxyObject.hpp"
#import "valdi_core/SCValdiInternedString+CPP.h"
#import "valdi_core/SCValdiFunctionWithBlock.h"
#import "valdi_core/SCValdiObjCConversionUtils.h"
#import "valdi_core/SCValdiRef.h"
#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiError.h"
#import "valdi_core/SCValdiMarshallable.h"
#import "valdi_core/SCValdiFunctionWithBlock.h"
#import "valdi_core/SCValdiNativeConvertible.h"

BOOL SCValdiIsNull(__unsafe_unretained id object) {
    if (!object) {
        return YES;
    }
    if ([object isKindOfClass:[NSNull class]]) {
        return YES;
    }

    return NO;
}

class IOSMarshaller: public Valdi::Marshaller {
public:
    IOSMarshaller(): Valdi::Marshaller(_exceptionTracker) {}

    ~IOSMarshaller() override = default;

private:
    Valdi::SimpleExceptionTracker _exceptionTracker;
};

static void SCValdiMarshallerCppCheck(Valdi::Marshaller *marshaller) {
    if (!marshaller->getExceptionTracker()) {
        ValdiIOS::NSExceptionThrowFromError(marshaller->getExceptionTracker().extractError());
    }
}

SCValdiMarshallerRef SCValdiMarshallerWrap(Valdi::Marshaller *marshaller)
{
    return reinterpret_cast<SCValdiMarshallerRef>(marshaller);
}

Valdi::Marshaller *SCValdiMarshallerUnwrap(SCValdiMarshallerRef marshallerRef)
{
    return reinterpret_cast<Valdi::Marshaller *>(marshallerRef);
}

Valdi::Value SCValdiMarshallerGetValue(SCValdiMarshallerRef marshaller, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    auto value = marshallerCpp->get(static_cast<int>(index));
    SCValdiMarshallerCppCheck(marshallerCpp);
    return value;
}

Valdi::Value SCValdiMarshallerGetValueOrUndefined(SCValdiMarshallerRef marshaller, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    auto value = marshallerCpp->getOrUndefined(static_cast<int>(index));
    SCValdiMarshallerCppCheck(marshallerCpp);
    return value;
}

FOUNDATION_EXPORT SCValdiMarshallerRef SCValdiMarshallerCreate() {
    Valdi::Marshaller *marshaller = new IOSMarshaller();
    return SCValdiMarshallerWrap(marshaller);
}

FOUNDATION_EXPORT void SCValdiMarshallerDestroy(SCValdiMarshallerRef marshaller)
{
    delete SCValdiMarshallerUnwrap(marshaller);
}

FOUNDATION_EXPORT void SCValdiMarshallerThrowBadType(SCValdiMarshallerRef marshaller, NSString *type, NSInteger index)
{
    auto cppType = SCValdiMarshallerGetValue(marshaller, index).getType();
    NSString *foundType = ValdiIOS::NSStringFromSTDStringView(Valdi::valueTypeToString(cppType));
    SCValdiErrorThrow([NSString stringWithFormat:@"Unexpectedly found type %@ instead of expected %@ at index %d", foundType, type, (int)index]);
}

FOUNDATION_EXPORT void SCValdiMarshallerCheck(SCValdiMarshallerRef marshaller)
{
    SCValdiMarshallerCppCheck(SCValdiMarshallerUnwrap(marshaller));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushMap(SCValdiMarshallerRef marshaller, NSInteger initialCapacity)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushMap(static_cast<int>(initialCapacity));
}

FOUNDATION_EXPORT void SCValdiMarshallerPutMapProperty(SCValdiMarshallerRef marshaller, SCValdiInternedStringRef propertyName, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    marshallerCpp->putMapProperty(*SCValdiInternedStringUnwrap(propertyName), static_cast<int>(index));
    SCValdiMarshallerCppCheck(marshallerCpp);
}

FOUNDATION_EXPORT void SCValdiMarshallerPutMapPropertyUninterned(SCValdiMarshallerRef marshaller, NSString *propertyName, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    marshallerCpp->putMapProperty(ValdiIOS::InternedStringFromNSString(propertyName), static_cast<int>(index));
    SCValdiMarshallerCppCheck(marshallerCpp);
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerGetMapProperty(SCValdiMarshallerRef marshaller, SCValdiInternedStringRef propertyName, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    auto result = marshallerCpp->getMapProperty(*SCValdiInternedStringUnwrap(propertyName), static_cast<int>(index), Valdi::Marshaller::GetMapPropertyFlags::IgnoreNullOrUndefined);
    SCValdiMarshallerCppCheck(marshallerCpp);

    return result;
}

FOUNDATION_EXPORT void SCValdiMarshallerMustGetMapProperty(SCValdiMarshallerRef marshaller, SCValdiInternedStringRef propertyName, NSInteger index)
{
    if (!SCValdiMarshallerGetMapProperty(marshaller, propertyName, index)) {
        SCValdiErrorThrow([NSString stringWithFormat:@"Could not find property %s in object at index %d", SCValdiInternedStringUnwrap(propertyName)->getCStr(), (int)index]);
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerGetTypedObjectProperty(SCValdiMarshallerRef marshaller, NSInteger propertyIndex, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    auto typedObject = marshallerCpp->getTypedObject(static_cast<int>(index));
    SCValdiMarshallerCppCheck(marshallerCpp);

    return marshallerCpp->push(typedObject->getProperty(static_cast<size_t>(propertyIndex)));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushArray(SCValdiMarshallerRef marshaller, NSInteger capacity)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushArray(static_cast<int>(capacity));
}

FOUNDATION_EXPORT void SCValdiMarshallerSetArrayItem(SCValdiMarshallerRef marshaller, NSInteger index, NSInteger arrayIndex)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    marshallerCpp->setArrayItem(static_cast<int>(index), static_cast<int>(arrayIndex));
    SCValdiMarshallerCppCheck(marshallerCpp);
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushString(SCValdiMarshallerRef marshaller, NSString *str)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushString(ValdiIOS::StringFromNSString(str));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushOptionalString(SCValdiMarshallerRef marshaller, NSString *_Nullable str)
{
    if (!SCValdiIsNull(str)) {
        return SCValdiMarshallerPushString(marshaller, str);
    } else {
        return SCValdiMarshallerPushUndefined(marshaller);
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushInternedString(SCValdiMarshallerRef marshaller, SCValdiInternedStringRef internedString)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushString(*SCValdiInternedStringUnwrap(internedString));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushFunction(SCValdiMarshallerRef marshaller, id<SCValdiFunction> function)
{
    return SCValdiMarshallerUnwrap(marshaller)->push(ValdiIOS::ValueFromFunction(function));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushFunctionWithBlock(SCValdiMarshallerRef marshaller,
                                                    SCValdiFunctionBlock functionBlock) {
    return SCValdiMarshallerPushFunction(marshaller, [SCValdiFunctionWithBlock functionWithBlock:functionBlock]);
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushNull(SCValdiMarshallerRef marshaller)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushNull();
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushUndefined(SCValdiMarshallerRef marshaller)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushUndefined();
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushBool(SCValdiMarshallerRef marshaller, BOOL boolean)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushBool(static_cast<bool>(boolean));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushOptionalBool(SCValdiMarshallerRef marshaller, NSNumber *_Nullable boolean)
{
    if (!SCValdiIsNull(boolean)) {
        return SCValdiMarshallerPushBool(marshaller, boolean.boolValue);
    } else {
        return SCValdiMarshallerPushUndefined(marshaller);
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushDouble(SCValdiMarshallerRef marshaller, double d)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushDouble(d);
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushOptionalDouble(SCValdiMarshallerRef marshaller, NSNumber *_Nullable d)
{
    if (!SCValdiIsNull(d)) {
        return SCValdiMarshallerPushDouble(marshaller, d.doubleValue);
    } else {
        return SCValdiMarshallerPushUndefined(marshaller);
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushOptionalLong(SCValdiMarshallerRef marshaller, NSNumber *_Nullable l)
{
    if (!SCValdiIsNull(l)) {
        return SCValdiMarshallerPushLong(marshaller, l.longValue);
    } else {
        return SCValdiMarshallerPushUndefined(marshaller);
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushUntyped(SCValdiMarshallerRef marshaller, id untypedObject)
{
    return SCValdiMarshallerUnwrap(marshaller)->push(ValdiIOS::ValueFromNSObject(untypedObject));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushInt(SCValdiMarshallerRef marshaller, int32_t i)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushInt(i);
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushLong(SCValdiMarshallerRef marshaller, int64_t i)
{
    return SCValdiMarshallerUnwrap(marshaller)->pushLong(i);
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushEnumInt(SCValdiMarshallerRef marshaller, int32_t enumValue)
{
    return SCValdiMarshallerPushInt(marshaller, enumValue);
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushOpaque(SCValdiMarshallerRef marshaller, id opaqueObject)
{
    Valdi::Value value;
    if ([opaqueObject respondsToSelector:@selector(valdi_toNative:)]) {
        value = ValdiIOS::ValueFromNSObject(opaqueObject);
    } else {
        auto valdiObject = ValdiIOS::ValdiObjectFromNSObject(opaqueObject);
        value = Valdi::Value(valdiObject);
    }

    return SCValdiMarshallerUnwrap(marshaller)->push(value);
}

FOUNDATION_EXPORT id SCValdiMarshallerGetOpaque(SCValdiMarshallerRef marshaller, NSInteger index)
{
    const Valdi::Value &value = SCValdiMarshallerGetValue(marshaller, index);
    id nativeOpaqueObject = ValdiIOS::NSObjectFromValdiObject(value.getValdiObject(), NO);
    if (nativeOpaqueObject) {
        return nativeOpaqueObject;
    } else {
        id wrappedValue = ValdiIOS::NSObjectFromValue(value);
        return wrappedValue;
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerUnwrapProxy(SCValdiMarshallerRef marshaller, NSInteger index)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    auto proxyObject = marshallerCpp->getProxyObject(static_cast<int>(index));
    SCValdiMarshallerCheck(marshaller);
    return marshallerCpp->push(Valdi::Value(proxyObject->getTypedObject()));
}

FOUNDATION_EXPORT id<SCValdiFunction> SCValdiMarshallerGetFunction(SCValdiMarshallerRef marshaller, NSInteger index)
{
    id<SCValdiFunction> function = ValdiIOS::FunctionFromValue(SCValdiMarshallerGetValue(marshaller, index));
    if (!function) {
        SCValdiMarshallerThrowBadType(marshaller, @"function", index);
    }

    return function;
}

FOUNDATION_EXPORT void SCValdiMarshallerOnPromiseComplete(SCValdiMarshallerRef marshaller, NSInteger index, SCValdiFunctionBlock callback)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    auto function = ValdiIOS::ValueFromFunction([SCValdiFunctionWithBlock functionWithBlock:callback]);
    marshallerCpp->onPromiseComplete(static_cast<int>(index), function.getFunctionRef());
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushData(SCValdiMarshallerRef marshaller, NSData *data)
{
    return SCValdiMarshallerUnwrap(marshaller)->push(ValdiIOS::ValueFromNSData(data));
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerPushOptionalData(SCValdiMarshallerRef marshaller, NSData *_Nullable data)
{
    if (!SCValdiIsNull(data)) {
        return SCValdiMarshallerPushData(marshaller, data);
    } else {
        return SCValdiMarshallerPushUndefined(marshaller);
    }
}

FOUNDATION_EXPORT NSInteger SCValdiMarshallerMarshallArray(SCValdiMarshallerRef marshaller, __unsafe_unretained NSArray<id> *array, NS_NOESCAPE SCValdiMarshallerMarshallBlock marshallItemBlock)
{
    NSInteger objectIndex = SCValdiMarshallerPushArray(marshaller, array.count);

    NSInteger i = 0;
    for (id item in array) {
        marshallItemBlock(item);
        SCValdiMarshallerSetArrayItem(marshaller, objectIndex, i);
        i++;
    }

    return objectIndex;
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerGetBool(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValue(marshaller, index).toBool();
}

FOUNDATION_EXPORT NSNumber* _Nullable SCValdiMarshallerGetOptionalBool(SCValdiMarshallerRef marshaller, NSInteger index)
{
    if (SCValdiMarshallerIsNullOrUndefined(marshaller, index)) {
        return nil;
    } else {
        return @(SCValdiMarshallerGetBool(marshaller, index));
    }
}

FOUNDATION_EXPORT double SCValdiMarshallerGetDouble(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValue(marshaller, index).toDouble();
}

FOUNDATION_EXPORT NSNumber *_Nullable SCValdiMarshallerGetOptionalDouble(SCValdiMarshallerRef marshaller, NSInteger index)
{
    if (SCValdiMarshallerIsNullOrUndefined(marshaller, index)) {
        return nil;
    } else {
        return @(SCValdiMarshallerGetDouble(marshaller, index));
    }
}

FOUNDATION_EXPORT int32_t SCValdiMarshallerGetInt(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValue(marshaller, index).toInt();
}

FOUNDATION_EXPORT int64_t SCValdiMarshallerGetLong(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValue(marshaller, index).toLong();
}

FOUNDATION_EXPORT NSNumber *_Nullable SCValdiMarshallerGetOptionalLong(SCValdiMarshallerRef marshaller, NSInteger index)
{
    if (SCValdiMarshallerIsNullOrUndefined(marshaller, index)) {
        return nil;
    } else {
        return @(SCValdiMarshallerGetLong(marshaller, index));
    }
}

FOUNDATION_EXPORT NSString *SCValdiMarshallerGetString(SCValdiMarshallerRef marshaller, NSInteger index)
{
    auto str = SCValdiMarshallerGetValue(marshaller, index).toStringBox();
    return ValdiIOS::NSStringFromString(str);
}

FOUNDATION_EXPORT NSString *_Nullable SCValdiMarshallerGetOptionalString(SCValdiMarshallerRef marshaller, NSInteger index)
{
    if (SCValdiMarshallerIsNullOrUndefined(marshaller, index)) {
        return nil;
    } else {
        return SCValdiMarshallerGetString(marshaller, index);
    }
}

FOUNDATION_EXPORT NSError *SCValdiMarshallerGetError(SCValdiMarshallerRef marshaller, NSInteger index)
{
    auto value = SCValdiMarshallerGetValue(marshaller, index);
    return ValdiIOS::NSErrorFromError(value.getError());
}

FOUNDATION_EXPORT id SCValdiMarshallerGetUntyped(SCValdiMarshallerRef marshaller, NSInteger index)
{
    const auto &value = SCValdiMarshallerGetValue(marshaller, index);
    return ValdiIOS::NSObjectFromValue(value);
}

FOUNDATION_EXPORT id SCValdiMarshallerGetNativeWrapper(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return ValdiIOS::NSObjectWrappingValue(SCValdiMarshallerGetValue(marshaller, index));
 }

FOUNDATION_EXPORT NSUInteger SCValdiMarshallerGetCount(SCValdiMarshallerRef marshaller)
{
    return (NSUInteger)SCValdiMarshallerUnwrap(marshaller)->size();
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerIsNullOrUndefined(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerUnwrap(marshaller)->getOrUndefined(static_cast<int>(index)).isNullOrUndefined();
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerIsString(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValueOrUndefined(marshaller, index).isString();
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerIsBool(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValueOrUndefined(marshaller, index).isBool();
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerIsDouble(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValueOrUndefined(marshaller, index).isDouble();
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerIsMap(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValueOrUndefined(marshaller, index).isMap();
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerIsError(SCValdiMarshallerRef marshaller, NSInteger index)
{
    return SCValdiMarshallerGetValueOrUndefined(marshaller, index).isError();
}

FOUNDATION_EXPORT int32_t SCValdiMarshallerGetAndResetLastInterfaceMarshallOutcome(void)
{
    return Valdi::getAndResetLastInterfaceMarshallOutcome();
}

FOUNDATION_EXPORT void SCValdiMarshallerSetInterfaceMarshallDiagnosticsEnabled(BOOL enabled)
{
    Valdi::setInterfaceMarshallDiagnosticsEnabled(enabled);
}

FOUNDATION_EXPORT void SCValdiMarshallerPop(SCValdiMarshallerRef marshaller)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    marshallerCpp->pop();
    SCValdiMarshallerCppCheck(marshallerCpp);
}

FOUNDATION_EXPORT void SCValdiMarshallerPopCount(SCValdiMarshallerRef marshaller, NSInteger count)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    marshallerCpp->popCount(static_cast<int>(count));
    SCValdiMarshallerCppCheck(marshallerCpp);
}

FOUNDATION_EXPORT NSString *SCValdiMarshallerToString(SCValdiMarshallerRef marshaller, NSInteger index, BOOL indent)
{
    auto str = SCValdiMarshallerGetValue(marshaller, index).toString(indent);
    return ValdiIOS::NSStringFromSTDStringView(str);
}

FOUNDATION_EXPORT void SCValdiMarshallerSwapIndexes(SCValdiMarshallerRef marshaller, NSInteger leftIndex, NSInteger rightIndex)
{
    auto *marshallerCpp = SCValdiMarshallerUnwrap(marshaller);
    marshallerCpp->swap(static_cast<int>(leftIndex), static_cast<int>(rightIndex));
    SCValdiMarshallerCppCheck(marshallerCpp);
}

FOUNDATION_EXPORT BOOL SCValdiMarshallerEquals(SCValdiMarshallerRef leftMarshaller, SCValdiMarshallerRef rightMarshaller)
{
    return *SCValdiMarshallerUnwrap(leftMarshaller) == *SCValdiMarshallerUnwrap(rightMarshaller);
}

FOUNDATION_EXPORT NSArray<id> *SCValdiMarshallerToUntypedArray(SCValdiMarshallerRef marshaller)
{
    NSInteger size = SCValdiMarshallerGetCount(marshaller);

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:size];

    for (NSInteger i = 0; i < size ; i++) {
        id nsObject = SCValdiMarshallerGetUntyped(marshaller, i);
        [array addObject:nsObject];
    }

    return array;
}
