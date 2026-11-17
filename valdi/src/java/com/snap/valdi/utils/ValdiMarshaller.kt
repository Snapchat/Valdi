package com.snap.valdi.utils

import androidx.annotation.Keep
import com.snap.valdi.BigInt
import com.snap.valdi.actions.ValdiAction
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.exceptions.ValdiException
import com.snapchat.client.valdi.UndefinedValue
import com.snapchat.client.valdi.utils.CppObjectWrapper
import com.snapchat.client.valdi.utils.NativeHandleWrapper
import java.util.Arrays

@Keep
abstract class ValdiMarshaller(handle: Long): NativeHandleWrapper(handle) {

    abstract val size: Int

    abstract fun pushMap(capacity: Int): Int

    abstract fun moveTopItemIntoMap(property: InternedString, index: Int)
    abstract fun moveTopItemIntoMap(property: String, index: Int)
    abstract fun moveMapPropertyIntoTop(property: InternedString, index: Int): Boolean
    abstract fun moveMapPropertyIntoTop(property: String, index: Int): Boolean
    abstract fun moveTypedObjectPropertyIntoTop(propertyIndex: Int, index: Int)

    fun mustMoveMapPropertyIntoTop(property: InternedString, index: Int) {
        if (!moveMapPropertyIntoTop(property, index)) {
            throw ValdiException("Could not get property $property at index $index")
        }
    }

    inline fun putMapProperty(property: InternedString, index: Int, putItem: () -> Int) {
        putItem()
        moveTopItemIntoMap(property, index)
    }

    inline fun putMapProperty(property: String, index: Int, putItem: () -> Int) {
        putItem()
        moveTopItemIntoMap(property, index)
    }

    inline fun<T> getMapProperty(property: InternedString, index: Int, unmarshall: (Int) -> T): T {
        mustMoveMapPropertyIntoTop(property, index)
        val value = unmarshall(-1)
        pop()
        return value
    }

    inline fun<T> getTypedObjectProperty(propertyIndex: Int, index: Int, unmarshall: (Int) -> T): T {
        moveTypedObjectPropertyIntoTop(propertyIndex, index)
        val value = unmarshall(-1)
        pop()
        return value
    }

    inline fun<T> getMapPropertyOptional(property: InternedString, index: Int, unmarshall: (Int) -> T): T? {
        if (!moveMapPropertyIntoTop(property, index)) {
            return null
        }
        val value = unmarshall(-1)
        pop()
        return value
    }

    inline fun<T> getMapPropertyOptional(property: String, index: Int, unmarshall: (Int) -> T): T? {
        if (!moveMapPropertyIntoTop(property, index)) {
            return null
        }
        val value = unmarshall(-1)
        pop()
        return value
    }

    open fun getMapPropertyDouble(property: InternedString, index: Int): Double {
        return getMapProperty(property, index) { getDouble(it) }
    }

    fun getMapPropertyOptionalDouble(property: InternedString, index: Int): Double? {
        return getMapPropertyOptional(property, index) { getDouble(it) }
    }

    open fun getMapPropertyLong(property: InternedString, index: Int): Long {
        return getMapProperty(property, index) { getLong(it) }
    }

    open fun getMapPropertyOptionalLong(property: InternedString, index: Int): Long? {
        return getMapPropertyOptional(property, index) { getLong(it) }
    }

    open fun getMapPropertyString(property: InternedString, index: Int): String {
        return getMapProperty(property, index) { getString(it) }
    }

    open fun getMapPropertyOptionalString(property: InternedString, index: Int): String? {
        return getMapPropertyOptional(property, index) { getString(it) }
    }

    open fun getMapPropertyBoolean(property: InternedString, index: Int): Boolean {
        return getMapProperty(property, index) { getBoolean(it) }
    }

    fun getMapPropertyOptionalBoolean(property: InternedString, index: Int): Boolean? {
        return getMapPropertyOptional(property, index) { getBoolean(it) }
    }

    fun getMapPropertyOptionalUntyped(property: InternedString, index: Int): Any? {
        return getMapPropertyOptional(property, index) { getUntyped(it) }
    }

    open fun getMapPropertyFunction(property: InternedString, index: Int): ValdiFunction {
        return getMapProperty(property, index) { getFunction(it) }
    }

    open fun getMapPropertyOptionalFunction(property: InternedString, index: Int): ValdiFunction? {
        return getMapPropertyOptional(property, index) { getFunction(it) }
    }

    open fun getMapPropertyOpaque(property: InternedString, index: Int): Any? {
        return getMapProperty(property, index) { getOpaqueObject(it) }
    }

    fun getMapPropertyUntypedMap(property: InternedString, index: Int): Map<String, Any?> {
        return getMapProperty(property, index) { getUntypedMap(it) }
    }

    fun getMapPropertyOptionalUntypedMap(property: InternedString, index: Int): Map<String, Any?>? {
        return getMapPropertyOptional(property, index) { getUntypedMap(it) }
    }

    open fun getMapPropertyByteArray(property: InternedString, index: Int): ByteArray {
        return getMapProperty(property, index) { getByteArray(it) }
    }

    open fun getMapPropertyOptionalByteArray(property: InternedString, index: Int): ByteArray? {
        return getMapPropertyOptional(property, index) { getByteArray(it) }
    }

    inline fun <reified T>getMapPropertyList(property: InternedString, index: Int, itemFactory: (Int) -> T): List<T>? {
        return getMapProperty(property, index) { getList(it, itemFactory) }
    }

    inline fun <reified T> getMapPropertyOpaqueCasted(property: InternedString, index: Int): T {
        val opaqueObject = getMapPropertyOpaque(property, index)
        val casted = opaqueObject as? T
        if (casted == null) {
            throwCannotCast(opaqueObject, T::class.java)
        }
        return casted!!
    }

    abstract fun pushList(capacity: Int): Int
    abstract fun setListItem(index: Int, arrayIndex: Int)

    abstract fun pushString(str: String): Int
    abstract fun pushInternedString(internedString: InternedString): Int

    inline fun <T>pushOptional(optional: T?, crossinline pushItem: (T) -> Int): Int {
        return if (optional == null) {
            pushNull()
        } else {
            pushItem(optional)
        }
    }

    inline fun <T>getOptional(index: Int, crossinline getItem: (Int) -> T): T? {
        return if (isNullOrUndefined(index)) {
            return null
        } else {
            getItem(index)
        }
    }

    abstract fun pushFunction(function: ValdiFunction): Int

    abstract fun pushNull(): Int
    abstract fun pushUndefined(): Int

    // Convenience for maths that returns float
    fun pushDouble(float: Float) = pushDouble(float.toDouble())
    fun pushInt(int: Int) = pushDouble(int.toDouble())

    abstract fun pushDouble(double: Double): Int
    abstract fun pushBoolean(boolean: Boolean): Int
    abstract fun pushLong(long: Long): Int
    abstract fun pushBigInt(value: BigInt): Int

    abstract fun setError(error: String)

    abstract fun pushOpaqueObject(obj: Any): Int

    abstract fun pushByteArray(byteArray: ByteArray): Int

    abstract fun getOpaqueObject(index: Int): Any?

    abstract fun getBoolean(index: Int): Boolean

    abstract fun getInt(index: Int): Int
    abstract fun getLong(index: Int): Long
    abstract fun getDouble(index: Int): Double

    abstract fun getByteArray(index: Int): ByteArray

    open fun getStringFromInternedString(index: Int) = getString(index)
    abstract fun getString(index: Int): String

    abstract fun getError(index: Int): String

    abstract fun checkError()

    abstract fun getNativeWrapper(index: Int): CppObjectWrapper

    abstract fun getFunction(index: Int): ValdiFunction

    abstract fun unwrapProxy(index: Int): Int

    fun pushOptionalString(value: String?): Int {
        return pushOptional(value) { pushString(it) }
    }

    fun pushOptionalBoolean(value: Boolean?): Int {
        return pushOptional(value) { pushBoolean(it) }
    }

    fun pushOptionalDouble(value: Double?): Int {
        return pushOptional(value) { pushDouble(it) }
    }

    fun pushOptionalLong(value: Long?): Int {
        return pushOptional(value) { pushLong(it) }
    }

    fun pushOptionalByteArray(value: ByteArray?): Int {
        return pushOptional(value) { pushByteArray(it) }
    }

    fun getOptionalString(index: Int): String? {
        return getOptional(index) { getString(index) }
    }

    fun getOptionalDouble(index: Int): Double? {
        return getOptional(index) { getDouble(index) }
    }

    fun getOptionalLong(index: Int): Long? {
        return getOptional(index) { getLong(index) }
    }

    fun getOptionalBoolean(index: Int): Boolean? {
        return getOptional(index) { getBoolean(index) }
    }

    fun getOptionalByteArray(index: Int): ByteArray? {
        return getOptional(index) { getByteArray(index) }
    }

    fun getOptionalError(index: Int): String? {
        return getOptional(index) { getError(index) }
    }

    open fun putMapPropertyString(property: InternedString, index: Int, value: String) {
        putMapProperty(property, index) {
            pushString(value)
        }
    }

    fun putMapPropertyInt(property: InternedString, index: Int, value: Int) {
        putMapPropertyDouble(property, index, value.toDouble())
    }

    open fun putMapPropertyLong(property: InternedString, index: Int, value: Long) {
        putMapProperty(property, index) {
            pushLong(value)
        }
    }

    fun putMapPropertyFloat(property: InternedString, index: Int, value: Float) {
        putMapPropertyDouble(property, index, value.toDouble())
    }

    open fun putMapPropertyDouble(property: InternedString, index: Int, value: Double) {
        putMapProperty(property, index) {
            pushDouble(value)
        }
    }

    open fun putMapPropertyBoolean(property: InternedString, index: Int, value: Boolean) {
        putMapProperty(property, index) {
            pushBoolean(value)
        }
    }

    open fun putMapPropertyFunction(property: InternedString, index: Int, value: ValdiFunction) {
        putMapProperty(property, index) {
            pushFunction(value)
        }
    }

    open fun putMapPropertyOpaque(property: InternedString, index: Int, value: Any) {
        putMapProperty(property, index) {
            pushOpaqueObject(value)
        }
    }

    open fun putMapPropertyByteArray(property: InternedString, index: Int, value: ByteArray) {
        putMapProperty(property, index) {
            pushByteArray(value)
        }
    }

    fun putMapPropertyUntypedMap(property: InternedString, index: Int, value: Map<String, Any?>) {
        putMapProperty(property, index) {
            pushUntypedMap(value)
        }
    }

    fun putMapPropertyOptionalString(property: InternedString, index: Int, value: String?) {
        value?.let { putMapPropertyString(property, index, it) }
    }

    fun putMapPropertyOptionalDouble(property: InternedString, index: Int, value: Double?) {
        value?.let { putMapPropertyDouble(property, index, it) }
    }

    fun putMapPropertyOptionalBoolean(property: InternedString, index: Int, value: Boolean?) {
        value?.let { putMapPropertyBoolean(property, index, it) }
    }

    fun putMapPropertyOptionalLong(property: InternedString, index: Int, value: Long?) {
        value?.let { putMapPropertyLong(property, index, it) }
    }

    fun putMapPropertyOptionalUntyped(property: InternedString, index: Int, value: Any?) {
        value?.let {
            putMapProperty(property, index) { pushUntyped(value) }
        }
    }

    fun putMapPropertyOptionalUntypedMap(property: InternedString, index: Int, value: Map<String, Any?>?) {
        value?.let { putMapPropertyUntypedMap(property, index, it) }
    }

    fun putMapPropertyOptionalByteArray(property: InternedString, index: Int, value: ByteArray?) {
        value?.let { putMapPropertyByteArray(property, index, it) }
    }

    fun <T>putMapPropertyList(property: InternedString, index: Int, value: List<T>, itemMarshaller: (T) -> Int) {
        putMapProperty(property, index) {
            pushList(value, itemMarshaller)
        }
    }

    abstract fun pushMapIterator(index: Int): Int
    abstract fun pushMapIteratorNext(iteratorIndex: Int): Boolean

    open fun pushMapIteratorNextAndPopPrevious(iteratorIndex: Int, popPrevious: Boolean): Boolean {
        if (popPrevious) {
            pop(2)
        }
        return pushMapIteratorNext(iteratorIndex)
    }

    inline fun <T>getMap(index: Int, itemFactory: (Int) -> T): Map<String, T> {
        val out = hashMapOf<String, T>()

        val iteratorIndex = pushMapIterator(index)
        var hasDequeued = false
        while (pushMapIteratorNextAndPopPrevious(iteratorIndex, hasDequeued)) {
            hasDequeued = true
            val keyIndex = -2
            val itemIndex = -1

            val key = getStringFromInternedString(keyIndex)
            val item = itemFactory(itemIndex)

            out[key] = item
        }

        pop()

        return out
    }

    fun getUntypedMap(index: Int): Map<String, Any?> {
        return getMap(index) {
            getUntyped(it)
        }
    }

    fun pushUntypedMap(map: Map<String, Any?>): Int {
        return pushMap(map) {
            pushUntyped(it)
        }
    }

    fun getOptionalUntypedMap(index: Int): Map<String, Any?>? {
        return getOptional(index) { getUntypedMap(index) }
    }

    fun pushOptionalUntypedMap(map: Map<String, Any?>?): Int {
        return pushOptional(map) { pushUntypedMap(it) }
    }

    inline fun <T>pushMap(map: Map<String, T>, itemMarshaller: (T) -> Int): Int {
        val objectIndex = pushMap(map.size)

        for (entry in map.entries) {
            putMapProperty(entry.key, objectIndex) {
                itemMarshaller(entry.value)
            }
        }

        return objectIndex
    }

    abstract fun getListLength(index: Int): Int
    abstract fun getListItem(index: Int, arrayIndex: Int): Int

    open fun getListItemAndPopPrevious(index: Int, arrayIndex: Int, popPrevious: Boolean): Int {
        if (popPrevious) {
            pop()
        }

        return getListItem(index, arrayIndex)
    }

    fun getUntypedList(index: Int): List<Any?> {
        return getList(index) {
            getUntyped(it)
        }
    }

    inline fun <reified T>getList(index: Int, itemFactory: (Int) -> T): List<T> {
        val length = getListLength(index)
        if (length == 0) {
            return emptyList()
        }

        val array = Array<T>(length) {
            val itemIndex = getListItemAndPopPrevious(index, it, it > 0)

            itemFactory(itemIndex)
        }
        pop()

        return array.asList()
    }

    fun getListOfInts(index: Int): List<Int> {
        return getList(index, { getInt(it) })
    }

    fun getListOfDoubles(index: Int): List<Double> {
        return getList(index, { getDouble(it) })
    }

    fun getListOfBooleans(index: Int): List<Boolean> {
        return getList(index, { getBoolean(it) })
    }

    fun getListOfStrings(index: Int): List<String> {
        return getList(index, { getString(it) })
    }

    fun <T> getListUnreified(index: Int, itemFactory: (Int) -> T): List<T> {
        val length = getListLength(index)
        if (length == 0) {
            return emptyList()
        }

        val list = MutableList<T>(length) {
            val itemIndex = getListItemAndPopPrevious(index, it, it > 0)

            itemFactory(itemIndex)
        }
        pop()

        return list
    }

    inline fun <T>pushList(list: List<T>, itemMarshaller: (T) -> Int): Int {
        val objectIndex = pushList(list.size)

        var i = 0
        for (item in list) {
            itemMarshaller(item)
            setListItem(objectIndex, i)
            i += 1
        }

        return objectIndex
    }

    inline fun<reified T> getUntypedCasted(index: Int): T {
        val value = getUntyped(index)
        val casted = value as? T
        if (casted == null) {
            throwCannotCast(value, T::class.java)
        }
        return casted!!
    }

    abstract fun pushCppObject(cppObject: CppObjectWrapper): Int

    open fun getUntyped(index: Int): Any? {
        val type = getType(index)
        return when (type) {
            ValueTypeNull -> null
            ValueTypeUndefined -> UndefinedValue.UNDEFINED
            ValueTypeInt -> getInt(index)
            ValueTypeDouble -> getDouble(index)
            ValueTypeBool -> getBoolean(index)
            ValueTypeMap -> getUntypedMap(index)
            ValueTypeArray -> getUntypedList(index)
            ValueTypeTypedArray -> getByteArray(index)
            ValueTypeFunction -> getFunction(index)
            ValueTypeError -> getError(index)
            else -> getOpaqueObject(index)
        }
    }

    fun getOptionalUntyped(index: Int): Any? {
        return getUntyped(index)
    }

    fun pushUntyped(untyped: Any?): Int {
        return if (untyped == null) {
            pushNull()
        } else if (untyped is BigInt) {
            pushBigInt(untyped)
        } else if (untyped is String) {
            pushString(untyped)
        } else if (untyped is Boolean) {
            pushBoolean(untyped)
        } else if (untyped is Number) {
            if (untyped is Int) {
                pushInt(untyped)
            } else {
                pushDouble(untyped.toDouble())
            }
        } else if (untyped is Map<*, *>) {
            pushUntypedMap(untyped as Map<String, Any>)
        } else if (untyped is Iterable<*>) {
            if (untyped is List<*>) {
                pushList(untyped) {
                    pushUntyped(it)
                }
            } else {
                val tmp = arrayListOf<Any?>()

                for (item in untyped) {
                    tmp.add(item)
                }

                pushList(tmp) {
                    pushUntyped(it)
                }
            }
        } else if (untyped is CppObjectWrapper) {
            // This needs need to be done before the ValdiFunctionClass, as
            // ValdiFunctionNative inherits from CppObjectWrapper, so that
            // we can directly take the function wrapped in the value and
            // avoid the unnecessary bridging.
            pushCppObject(untyped)
        } else if (untyped is ValdiFunction) {
            pushFunction(untyped)
        } else if (untyped is Array<*>) {
            val index = pushList(untyped.size)

            var i = 0
            for (item in untyped) {
                pushUntyped(item)
                setListItem(index, i)
                i += 1
            }

            index
        } else if (untyped is ByteArray) {
            pushByteArray(untyped)
        } else if (untyped is UndefinedValue) {
            pushUndefined()
        } else if (untyped is ValdiMarshallable) {
            untyped.pushToMarshaller(this)
        } else {
            pushOpaqueObject(untyped)
        }
    }

    fun pushOptionalUntyped(untyped: Any?): Int {
        return pushUntyped(untyped)
    }

    abstract fun getType(index: Int): Int

    fun isString(index: Int): Boolean {
        return getType(index) == ValueTypeString
    }

    fun isNullOrUndefined(index: Int): Boolean {
        val type = getType(index)
        return type == ValueTypeNull || type == ValueTypeUndefined
    }

    fun isDouble(index: Int): Boolean {
        return getType(index) == ValueTypeDouble
    }

    fun isBoolean(index: Int): Boolean {
        return getType(index) == ValueTypeBool
    }

    fun isNumber(index: Int): Boolean {
        val type = getType(index)
        return type == ValueTypeInt || type == ValueTypeLong || type == ValueTypeDouble || type == ValueTypeBool
    }

    fun isMap(index: Int): Boolean {
        return getType(index) == ValueTypeMap
    }

    fun isList(index: Int): Boolean {
        return getType(index) == ValueTypeArray
    }

    fun isError(index: Int): Boolean {
        return getType(index) == ValueTypeError
    }

    abstract fun toString(index: Int, indent: Boolean): String

    abstract fun pop()
    abstract fun pop(count: Int)

    companion object {

        // Must match MarshallerJNI.cpp

        @JvmStatic
        val ValueTypeNull = 0
        @JvmStatic
        val ValueTypeUndefined = 1
        @JvmStatic
        val ValueTypeString = 2
        @JvmStatic
        val ValueTypeInt = 3
        @JvmStatic
        val ValueTypeLong = 4
        @JvmStatic
        val ValueTypeDouble = 5
        @JvmStatic
        val ValueTypeBool = 6
        @JvmStatic
        val ValueTypeMap = 7
        @JvmStatic
        val ValueTypeArray = 8
        @JvmStatic
        val ValueTypeTypedArray = 9
        @JvmStatic
        val ValueTypeFunction = 10
        @JvmStatic
        val ValueTypeError = 11
        @JvmStatic
        val ValueTypeTypedObject = 12
        @JvmStatic
        val ValueTypeProxyObject = 13
        @JvmStatic
        val ValueTypeWrappedObject = 14
        val ValueTypeBigInt = 15

        @Keep
        @JvmStatic
        fun create(): ValdiMarshaller {
            return if (ValdiJNI.available) {
                ValdiMarshallerCPP()
            } else {
                ValdiMarshallerJava()
            }
        }

        inline fun <T>use(crossinline closure: (ValdiMarshaller) -> T): T {
            val marshaller = create()
            val retValue = closure(marshaller)
            marshaller.destroy()
            return retValue
        }

        fun throwCannotCast(value: Any?, expectedType: Class<*>) {
            throw AttributeError("Cannot cast $value into expected type ${expectedType}")
        }
    }
}
