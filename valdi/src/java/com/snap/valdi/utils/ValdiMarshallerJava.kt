package com.snap.valdi.utils

import com.snap.valdi.BigInt
import com.snap.valdi.callable.ValdiFunction
import com.snapchat.client.valdi.UndefinedValue
import com.snapchat.client.valdi.utils.CppObjectWrapper
import com.snap.valdi.exceptions.ValdiException

class ValdiMarshallerJava: ValdiMarshaller(0) {

    private val stack = mutableListOf<Any?>()

    override val size: Int
        get() = stack.size

    private fun push(item: Any?): Int {
        val index = stack.size
        stack.add(item)
        return index
    }

    private fun get(index: Int): Any? {
        return if (index >= 0) {
            stack[index]
        } else {
            return stack[stack.size + index]
        }
    }

    private fun getOrNull(index: Int): Any? {
        return if (index >= 0) {
            if (index >= stack.size) {
                null
            } else {
                stack[index]
            }
        } else {
            val resolvedIndex = stack.size + index
            return if (resolvedIndex < 0) {
                null
            } else {
                stack[resolvedIndex]
            }
        }
    }

    override fun pushMap(capacity: Int): Int {
        return push(hashMapOf<String, Any?>())
    }

    override fun pushCppObject(cppObject: CppObjectWrapper): Int {
        return push(cppObject)
    }

    override fun moveMapPropertyIntoTop(property: InternedString, index: Int): Boolean {
        return moveMapPropertyIntoTop(property.toString(), index)
    }

    override fun moveMapPropertyIntoTop(property: String, index: Int): Boolean {
        val map = get(index) as HashMap<String, Any?>

        val item = map[property] ?: return false
        push(item)
        return true
    }

    override fun moveTopItemIntoMap(property: InternedString, index: Int) {
        moveTopItemIntoMap(property.toString(), index)
    }

    override fun moveTopItemIntoMap(property: String, index: Int) {
        val map = get(index) as HashMap<String, Any?>
        map[property] = get(-1)
        pop()
    }

    private fun throwInvalidCall(): Nothing {
        throw ValdiException("This operation only works on JNI marshallers")
    }

    override fun moveTypedObjectPropertyIntoTop(propertyIndex: Int, index: Int) {
        throwInvalidCall()
    }

    override fun unwrapProxy(index: Int): Int {
        throwInvalidCall()
    }

    override fun pushList(capacity: Int): Int {
        val list = mutableListOf<Any?>()

        while (list.size < capacity) {
            list.add(null)
        }

        return push(list)
    }

    override fun setListItem(index: Int, arrayIndex: Int) {
        val list = get(index) as MutableList<Any?>

        list[arrayIndex] = get(-1)

        pop()
    }

    override fun pushString(str: String): Int {
        return push(str)
    }

    override fun setError(error: String) {
        throw ValdiException(error)
    }

    override fun checkError() {
        // no-op
    }

    override fun getError(index: Int): String {
        return (get(index) as Throwable).message!!
    }

    override fun pushInternedString(internedString: InternedString): Int {
        return push(internedString.toString())
    }

    override fun pushFunction(function: ValdiFunction): Int {
        return push(function)
    }

    override fun pushNull(): Int {
        return push(null)
    }

    override fun pushUndefined(): Int {
        return push(UndefinedValue.UNDEFINED)
    }

    override fun pushDouble(double: Double): Int {
        return push(double)
    }

    override fun pushBoolean(boolean: Boolean): Int {
        return push(boolean)
    }

    override fun pushLong(long: Long): Int {
        return push(long)
    }

    override fun pushBigInt(value: BigInt): Int {
        return push(value)
    }

    override fun pushOpaqueObject(obj: Any): Int {
        return push(obj)
    }

    override fun pushByteArray(byteArray: ByteArray): Int {
        return push(byteArray)
    }

    override fun getOpaqueObject(index: Int): Any? {
        return get(index)
    }

    override fun getBoolean(index: Int): Boolean {
        return get(index) as Boolean
    }

    override fun getInt(index: Int): Int {
        return (get(index) as Number).toInt()
    }

    override fun getLong(index: Int): Long {
        return (get(index) as Number).toLong()
    }

    override fun getDouble(index: Int): Double {
        return (get(index) as Number).toDouble()
    }

    override fun getByteArray(index: Int): ByteArray {
        return get(index) as ByteArray
    }

    override fun getString(index: Int): String {
        return get(index) as String
    }

    override fun getNativeWrapper(index: Int): CppObjectWrapper {
        return get(index) as CppObjectWrapper
    }

    override fun getFunction(index: Int): ValdiFunction {
        return get(index) as ValdiFunction
    }

    override fun pushMapIterator(index: Int): Int {
        val map = get(index) as HashMap<String, Any?>

        return push(map.iterator())
    }

    override fun pushMapIteratorNext(iteratorIndex: Int): Boolean {
        val map = get(iteratorIndex) as MutableIterator<MutableMap.MutableEntry<String, Any?>>

        if (!map.hasNext()) {
            return false
        }

        val entry = map.next()
        push(entry.key)
        push(entry.value)
        return true
    }

    override fun getListLength(index: Int): Int {
        return (get(index) as MutableList<Any?>).size
    }

    override fun getListItem(index: Int, arrayIndex: Int): Int {
        val item = (get(index) as MutableList<Any?>)[arrayIndex]
        return push(item)
    }

    override fun getType(index: Int): Int {
        val type = getOrNull(index)
        return when (type) {
            null -> ValueTypeNull
            is UndefinedValue -> ValueTypeUndefined
            is String -> ValueTypeString
            is Int -> ValueTypeInt
            is Double -> ValueTypeDouble
            is Long -> ValueTypeLong
            is BigInt -> ValueTypeBigInt
            is Boolean -> ValueTypeBool
            is Map<*, *> -> ValueTypeMap
            is Iterable<*> -> ValueTypeArray
            is Array<*> -> ValueTypeArray
            is ByteArray -> ValueTypeTypedArray
            is ValdiFunction -> ValueTypeFunction
            is Throwable -> ValueTypeError
            else -> ValueTypeWrappedObject
        }
    }

    override fun toString(index: Int, indent: Boolean): String {
        return get(index).toString()
    }

    override fun pop() {
        stack.removeAt(stack.lastIndex)
    }

    override fun pop(count: Int) {
        for (i in 0 until count) {
            pop()
        }
    }

    override fun destroyHandle(handle: Long) {
        // no actual handle
    }

    override fun equals(other: Any?): Boolean {
        if (other !is ValdiMarshallerJava) {
            return false
        }

        return stack == other.stack
    }

    override fun hashCode(): Int {
        return stack.hashCode()
    }

    override fun toString(): String {
        return "[${stack.map { it.toString() }.joinToString(", ")}]"
    }
}
