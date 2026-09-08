package com.snap.valdi.attributes

import android.content.Context
import android.graphics.Typeface
import android.os.Build
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.attributes.impl.AbstractTextViewAttributesBinder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiTextBindableView
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
internal class TextViewAttributesBinderTextShadowTest {

    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    /** A bare TextView that binds as itself; `textShadow` never touches the TextViewHelper. */
    private class BindableTextView(context: Context) : TextView(context), ValdiTextBindableView {
        override val bindingTextView: TextView
            get() = this

        override fun getOrCreateTextViewHelper(
            fontManager: FontManager,
            defaultAttributes: FontAttributes,
            valueAttributeId: Int,
            logger: Logger,
        ): TextViewHelper = throw UnsupportedOperationException("not needed for textShadow")
    }

    private class TestBinder(
        context: Context,
        override val textShadowSupported: Boolean,
    ) : AbstractTextViewAttributesBinder<BindableTextView>(
        context,
        FontManager(context, object : TypefaceResLoader {
            override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
        }),
        FontAttributes.default,
        NoopLogger,
    ) {
        override val viewClass: Class<BindableTextView>
            get() = BindableTextView::class.java

        override fun bindAttributes(attributesBindingContext: AttributesBindingContext<BindableTextView>) {
            bindTextAttributes(attributesBindingContext)
        }
    }

    // color, radius, opacity, offsetX, offsetY: the array the runtime's textShadow preprocessor hands the binder.
    private val shadow = arrayOf<Any?>(0x000000FFL, 4.0, 1.0, 1.0, 2.0)

    @Test
    fun applyTextShadowSetsShadowLayerWhereSupported() {
        val context = getApplicationContext<Context>()
        val view = BindableTextView(context)

        TestBinder(context, textShadowSupported = true).applyTextShadow(view, shadow, null)

        assertTrue(view.shadowRadius > 0f)
    }

    @Test
    fun applyTextShadowDropsShadowLayerWhereUnsupported() {
        val context = getApplicationContext<Context>()
        val view = BindableTextView(context)
        view.setShadowLayer(3f, 1f, 1f, 0xFF000000.toInt())

        TestBinder(context, textShadowSupported = false).applyTextShadow(view, shadow, null)

        assertEquals(0f, view.shadowRadius, 0f)
    }

    @Test
    fun textShadowIsUnsupportedThroughApi23() {
        assertFalse(AbstractTextViewAttributesBinder.isTextShadowSupported(Build.VERSION_CODES.LOLLIPOP))
        assertFalse(AbstractTextViewAttributesBinder.isTextShadowSupported(Build.VERSION_CODES.M))
        assertTrue(AbstractTextViewAttributesBinder.isTextShadowSupported(Build.VERSION_CODES.N))
    }
}
