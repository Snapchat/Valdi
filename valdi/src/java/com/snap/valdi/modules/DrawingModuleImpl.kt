package com.snap.valdi.modules

import android.graphics.Typeface
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextAlignment
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.exceptions.messageWithCauses
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.modules.drawing.DrawingModule
import com.snap.valdi.modules.drawing.Font
import com.snap.valdi.modules.drawing.FontSpecs
import com.snap.valdi.modules.drawing.FontStyle
import com.snap.valdi.modules.drawing.FontWeight
import com.snap.valdi.utils.CoordinateResolver
import com.snapchat.client.valdi_core.ModuleFactory
import com.snap.valdi.attributes.impl.fonts.FontWeight as FontWeightAndroid
import com.snap.valdi.attributes.impl.fonts.FontStyle as FontStyleAndroid

class DrawingModuleImpl(
    private val coordinateResolver: CoordinateResolver,
    private val fontManager: FontManager,
    private val logger: Logger
): ModuleFactory(), DrawingModule {

    override fun getModulePath(): String {
        return "Drawing"
    }

    override fun loadModule(): Any {
        return mapOf("Drawing" to this)
    }

    override fun getFont(specs: FontSpecs): Font {
        val fontName = specs.font ?: throw ValdiException("No font passed in")

        val fontAttributes = FontAttributes(null,
                0f,
                null,
                null,
                null,
                null,
                null,
                null,
                0,
                TextAlignment.LEFT,
                false,
                null,
                0F)
        fontAttributes.applyFont(fontName)

        val descriptor = FontDescriptor(fontAttributes.fontName!!)
        val typeface = try {
            fontManager.loadSynchronously(descriptor)
        } catch (exc: Exception) {
            val backupDescriptor = FontDescriptor("Helvetica")
            try {
                fontManager.loadSynchronously(backupDescriptor)
            } catch (exc: Exception) {
                throw ValdiException(exc.messageWithCauses())
            }
        }

        return DrawingModuleFontImpl(typeface,
                fontAttributes.resolvedFontSizeValue,
                specs.lineHeight,
                coordinateResolver)
    }

    override fun isFontRegistered(fontName: String): Boolean {
        val descriptor = FontDescriptor(fontName)
        return try {
            fontManager.loadSynchronously(descriptor)
            true
        } catch (exc: Exception) {
            false
        }
    }

    override fun registerFont(
        fontName: String,
        weight: FontWeight,
        style: FontStyle,
        filename: String,
    ) {
        val file = java.io.File(filename)
        if (!file.exists()) {
            logger.log(LogLevel.WARN, "Font file not found, skipping registration: $filename")
            return
        }

        val descriptor = FontDescriptor(
            name = fontName,
            weight = weight.toNative(),
            style = style.toNative()
        )

        val typeface = try {
            Typeface.createFromFile(filename)
        } catch (exc: Exception) {
            logger.log(LogLevel.ERROR, exc, "Failed to load font file: $filename")
            return
        }

        fontManager.register(descriptor, typeface)
    }

    private fun FontWeight.toNative(): FontWeightAndroid {
        return FontWeightAndroid.fromString(this.toString())
    }

    private fun FontStyle.toNative(): FontStyleAndroid {
        return FontStyleAndroid.fromString(this.toString())
    }
}
