package com.snap.valdi.utils

import com.snap.valdi.BigInt
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertNotEquals
import org.junit.jupiter.api.Assertions.assertSame
import org.junit.jupiter.api.Test

internal class BigIntTest {
    @Test
    fun compactValuesCompareAndHashByValue() {
        for (value in listOf(Long.MIN_VALUE, -1L, 0L, 42L, Long.MAX_VALUE)) {
            val compact = BigInt(value)
            val parsed = BigInt(value.toString())
            assertEquals(compact, parsed)
            assertEquals(compact.hashCode(), parsed.hashCode())
            assertEquals(value, mapOf(compact to value)[parsed])
        }
        assertEquals(BigInt(0L), BigInt("-000"))
        assertNotEquals(BigInt(-42L), BigInt(42L))
    }

    @Test
    fun smallValuesDoNotRequireNativeLibrary() {
        for (value in listOf(0L, -1L, 9007199254740993L, Long.MIN_VALUE, Long.MAX_VALUE)) {
            val integer = BigInt(value)
            assertEquals(value, integer.toInt64())
            assertEquals(value.toString(), integer.toString())
            assertEquals(value, BigInt(integer.toString()).toInt64())
        }
        assertEquals(0L, BigInt("-000").toInt64())
        assertEquals(123L, BigInt("+000123").toInt64())
        val marshaller = ValdiMarshallerJava()
        val integer = BigInt(Long.MIN_VALUE)
        val typedIndex = marshaller.pushBigInt(integer)
        val untypedIndex = marshaller.pushUntyped(integer)
        for (index in listOf(typedIndex, untypedIndex)) {
            assertEquals(ValdiMarshaller.ValueTypeBigInt, marshaller.getType(index))
            assertSame(integer, marshaller.getUntyped(index))
        }
    }
}
