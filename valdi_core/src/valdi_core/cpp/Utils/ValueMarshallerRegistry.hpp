//
//  MarshallableObjectRegistry.hpp
//  valdi_core-android
//
//  Created by Simon Corsin on 1/24/23.
//

#pragma once

#include "valdi_core/cpp/Schema/ValueSchemaRegistry.hpp"
#include "valdi_core/cpp/Schema/ValueSchemaTypeResolver.hpp"
#include "valdi_core/cpp/Utils/DefaultValueMarshallers.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/PlatformValueDelegate.hpp"
#include "valdi_core/cpp/Utils/Trace.hpp"
#include "valdi_core/cpp/Utils/ValueMarshaller.hpp"
#include "valdi_core/cpp/Utils/ValueMarshallerFunctionTrampoline.hpp"
#include <atomic>
#include <deque>

namespace Valdi {

// Process-wide toggle for lazy resolution of function synchronous-value return marshallers. It must be
// GLOBAL, not per-registry: createFunctionValueMarshaller runs on every ValueMarshallerRegistry<T>
// specialization (JavaValue for JNI marshalling, JSValueRef for the JS engine, ...), and a per-instance
// flag would only ever be set on the one instance the platform setter can reach — leaving the others
// eager and the registries non-uniform (a mixed lazy/eager registry does not render). One global read
// by all specializations keeps them uniform. Set once at startup via the COF (see
// setLazyFunctionReturnMarshallerEnabled); the inline function-local static gives a single instance
// across translation units.
inline std::atomic<bool>& lazyFunctionReturnMarshallerFlag() {
    static std::atomic<bool> flag{false};
    return flag;
}

template<typename ValueType>
struct PendingIndirectValueMarshaller {
    ValueSchemaRegistryKey schemaKey;
    ValueSchema schema;
    // Registry owning the referenced entry; used to resolve named type references inside the
    // entry when this marshaller registry's own type resolver has no registry (JS side).
    Ref<ValueSchemaRegistry> owningRegistry;
    Ref<IndirectValueMarshaller<ValueType>> indirectValueMarshaller;
    PendingIndirectValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                   const ValueSchema& schema,
                                   const Ref<ValueSchemaRegistry>& owningRegistry,
                                   const Ref<IndirectValueMarshaller<ValueType>>& indirectValueMarshaller)
        : schemaKey(schemaKey),
          schema(schema),
          owningRegistry(owningRegistry),
          indirectValueMarshaller(indirectValueMarshaller) {}
};

class ValueMarshallerRegistryListener {
public:
    virtual ~ValueMarshallerRegistryListener() = default;

    /**
     Return the schema to use for unmarshalling a protocol property.
     Some platforms, like Java, might want a different schema when unmarshalling
     protocol properties.
     */
    virtual ValueSchema getSchemaForInterfacePropertyUnmarshaller(const ValueSchema& schema) = 0;
};

template<typename ValueType>
struct ValueMarshallerWithSchema {
    Ref<ValueMarshaller<ValueType>> valueMarshaller;
    ValueSchema schema;

    ValueMarshallerWithSchema() = default;
    ValueMarshallerWithSchema(const Ref<ValueMarshaller<ValueType>>& valueMarshaller, const ValueSchema& schema)
        : valueMarshaller(valueMarshaller), schema(schema) {}
};

template<typename ValueType>
class ValueMarshallerRegistry {
public:
    ValueMarshallerRegistry(const ValueSchemaTypeResolver& typeResolver,
                            const Ref<PlatformValueDelegate<ValueType>>& delegate,
                            const Ref<IDispatchQueue>& workerQueue,
                            const StringBox& platformName)
        : _typeResolver(typeResolver),
          _delegate(delegate),
          _workerQueue(workerQueue),
          _cppToPlatformTraceName(STRING_FORMAT("Valdi.cppTo{}", platformName)),
          _platformToCppTraceName(STRING_FORMAT("Valdi.{}ToCpp", platformName.lowercased())) {}
    ~ValueMarshallerRegistry() = default;

    ValueMarshallerWithSchema<ValueType> getValueMarshaller(const ValueSchema& schema,
                                                            ExceptionTracker& exceptionTracker) {
        auto schemaKey = _typeResolver.resolveTypeReferences(schema, ValueSchemaRegistryResolveType::Key);
        if (!schemaKey) {
            exceptionTracker.onError(schemaKey.moveError());
            return ValueMarshallerWithSchema<ValueType>();
        }

        return getValueMarshaller(ValueSchemaRegistryKey(schemaKey.value()), schema, exceptionTracker);
    }

    ValueMarshallerWithSchema<ValueType> getValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                            const ValueSchema& schema,
                                                            ExceptionTracker& exceptionTracker) {
        auto result = getValueMarshallerForSchemaKey(schemaKey, schema, exceptionTracker);

        if (exceptionTracker && hasPendingIndirectValueMarshaller()) {
            flushIndirectValueMarshallers(exceptionTracker);
            if (!exceptionTracker) {
                return ValueMarshallerWithSchema<ValueType>();
            }
        }

        return result;
    }

    void registerProcessor(const ValueSchemaRegistryKey& schemaKey,
                           const Ref<ValueMarshallerProcessor<ValueType>>& processor) {
        _processorBySchemaKey[schemaKey] = processor;
    }

    const std::vector<ValueSchema>& getValueMarshallerKeys() const {
        return _valueMarshallerSchemaKeys;
    }

    void setListener(ValueMarshallerRegistryListener* listener) {
        _listener = listener;
    }

    // When enabled, a function's synchronous value return-type marshaller is resolved lazily on first
    // call instead of at root-create, keeping its type closure off the CCD critical path. Writes the
    // process-global flag (see lazyFunctionReturnMarshallerFlag) so every registry specialization sees
    // it. Set once before any marshallable class is registered (see SnapValdiRuntimeManagerFactory.create).
    void setLazyFunctionReturnMarshallerEnabled(bool enabled) {
        lazyFunctionReturnMarshallerFlag().store(enabled, std::memory_order_relaxed);
    }

private:
    ValueSchemaTypeResolver _typeResolver;
    Ref<PlatformValueDelegate<ValueType>> _delegate;
    FlatMap<ValueSchemaRegistryKey, ValueMarshallerWithSchema<ValueType>> _valueMarshallerBySchemaKey;
    FlatMap<ValueSchemaRegistryKey, Ref<ValueMarshallerProcessor<ValueType>>> _processorBySchemaKey;
    std::vector<ValueSchema> _valueMarshallerSchemaKeys;
    std::deque<PendingIndirectValueMarshaller<ValueType>> _pendingIndirectValueMarshallers;
    Ref<IDispatchQueue> _workerQueue;
    StringBox _cppToPlatformTraceName;
    StringBox _platformToCppTraceName;
    ValueMarshallerRegistryListener* _listener = nullptr;

    bool hasPendingIndirectValueMarshaller() const {
        return !_pendingIndirectValueMarshallers.empty();
    }

    void flushIndirectValueMarshallers(ExceptionTracker& exceptionTracker) {
        while (!_pendingIndirectValueMarshallers.empty()) {
            auto pendingIndirectValueMarshaller = std::move(_pendingIndirectValueMarshallers.front());
            _pendingIndirectValueMarshallers.pop_front();

            auto schema =
                resolveReferencedSchemaAgainstOwningRegistry(pendingIndirectValueMarshaller, exceptionTracker);
            if (!exceptionTracker) {
                _pendingIndirectValueMarshallers.push_front(std::move(pendingIndirectValueMarshaller));
                return;
            }

            auto resolvedValueMarshaller =
                getValueMarshallerForSchemaKey(pendingIndirectValueMarshaller.schemaKey, schema, exceptionTracker);
            if (!exceptionTracker) {
                _pendingIndirectValueMarshallers.push_front(std::move(pendingIndirectValueMarshaller));
                return;
            }

            pendingIndirectValueMarshaller.indirectValueMarshaller->setInner(
                resolvedValueMarshaller.valueMarshaller.get());
        }
    }

    // A registry whose type resolver has no ValueSchemaRegistry (the JS-side registry) cannot
    // resolve named type references itself. That used to be safe because the platform-side
    // registry eagerly resolved every referenced entry (and wrote the resolved schema back)
    // before any schema crossed to the JS side; lazy function return-marshalling breaks that
    // invariant — an entry can still contain unresolved named references when the JS side is
    // the first to walk it (the JS->Value marshall of a return value runs before the platform
    // side's unmarshall). Resolve such entries against the registry that owns the reference,
    // and write the resolved schema back so every other consumer of the entry benefits, exactly
    // as the eager platform-side path does. The owning registry's recursive lock serializes the
    // resolve+write-back against platform-side registration and lazy resolution.
    ValueSchema resolveReferencedSchemaAgainstOwningRegistry(const PendingIndirectValueMarshaller<ValueType>& pending,
                                                             ExceptionTracker& exceptionTracker) {
        if (_typeResolver.getRegistry() != nullptr || pending.owningRegistry == nullptr ||
            _valueMarshallerBySchemaKey.find(pending.schemaKey) != _valueMarshallerBySchemaKey.end()) {
            return pending.schema;
        }

        auto guard = pending.owningRegistry->lock();
        ValueSchemaTypeResolver owningResolver(pending.owningRegistry.get());
        bool didChange = false;
        auto resolvedSchema =
            owningResolver.resolveTypeReferences(pending.schema, ValueSchemaRegistryResolveType::Schema, &didChange);
        if (!resolvedSchema) {
            exceptionTracker.onError(resolvedSchema.error().rethrow(
                STRING_FORMAT("Could not resolve type references of schema key '{}' against its owning registry",
                              pending.schemaKey.getSchemaKey().toString())));
            return pending.schema;
        }

        if (didChange) {
            pending.owningRegistry->updateSchemaIfKeyExists(pending.schemaKey, resolvedSchema.value());
        }

        return resolvedSchema.value();
    }

    ValueMarshallerWithSchema<ValueType> getValueMarshallerForSchemaKey(const ValueSchemaRegistryKey& schemaKey,
                                                                        const ValueSchema& schema,
                                                                        ExceptionTracker& exceptionTracker) {
        if (schema.isSchemaReference()) {
            auto valueMarshaller =
                getValueMarshallerForSchemaReference(*schema.getSchemaReference(), schema.isOptional());
            return ValueMarshallerWithSchema<ValueType>(valueMarshaller, schema);
        }

        const auto& it = _valueMarshallerBySchemaKey.find(schemaKey);
        if (it != _valueMarshallerBySchemaKey.end()) {
            return it->second;
        }

        bool didChange = false;

        auto resolvedSchema =
            _typeResolver.resolveTypeReferences(schema, ValueSchemaRegistryResolveType::Schema, &didChange);
        if (!resolvedSchema) {
            exceptionTracker.onError(resolvedSchema.error().rethrow(STRING_FORMAT(
                "Could not resolve type references of schema key '{}'", schemaKey.getSchemaKey().toString())));
            return ValueMarshallerWithSchema<ValueType>();
        }

        if (didChange && _typeResolver.getRegistry() != nullptr) {
            // Update the schema into the registry so that other ValueMarshallerRegistrsy instances
            // can leverage the fully resolved schema without hitting the registry
            _typeResolver.getRegistry()->updateSchemaIfKeyExists(schemaKey, resolvedSchema.value());
        }

        auto valueMarshaller = createValueMarshallerForSchema(schemaKey, resolvedSchema.value(), exceptionTracker);
        if (!exceptionTracker) {
            return ValueMarshallerWithSchema<ValueType>();
        }

        auto valueMarshallerWithSchema = ValueMarshallerWithSchema<ValueType>(valueMarshaller, resolvedSchema.value());

        _valueMarshallerBySchemaKey[schemaKey] = valueMarshallerWithSchema;
        _valueMarshallerSchemaKeys.emplace_back(schemaKey.getSchemaKey());

        return valueMarshallerWithSchema;
    }

    ValueSchemaRegistryKey toSchemaKey(const ValueSchema& schema, ExceptionTracker& exceptionTracker) {
        auto schemaKey = _typeResolver.resolveTypeReferences(schema, ValueSchemaRegistryResolveType::Key);
        if (!schemaKey) {
            exceptionTracker.onError(schemaKey.error().rethrow("Could not resolve schema key"));
            return ValueSchemaRegistryKey(ValueSchema::untyped());
        }

        return ValueSchemaRegistryKey(schemaKey.value());
    }

    Ref<ValueMarshaller<ValueType>> createValueMarshallerForSchema(const ValueSchemaRegistryKey& schemaKey,
                                                                   const ValueSchema& schema,
                                                                   ExceptionTracker& exceptionTracker) {
        if (schema.isVoid()) {
            return Ref<ValueMarshaller<ValueType>>(makeShared<UndefinedValueMarshaller<ValueType>>(_delegate));
        }

        if (schema.isSchemaReference()) {
            return getValueMarshallerForSchemaReference(*schema.getSchemaReference(), schema.isOptional());
        }

        if (schema.isOptional()) {
            // Create a wrapper optional ValueMarshaller
            auto innerValue =
                getValueMarshallerForSchemaKey(ValueSchemaRegistryKey(schemaKey.getSchemaKey().asNonOptional()),
                                               schema.asNonOptional(),
                                               exceptionTracker);
            if (!exceptionTracker) {
                return nullptr;
            }

            return Ref<ValueMarshaller<ValueType>>(
                makeShared<OptionalValueMarshaller<ValueType>>(_delegate, innerValue.valueMarshaller));
        }

        if (schema.isBoolean()) {
            if (schema.isBoxed()) {
                return Ref<ValueMarshaller<ValueType>>(makeShared<BoolObjectValueMarshaller<ValueType>>(_delegate));
            } else {
                return Ref<ValueMarshaller<ValueType>>(makeShared<BoolValueMarshaller<ValueType>>(_delegate));
            }
        } else if (schema.isInteger()) {
            if (schema.isBoxed()) {
                return Ref<ValueMarshaller<ValueType>>(makeShared<IntObjectValueMarshaller<ValueType>>(_delegate));
            } else {
                return Ref<ValueMarshaller<ValueType>>(makeShared<IntValueMarshaller<ValueType>>(_delegate));
            }
        } else if (schema.isLongInteger()) {
            if (schema.isBoxed()) {
                return Ref<ValueMarshaller<ValueType>>(makeShared<LongObjectValueMarshaller<ValueType>>(_delegate));
            } else {
                return Ref<ValueMarshaller<ValueType>>(makeShared<LongValueMarshaller<ValueType>>(_delegate));
            }
        } else if (schema.isDouble()) {
            if (schema.isBoxed()) {
                return Ref<ValueMarshaller<ValueType>>(makeShared<DoubleObjectValueMarshaller<ValueType>>(_delegate));
            } else {
                return Ref<ValueMarshaller<ValueType>>(makeShared<DoubleValueMarshaller<ValueType>>(_delegate));
            }
        } else if (schema.isUntyped()) {
            return Ref<ValueMarshaller<ValueType>>(makeShared<UntypedValueMarshaller<ValueType>>(_delegate));
        } else if (schema.isString()) {
            return Ref<ValueMarshaller<ValueType>>(makeShared<StringValueMarshaller<ValueType>>(_delegate));
        } else if (schema.isValueTypedArray()) {
            return Ref<ValueMarshaller<ValueType>>(makeShared<ByteArrayValueMarshaller<ValueType>>(_delegate));
        } else if (schema.isFunction()) {
            return createFunctionValueMarshaller(schemaKey, schema, exceptionTracker);
        } else if (schema.isClass()) {
            return createClassValueMarshaller(schemaKey, schema, exceptionTracker);
        } else if (schema.isArray()) {
            return createArrayValueMarshaller(schemaKey, schema.getArrayItemSchema(), exceptionTracker);
        } else if (schema.isMap()) {
            return createMapValueMarshaller(schemaKey, *schema.getMap(), exceptionTracker);
        } else if (schema.isES6Map()) {
            return createES6MapValueMarshaller(schemaKey, *schema.getES6Map(), exceptionTracker);
        } else if (schema.isES6Set()) {
            return createES6SetValueMarshaller(schemaKey, *schema.getES6Set(), exceptionTracker);
        } else if (schema.isOutcome()) {
            return createOutcomeValueMarshaller(schemaKey, *schema.getOutcome(), exceptionTracker);
        } else if (schema.isDate()) {
            return Ref<ValueMarshaller<ValueType>>(makeShared<DateValueMarshaller<ValueType>>(_delegate));
        } else if (schema.isProto()) {
            return createProtoValueMarshaller(schemaKey, *schema.getProto(), exceptionTracker);
        } else if (schema.isEnum()) {
            return createEnumValueMarshaller(schemaKey, schema, exceptionTracker);
        } else if (schema.isPromise()) {
            return createPromiseValueMarshaller(schemaKey, schema.getPromise()->getValueSchema(), exceptionTracker);
        }

        exceptionTracker.onError(Error(STRING_FORMAT("Unsupported schema '{}'", schema.toString())));
        return nullptr;
    }

    static ValueSchemaRegistryKey getProcessorSchemaKey(const ValueSchemaRegistryKey& key) {
        // Remove generic types for processor schema key
        if (key.getSchemaKey().isGenericTypeReference()) {
            return ValueSchemaRegistryKey(
                ValueSchema::typeReference(key.getSchemaKey().getGenericTypeReference()->getType()));
        } else {
            return key;
        }
    }

    Ref<ValueMarshaller<ValueType>> getValueMarshallerForSchemaReference(const ValueSchemaReference& schemaReference,
                                                                         bool isOptional) {
        auto registryKey = ValueSchemaRegistryKey(schemaReference.getKey());
        auto indirectValueMarshaller = makeShared<IndirectValueMarshaller<ValueType>>(_delegate, isOptional);
        const auto& it = _processorBySchemaKey.find(getProcessorSchemaKey(registryKey));
        if (it != _processorBySchemaKey.end()) {
            indirectValueMarshaller->setProcessor(it->second);
        }

        _pendingIndirectValueMarshallers.emplace_back(
            registryKey, schemaReference.getSchema(), schemaReference.getOwningRegistry(), indirectValueMarshaller);
        return indirectValueMarshaller;
    }

    Ref<ValueMarshaller<ValueType>> createFunctionValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                                  const ValueSchema& schema,
                                                                  ExceptionTracker& exceptionTracker) {
        auto functionRef = schema.getFunctionRef();

        auto returnKey = toSchemaKey(functionRef->getReturnValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError(fmt::format("While processing function return value of {}: ", schema.toString()));
            return nullptr;
        }

        // Defer the return-type marshaller resolution (and its full type closure) to the first call that
        // marshalls a return value, keeping it off the root-create / CCD critical path. For a Provider<X>
        // field this leaves X's closure unresolved until the tool is actually used. The resolver goes
        // through the public getValueMarshaller so any indirect marshallers it creates are flushed.
        //
        // Scoped to SYNCHRONOUS value returns; promise and worker-dispatched returns are dispatched to a
        // queue (their marshaller closure runs later, decoupled from this call) and void has no closure
        // worth deferring, so all three resolve eagerly. The deferred resolution itself mutates the
        // registry and can be triggered from any thread (a sync value return is unmarshalled on the
        // caller's thread — e.g. a Java thread in the JNI bridge, not necessarily the JS thread), so the
        // lazy marshaller serializes its first-call resolution with registration via the schema registry
        // lock (see LazyFunctionReturnValueMarshaller::ensureInner); we pass the registry down for that.
        //
        // ┌─ LAZY_RETURN_DEFERRAL_PREDICATE v1 ─ keep in sync with the compiler ────────────────────────┐
        // │ "Deferrable" = a synchronous value return: NOT a promise, NOT void, NOT worker-dispatched.   │
        // │ This predicate is duplicated by the Swift compiler (SchemaWriter.appendFunction), which uses │
        // │ it to emit lazyReturnTypeReferences so the Android batched-descriptor walk skips exactly the │
        // │ types deferred here. The two must agree or batch/lazy stop compounding (drift only over/under-│
        // │ fetches — self-healing, never a correctness bug). If you change the rule, bump the version    │
        // │ tag in BOTH places and rebuild the prebuilt compiler archive (open_source_archives.bzl).      │
        // └──────────────────────────────────────────────────────────────────────────────────────────────┘
        auto returnSchema = functionRef->getReturnValue();
        const bool deferReturnMarshaller = lazyFunctionReturnMarshallerFlag().load(std::memory_order_relaxed) &&
                                           !returnSchema.isPromise() && !returnSchema.isVoid() &&
                                           !functionRef->getAttributes().shouldDispatchToWorkerThread();
        Ref<ValueMarshaller<ValueType>> returnValueMarshaller;
        if (deferReturnMarshaller) {
            returnValueMarshaller = makeShared<LazyFunctionReturnValueMarshaller<ValueType>>(
                _delegate,
                returnSchema.isOptional(),
                _typeResolver.getRegistry(),
                [this, returnKey, returnSchema](ExceptionTracker& et) -> Ref<ValueMarshaller<ValueType>> {
                    auto resolved = getValueMarshaller(returnKey, returnSchema, et).valueMarshaller;
                    if (!et) {
                        // Runs only on first-call resolution (off the root-create critical path), so the
                        // schema stringification is not a startup cost; it names the offending return
                        // type, which the generic marshall-time failure message otherwise omits.
                        et.onError(
                            fmt::format("While lazily resolving function return value {}: ", returnSchema.toString()));
                    }
                    return resolved;
                });
        } else {
            auto returnMarshaller = getValueMarshallerForSchemaKey(returnKey, returnSchema, exceptionTracker);
            if (!exceptionTracker) {
                exceptionTracker.onError(
                    fmt::format("While processing function return value of {}: ", schema.toString()));
                return nullptr;
            }
            returnValueMarshaller = returnMarshaller.valueMarshaller;
        }
        auto isPromiseReturnType = false;
        auto callFlags = ValueFunctionFlagsNone;
        if (functionRef->getReturnValue().isPromise()) {
            callFlags = ValueFunctionFlagsNeverCallSync;
            isPromiseReturnType = true;
        } else if (!functionRef->getReturnValue().isVoid()) {
            callFlags = static_cast<ValueFunctionFlags>(ValueFunctionFlagsCallSync | ValueFunctionFlagsPropagatesError);
        }

        auto functionTrampoline = ValueMarshallerFunctionTrampoline<ValueType>::make(
            returnValueMarshaller,
            functionRef->getParametersSize(),
            callFlags,
            isPromiseReturnType,
            (isPromiseReturnType || functionRef->getAttributes().shouldDispatchToWorkerThread()) ? _workerQueue :
                                                                                                   nullptr,
            _cppToPlatformTraceName,
            _platformToCppTraceName);

        for (size_t i = 0; i < functionRef->getParametersSize(); i++) {
            const auto& parameter = functionRef->getParameter(i);

            auto parameterKey = toSchemaKey(parameter, exceptionTracker);
            if (!exceptionTracker) {
                exceptionTracker.onError(
                    fmt::format("While processing function parameter {} of {}: ", i, schema.toString()));

                return nullptr;
            }

            auto parameterMarshaller = getValueMarshallerForSchemaKey(parameterKey, parameter, exceptionTracker);
            if (!exceptionTracker) {
                exceptionTracker.onError(
                    fmt::format("While processing function parameter {} of {}: ", i, schema.toString()));

                return nullptr;
            }

            functionTrampoline->setParameterValueMarshaller(i, parameterMarshaller.valueMarshaller);
        }

        auto functionClass = _delegate->newFunctionClass(functionTrampoline, functionRef, exceptionTracker);

        if (!exceptionTracker || functionClass == nullptr) {
            exceptionTracker.onError(
                Error(STRING_FORMAT("Could not create function class for {}: ", schema.toString())));
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(
            makeShared<FunctionValueMarshaller<ValueType>>(_delegate, std::move(functionClass)));
    }

    Ref<ValueMarshaller<ValueType>> createClassValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                               const ValueSchema& schema,
                                                               ExceptionTracker& exceptionTracker) {
        auto classSchema = schema.getClassRef();
        auto objectClass = _delegate->newObjectClass(classSchema, exceptionTracker);

        if (!exceptionTracker) {
            exceptionTracker.onError(fmt::format("Unable to resolve Object class '{}'", classSchema->getClassName()));
            return nullptr;
        }

        auto isInterface = classSchema->isInterface();

        auto objectValueMarshaller =
            ObjectValueMarshaller<ValueType>::make(_delegate, classSchema, std::move(objectClass));

        for (size_t i = 0; i < classSchema->getPropertiesSize(); i++) {
            const auto& property = classSchema->getProperty(i);

            auto propertyMarshaller =
                getPropertyValueMarshaller(*classSchema, property.name, property.schema, exceptionTracker);
            if (!exceptionTracker) {
                return nullptr;
            }

            if (isInterface && _listener != nullptr) {
                auto unmarshallSchema = _listener->getSchemaForInterfacePropertyUnmarshaller(property.schema);
                if (unmarshallSchema != property.schema) {
                    auto unbalancedMarshaller = makeShared<UnbalancedValueMarshaller<ValueType>>(_delegate);

                    auto propertyUnmarshaller =
                        getPropertyValueMarshaller(*classSchema, property.name, unmarshallSchema, exceptionTracker);
                    if (!exceptionTracker) {
                        return nullptr;
                    }

                    unbalancedMarshaller->setMarshaller(propertyMarshaller);
                    unbalancedMarshaller->setUnmarshaller(propertyUnmarshaller);

                    objectValueMarshaller->setPropertyMarshaller(i, unbalancedMarshaller);
                }
            }

            if (objectValueMarshaller->getPropertyMarshaller(i) == nullptr) {
                objectValueMarshaller->setPropertyMarshaller(i, propertyMarshaller);
            }
        }

        return Ref<ValueMarshaller<ValueType>>(objectValueMarshaller);
    }

    Ref<ValueMarshaller<ValueType>> getPropertyValueMarshaller(const ClassSchema& classSchema,
                                                               const StringBox& propertyName,
                                                               const ValueSchema& propertySchema,
                                                               ExceptionTracker& exceptionTracker) {
        auto propertyKey = toSchemaKey(propertySchema, exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError(
                fmt::format("While processing property '{}' of {}: ", propertyName, classSchema.getClassName()));
            return nullptr;
        }

        auto propertyMarshaller = getValueMarshallerForSchemaKey(propertyKey, propertySchema, exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError(
                fmt::format("While processing property '{}' of {}: ", propertyName, classSchema.getClassName()));
            return nullptr;
        }

        return propertyMarshaller.valueMarshaller;
    }

    Ref<ValueMarshaller<ValueType>> createEnumValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                              const ValueSchema& schema,
                                                              ExceptionTracker& exceptionTracker) {
        auto enumSchema = schema.getEnumRef();
        auto enumClass = _delegate->newEnumClass(enumSchema, exceptionTracker);

        if (!exceptionTracker) {
            exceptionTracker.onError(fmt::format("Unable to resolve Enum class '{}'", enumSchema->getName()));
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(
            makeShared<EnumValueMarshaller<ValueType>>(_delegate, enumSchema, std::move(enumClass), schema.isBoxed()));
    }

    Ref<ValueMarshaller<ValueType>> createPromiseValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                                 const ValueSchema& valueSchema,
                                                                 ExceptionTracker& exceptionTracker) {
        auto itemKey = toSchemaKey(valueSchema, exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing array item type: ");
            return nullptr;
        }

        auto valueMarshallerWithSchema = getValueMarshallerForSchemaKey(itemKey, valueSchema, exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing array item type: ");
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(
            makeShared<PromiseValueMarshaller<ValueType>>(_delegate, valueMarshallerWithSchema.valueMarshaller));
    }

    Ref<ValueMarshaller<ValueType>> createArrayValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                               const ValueSchema& itemSchema,
                                                               ExceptionTracker& exceptionTracker) {
        auto itemKey = toSchemaKey(itemSchema, exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing array item type: ");
            return nullptr;
        }

        auto itemValueMarshaller = getValueMarshallerForSchemaKey(itemKey, itemSchema, exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing array item type: ");
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(
            makeShared<ArrayValueMarshaller<ValueType>>(_delegate, itemValueMarshaller.valueMarshaller));
    }

    Ref<ValueMarshaller<ValueType>> createMapValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                             const MapSchema& mapSchema,
                                                             ExceptionTracker& exceptionTracker) {
        auto mapKey = toSchemaKey(mapSchema.getKey(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map key type: ");
            return nullptr;
        }

        auto mapValue = toSchemaKey(mapSchema.getValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map value type: ");
            return nullptr;
        }

        auto keyValueMarshaller = getValueMarshallerForSchemaKey(mapKey, mapSchema.getKey(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map key type: ");
            return nullptr;
        }

        auto valueValueMarshaller = getValueMarshallerForSchemaKey(mapValue, mapSchema.getValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map value type: ");
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(makeShared<MapValueMarshaller<ValueType>>(
            _delegate, keyValueMarshaller.valueMarshaller, valueValueMarshaller.valueMarshaller));
    }

    Ref<ValueMarshaller<ValueType>> createES6MapValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                                const ES6MapSchema& mapSchema,
                                                                ExceptionTracker& exceptionTracker) {
        auto mapKey = toSchemaKey(mapSchema.getKey(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map key type: ");
            return nullptr;
        }

        auto mapValue = toSchemaKey(mapSchema.getValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map value type: ");
            return nullptr;
        }

        auto keyValueMarshaller = getValueMarshallerForSchemaKey(mapKey, mapSchema.getKey(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map key type: ");
            return nullptr;
        }

        auto valueValueMarshaller = getValueMarshallerForSchemaKey(mapValue, mapSchema.getValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing map value type: ");
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(makeShared<ES6MapValueMarshaller<ValueType>>(
            _delegate, keyValueMarshaller.valueMarshaller, valueValueMarshaller.valueMarshaller));
    }

    Ref<ValueMarshaller<ValueType>> createES6SetValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                                const ES6SetSchema& setSchema,
                                                                ExceptionTracker& exceptionTracker) {
        auto setKey = toSchemaKey(setSchema.getKey(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing set key type: ");
            return nullptr;
        }

        auto keyValueMarshaller = getValueMarshallerForSchemaKey(setKey, setSchema.getKey(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing set key type: ");
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(
            makeShared<ES6SetValueMarshaller<ValueType>>(_delegate, keyValueMarshaller.valueMarshaller));
    }

    Ref<ValueMarshaller<ValueType>> createOutcomeValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                                 const OutcomeSchema& outcomeSchema,
                                                                 ExceptionTracker& exceptionTracker) {
        auto outcomeValue = toSchemaKey(outcomeSchema.getValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing outcome value type: ");
            return nullptr;
        }

        auto outcomeError = toSchemaKey(outcomeSchema.getError(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing outcome error type: ");
            return nullptr;
        }

        auto valueValueMarshaller =
            getValueMarshallerForSchemaKey(outcomeValue, outcomeSchema.getValue(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing outcome value type: ");
            return nullptr;
        }

        auto errorValueMarshaller =
            getValueMarshallerForSchemaKey(outcomeError, outcomeSchema.getError(), exceptionTracker);
        if (!exceptionTracker) {
            exceptionTracker.onError("While processing outcome error type: ");
            return nullptr;
        }

        return Ref<ValueMarshaller<ValueType>>(makeShared<OutcomeValueMarshaller<ValueType>>(
            _delegate, valueValueMarshaller.valueMarshaller, errorValueMarshaller.valueMarshaller));
    }

    Ref<ValueMarshaller<ValueType>> createProtoValueMarshaller(const ValueSchemaRegistryKey& schemaKey,
                                                               const ProtoSchema& protoSchema,
                                                               ExceptionTracker& exceptionTracker) {
        return Ref<ValueMarshaller<ValueType>>(
            makeShared<ProtoValueMarshaller<ValueType>>(_delegate, protoSchema.classNameParts()));
    }
};

} // namespace Valdi
