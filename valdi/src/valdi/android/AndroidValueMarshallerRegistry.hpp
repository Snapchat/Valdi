//
//  AndroidValueMarshallerRegistry.hpp
//  valdi-android
//
//  Created by Simon Corsin on 2/14/23.
//

#pragma once

#include "valdi/android/JavaValueDelegate.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/ValueMarshallerRegistry.hpp"
#include "valdi_core/jni/JavaValue.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Valdi {
class ValueSchemaRegistry;
class Marshaller;
class ValueSchema;
} // namespace Valdi

namespace ValdiAndroid {

class JavaClassDelegate;
class JavaObjectClassDelegate;
class JavaInterfaceClassDelegate;
class JavaEnumClassDelegate;
class JavaUntypedClassDelegate;
class JavaClass;
class ValueSchemaRegistryListenerImpl;

class FieldNameMap {
public:
    FieldNameMap();
    ~FieldNameMap();

    enum class FieldType {
        Class,
        InterfaceMethod,
        InterfaceProperty,
        EnumCaseValue,
    };

    Valdi::StringBox getFieldName(size_t fieldIndex, const Valdi::StringBox& propertyName, FieldType fieldType) const;

    void appendFieldName(size_t fieldIndex, const Valdi::StringBox& fieldName);

private:
    Valdi::FlatMap<size_t, Valdi::StringBox> _map;
};

/**
 Android Integration of the ValueSchemaRegistry and ValueMarshallerRegistry.
 Allows marshalling and unmarshalling of Java objects as ValueTypedObject and
 ValueTypedProxyObject instances.
 */
class AndroidValueMarshallerRegistry : public Valdi::SimpleRefCountable,
                                       public JavaTypesResolver,
                                       public Valdi::ValueMarshallerRegistryListener {
public:
    AndroidValueMarshallerRegistry();
    ~AndroidValueMarshallerRegistry() override;

    Valdi::Result<Valdi::Value> marshallObject(const Valdi::StringBox& className, const JavaValue& object);

    Valdi::Result<JavaValue> unmarshallObject(const Valdi::StringBox& className, const Valdi::Value& value);

    Valdi::Result<Valdi::Void> setActiveSchemaInMarshaller(const Valdi::StringBox& className,
                                                           Valdi::Marshaller& marshaller);

    Valdi::Result<Valdi::Value> getEnumValue(const Valdi::StringBox& className, const JavaValue& enumValue);

    Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> getObjectClassDelegateForName(
        const Valdi::StringBox& className) final;

    Valdi::Ref<Valdi::PlatformObjectClassDelegate<JavaValue>> getInterfaceClassDelegateForName(
        const Valdi::StringBox& className) final;

    Valdi::Ref<Valdi::PlatformEnumClassDelegate<JavaValue>> getEnumClassDelegateForName(
        const Valdi::StringBox& className) final;

    Valdi::ValueSchema getSchemaForInterfacePropertyUnmarshaller(const Valdi::ValueSchema& schema) final;

    // Enables the batched getDescriptorClosure fast path in getOrCreateRegisteredMarshallableClass.
    // Default off: the registry uses the legacy per-class descriptor fetch until the app opts in
    // (tweak-gated, set once at startup). See ValdiValueMarshallerRegistry.setDescriptorClosureEnabled.
    void setDescriptorClosureEnabled(bool enabled);

    // Enables lazy resolution of function synchronous-value return marshallers (deferred to first call
    // instead of root-create). Default off; set once at startup via the runtime tweak. See
    // ValueMarshallerRegistry.setLazyFunctionReturnMarshallerEnabled.
    void setLazyFunctionReturnMarshallerEnabled(bool enabled);

private:
    // Parsed form of a class's marshalling descriptor, sourced either from the legacy per-class JNI
    // call or from a batched getDescriptorClosure ByteBuffer. Holds the data C++ needs to register the
    // class delegate, with type references and proxy class as names (resolved on demand).
    struct ParsedDescriptor {
        int type = 0;
        std::string schema;
        std::string propertyReplacements;
        std::string proxyClassName; // empty unless interface
        std::vector<std::string> typeReferenceNames;
    };

    Valdi::Ref<Valdi::ValueSchemaRegistry> _schemaRegistry;
    Valdi::ValueMarshallerRegistry<JavaValue> _valueMarshallerRegistry;
    Valdi::FlatMap<Valdi::StringBox, Valdi::Ref<RefCountable>> _registeredClassByName;
    // Descriptors prefetched via getDescriptorClosure (a class + its transitive references), keyed by
    // class name. Lets reference recursion resolve descriptors without a per-class JNI round-trip.
    // Java prunes its walk using its own persistent set of already-packed classes, so this cache is
    // not echoed back to it (see ValdiMarshallableObjectDescriptor.resolvedClassNames).
    Valdi::FlatMap<Valdi::StringBox, ParsedDescriptor> _descriptorByName;
    // Gates the batched getDescriptorClosure fast path. Off by default; flipped on via
    // setDescriptorClosureEnabled when the tweak is enabled. When off, only the legacy path runs.
    bool _descriptorClosureEnabled = false;

    struct RegisteredSchema {
        Valdi::ValueSchemaRegistrySchemaIdentifier identifier;
        Valdi::ValueSchema schema;

        inline RegisteredSchema(Valdi::ValueSchemaRegistrySchemaIdentifier identifier, const Valdi::ValueSchema& schema)
            : identifier(identifier), schema(schema) {}
    };

    friend ValueSchemaRegistryListenerImpl;

    Valdi::Result<RegisteredSchema> parseAndRegisterSchema(const std::string& prefix,
                                                           const std::string_view& suffix,
                                                           const std::vector<std::string>& typeReferenceNames);

    // Fetch (if needed) and cache the descriptor closure rooted at [className], returning the parsed
    // descriptor for [className] itself, or null if the batch did not contain it (caller falls back to
    // the legacy per-class JNI path).
    const ParsedDescriptor* getOrFetchDescriptorClosure(const Valdi::StringBox& className);

    // Parse a getDescriptorClosure ByteBuffer payload into _descriptorByName.
    void parseDescriptorClosure(const uint8_t* data, size_t size);

    // Register a class delegate from already-parsed descriptor pieces (shared by the batched and the
    // legacy paths). [proxyJavaClass] is non-null only for interfaces.
    Valdi::Result<JavaClassDelegate*> registerParsedDescriptor(const Valdi::StringBox& className,
                                                               const JavaClass& javaClass,
                                                               int type,
                                                               const std::string_view& schemaSuffix,
                                                               const FieldNameMap& fieldMap,
                                                               const JavaClass* proxyJavaClass,
                                                               const std::vector<std::string>& typeReferenceNames);

    Valdi::ValueSchemaRegistrySchemaIdentifier registerUntyped(const Valdi::StringBox& className);

    Valdi::Result<JavaClassDelegate*> getLoadedClassDelegateForClassName(const Valdi::StringBox& className);

    Valdi::Result<JavaClassDelegate*> getOrCreateRegisteredMarshallableClass(const Valdi::StringBox& className);

    static FieldNameMap toFieldMap(const std::string& propertyReplacements);

    Valdi::Result<JavaClassDelegate*> registerObjectClassDelegate(const Valdi::StringBox& className,
                                                                  const JavaClass& javaClass,
                                                                  const std::string_view& schemaSuffix,
                                                                  const FieldNameMap& fieldMap,
                                                                  const std::vector<std::string>& typeReferenceNames);

    Valdi::Result<JavaClassDelegate*> registerInterfaceClassDelegate(
        const Valdi::StringBox& className,
        const JavaClass& javaClass,
        const std::string_view& schemaSuffix,
        const FieldNameMap& fieldMap,
        const JavaClass& proxyJavaClass,
        const std::vector<std::string>& typeReferenceNames);

    Valdi::Result<JavaClassDelegate*> registerStringEnumClassDelegate(const Valdi::StringBox& className,
                                                                      const JavaClass& javaClass,
                                                                      const std::string_view& schemaSuffix,
                                                                      const FieldNameMap& fieldMap);

    Valdi::Result<JavaClassDelegate*> registerIntEnumClassDelegate(const Valdi::StringBox& className,
                                                                   const JavaClass& javaClass,
                                                                   const std::string_view& schemaSuffix,
                                                                   const FieldNameMap& fieldMap);

    Valdi::Result<JavaClassDelegate*> registerEnumClassDelegate(char enumType,
                                                                const Valdi::StringBox& className,
                                                                const JavaClass& javaClass,
                                                                const std::string_view& schemaSuffix,
                                                                const FieldNameMap& fieldMap);

    Valdi::Result<JavaClassDelegate*> registerUntypedClassDelegate(const Valdi::StringBox& className,
                                                                   const JavaClass& javaClass);
};

} // namespace ValdiAndroid
