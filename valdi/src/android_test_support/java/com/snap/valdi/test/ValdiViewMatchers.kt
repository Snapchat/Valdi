package com.snap.valdi.test

import android.view.View
import android.widget.EditText
import androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom
import androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA
import com.snap.valdi.test.adapters.ValdiLegacyViewMatcherAdapter
import com.snap.valdi.test.matchers.ValdiRootViewMatcher
import org.hamcrest.Matcher
import org.hamcrest.core.AllOf.allOf

@Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
object ValdiViewMatchers {

    /**
     * Matches a View which has the given Composer attribute name and value.
     */
    @Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
    @JvmStatic
    fun <T> withValdiAttribute(attributeName: String, attributeValue: T) =
        adapt(ValdiElementMatchers.withValdiAttribute(attributeName, attributeValue))

    /**
     * Matches any Root View inflated from Composer.
     */
    @Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
    @JvmStatic
    fun withValdiRootView(): Matcher<View> {
        return ValdiRootViewMatcher()
    }

    /**
     * Matches any Composer View that has an accessibilityId attribute set to the given value
     */
    @Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
    @JvmStatic
    fun withAccessibilityId(accessibilityId: String) =
        adapt(ValdiElementMatchers.withAccessibilityId(accessibilityId))

    /**
     * Matches any Composer View that has an accessibilityId attribute value that matches the given prefix
     */
    @Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
    @JvmStatic
    fun withAccessibilityIdPrefix(accessibilityIdPrefix: String) =
        adapt(ValdiElementMatchers.withAccessibilityIdPrefix(accessibilityIdPrefix))

    /**
     * Matches the Android [EditText] backing the Valdi textfield matched by [elementMatcher].
     *
     * Use this instead of matching the element directly for Espresso text actions (`typeText`,
     * `replaceText`, `clearText`, `pressImeActionButton`). A Valdi textfield is a
     * [com.snap.valdi.views.ValdiTextViewBase] ViewGroup whose inner
     * [com.snap.valdi.views.ValdiEditTextInput] is the real EditText, so those actions' constraints
     * (`onCreateInputConnection() != null`, `isAssignableFrom(EditText)`) fail against the element's
     * own view.
     */
    @Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
    @JvmStatic
    fun withTextInputIn(elementMatcher: Matcher<View>): Matcher<View> = allOf(
        isAssignableFrom(EditText::class.java),
        isDescendantOfA(elementMatcher),
    )

    /**
     * Matches the Android [EditText] backing the Valdi textfield with the given accessibilityId.
     *
     * See [withTextInputIn].
     */
    @Deprecated("Please migrate to the ValdiElementMatchers API which is compatible with SnapDrawing")
    @JvmStatic
    fun withTextInputAccessibilityId(accessibilityId: String): Matcher<View> =
        withTextInputIn(withAccessibilityId(accessibilityId))

    @JvmStatic
    private fun adapt(matcher: Matcher<ValdiElementWithRootView>): Matcher<View> {
        return ValdiLegacyViewMatcherAdapter(matcher)
    }
}
