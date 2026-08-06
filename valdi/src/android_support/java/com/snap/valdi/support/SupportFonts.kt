package com.snap.valdi.support

import android.graphics.Typeface
import android.os.Build
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontStyle
import com.snap.valdi.attributes.impl.fonts.FontWeight
import com.snap.valdi.support.R

object SupportFonts {

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

    @JvmStatic
    fun registerFonts(manager: ValdiRuntimeManager) {
        val fontManager = manager.fontManager
        val context = manager.context

        val regular = FontDescriptor(name = "montserrat-regular",
                family = "montserrat",
                weight = FontWeight.NORMAL)
        fontManager.loadSyncAndRegister(regular, context, R.font.montserrat_regular)

        val medium = FontDescriptor(name = "montserrat-medium",
                family = "montserrat",
                weight = FontWeight.MEDIUM)
        fontManager.loadSyncAndRegister(medium, context, R.font.montserrat_medium)

        val bold = FontDescriptor(name = "montserrat-bold",
                family = "montserrat",
                weight = FontWeight.BOLD)
        fontManager.loadSyncAndRegister(bold, context, R.font.montserrat_bold)

        val semiBold = FontDescriptor(name = "montserrat-semibold",
                family = "montserrat",
                weight = FontWeight.DEMI_BOLD)
        fontManager.loadSyncAndRegister(semiBold, context, R.font.montserrat_semi_bold)

        val monoRegular = FontDescriptor(name = "robotomono-regular",
                family = "robotomono",
                weight = FontWeight.NORMAL)
        fontManager.loadSyncAndRegister(monoRegular, context, R.font.roboto_mono_regular)

        val monoBold = FontDescriptor(name = "robotomono-bold",
                family = "robotomono",
                weight = FontWeight.BOLD)
        fontManager.loadSyncAndRegister(monoBold, context, R.font.roboto_mono_bold)

        fontManager.register(FontDescriptor("system"), Typeface.DEFAULT)
        fontManager.register(FontDescriptor("system-medium", weight = FontWeight.MEDIUM), systemTypeface(500, false))
        fontManager.register(FontDescriptor("system-semibold", weight = FontWeight.DEMI_BOLD), systemTypeface(600, false))
        fontManager.register(FontDescriptor("system-demi-bold", weight = FontWeight.DEMI_BOLD), systemTypeface(600, false))
        fontManager.register(FontDescriptor("system-bold"), Typeface.DEFAULT_BOLD)
        fontManager.register(
                FontDescriptor("system-italic", style = FontStyle.ITALIC),
                Typeface.defaultFromStyle(Typeface.ITALIC))
        fontManager.register(
                FontDescriptor("system-medium-italic", weight = FontWeight.MEDIUM, style = FontStyle.ITALIC),
                systemTypeface(500, true))
        fontManager.register(
                FontDescriptor("system-semibold-italic", weight = FontWeight.DEMI_BOLD, style = FontStyle.ITALIC),
                systemTypeface(600, true))
        fontManager.register(
                FontDescriptor("system-demi-bold-italic", weight = FontWeight.DEMI_BOLD, style = FontStyle.ITALIC),
                systemTypeface(600, true))
        fontManager.register(
                FontDescriptor("system-bold-italic", weight = FontWeight.BOLD, style = FontStyle.ITALIC),
                Typeface.defaultFromStyle(Typeface.BOLD_ITALIC))
    }
}
