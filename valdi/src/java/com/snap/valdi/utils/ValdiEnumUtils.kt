package com.snap.valdi.utils

import com.snap.valdi.schema.ValdiValueMarshallerRegistry

object ValdiEnumUtils {
    @JvmStatic
    fun getEnumStringValue(enumCase: Enum<*>): String {
        return ValdiValueMarshallerRegistry.shared.getEnumStringValue(enumCase::class.java, enumCase)
    }

    @JvmStatic
    fun getEnumIntValue(enumCase: Enum<*>): Int {
        return ValdiValueMarshallerRegistry.shared.getEnumIntValue(enumCase::class.java, enumCase)
    }
}
