package com.snap.valdi.attributes

import com.snap.valdi.attributes.impl.richtext.CustomUnderlineStyle
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextDecoration
import com.snap.valdi.exceptions.AttributeError
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertThrows
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

    @Test
    fun customUnderlineStyleParsesPatternedStyle() {
        val style = CustomUnderlineStyle.parse("1 1 1 -2")

        assertEquals(CustomUnderlineStyle(1f, 1f, 1f, -2f), style)
    }

    @Test
    fun customUnderlineStyleParsesSolidStyle() {
        val style = CustomUnderlineStyle.parse("1 0 0 -2")

        assertEquals(CustomUnderlineStyle(1f, 0f, 0f, -2f), style)
    }

    @Test
    fun customUnderlineStyleRejectsInvalidValues() {
        assertThrows(AttributeError::class.java) { CustomUnderlineStyle.parse("1 1 1 -2 3") }
        assertThrows(AttributeError::class.java) { CustomUnderlineStyle.parse("0 1 1 -2") }
        assertThrows(AttributeError::class.java) { CustomUnderlineStyle.parse("1 0 1 -2") }
        assertThrows(AttributeError::class.java) { CustomUnderlineStyle.parse("1 1 0 -2") }
        assertThrows(AttributeError::class.java) { CustomUnderlineStyle.parse("1 -1 1 -2") }
        assertThrows(AttributeError::class.java) { CustomUnderlineStyle.parse("1 nope 1 -2") }
    }
}
