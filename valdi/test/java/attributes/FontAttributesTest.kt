package com.snap.valdi.attributes

import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test

internal class FontAttributesTest {
    @Test
    fun applyTextDecorationSupportsDashedUnderline() {
        val attributes = FontAttributes.default.copy()

        attributes.applyTextDecoration("dashed-underline")

        assertEquals(TextDecoration.DASHED_UNDERLINE, attributes.textDecoration)
    }

    @Test
    fun applyTextDecorationSupportsDottedUnderline() {
        val attributes = FontAttributes.default.copy()

        attributes.applyTextDecoration("dotted-underline")

        assertEquals(TextDecoration.DOTTED_UNDERLINE, attributes.textDecoration)
    }
}
