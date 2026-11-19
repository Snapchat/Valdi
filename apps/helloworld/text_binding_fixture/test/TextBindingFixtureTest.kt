package com.snap.valdi.helloworld.textbinding

import android.content.Context
import android.graphics.Typeface
import android.view.View
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import com.snap.valdi.NoOpValdiRuntime
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.AttributesBindingContextNative
import com.snap.valdi.attributes.ObjectAttributeHandlerDelegate
import com.snap.valdi.attributes.impl.AbstractTextViewAttributesBinder
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiRecyclableView
import com.snap.valdi.views.ValdiTextBindableView
import com.snap.valdi.views.ValdiTextHolder
import com.snapchat.client.valdi.NativeBridge
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.annotation.Implementation
import org.robolectric.annotation.Implements

/**
 * Binds the fixture views the way the runtime does: runs each binder's bindAttributes() against a
 * real AttributesBindingContext, then applies `value` through the handler the binder registered and
 * checks that the text reaches the TextView. Also checks that [TextBindingFixtureBinders] hands each
 * binder to IValdiRuntime.registerGlobalAttributesBinder: a binder that is never registered binds
 * nothing, with no build error.
 *
 * ValdiViewLoaderManager is not used: its constructor creates the C++ runtime manager over JNI, so
 * it cannot be built on the JVM. The registration tests stop at the IValdiRuntime call that
 * ValdiRuntime forwards to it.
 */
@RunWith(RobolectricTestRunner::class)
@Config(
    sdk = [28],
    manifest = Config.NONE,
    shadows = [TextBindingFixtureTest.ShadowNativeBridge::class],
    instrumentedPackages = ["com.snapchat.client.valdi"],
)
class TextBindingFixtureTest {

    /** Records attribute registrations, which would otherwise go to the C++ runtime over JNI. */
    @Implements(NativeBridge::class)
    class ShadowNativeBridge {
        companion object {
            val boundDelegates = linkedMapOf<String, Any>()
            var placeholderViewProvider: Lazy<*>? = null

            @JvmStatic
            @Implementation
            fun bindAttribute(
                bindingContextHandle: Long,
                type: Int,
                name: String,
                invalidateLayoutOnChange: Boolean,
                delegate: Any,
                compositeParts: Any?,
            ): Int {
                boundDelegates[name] = delegate
                return boundDelegates.size
            }

            @JvmStatic
            @Implementation
            fun registerAttributePreprocessor(
                bindingContextHandle: Long,
                name: String,
                enableCache: Boolean,
                preprocessor: Any,
            ) = Unit

            @JvmStatic
            @Implementation
            fun setPlaceholderViewMeasureDelegate(bindingContextHandle: Long, placeholderViewProvider: Any) {
                this.placeholderViewProvider = placeholderViewProvider as Lazy<*>
            }
        }
    }

    /** Records what is handed to IValdiRuntime.registerGlobalAttributesBinder. */
    private class RecordingRuntime : NoOpValdiRuntime() {
        val registeredBinders = mutableListOf<AttributesBinder<*>>()

        override fun <T : View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {
            registeredBinders += attributesBinder
        }

        /** Like the runtime's lookup, keyed on the binder's exact view class. */
        @Suppress("UNCHECKED_CAST")
        fun <T : View> binderFor(viewClass: Class<T>): AttributesBinder<T> {
            return registeredBinders.firstOrNull { it.viewClass == viewClass } as? AttributesBinder<T>
                ?: throw AssertionError(
                    "No binder registered for ${viewClass.simpleName}, so the runtime binds nothing to it and " +
                        "it renders blank. Registered: ${registeredBinders.map { it.javaClass.simpleName }}"
                )
        }
    }

    private object NoopLogger : Logger {
        override fun log(level: Int, message: String?) = Unit
        override fun log(level: Int, err: Throwable?, message: String?) = Unit
    }

    private val context: Context = getApplicationContext()
    private val fontManager = FontManager(context, object : TypefaceResLoader {
        override fun loadTypeface(context: Context, resId: Int): Typeface = Typeface.DEFAULT
    })

    @Before
    fun setUp() {
        ShadowNativeBridge.boundDelegates.clear()
        ShadowNativeBridge.placeholderViewProvider = null
    }

    private fun <T : View> bind(binder: AttributesBinder<T>) {
        binder.bindAttributes(AttributesBindingContext(AttributesBindingContextNative(binder.viewClass, 1L), NoopLogger))
    }

    private fun apply(view: View, attribute: String, value: Any?) {
        val delegate = ShadowNativeBridge.boundDelegates[attribute] as? ObjectAttributeHandlerDelegate
            ?: throw AssertionError("'$attribute' was not bound; bound: ${ShadowNativeBridge.boundDelegates.keys}")
        delegate.onApply(view, value, null)
    }

    private fun measure(view: View) {
        view.measure(
            View.MeasureSpec.makeMeasureSpec(600, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(160, View.MeasureSpec.EXACTLY),
        )
    }

    @Test
    fun textViewBinderBindsSharedAndCustomAttributes() {
        bind(HelloWorldTextViewAttributesBinder(context, fontManager, FontAttributes.default, NoopLogger))

        assertTrue(
            ShadowNativeBridge.boundDelegates.keys.toString(),
            ShadowNativeBridge.boundDelegates.keys.containsAll(listOf("value", "fontAttributes", "placeholder")),
        )
    }

    @Test
    fun textViewReceivesBoundText() {
        bind(HelloWorldTextViewAttributesBinder(context, fontManager, FontAttributes.default, NoopLogger))
        val view = HelloWorldTextView(context)

        apply(view, "value", "Hello from Valdi")
        apply(view, "placeholder", "Say hi")
        measure(view)

        assertEquals("Hello from Valdi", view.text.toString())
        assertEquals("Say hi", view.placeholder.toString())
    }

    @Test
    fun emojiTextViewBinderBindsTextAttributes() {
        bind(HelloWorldEmojiTextViewAttributesBinder(context, fontManager, FontAttributes.default, NoopLogger))

        assertTrue(
            ShadowNativeBridge.boundDelegates.keys.toString(),
            ShadowNativeBridge.boundDelegates.keys.containsAll(listOf("value", "fontAttributes", "textOverflow")),
        )
    }

    @Test
    fun emojiTextViewReceivesBoundText() {
        bind(HelloWorldEmojiTextViewAttributesBinder(context, fontManager, FontAttributes.default, NoopLogger))
        val view = HelloWorldEmojiTextView(context)

        apply(view, "value", "Hello 👋")
        measure(view)

        assertEquals("Hello 👋", view.text.toString())
    }

    @Test
    fun emojiTextViewBinderInstallsPlaceholderMeasureDelegate() {
        bind(HelloWorldEmojiTextViewAttributesBinder(context, fontManager, FontAttributes.default, NoopLogger))

        val provider = ShadowNativeBridge.placeholderViewProvider
        assertNotNull("bindAttributes() did not call setPlaceholderViewMeasureDelegate", provider)
        assertTrue(provider!!.value is HelloWorldEmojiTextView)
    }

    @Test
    fun registrationHandsBothBindersToTheRuntime() {
        val runtime = RecordingRuntime()

        TextBindingFixtureBinders.register(runtime, context, fontManager, FontAttributes.default, NoopLogger)

        assertTrue(runtime.binderFor(HelloWorldTextView::class.java) is HelloWorldTextViewAttributesBinder)
        assertTrue(runtime.binderFor(HelloWorldEmojiTextView::class.java) is HelloWorldEmojiTextViewAttributesBinder)
    }

    @Test
    fun registeredEmojiBinderRendersText() {
        val runtime = RecordingRuntime()
        TextBindingFixtureBinders.register(runtime, context, fontManager, FontAttributes.default, NoopLogger)

        bind(runtime.binderFor(HelloWorldEmojiTextView::class.java))
        val view = HelloWorldEmojiTextView(context)
        apply(view, "value", "Registered 👋")
        measure(view)

        assertEquals("Registered 👋", view.text.toString())
    }

    @Test
    fun emojiTextViewHasTheAppTextViewHierarchy() {
        val viewClass = HelloWorldEmojiTextView::class.java
        assertTrue("must be a TextView", TextView::class.java.isAssignableFrom(viewClass))
        assertTrue("must implement ValdiTextHolder", ValdiTextHolder::class.java.isAssignableFrom(viewClass))
        assertTrue("must implement ValdiRecyclableView", ValdiRecyclableView::class.java.isAssignableFrom(viewClass))
        assertTrue(
            "must implement ValdiTextBindableView, or Valdi text attributes cannot be bound to it",
            ValdiTextBindableView::class.java.isAssignableFrom(viewClass),
        )
        assertTrue(
            "the binder must extend AbstractTextViewAttributesBinder to bind value/fontAttributes/...",
            AbstractTextViewAttributesBinder::class.java.isAssignableFrom(HelloWorldEmojiTextViewAttributesBinder::class.java),
        )
    }
}
