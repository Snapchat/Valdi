package com.snap.valdi

import androidx.annotation.Keep

/** Immutable integer. Values in the signed Long range never own a native handle. */
@Keep
class BigInt private constructor(private val longValue: Long, private val handle: Long) {
    constructor(value: Long) : this(value, 0L)

    constructor(decimal: String) : this(parseLong(decimal), decimal)

    private constructor(value: Long?, decimal: String) : this(
        value ?: 0L,
        if (value == null) {
            nativeParse(decimal)
        } else {
            0L
        },
    )

    private fun getLongValue(): Long {
        return longValue
    }

    private fun getHandle(): Long {
        return handle
    }

    fun toInt64(): Long? {
        return if (handle == 0L) {
            longValue
        } else {
            null
        }
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is BigInt) return false
        if (handle == 0L) return other.handle == 0L && longValue == other.longValue
        return other.handle != 0L && nativeEquals(other)
    }

    override fun hashCode(): Int = if (handle == 0L) longValue.hashCode() else nativeHashCode()

    private external fun nativeEquals(other: BigInt): Boolean
    private external fun nativeHashCode(): Int

    override fun toString(): String {
        return if (handle == 0L) {
            longValue.toString()
        } else {
            nativeToString(handle)
        }
    }

    // Like NativeHandleWrapper, release only at finalization. BigInt has no mutable
    // destroy/setHandle API, so the two-field representation remains valid for its lifetime.
    protected fun finalize() {
        if (handle != 0L) {
            nativeRelease(handle)
        }
    }

    private companion object {
        private fun parseLong(decimal: String): Long? {
            val value = decimal.toLongOrNull() ?: return null
            // Kotlin accepts Unicode digits; the native decimal contract is ASCII.
            require(decimal.all { it == '+' || it == '-' || it in '0'..'9' }) {
                "Invalid BigInt decimal string"
            }
            return value
        }

        @JvmStatic private external fun nativeParse(decimal: String): Long
        @JvmStatic private external fun nativeToString(handle: Long): String
        @JvmStatic private external fun nativeRelease(handle: Long)
    }
}
