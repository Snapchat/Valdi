package com.snap.valdi.hello_world.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.snap.valdi.BigInt
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BigIntTest {
    @Test
    fun nativeBackedValuesCompareAndHashByValue() {
        System.loadLibrary("valdi")
        val positive = BigInt("9223372036854775808")
        val negative = BigInt("-9223372036854775809")
        val map = mapOf(positive to "positive", negative to "negative")

        assertEquals("positive", map[BigInt("+009223372036854775808")])
        assertEquals("negative", map[BigInt("-009223372036854775809")])
        assertNotEquals(positive, negative)
        assertNotEquals(positive, BigInt("9223372036854775809"))
        assertNotEquals(BigInt(Long.MAX_VALUE), positive)
        assertNotEquals(negative, BigInt(Long.MIN_VALUE))
    }
}
