package com.snap.valdi.utils

import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test

class ValdiSVGRendererTest {

    @Test
    fun isSVG_svgPrefix_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("<svg xmlns=\"http://www.w3.org/2000/svg\"/>".toByteArray()))
    }

    @Test
    fun isSVG_xmlPrefix_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("<?xml version=\"1.0\"?><svg/>".toByteArray()))
    }

    @Test
    fun isSVG_leadingSpaces_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("   <svg/>".toByteArray()))
    }

    @Test
    fun isSVG_leadingLF_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("\n<svg/>".toByteArray()))
    }

    @Test
    fun isSVG_leadingCR_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("\r<svg/>".toByteArray()))
    }

    @Test
    fun isSVG_leadingCRLF_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("\r\n<svg/>".toByteArray()))
    }

    @Test
    fun isSVG_leadingTab_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG("\t<svg/>".toByteArray()))
    }

    @Test
    fun isSVG_mixedWhitespace_returnsTrue() {
        assertTrue(ValdiSVGRenderer.isSVG(" \t\r\n<svg/>".toByteArray()))
    }

    @Test
    fun isSVG_png_returnsFalse() {
        val pngHeader = byteArrayOf(0x89.toByte(), 0x50, 0x4E, 0x47)
        assertFalse(ValdiSVGRenderer.isSVG(pngHeader))
    }

    @Test
    fun isSVG_empty_returnsFalse() {
        assertFalse(ValdiSVGRenderer.isSVG(byteArrayOf()))
    }
}
