//
//  SCValdiMarshallableObjectRegistry.h
//  valdi-ios
//
//  Created by Simon Corsin on 5/29/20.
//

#import "valdi_core/SCMacros.h"
#import "valdi_core/SCValdiEnums.h"
#import "valdi_core/SCValdiFieldValue.h"
#import "valdi_core/SCValdiMarshallableObjectDescriptor.h"
#import "valdi_core/SCValdiMarshaller.h"
#import <Foundation/Foundation.h>

SC_EXTERN_C_BEGIN

/**
 The Marshallable object registry handles marshalling and unmarshalling
 of objects through a reflection based API.
 */
@interface SCValdiMarshallableObjectRegistry : NSObject

/**
 Register a marshallable class with the given object descriptor.
 If the class implements the class method "valdiMarshallableObjectDescriptor",
 calling this method won't be required.
 */
- (void)registerClass:(Class)objectClass objectDescriptor:(SCValdiMarshallableObjectDescriptor)objectDescriptor;

/**
 Register the given enum into the marshallable object registry.
 */
- (void)registerEnum:(id<SCValdiEnum>)valdiEnum;

/**
 Register a class that will be marshalled/unmarshalled as untyped
 */
- (void)registerUntypedClass:(Class)objectClass;

/**
 Register a protocol that will be marshalled/unmarshalled as untyped
 */
- (void)registerUntypedProtocol:(Protocol*)protocol;

/**
 Marshall the given object of the given class into the marshaller.
 Return the marshaller index.
 */
- (NSInteger)marshallObject:(id)object ofClass:(Class)objectClass toMarshaller:(SCValdiMarshallerRef)marshaller;

/**
 Marshall the given object into the marshaller.
 Return the marshaller index.
 */
- (NSInteger)marshallObject:(id)object toMarshaller:(SCValdiMarshallerRef)marshaller;

/**
 Unmarshall an object of the given class from the marshaller at the
 given index. The class must have been previously registered or must
 implement the class method "valdiMarshallableObjectDescriptor" so
 that the fields can be resolved
 */
- (id _Nullable)unmarshallObjectOfClass:(Class)objectClass
                         fromMarshaller:(SCValdiMarshallerRef)marshaller
                                atIndex:(NSInteger)index;

/**
 Set the active current schema to the given object class in the given marshaller.
 */
- (void)setSchemaOfClass:(Class)objectClass inMarshaller:(SCValdiMarshallerRef)marshaller;

/**
 Allocate the fields storage for the given class
 */
- (void* _Nullable)allocateStorageForClass:(Class)cls;

/**
 Allocate the fields storage for the given class, and populate
 the fields with the given values as va_list
*/
- (void* _Nullable)allocateStorageForClass:(Class)cls fieldValues:(va_list)fieldValues;

/**
 Deallocate the given storage for the given class, releasing any objective-c
 objects if necessary
 */
- (void)deallocateStorage:(void*)storage forClass:(Class)cls;

/**
 Create an object of the given class with an empty storage.
 */
- (id _Nullable)makeObjectOfClass:(Class)objectClass;

/**
 Create an object of the given class, with the storage populated
 with the given values as variable arguments.
 */
- (id _Nullable)makeObjectWithFieldValuesOfClass:(Class)objectClass, ...;

/**
 Force a class to be loaded inside the object registry.
 This can be call to trigger an eager load of a class.
 */
- (void)forceLoadClass:(Class)objectClass;

/**
 Returns whether the given objects are equal
 */
- (BOOL)object:(id)object equalsToObject:(id)otherObject forClass:(Class)cls;

/**
 Returns a pointer to the underlying ValueSchemaRegistry.
 Used to provide other language marshallers (Swift) with access to the ObjC registry.
 */
- (void* _Nullable)getValueSchemaRegistryPtr;

@end

SCValdiMarshallableObjectRegistry* _Nonnull SCValdiMarshallableObjectRegistryGetSharedInstance();

/**
 * Registers the protocol as a proxy protocol on the class of the given object.
 * This can be called to ensure that pushToValdiMarshaller: is implemented for
 * subclasses of generated proxy protocols.
 */
void SCValdiMarshallerObjectRegistryRegisterProxyIfNeeded(id object, Protocol* proxyProtocol);

SC_EXTERN_C_END
