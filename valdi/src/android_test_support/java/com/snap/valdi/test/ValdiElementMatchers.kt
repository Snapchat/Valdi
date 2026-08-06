package com.snap.valdi.test

import android.content.Context
import android.view.View
import androidx.test.core.app.ApplicationProvider
import com.snap.valdi.test.adapters.EspressoViewMatcherAdapter
import com.snap.valdi.test.adapters.EspressoViewMatcherToElementMatcher
import com.snap.valdi.test.matchers.AncestorElementMatcher
import com.snap.valdi.test.matchers.ChildElementMatcher
import com.snap.valdi.test.matchers.ValdiAttributeValueMatcher
import com.snap.valdi.test.matchers.ValdiIndexMatcher
import com.snap.valdi.test.matchers.ValdiResourceAttributeValueMatcher
import com.snap.valdi.test.matchers.ValdiScrollOffsetMatcher
import com.snap.valdi.test.matchers.DescendantElementMatcher
import com.snap.valdi.test.matchers.ElementTypeMatcher
import com.snap.valdi.test.matchers.VisibilityPercentageViewNodeMatcher
import com.snap.valdi.test.matchers.ViewHasKeyboardMatcher
import com.snap.valdi.test.utils.CapturedValdiElementWithRootView
import org.hamcrest.Matcher
import org.hamcrest.Matchers

object ValdiElementMatchers {

    /**
     * Matches a View which has the given Composer attribute name and value.
     */
    @JvmStatic
    fun <T> withValdiAttribute(attributeName: String, attributeValue: T): Matcher<ValdiElementWithRootView> {
        return ValdiAttributeValueMatcher(attributeName, Matchers.equalTo(attributeValue))
    }

    @JvmStatic
    fun <T> withValdiAttributeMatcher(attributeName: String, attributeMatcher: Matcher<T>):
        Matcher<ValdiElementWithRootView> {
        return ValdiAttributeValueMatcher(attributeName, attributeMatcher)
    }

    @JvmStatic
    fun withText(value: String): Matcher<ValdiElementWithRootView> {
        return withText(Matchers.equalTo(value))
    }

    @JvmStatic
    fun withText(matcher: Matcher<String>): Matcher<ValdiElementWithRootView> {
        return ValdiAttributeValueMatcher("value", matcher)
    }

    @JvmStatic
    fun withText(resId: Int): Matcher<ValdiElementWithRootView> {
        return ValdiResourceAttributeValueMatcher("value", resId)
    }

    /**
     * Matches any Composer ViewNode that has an accessibilityId attribute set to the given value
     */
    @JvmStatic
    fun withAccessibilityId(accessibilityId: String) =
        withValdiAttribute("accessibilityId", accessibilityId)

    /**
     * Matches any Composer ViewNode that has an accessibilityStateSelected attribute set to the given value
     */
    @JvmStatic
    fun isSelected(selected: Boolean) =
        withValdiAttribute("accessibilityStateSelected", selected)

    /**
     * Matches any Composer ViewNode that has an accessibilityId attribute set to the given value
     */
    @JvmStatic
    fun withAccessibilityId(accessibilityId: Int): Matcher<ValdiElementWithRootView> {
        var context = ApplicationProvider.getApplicationContext<Context>()
        var idName = context.resources.getResourceEntryName(accessibilityId)
        return withAccessibilityId(idName.replace("__", "/"))
    }

    /**
     * Matches any Composer ViewNode that has an accessibilityId attribute value that matches the given prefix
     */
    @JvmStatic
    fun withAccessibilityIdPrefix(accessibilityIdPrefix: String): Matcher<ValdiElementWithRootView> =
        ValdiAttributeValueMatcher("accessibilityId", Matchers.startsWith(accessibilityIdPrefix))

    /**
     * Matches any Composer ViewNode that has an accessibilityId attribute value that matches the given sufix
     */
    @JvmStatic
    fun withAccessibilityIdSuffix(accessibilityIdSuffix: String): Matcher<ValdiElementWithRootView> =
        ValdiAttributeValueMatcher("accessibilityId", Matchers.startsWith(accessibilityIdSuffix))

    /**
     * Matches a node that has a computed visible area between 50% and 100%
     */
    @JvmStatic
    fun isDisplayed(): Matcher<ValdiElementWithRootView> {
        return VisibilityPercentageViewNodeMatcher(50, 100)
    }

    /**
     * Matches a node that has a computed visible area between at 100%.
     * Nodes that are larger than the screen are considered visible at 100%
     * if their visible bounds is as large as what can physically fit on the screen.
     */
    @JvmStatic
    fun isCompletelyDisplayed(): Matcher<ValdiElementWithRootView> {
        return VisibilityPercentageViewNodeMatcher(100, 100)
    }

    /**
     * Match a node that has a computed visible area are between the given percent and 100%.
     */
    @JvmStatic
    fun isDisplayingAtLeast(minAreaPercentage: Int): Matcher<ValdiElementWithRootView> {
        return VisibilityPercentageViewNodeMatcher(minAreaPercentage, 100)
    }

    /**
     * Match a node that has a computed visible area between the given min and max percent
     */
    @JvmStatic
    fun isDisplayingBetween(minAreaPercentage: Int, maxAreaPercentage: Int): Matcher<ValdiElementWithRootView> {
        return VisibilityPercentageViewNodeMatcher(minAreaPercentage, maxAreaPercentage)
    }

    /**
     * Match a scroll node that has a vertical scroll ratio between the given min and max percent.
     * The scroll offset percent is defined as the ratio in percent between
     * the scroll offset and the scroll element size. The percent will be at
     * 0% is when the scroll element is at the beginning (contentOffset of 0).
     * It will be at 100% is when it has been scrolled by a whole "page",
     * meaning if the scroll element is 400 pts, at that its content is at 10000 pts,
     * it will be at 50% when content offset is at 200pts (half the scroll element size),
     * 100% at 400pts, 200% at 800pts etc... Therefore, 100% represent one whole page
     * scrolled, 200% represent 2 pages.
     */
    @JvmStatic
    fun isVerticalScrollOffsetBetween(
        minVerticalScrollPercent: Int,
        maxVerticalScrollPercent: Int
    ): Matcher<ValdiElementWithRootView> {
        return ValdiScrollOffsetMatcher(minVerticalScrollPercent, maxVerticalScrollPercent, false)
    }

    /**
     * Match a scroll node that has a horizontal scroll ratio between the given min and max percent.
     * The scroll offset percent is defined as the ratio in percent between
     * the scroll offset and the scroll element size. The percent will be at
     * 0% is when the scroll element is at the beginning (contentOffset of 0).
     * It will be at 100% is when it has been scrolled by a whole "page",
     * meaning if the scroll element is 400 pts, at that its content is at 10000 pts,
     * it will be at 50% when content offset is at 200pts (half the scroll element size),
     * 100% at 400pts, 200% at 800pts etc... Therefore, 100% represent one whole page
     * scrolled, 200% represent 2 pages.
     */
    @JvmStatic
    fun isHorizontalScrollOffsetBetween(
        minHorizontalScrollPercent: Int,
        maxHorizontalScrollPercent: Int
    ): Matcher<ValdiElementWithRootView> {
        return ValdiScrollOffsetMatcher(minHorizontalScrollPercent, maxHorizontalScrollPercent, true)
    }

    /**
     * Matches an element that has a child matching the given matcher.
     */
    @JvmStatic
    fun withChild(matcher: Matcher<ValdiElementWithRootView>): Matcher<ValdiElementWithRootView> {
        return ChildElementMatcher(matcher, false)
    }

    /**
     * Matches an element that has a non layout child matching the given matcher.
     * Only view elements will be considered in the search. When encountering a child
     * that is a layout element, it will iterate over its view children.
     */
    @JvmStatic
    fun withChildIgnoringLayouts(matcher: Matcher<ValdiElementWithRootView>): Matcher<ValdiElementWithRootView> {
        return ChildElementMatcher(matcher, true)
    }

    @JvmStatic
    fun withParent(matcher: Matcher<ValdiElementWithRootView>): Matcher<ValdiElementWithRootView> {
        return withAncestor(1, false, matcher)
    }

    @JvmStatic
    fun withParentIgnoringLayout(matcher: Matcher<ValdiElementWithRootView>): Matcher<ValdiElementWithRootView> {
        return withAncestor(1, true, matcher)
    }

    @JvmStatic
    fun withAncestor(matcher: Matcher<ValdiElementWithRootView>): Matcher<ValdiElementWithRootView> {
        return withAncestor(Int.MAX_VALUE, false, matcher)
    }

    @JvmStatic
    fun withIndex(matcher: Matcher<ValdiElementWithRootView>, index: Int): Matcher<ValdiElementWithRootView> {
        return ValdiIndexMatcher(index, matcher)
    }

    @JvmStatic
    fun withAncestor(
        maxDepth: Int,
        ignoreLayouts: Boolean,
        matcher: Matcher<ValdiElementWithRootView>
    ): Matcher<ValdiElementWithRootView> {
        return AncestorElementMatcher(matcher, maxDepth, ignoreLayouts)
    }

    @JvmStatic
    fun hasDescendant(matcher: Matcher<ValdiElementWithRootView>): Matcher<ValdiElementWithRootView> {
        return DescendantElementMatcher(matcher)
    }

    @JvmStatic
    fun isType(elementType: String): Matcher<ValdiElementWithRootView> {
        return ElementTypeMatcher(elementType)
    }

    @JvmStatic
    fun isScroll(): Matcher<ValdiElementWithRootView> {
        return isType("scroll")
    }

    @JvmStatic
    fun isTextField(): Matcher<ValdiElementWithRootView> {
        return isType("textfield")
    }

    @JvmStatic
    fun hasKeyboard(): Matcher<ValdiElementWithRootView> {
        return EspressoViewMatcherToElementMatcher(ViewHasKeyboardMatcher())
    }

    /**
     * Converts a ViewNode matcher into a View matcher.
     */
    @JvmStatic
    fun toViewMatcher(matcher: Matcher<ValdiElementWithRootView>): Matcher<View> {
        return EspressoViewMatcherAdapter(CapturedValdiElementWithRootView(), matcher)
    }
}
