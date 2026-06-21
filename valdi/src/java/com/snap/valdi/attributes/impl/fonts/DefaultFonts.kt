package com.snap.valdi.attributes.impl.fonts

import android.graphics.Typeface
import android.os.Build
import com.snap.valdi.utils.LoadCompletion

object DefaultFonts {

    private fun registerDefaultTypeface(fontManager: FontManager, descriptor: FontDescriptor, typeface: Typeface) {
        fontManager.register(descriptor, object: FontLoader {
            override fun load(completion: LoadCompletion<Typeface>) {
                completion.onSuccess(typeface)
            }
        })
    }

    private fun registerDefault(fontManager: FontManager, descriptor: FontDescriptor, style: Int) {
        registerDefaultTypeface(fontManager, descriptor, Typeface.defaultFromStyle(style))
    }

    private fun registerSystemWeight(fontManager: FontManager,
                                     name: String,
                                     weight: FontWeight,
                                     androidWeight: Int,
                                     italic: Boolean) {
        val style = if (italic) FontStyle.ITALIC else FontStyle.NORMAL
        registerDefaultTypeface(
                fontManager,
                FontDescriptor(name = name, family = "default", weight = weight, style = style),
                systemTypeface(androidWeight, italic))
    }

    private fun systemTypeface(weight: Int, italic: Boolean): Typeface {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return Typeface.create(Typeface.DEFAULT, weight, italic)
        }
        if (weight >= 600) {
            return Typeface.defaultFromStyle(if (italic) Typeface.BOLD_ITALIC else Typeface.BOLD)
        }
        if (weight >= 500) {
            return Typeface.create("sans-serif-medium", if (italic) Typeface.ITALIC else Typeface.NORMAL)
        }
        return Typeface.defaultFromStyle(if (italic) Typeface.ITALIC else Typeface.NORMAL)
    }

    private fun registerIfExists(fontManager: FontManager,
                                 descriptor: FontDescriptor,
                                 resName: String) {
        val context = fontManager.context
        val identifier = context.resources.getIdentifier(resName, "font", context.packageName)
        if (identifier != 0) {
            fontManager.loadSyncAndRegister(descriptor, context, identifier)
        }
    }

    fun register(fontManager: FontManager) {
        registerDefault(fontManager, FontDescriptor(name = "body", family = "default"), Typeface.NORMAL)
        registerDefault(fontManager, FontDescriptor(name = "title1", family = "default"), Typeface.NORMAL)
        registerDefault(fontManager, FontDescriptor(name = "title2", family = "default"), Typeface.NORMAL)
        registerDefault(fontManager, FontDescriptor(name = "title3", family = "default", weight = FontWeight.BOLD), Typeface.BOLD)
        registerDefault(fontManager, FontDescriptor(name = "system", family = "default"), Typeface.NORMAL)
        registerSystemWeight(fontManager, "system-medium", FontWeight.MEDIUM, 500, false)
        registerSystemWeight(fontManager, "system-semibold", FontWeight.DEMI_BOLD, 600, false)
        registerSystemWeight(fontManager, "system-demi-bold", FontWeight.DEMI_BOLD, 600, false)
        registerDefault(fontManager, FontDescriptor(name = "system-bold", family = "default", weight = FontWeight.BOLD), Typeface.BOLD)
        registerDefault(fontManager, FontDescriptor(name = "system-italic", family = "default", style = FontStyle.ITALIC), Typeface.ITALIC)
        registerSystemWeight(fontManager, "system-medium-italic", FontWeight.MEDIUM, 500, true)
        registerSystemWeight(fontManager, "system-semibold-italic", FontWeight.DEMI_BOLD, 600, true)
        registerSystemWeight(fontManager, "system-demi-bold-italic", FontWeight.DEMI_BOLD, 600, true)
        registerDefault(fontManager, FontDescriptor(name = "system-bold-italic", family = "default", weight = FontWeight.BOLD, style = FontStyle.ITALIC), Typeface.BOLD_ITALIC)

        registerIfExists(fontManager, FontDescriptor(name = "menlo-regular", family = "menlo", weight = FontWeight.NORMAL), "menlo_regular")
        registerIfExists(fontManager, FontDescriptor(name = "menlo-bold", family = "menlo", weight = FontWeight.BOLD), "menlo_bold")
    }

}
