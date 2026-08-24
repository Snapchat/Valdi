//
//  SCValdiMarshaller.h
//  valdi-ios
//
//  Created by Simon Corsin on 7/19/19.
//

#import "valdi_core/SCValdiInternedString.h"
#import <Foundation/Foundation.h>

@protocol SCValdiFunction;
@protocol SCValdiFunction;

@protocol SCValdiMarshallable;
@protocol SCValdiMarshallable;

#ifdef __cplusplus
extern "C" {
#endif

NS_ASSUME_NONNULL_BEGIN

struct SCValdiMarshaller;
typedef struct SCValdiMarshaller SCValdiMarshaller;
typedef SCValdiMarshaller* SCValdiMarshallerRef;

typedef _Nullable id (^SCValdiMarshallerUnmarshallBlock)(NSInteger objectIndex);
typedef void (^SCValdiMarshallerMarshallBlock)(_Nonnull id item);

typedef BOOL (^SCValdiFunctionBlock)(SCValdiMarshallerRef parameters);

typedef _Nonnull id<SCValdiMarshallable> (*SCValdiMarshallableUnmarshallFunction)(_Nonnull SCValdiMarshallerRef,
                                                                                  NSInteger);
/**
 Create a new Marshaller, which can be used to marshall/unmarshall
 data and functions inside Valdi. The returned instance MUST be
 destroyed using SCValdiMarshallerDestroy() or it will leak.
 */
SCValdiMarshallerRef SCValdiMarshallerCreate();

/**
 Destroy a Marshaller which was previously created using SCValdiMarshallerCreate().
 It is undefined behavior to call this function on a Marshaller provided from a ValdiFunction
 for instance. Only the instances explicitly created using SCValdiMarshallerCreate() should
 be destroyed.
 */
void SCValdiMarshallerDestroy(SCValdiMarshallerRef marshaller);

/**
 Execute the given block with a new Marshaller instance. The Marshaller variable name
 is exposed through the first argument of this macro. This is a convenience method which
 will ensure the Marshaller is properly destroyed, even if an exception is thrown.
 SCValdiMarshallerDestroy() should NOT be called when using this function.
 */
#define SCValdiMarshallerScoped(marshallerVarName, __block)                                                            \
    SCValdiMarshallerRef marshallerVarName = SCValdiMarshallerCreate();                                                \
    @try                                                                                                               \
        __block @finally {                                                                                             \
            SCValdiMarshallerDestroy(marshallerVarName);                                                               \
        }

NSUInteger SCValdiMarshallerGetCount(SCValdiMarshallerRef marshaller);

void SCValdiMarshallerPop(SCValdiMarshallerRef marshaller);
void SCValdiMarshallerPopCount(SCValdiMarshallerRef marshaller, NSInteger count);

NSInteger SCValdiMarshallerPushMap(SCValdiMarshallerRef marshaller, NSInteger initialCapacity);
void SCValdiMarshallerPutMapProperty(SCValdiMarshallerRef marshaller,
                                     SCValdiInternedStringRef propertyName,
                                     NSInteger index);
void SCValdiMarshallerPutMapPropertyUninterned(SCValdiMarshallerRef marshaller,
                                               NSString* propertyName,
                                               NSInteger index);
BOOL SCValdiMarshallerGetMapProperty(SCValdiMarshallerRef marshaller,
                                     SCValdiInternedStringRef propertyName,
                                     NSInteger index);
void SCValdiMarshallerMustGetMapProperty(SCValdiMarshallerRef marshaller,
                                         SCValdiInternedStringRef propertyName,
                                         NSInteger index);

NSInteger SCValdiMarshallerGetTypedObjectProperty(SCValdiMarshallerRef marshaller,
                                                  NSInteger propertyIndex,
                                                  NSInteger index);

NSInteger SCValdiMarshallerPushArray(SCValdiMarshallerRef marshaller, NSInteger capacity);
void SCValdiMarshallerSetArrayItem(SCValdiMarshallerRef marshaller, NSInteger index, NSInteger arrayIndex);

NSInteger SCValdiMarshallerPushNull(SCValdiMarshallerRef marshaller);
NSInteger SCValdiMarshallerPushUndefined(SCValdiMarshallerRef marshaller);

NSInteger SCValdiMarshallerPushString(SCValdiMarshallerRef marshaller, NSString* str);
NSInteger SCValdiMarshallerPushOptionalString(SCValdiMarshallerRef marshaller, NSString* _Nullable str);
NSInteger SCValdiMarshallerPushInternedString(SCValdiMarshallerRef marshaller, SCValdiInternedStringRef propertyName);
NSInteger SCValdiMarshallerPushFunction(SCValdiMarshallerRef marshaller, id<SCValdiFunction> function);
NSInteger SCValdiMarshallerPushFunctionWithBlock(SCValdiMarshallerRef marshaller, SCValdiFunctionBlock functionBlock);

NSInteger SCValdiMarshallerPushBool(SCValdiMarshallerRef marshaller, BOOL boolean);
NSInteger SCValdiMarshallerPushOptionalBool(SCValdiMarshallerRef marshaller, NSNumber* _Nullable boolean);

NSInteger SCValdiMarshallerPushDouble(SCValdiMarshallerRef marshaller, double d);
NSInteger SCValdiMarshallerPushOptionalDouble(SCValdiMarshallerRef marshaller, NSNumber* _Nullable d);

NSInteger SCValdiMarshallerPushInt(SCValdiMarshallerRef marshaller, int32_t i);

NSInteger SCValdiMarshallerPushLong(SCValdiMarshallerRef marshaller, int64_t l);
NSInteger SCValdiMarshallerPushOptionalLong(SCValdiMarshallerRef marshaller, NSNumber* _Nullable l);

NSInteger SCValdiMarshallerPushData(SCValdiMarshallerRef marshaller, NSData* data);
NSInteger SCValdiMarshallerPushOptionalData(SCValdiMarshallerRef marshaller, NSData* _Nullable data);

NSInteger SCValdiMarshallerPushOpaque(SCValdiMarshallerRef marshaller, id opaqueObject);
NSInteger SCValdiMarshallerPushUntyped(SCValdiMarshallerRef marshaller, _Nullable id untypedObject);

NSInteger SCValdiMarshallerPushEnumInt(SCValdiMarshallerRef marshaller, int32_t enumValue);

void SCValdiMarshallerOnPromiseComplete(SCValdiMarshallerRef marshaller,
                                        NSInteger index,
                                        SCValdiFunctionBlock callback);

NSInteger SCValdiMarshallerUnwrapProxy(SCValdiMarshallerRef marshaller, NSInteger index);

BOOL SCValdiMarshallerIsNullOrUndefined(SCValdiMarshallerRef marshaller, NSInteger index);
BOOL SCValdiMarshallerIsString(SCValdiMarshallerRef marshaller, NSInteger index);
BOOL SCValdiMarshallerIsDouble(SCValdiMarshallerRef marshaller, NSInteger index);
BOOL SCValdiMarshallerIsBool(SCValdiMarshallerRef marshaller, NSInteger index);
BOOL SCValdiMarshallerIsMap(SCValdiMarshallerRef marshaller, NSInteger index);
BOOL SCValdiMarshallerIsNullOrUndefined(SCValdiMarshallerRef marshaller, NSInteger index);
BOOL SCValdiMarshallerIsError(SCValdiMarshallerRef marshaller, NSInteger index);

/**
 Returns the outcome code of the most recent interface (proxy) marshall on the
 current thread and resets it to 0. Positive codes are successes, negative codes
 identify the failure path; see InterfaceMarshallDiagnostics.hpp for the values.
 Intended for diagnosing null results after pushing an interface object.
 */
int32_t SCValdiMarshallerGetAndResetLastInterfaceMarshallOutcome(void);

/**
 Enables or disables interface-marshall outcome recording process-wide.
 Disabled by default; while disabled, marshalls perform no outcome writes and
 SCValdiMarshallerGetAndResetLastInterfaceMarshallOutcome always returns 0.
 */
void SCValdiMarshallerSetInterfaceMarshallDiagnosticsEnabled(BOOL enabled);

BOOL SCValdiMarshallerGetBool(SCValdiMarshallerRef marshaller, NSInteger index);
NSNumber* _Nullable SCValdiMarshallerGetOptionalBool(SCValdiMarshallerRef marshaller, NSInteger index);

double SCValdiMarshallerGetDouble(SCValdiMarshallerRef marshaller, NSInteger index);
NSNumber* _Nullable SCValdiMarshallerGetOptionalDouble(SCValdiMarshallerRef marshaller, NSInteger index);

int32_t SCValdiMarshallerGetInt(SCValdiMarshallerRef marshaller, NSInteger index);

int64_t SCValdiMarshallerGetLong(SCValdiMarshallerRef marshaller, NSInteger index);
NSNumber* _Nullable SCValdiMarshallerGetOptionalLong(SCValdiMarshallerRef marshaller, NSInteger index);

NSString* SCValdiMarshallerGetString(SCValdiMarshallerRef marshaller, NSInteger index);
NSString* _Nullable SCValdiMarshallerGetOptionalString(SCValdiMarshallerRef marshaller, NSInteger index);

id<SCValdiFunction> SCValdiMarshallerGetFunction(SCValdiMarshallerRef marshaller, NSInteger index);

NSError* SCValdiMarshallerGetError(SCValdiMarshallerRef marshaller, NSInteger index);

NSInteger SCValdiMarshallerMarshallArray(SCValdiMarshallerRef marshaller,
                                         __unsafe_unretained NSArray<id>* array,
                                         NS_NOESCAPE SCValdiMarshallerMarshallBlock marshallItemBlock);

_Nullable id SCValdiMarshallerGetUntyped(SCValdiMarshallerRef marshaller, NSInteger index);
id SCValdiMarshallerGetOpaque(SCValdiMarshallerRef marshaller, NSInteger index);

id SCValdiMarshallerGetNativeWrapper(SCValdiMarshallerRef marshaller, NSInteger index);

NSString* SCValdiMarshallerToString(SCValdiMarshallerRef marshaller, NSInteger index, BOOL indent);

NSArray<id>* SCValdiMarshallerToUntypedArray(SCValdiMarshallerRef marshaller);

void SCValdiMarshallerSwapIndexes(SCValdiMarshallerRef marshaller, NSInteger leftIndex, NSInteger rightIndex);

/**
 Check the marshaller for any pending error and rethrow as Objective-C error if an error was found.
 */
void SCValdiMarshallerCheck(SCValdiMarshallerRef marshaller);

BOOL SCValdiMarshallerEquals(SCValdiMarshallerRef leftMarshaller, SCValdiMarshallerRef rightMarshaller);

BOOL SCValdiIsNull(__unsafe_unretained id object);

NS_ASSUME_NONNULL_END

#ifdef __cplusplus
}
#endif
