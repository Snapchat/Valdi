package com.snap.valdi.nodes

import android.graphics.RectF
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.view.View
import com.snap.valdi.context.IValdiContext
import com.snap.valdi.schema.ValdiUntypedClass
import com.snap.valdi.utils.InternedString
import com.snap.valdi.views.ValdiRootView.ScrollDirection

@ValdiUntypedClass
interface IValdiViewNode {

    enum class AccessibilityCategory {
        Auto,
        View,
        Text,
        Button,
        Image,
        ImageButton,
        Input,
        Header,
        Link,
        CheckBox,
        Radio,
        KeyboardKey,
    }

    data class AccessibilityNode(
        val viewNode: ValdiViewNode?,
        val customView: View?,
        val id: Int,
        val parent: AccessibilityNode?,
        val children: List<AccessibilityNode>,
        val boundsInRoot: Rect,
        val accessibilityCategory: AccessibilityCategory,
        val accessibilityLabel: String,
        val accessibilityHint: String,
        val accessibilityValue: String,
        val accessibilityId: String,
        val accessibilityStateDisabled: Boolean,
        val accessibilityStateSelected: Boolean,
        val accessibilityStateLiveRegion: Boolean,
    )

    /**
     * Returns the generated ViewNode id, which is a stable identifier generated at runtime
     * that uniquely identifies the node.
     */
    val id: Int

    /**
     * Returns the ViewClass name used by this ViewNode
     */
    val viewClassName: String

    fun invalidateLayout()

    /**
     * Set the attribute name with the given attribute value. If "keepAsOverride" is true,
     * the attribute will remain on the element until another call of setAttribute for the
     * same attribute is made. If "keepAsOverride" is false, the attribute will be allowed
     * to be overridden back from TypeScript.
     */
    fun setAttribute(attributeName: String, attributeValue: Any?, keepAsOverride: Boolean)

    fun getAttribute(attributeName: String): Any?

    fun setStoredObject(key: InternedString, value: Any?)

    fun getStoredObject(key: InternedString): Any?

    fun reapplyAttribute(attributeName: String)

    fun canScrollAtPoint(locationX: Int, locationY: Int, direction: ScrollDirection): Boolean

    /*
     * Returns whether this view node is currently scrolling or animating a scroll.
     */
    fun isScrollingOrAnimatingScroll(): Boolean

    /**
     * Get and stores the frame of the ViewNode relative to its parent into the given RectF.
     * The frame is in direction agnostic coordinates and matches what the TypeScript code will
     * see as the frame value for the backing element.
     */
    fun getRelativeFrame(frame: RectF)

    /**
     * Get and stores the frame of the ViewNode absolute from the root into the given RectF
     * The frame is in direction agnostic coordinates and matches what the TypeScript code will
     * see as the frame value for the backing element.
     */
    fun getAbsoluteFrame(frame: RectF)

    /**
     * Get and stores the visual frame of the ViewNode relative to its parent into the given RectF.
     * The visual frame is how the node actually appears to the user, taking in account LTR/RTL,
     * translations and scroll offsets.
     */
    fun getVisualRelativeFrame(frame: RectF)

    /**
     * Get and stores the visual frame of the ViewNode from the root into the given RectF.
     * The visual frame is how the node actually appears to the user, taking in account LTR/RTL,
     * translations and scroll offsets.
     */
    fun getVisualAbsoluteFrame(frame: RectF)

    /**
     * Get the visible viewport within the node, which is the area within the view node's coordinate
     * space that is visible from the root.
     */
    fun getViewport(viewport: RectF)

    /**
     * Get the visible viewport of this node from the root coordinate's space, which is the area
     * within the root view node's coordinate space that is visible.
     */
    fun getAbsoluteViewport(frame: RectF)

    /**
     * Get the list of direct children of this node
     */
    fun getChildren(): List<IValdiViewNode>

    /**
     * Get the list of visible direct children of this node
     */
    fun getVisibleChildren(): List<IValdiViewNode>

    /**
     * Search for an accessibility view node at the given location
     */
    fun hitTestAccessibility(x: Int, y: Int): IValdiViewNode?

    /**
     * Read the full (recursive) accessibility representation of the view node
     */
    fun getAccessibilityHierarchy(): List<AccessibilityNode>

    /**
     * Set text for accessibility reasons. Only works for ValditText
     */
    fun setTextAccessibility(text: CharSequence?)

    /**
     * Scroll the parents scrollviews so that this view node's frame becomes visible in the viewport
     */
    fun performEnsureFrameIsVisibleWithinParentScrolls(animated: Boolean)

    /**
     * Scroll the closest parent scrollview by one full page of the content
     */
    fun performScrollByOnePage(direction: ScrollDirection, animated: Boolean): Boolean

    /**
     * Returns the ValdiContext from where this ViewNode comes from.
     */
    fun getValdiContext(): IValdiContext?

    /**
     * Create a Snapshot representation of the ViewNode as a Drawable
     */
    fun snapshot(): Drawable
}
