package com.snap.valdi.utils

import androidx.annotation.Keep
import com.snap.valdi.BigInt
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.exceptions.MarshallerException
import com.snap.valdi.callable.ValdiFunctionNative
import com.snap.valdi.exceptions.GlobalExceptionHandler
import com.snapchat.client.valdi.utils.CppObjectWrapper
import java.util.Arrays
import kotlin.collections.HashMap

@Keep
internal class ValdiMarshallerCPP: ValdiMarshaller {

    private var owned: Boolean

    private var stringToInternedStringCache: HashMap<String, InternedStringCPP>? = null
    private var internedStringToStringCache: HashMap<Long, InternedStringCPP>? = null

    constructor(): super(nativeCreate()) {
        owned = true
    }

    constructor(ptr: Long): super(ptr) {
        owned = false
    }

    private fun clearCache() {
        val stringToInternedStringCache = stringToInternedStringCache
        if (stringToInternedStringCache != null) {
            stringToInternedStringCache.values.forEach { it.destroy() }
            stringToInternedStringCache.clear()
            this.stringToInternedStringCache = null
        }

        val internedStringToStringCache = internedStringToStringCache
        if (internedStringToStringCache != null) {
            internedStringToStringCache.values.forEach { it.destroy() }
            internedStringToStringCache.clear()
            this.internedStringToStringCache = null
        }
    }

    override fun destroyHandle(handle: Long) {
        clearCache()
        if (owned) {
            owned = false
            nativeDestroy(handle)
        }
    }

    override val size: Int
        get() = nativeSize(nativeHandle)

    override fun pushMap(capacity: Int): Int = nativePushMap(nativeHandle, capacity)

    override fun moveMapPropertyIntoTop(property: InternedString, index: Int) = nativeGetMapProperty(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun moveMapPropertyIntoTop(property: String, index: Int): Boolean = moveMapPropertyIntoTop(getInternedString(property), index)

    override fun moveTypedObjectPropertyIntoTop(propertyIndex: Int, index: Int) {
        nativeGetTypedObjectProperty(nativeHandle, propertyIndex, index)
    }

    private fun getInternedString(str: String): InternedStringCPP {
        var internedStringCache = this.stringToInternedStringCache
        var internedProperty: InternedStringCPP? = null
        if (internedStringCache == null) {
            internedStringCache = hashMapOf()
            this.stringToInternedStringCache = internedStringCache
        } else {
            internedProperty = internedStringCache[str]
        }

        if (internedProperty == null) {
            internedProperty = InternedStringCPP(str, false)
            internedStringCache[str] = internedProperty
        }

        return internedProperty
    }

    override fun moveTopItemIntoMap(property: String, index: Int) = nativePutMapPropertyInterned(nativeHandle, getInternedString(property).nativeHandle, index)

    override fun moveTopItemIntoMap(property: InternedString, index: Int) = nativePutMapPropertyInterned(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun putMapPropertyString(property: InternedString, index: Int, value: String) = nativePutMapPropertyInternedString(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun putMapPropertyDouble(property: InternedString, index: Int, value: Double) = nativePutMapPropertyInternedDouble(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun putMapPropertyLong(property: InternedString, index: Int, value: Long) = nativePutMapPropertyInternedLong(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun putMapPropertyBoolean(property: InternedString, index: Int, value: Boolean) = nativePutMapPropertyInternedBoolean(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun putMapPropertyFunction(property: InternedString, index: Int, value: ValdiFunction) = nativePutMapPropertyInternedFunction(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun putMapPropertyByteArray(property: InternedString, index: Int, value: ByteArray) = nativePutMapPropertyInternedByteArray(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun putMapPropertyOpaque(property: InternedString, index: Int, value: Any) = nativePutMapPropertyInternedOpaque(nativeHandle, (property as InternedStringCPP).nativeHandle, index, value)

    override fun pushList(capacity: Int) = nativePushArray(nativeHandle, capacity)
    override fun setListItem(index: Int, arrayIndex: Int) = nativeSetArrayItem(nativeHandle, index, arrayIndex)

    override fun pushString(str: String) = nativePushString(nativeHandle, str)
    override fun pushInternedString(internedString: InternedString) = nativePushInternedString(nativeHandle, (internedString as InternedStringCPP).nativeHandle)

    override fun setError(error: String) = nativeSetError(nativeHandle, error)

    override fun checkError() = nativeCheckError(nativeHandle)

    override fun getError(index: Int): String {
        return (getUntyped(index) as Throwable).message!!
    }

    override fun pushFunction(function: ValdiFunction): Int {
        return if (function is ValdiFunctionNative) {
            nativePushCppObject(nativeHandle, function.nativeHandle)
        } else {
            nativePushFunction(nativeHandle, function)
        }
    }

    override fun pushNull() = nativePushNull(nativeHandle)
    override fun pushUndefined() = nativePushUndefined(nativeHandle)

    override fun pushDouble(double: Double) = nativePushDouble(nativeHandle, double)
    override fun pushBoolean(boolean: Boolean) = nativePushBoolean(nativeHandle, boolean)
    override fun pushLong(long: Long) = nativePushLong(nativeHandle, long)

    override fun pushBigInt(value: BigInt) = nativePushBigInt(nativeHandle, value)

    override fun pushOpaqueObject(obj: Any) = nativePushOpaqueObject(nativeHandle, obj)

    override fun pushByteArray(byteArray: ByteArray) = nativePushByteArray(nativeHandle, byteArray)

    override fun pushCppObject(cppObject: CppObjectWrapper): Int {
        return nativePushCppObject(nativeHandle, cppObject.nativeHandle)
    }

    override fun getOpaqueObject(index: Int) = nativeGetOpaqueObject(nativeHandle, index)

    override fun getBoolean(index: Int) = nativeGetBoolean(nativeHandle, index)

    override fun getInt(index: Int) = getDouble(index).toInt()
    override fun getDouble(index: Int) = nativeGetDouble(nativeHandle, index)
    override fun getLong(index: Int) = nativeGetLong(nativeHandle, index)

    override fun getByteArray(index: Int): ByteArray {
        return nativeGetByteArray(nativeHandle, index) ?: throw MarshallerException("No ByteArray at index $index")
    }

    override fun getString(index: Int) = nativeGetString(nativeHandle, index)

    override fun getStringFromInternedString(index: Int): String {
        val internedStringPtr = nativeGetInternedString(nativeHandle, index)
        if (internedStringPtr == 0L) {
            return ""
        }

        var stringCache = this.internedStringToStringCache
        var string: InternedStringCPP? = null
        if (stringCache == null) {
            stringCache = hashMapOf()
            this.internedStringToStringCache = stringCache
        } else {
            string = stringCache[internedStringPtr]
        }

        if (string == null) {
            string = InternedStringCPP(internedStringPtr)
            string.cacheJavaString()
            stringCache[internedStringPtr] = string
        }

        return string.toString()
    }

    override fun getNativeWrapper(index: Int): CppObjectWrapper {
        return (nativeGetNativeWrapper(nativeHandle, index) as? CppObjectWrapper) ?: throw MarshallerException("No NativeWrapper at index $index")
    }

    override fun getFunction(index: Int): ValdiFunction {
        return (nativeGetFunction(nativeHandle, index) as? ValdiFunction) ?: throw MarshallerException("No Function at index $index")
    }

    override fun unwrapProxy(index: Int): Int = nativeUnwrapProxy(nativeHandle, index)

    override fun getType(index: Int) = nativeGetType(nativeHandle, index)

    override fun equals(other: Any?): Boolean {
        if (other !is ValdiMarshallerCPP) {
            return false
        }
        if (this === other) {
            return true
        }

        return nativeEquals(nativeHandle, other.nativeHandle)
    }

    override fun toString(): String {
        return nativeToString(nativeHandle, true)
    }

    override fun toString(index: Int, indent: Boolean) = nativeToStringAtIndex(nativeHandle, index, indent)

    override fun pop() = nativePop(nativeHandle)
    override fun pop(count: Int) = nativePopCount(nativeHandle, count)

    override fun pushMapIterator(index: Int) = nativePushMapIterator(nativeHandle, index)

    override fun pushMapIteratorNext(iteratorIndex: Int) = nativeMapIteratorPushNext(nativeHandle, iteratorIndex)

    override fun getListLength(index: Int) = nativeGetArrayLength(nativeHandle, index)

    override fun getListItem(index: Int, arrayIndex: Int) = nativeGetArrayItem(nativeHandle, index, arrayIndex)

    override fun getListItemAndPopPrevious(index: Int, arrayIndex: Int, popPrevious: Boolean) = nativeGetArrayItemAndPopPrevious(nativeHandle, index, arrayIndex, popPrevious)

    override fun getMapPropertyDouble(property: InternedString, index: Int): Double = nativeGetMapPropertyDouble(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyLong(property: InternedString, index: Int): Long = nativeGetMapPropertyLong(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyString(property: InternedString, index: Int): String = nativeGetMapPropertyString(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyOptionalString(property: InternedString, index: Int): String? = nativeGetMapPropertyOptionalString(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyBoolean(property: InternedString, index: Int): Boolean = nativeGetMapPropertyBoolean(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyFunction(property: InternedString, index: Int) = nativeGetMapPropertyFunction(nativeHandle, (property as InternedStringCPP).nativeHandle, index) as ValdiFunction

    override fun getMapPropertyOptionalFunction(property: InternedString, index: Int) = nativeGetMapPropertyOptionalFunction(nativeHandle, (property as InternedStringCPP).nativeHandle, index) as? ValdiFunction

    override fun getMapPropertyOpaque(property: InternedString, index: Int): Any? = nativeGetMapPropertyOpaque(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyByteArray(property: InternedString, index: Int): ByteArray = nativeGetMapPropertyByteArray(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    override fun getMapPropertyOptionalByteArray(property: InternedString, index: Int): ByteArray? = nativeGetMapPropertyOptionalByteArray(nativeHandle, (property as InternedStringCPP).nativeHandle, index)

    // TODO(simon): This is still faster than the Java implementation.
    // Figure out why that is.
    override fun getUntyped(index: Int) = nativeGetUntyped(nativeHandle, index)

    companion object {

        @JvmStatic
        private val pool = arrayListOf<ValdiMarshallerCPP>()

        @JvmStatic
        fun wrap(handle: Long): ValdiMarshallerCPP {
            return synchronized(pool) {
                if (pool.isEmpty()) {
                    ValdiMarshallerCPP(handle)
                } else {
                    val marshaller = pool.removeAt(pool.lastIndex)
                    marshaller.setNativeHandle(handle)
                    marshaller
                }
            }
        }

        // ONLY Marshaller instances with UNOWNED handles should be released
        @JvmStatic
        fun release(marshaller: ValdiMarshallerCPP) {
            marshaller.setNativeHandle(0L)
            marshaller.clearCache()

            synchronized(pool) {
                pool.add(marshaller)
            }
        }

        @JvmStatic
        fun pushMarshallable(marshallable: ValdiMarshallable, marshallerHandle: Long): Int {
            return javaEntry(marshallerHandle) {
                marshallable.pushToMarshaller(it)
            }
        }

        @JvmStatic
        fun arrayToList(array: Array<Any>): Any {
            @Suppress("ReplaceJavaStaticMethodWithKotlinAnalog", "UNCHECKED_CAST")
            return Arrays.asList(*array)
        }

        @JvmStatic
        fun listToArray(list: Any): Array<Any> {
            @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN")
            val thisCollection = list as java.util.Collection<*>
            return thisCollection.toArray()
        }

        inline fun <T> javaEntry(marshallerHandle: Long, crossinline block: (ValdiMarshaller) -> T): T {
            val marshaller = wrap(marshallerHandle)

            try {
                val result = block(marshaller)
                release(marshaller)
                return result
            } catch (exc: Throwable) {
                GlobalExceptionHandler.onFatalException(exc)
            }
        }

        @JvmStatic
        private external fun nativeCreate(): Long
        @JvmStatic
        private external fun nativeDestroy(ptr: Long)

        @JvmStatic
        private external fun nativeSize(ptr: Long): Int
        @JvmStatic
        private external fun nativePushMap(ptr: Long, capacity: Int): Int
        @JvmStatic
        private external fun nativePutMapPropertyInterned(ptr: Long, propPtr: Long, index: Int)
        @JvmStatic
        private external fun nativePutMapProperty(ptr: Long, prop: String, index: Int)
        @JvmStatic
        private external fun nativeGetMapProperty(ptr: Long, propPtr: Long, index: Int): Boolean
        @JvmStatic
        private external fun nativeGetTypedObjectProperty(ptr: Long, propertyIndex: Int, index: Int): Int

        @JvmStatic
        private external fun nativeUnwrapProxy(ptr: Long, index: Int): Int

        @JvmStatic
        private external fun nativePushMapIterator(ptr: Long, index: Int): Int
        @JvmStatic
        private external fun nativeMapIteratorPushNext(ptr: Long, index: Int): Boolean
        @JvmStatic
        private external fun nativeMapIteratorPushNextAndPopPrevious(ptr: Long, index: Int, popPrevious: Boolean): Boolean

        @JvmStatic
        private external fun nativePushArray(ptr: Long, capacity: Int): Int
        @JvmStatic
        private external fun nativeSetArrayItem(ptr: Long, index: Int, arrayIndex: Int)

        @JvmStatic
        private external fun nativePushString(ptr: Long, str: String): Int
        @JvmStatic
        private external fun nativeSetError(ptr: Long, error: String)
        @JvmStatic
        private external fun nativeCheckError(ptr: Long)
        @JvmStatic
        private external fun nativePushInternedString(ptr: Long, internedStringPtr: Long): Int
        @JvmStatic
        private external fun nativePushFunction(ptr: Long, function: Any): Int
        @JvmStatic
        private external fun nativePushNull(ptr: Long): Int
        @JvmStatic
        private external fun nativePushUndefined(ptr: Long): Int
        @JvmStatic
        private external fun nativePushBoolean(ptr: Long, boolean: Boolean): Int
        @JvmStatic
        private external fun nativePushDouble(ptr: Long, double: Double): Int
        @JvmStatic
        private external fun nativePushLong(ptr: Long, long: Long): Int
        @JvmStatic
        private external fun nativePushOpaqueObject(ptr: Long, obj: Any?): Int
        @JvmStatic
        private external fun nativePushCppObject(ptr: Long, cppHandle: Long): Int
        @JvmStatic
        private external fun nativePushByteArray(ptr: Long, byteArray: ByteArray): Int

        @JvmStatic
        private external fun nativeGetType(ptr: Long, index: Int): Int

        // JNI registers jobject as java.lang.Object, so this private bridge must use Any.
        @JvmStatic
        private external fun nativePushBigInt(ptr: Long, value: Any): Int

        @JvmStatic
        private external fun nativeGetUntyped(ptr: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetBoolean(ptr: Long, index: Int): Boolean
        @JvmStatic
        private external fun nativeGetDouble(ptr: Long, index: Int): Double
        @JvmStatic
        private external fun nativeGetLong(ptr: Long, index: Int): Long
        @JvmStatic
        private external fun nativeGetString(ptr: Long, index: Int): String
        @JvmStatic
        private external fun nativeGetInternedString(ptr: Long, index: Int): Long
        @JvmStatic
        private external fun nativeGetNativeWrapper(ptr: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetOpaqueObject(ptr: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetByteArray(ptr: Long, index: Int): ByteArray?
        @JvmStatic
        private external fun nativeGetFunction(ptr: Long, index: Int): Any?

        @JvmStatic
        private external fun nativeGetArrayLength(ptr: Long, index: Int): Int
        @JvmStatic
        private external fun nativeGetArrayItem(ptr: Long, index: Int, arrayIndex: Int): Int
        @JvmStatic
        private external fun nativeGetArrayItemAndPopPrevious(ptr: Long, index: Int, arrayIndex: Int, popPrevious: Boolean): Int

        @JvmStatic
        private external fun nativeEquals(ptr: Long, otherPtr: Long): Boolean

        @JvmStatic
        private external fun nativePop(ptr: Long)
        @JvmStatic
        private external fun nativePopCount(ptr: Long, count: Int)

        @JvmStatic
        private external fun nativeToString(ptr: Long, indent: Boolean): String

        @JvmStatic
        private external fun nativeToStringAtIndex(ptr: Long, index: Int, indent: Boolean): String

        // Specialized functions, allowing to do multiple operations at once
        @JvmStatic
        private external fun nativeGetMapPropertyDouble(ptr: Long, propPtr: Long, index: Int): Double
        @JvmStatic
        private external fun nativeGetMapPropertyLong(ptr: Long, propPtr: Long, index: Int): Long
        @JvmStatic
        private external fun nativeGetMapPropertyString(ptr: Long, propPtr: Long, index: Int): String
        @JvmStatic
        private external fun nativeGetMapPropertyBoolean(ptr: Long, propPtr: Long, index: Int): Boolean
        @JvmStatic
        private external fun nativeGetMapPropertyOptionalString(ptr: Long, propPtr: Long, index: Int): String?
        @JvmStatic
        private external fun nativeGetMapPropertyFunction(ptr: Long, propPtr: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetMapPropertyOptionalFunction(ptr: Long, propPtr: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetMapPropertyOpaque(ptr: Long, propPtr: Long, index: Int): Any?
        @JvmStatic
        private external fun nativeGetMapPropertyByteArray(ptr: Long, propPtr: Long, index: Int): ByteArray
        @JvmStatic
        private external fun nativeGetMapPropertyOptionalByteArray(ptr: Long, propPtr: Long, index: Int): ByteArray?

        @JvmStatic
        private external fun nativePutMapPropertyInternedString(ptr: Long, propPtr: Long, index: Int, value: String)
        @JvmStatic
        private external fun nativePutMapPropertyInternedDouble(ptr: Long, propPtr: Long, index: Int, value: Double)
        @JvmStatic
        private external fun nativePutMapPropertyInternedLong(ptr: Long, propPtr: Long, index: Int, value: Long)
        @JvmStatic
        private external fun nativePutMapPropertyInternedBoolean(ptr: Long, propPtr: Long, index: Int, value: Boolean)
        @JvmStatic
        private external fun nativePutMapPropertyInternedFunction(ptr: Long, propPtr: Long, index: Int, value: Any?)
        @JvmStatic
        private external fun nativePutMapPropertyInternedByteArray(ptr: Long, propPtr: Long, index: Int, value: ByteArray)
        @JvmStatic
        private external fun nativePutMapPropertyInternedOpaque(ptr: Long, propPtr: Long, index: Int, value: Any)
    }
}
