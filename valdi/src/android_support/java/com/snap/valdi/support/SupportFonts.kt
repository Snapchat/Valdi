package com.snap.valdi.support

import android.graphics.Typeface
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontWeight
import com.snap.valdi.support.R

object SupportFonts {

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
        fontManager.register(FontDescriptor("system-bold"), Typeface.DEFAULT_BOLD)
    }
}
