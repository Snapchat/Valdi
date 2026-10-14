package com.snap.valdi.schema

import com.snap.valdi.utils.ValdiJNI
import com.snap.valdi.utils.ValdiMarshaller

typealias ValdiSchemaIdentifier = Int
interface ValdiValueMarshallerRegistry {

    fun marshallObject(cls: Class<*>, marshaller: ValdiMarshaller, obj: Any): Int
    fun marshallObjectAsMap(cls: Class<*>, obj: Any): Map<String, Any?>
    fun <T> unmarshallObject(cls: Class<T>, marshaller: ValdiMarshaller, objectIndex: Int): T
    fun <T> setActiveSchemaOfClassToMarshaller(cls: Class<T>, marshaller: ValdiMarshaller)
    fun getEnumStringValue(cls: Class<*>, value: Enum<*>): String
    fun getEnumIntValue(cls: Class<*>, value: Enum<*>): Int
    fun objectEquals(cls: Class<*>, left: Any, right: Any): Boolean

    fun <T: Any> copyObject(cls: Class<*>, obj: T): T

    fun disposeObject(cls: Class<*>, obj: Any)

    /**
     * Enables the batched descriptor-closure fast path in the native registry (default off). Gated by
     * a tweak and set once at startup; a no-op for backends that don't implement the optimization.
     */
    fun setDescriptorClosureEnabled(enabled: Boolean) {}

    /**
     * Enables lazy resolution of function synchronous-value return marshallers in the native registry
     * (default off): the return-type marshaller and its type closure are resolved on first call instead
     * of at root-create, keeping them off the startup critical path. Gated by a tweak and set once at startup;
     * a no-op for backends that don't implement the optimization.
     */
    fun setLazyFunctionReturnMarshallerEnabled(enabled: Boolean) {}


    companion object {
        @JvmStatic
        val shared: ValdiValueMarshallerRegistry = if (ValdiJNI.available) {
            ValdiValueMarshallerRegistryCpp()
        } else {
            ValdiValueMarshallerRegistryJava()
        }
    }

}
