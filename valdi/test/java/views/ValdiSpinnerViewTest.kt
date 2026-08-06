package com.snap.valdi.views

import android.graphics.Color
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class ValdiSpinnerViewTest {

    @Test
    fun setColorUpdatesSpinnerColor() {
        val spinner = ValdiSpinnerView(getApplicationContext())
        spinner.setColor(Color.RED)
        assertEquals(Color.RED, spinner.spinnerColor)
    }

    @Test
    fun resetColorRestoresWhite() {
        val spinner = ValdiSpinnerView(getApplicationContext())
        spinner.setColor(Color.BLUE)
        spinner.resetColor()
        assertEquals(Color.WHITE, spinner.spinnerColor)
    }

    @Test
    fun setColorAppliesIndeterminateTintOnLollipopAndAbove() {
        val spinner = ValdiSpinnerView(getApplicationContext())
        spinner.setColor(Color.GREEN)
        assertNotNull(spinner.indeterminateTintList)
        assertEquals(Color.GREEN, spinner.indeterminateTintList!!.defaultColor)
    }

    @Test
    fun settingColorOnTwoSpinnersDoesNotCrossContaminate() {
        val spinner1 = ValdiSpinnerView(getApplicationContext())
        val spinner2 = ValdiSpinnerView(getApplicationContext())
        spinner1.setColor(Color.RED)
        spinner2.setColor(Color.BLUE)
        assertEquals(Color.RED, spinner1.spinnerColor)
        assertEquals(Color.BLUE, spinner2.spinnerColor)
    }
}
