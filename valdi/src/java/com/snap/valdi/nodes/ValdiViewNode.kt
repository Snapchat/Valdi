package com.snap.valdi.nodes

import android.graphics.Bitmap
import android.graphics.RectF
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.HeaderViewListAdapter
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import com.snap.valdi.ViewRef
import com.snap.valdi.views.ValdiTextHolder
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.context.IValdiContext
import com.snap.valdi.drawables.ValdiBitmapDrawable
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.exceptions.messageWithCauses
import com.snap.valdi.extensions.ViewUtils
import com.snapchat.client.valdi.NativeBridge
import com.snap.valdi.utils.NativeRef
import com.snap.valdi.nodes.IValdiViewNode.AccessibilityNode
import com.snap.valdi.nodes.IValdiViewNode.AccessibilityCategory
import com.snap.valdi.utils.CanvasClipper
import com.snap.valdi.utils.Ref
import com.snap.valdi.utils.error
import com.snap.valdi.views.ValdiRootView.ScrollDirection

/**
 * A ValdiViewNode identifies a View inside a ValdiContext.
 */
class ValdiViewNode(nativeHandle: Long,
                       private var context: ValdiContext?): NativeRef(nativeHandle), IValdiViewNode {

    private data class AccessibilityHierarchyRepresentation(
        val array: Array<Any?>,
        var index: Int,
    )

    override val id: Int
        get() = NativeBridge.getNodeId(nativeHandle)

    override val viewClassName: String
        get() = NativeBridge.getViewClassName(nativeHandle) ?: ""

    val isLayoutDirectionHorizontal: Boolean
        get() = NativeBridge.isViewNodeLayoutDirectionHorizontal(nativeHandle)

    private val runtimeNativeHandle: Long
        get() {
            val contextNative = this.context?.native ?: return 0L

            return contextNative.runtimeNative.nativeHandle
        }

    override fun invalidateLayout()  {
        NativeBridge.invalidateLayout(nativeHandle)
    }

    override fun setAttribute(attributeName: String, attributeValue: Any?, keepAsOverride: Boolean) {
        NativeBridge.setValueForAttribute(nativeHandle, attributeName, attributeValue, keepAsOverride)
    }

    override fun getAttribute(attributeName: String): Any? {
        return NativeBridge.getValueForAttribute(nativeHandle, attributeName)
    }

    override fun reapplyAttribute(attributeName: String) {
        NativeBridge.reapplyAttribute(nativeHandle, attributeName)
    }

    fun convertPointToRelativeDirectionAgnostic(directionDependentX: Int, directionDependentY: Int): Long {
        return NativeBridge.getViewNodePoint(runtimeNativeHandle, nativeHandle, directionDependentX, directionDependentY, POINT_MODE_DIRECTION_AGNOSTIC_RELATIVE, false)
    }

    fun convertPointToAbsoluteDirectionAgnostic(directionDependentX: Int, directionDependentY: Int): Long {
        return NativeBridge.getViewNodePoint(runtimeNativeHandle, nativeHandle, directionDependentX, directionDependentY, POINT_MODE_DIRECTION_AGNOSTIC_ABSOLUTE, false)
    }

    override fun canScrollAtPoint(locationX: Int, locationY: Int, direction: ScrollDirection): Boolean {
        return NativeBridge.canViewNodeScroll(runtimeNativeHandle, nativeHandle, locationX, locationY, direction.value)
    }

    override fun isScrollingOrAnimatingScroll(): Boolean {
        return NativeBridge.isViewNodeScrollingOrAnimating(nativeHandle)
    }

    private fun doGetChildren(mode: Int): List<IValdiViewNode> {
        val childrenHandleArray = NativeBridge.getRetainedViewNodeChildren(nativeHandle, mode) as? LongArray
        if (childrenHandleArray == null) {
            return emptyList()
        }
        return childrenHandleArray.map { ValdiViewNode(it, context) }
    }

    override fun getChildren(): List<IValdiViewNode> {
        return doGetChildren(CHILDREN_SHALLOW_ALL)
    }

    override fun getVisibleChildren(): List<IValdiViewNode> {
        return doGetChildren(CHILDREN_SHALLOW_VISIBLE)
    }

    override fun hitTestAccessibility(x: Int, y: Int): IValdiViewNode? {
        val viewNodeHandle = NativeBridge.getRetainedViewNodeHitTestAccessibility(runtimeNativeHandle, nativeHandle, x, y)
        if (viewNodeHandle == 0L) {
            return null
        }
        return ValdiViewNode(viewNodeHandle, context)
    }

    override fun getAccessibilityHierarchy(): List<AccessibilityNode> {
        val array = NativeBridge.getViewNodeAccessibilityHierarchyRepresentation(runtimeNativeHandle, nativeHandle) as Array<Any?>
        val output = mutableListOf<AccessibilityNode>()
        val representation = AccessibilityHierarchyRepresentation(array, 0)
        readAccessibilityHierarchyRepresentation(representation, null, output)
        return output
    }

    override fun setTextAccessibility(text: CharSequence?) {
        val ref = getBackingViewRef()
        val handle = ref?.get()
        if (handle is ValdiTextHolder) {
            handle.setTextAccessibility(text)
        }
    }

    private fun doPerformAction(action: Int, param1: Int, param2: Int): Int {
        return NativeBridge.performViewNodeAction(nativeHandle, action, param1, param2)
    }

    override fun performEnsureFrameIsVisibleWithinParentScrolls(animated: Boolean) {
        doPerformAction(
            ACTION_ENSURE_FRAME_IS_VISIBLE_WITHIN_PARENT_SCROLLS,
            if (animated) 1 else 0,
            0
        )
    }

    override fun performScrollByOnePage(direction: ScrollDirection, animated: Boolean): Boolean {
        val handled = doPerformAction(
            ACTION_SCROLL_BY_ONE_PAGE,
            if (animated) 1 else 0,
            direction.value
        )
        return handled != 0
    }

    private fun doGetFrame(pointMode: Int, sizeMode: Int, frame: RectF) {
        val position = NativeBridge.getViewNodePoint(runtimeNativeHandle,
                nativeHandle,
                0,
                0,
                pointMode,
                true)
        val size = NativeBridge.getViewNodeSize(runtimeNativeHandle, nativeHandle, sizeMode)

        val x = horizontalFromEncodedLong(position)
        val y = verticalFromEncodedLong(position)
        val width = horizontalFromEncodedLong(size)
        val height = verticalFromEncodedLong(size)

        frame.left = x.toFloat()
        frame.top = y.toFloat()
        frame.right = (x + width).toFloat()
        frame.bottom = (y + height).toFloat()
    }

    override fun getAbsoluteFrame(frame: RectF) {
        doGetFrame(POINT_MODE_DIRECTION_AGNOSTIC_ABSOLUTE, SIZE_MODE_FRAME, frame)
    }

    override fun getRelativeFrame(frame: RectF) {
        doGetFrame(POINT_MODE_DIRECTION_AGNOSTIC_RELATIVE, SIZE_MODE_FRAME, frame)
    }

    override fun getVisualAbsoluteFrame(frame: RectF) {
        doGetFrame(POINT_MODE_VISUAL_ABSOLUTE, SIZE_MODE_FRAME, frame)
    }

    override fun getVisualRelativeFrame(frame: RectF) {
        doGetFrame(POINT_MODE_VISUAL_RELATIVE, SIZE_MODE_FRAME, frame)
    }

    override fun getViewport(viewport: RectF) {
        doGetFrame(POINT_MODE_VIEWPORT_RELATIVE, SIZE_MODE_VIEWPORT, viewport)
    }

    override fun getAbsoluteViewport(frame: RectF) {
        doGetFrame(POINT_MODE_VIEWPORT_ABSOLUTE, SIZE_MODE_VIEWPORT, frame)
    }

    override fun toString(): String {
        return NativeBridge.getViewNodeDebugDescription(nativeHandle)
    }

    override fun dispose() {
        super.dispose()
        this.context = null
    }

    override fun getValdiContext(): IValdiContext? {
        return this.context
    }

    fun getBackingViewRef(): Ref? {
        return NativeBridge.getViewNodeBackingView(nativeHandle) as? Ref
    }

    override fun snapshot(): Drawable {
        val ref = getBackingViewRef()
        val handle = ref?.get()

        val rect = RectF()
        getVisualRelativeFrame(rect)

        val bitmapWidth = rect.width().toInt()
        val bitmapHeight = rect.height().toInt()

        if (bitmapWidth > 0 && bitmapHeight > 0) {
            when (handle) {
                is View -> {
                    return ViewUtils.viewToDrawable(handle)
                }
                is Long -> {
                    val bitmap = Bitmap.createBitmap(bitmapWidth, bitmapHeight, Bitmap.Config.ARGB_8888)
                    try {
                        NativeBridge.snapDrawingDrawLayerInBitmap(handle, bitmap)
                    } catch (exc: ValdiException) {
                        context?.logger?.error("Failed to create Snapshot: ${exc.messageWithCauses()}")
                    }

                    return ValdiBitmapDrawable(CanvasClipper()).also {
                        it.bitmap = bitmap
                    }
                }
            }
        }

        return ValdiBitmapDrawable(CanvasClipper())
    }

    fun notifyApplyAttributeFailed(attributeId: Int, errorMessage: String) {
        NativeBridge.notifyApplyAttributeFailed(nativeHandle, attributeId, errorMessage)
    }

    fun notifyScroll(eventType: Int,
                     pixelDirectionDependentContentOffsetX: Int,
                     pixelDirectionDependentContentOffsetY: Int,
                     pixelDirectionDependentUnclampedContentOffsetX: Int,
                     pixelDirectionDependentUnclampedContentOffsetY: Int,
                     pixelInvertedVelocityX: Float,
                     pixelInvertedVelocityY: Float): Long {
        return NativeBridge.notifyScroll(
                runtimeNativeHandle,
                nativeHandle,
                eventType,
                pixelDirectionDependentContentOffsetX,
                pixelDirectionDependentContentOffsetY,
                pixelDirectionDependentUnclampedContentOffsetX,
                pixelDirectionDependentUnclampedContentOffsetY,
                pixelInvertedVelocityX,
                pixelInvertedVelocityY)
    }

    private fun readAccessibilityHierarchyRepresentation(
        representation: AccessibilityHierarchyRepresentation,
        parent: AccessibilityNode?,
        output: MutableList<AccessibilityNode>
    ) {
        val childrenCount = readAccessibilityHierarchyRepresentationInt32(representation)
        for (i in 1..childrenCount) {
            val hasCustomView = readAccessibilityHierarchyRepresentationBoolean(representation)
            var customViewHandle: ViewRef? = null
            if (hasCustomView) {
                customViewHandle = (representation.array[representation.index++] as ViewRef?) ?: null
            }

            val viewNodeHandle = readAccessibilityHierarchyRepresentationInt64(representation)

            val rawId = readAccessibilityHierarchyRepresentationInt32(representation)

            val accessibilityCategory = readAccessibilityHierarchyRepresentationInt32(representation)

            val accessibilityLabel = readAccessibilityHierarchyRepresentationString(representation)
            val accessibilityHint = readAccessibilityHierarchyRepresentationString(representation)
            val accessibilityValue = readAccessibilityHierarchyRepresentationString(representation)
            val accessibilityId = readAccessibilityHierarchyRepresentationString(representation)

            val accessibilityStateDisabled = readAccessibilityHierarchyRepresentationBoolean(representation)
            val accessibilityStateSelected = readAccessibilityHierarchyRepresentationBoolean(representation)
            val accessibilityStateLiveRegion = readAccessibilityHierarchyRepresentationBoolean(representation)

            val frameInRootX = readAccessibilityHierarchyRepresentationInt32(representation)
            val frameInRootY = readAccessibilityHierarchyRepresentationInt32(representation)
            val frameInRootWidth = readAccessibilityHierarchyRepresentationInt32(representation)
            val frameInRootHeight = readAccessibilityHierarchyRepresentationInt32(representation)

            val children = mutableListOf<AccessibilityNode>()

            val accessibilityNode = AccessibilityNode(
                viewNode = ValdiViewNode(viewNodeHandle, context),
                customView = customViewHandle?.get(),
                id = rawId,
                parent = parent,
                children = children,
                boundsInRoot = Rect(
                    frameInRootX,
                    frameInRootY,
                    frameInRootX + frameInRootWidth,
                    frameInRootY + frameInRootHeight
                ),
                accessibilityCategory = convertIntToAccessibilityCategory(accessibilityCategory),
                accessibilityLabel = accessibilityLabel,
                accessibilityHint = accessibilityHint,
                accessibilityValue = accessibilityValue,
                accessibilityId = accessibilityId,
                accessibilityStateDisabled = accessibilityStateDisabled,
                accessibilityStateSelected = accessibilityStateSelected,
                accessibilityStateLiveRegion = accessibilityStateLiveRegion,
            )

            output.add(accessibilityNode)

            // If we have a custom view, we need to harvest it's children.
            if (hasCustomView) {
                readAccessibilityHierarchyForCustomView(accessibilityNode, children)
            }

            // We always want to keep walking the tree.
            readAccessibilityHierarchyRepresentation(
                representation,
                accessibilityNode,
                children
            )
        }
    }

    private fun readAccessibilityHierarchyForCustomView(parent: AccessibilityNode, output: MutableList<AccessibilityNode>) {
        // This function only gathers information to build out ViewNodeCompats later.
        // This is only used for displaying outlines and metadata.
        // Hit testing will be done on the View itself.

        if (parent.customView == null) {
            return;
        }

        var view = parent.customView

        if (view is ViewGroup) {
            val childCount = view.childCount
            for (i in 0 until childCount) {
                var child = view.getChildAt(i)
                val children = mutableListOf<AccessibilityNode>()
                // These are dummy values, nothing is used beyond the structure of the tree and the
                // id
                val accessibilityNode = AccessibilityNode(
                    viewNode = null,
                    customView = child,
                    id = child.id,
                    parent = parent,
                    children = children,
                    boundsInRoot = Rect(),
                    accessibilityCategory = AccessibilityCategory.View,
                    accessibilityLabel = "",
                    accessibilityHint = "",
                    accessibilityValue = "",
                    accessibilityId = "",
                    accessibilityStateDisabled = false,
                    accessibilityStateSelected = false,
                    accessibilityStateLiveRegion = true,
                )
                output.add(accessibilityNode)
                readAccessibilityHierarchyForCustomView(accessibilityNode, children)
            }
        }
    }

    private fun readAccessibilityHierarchyRepresentationInt32(representation: AccessibilityHierarchyRepresentation): Int {
        return (representation.array[representation.index++] as Int?) ?: 0
    }

    private fun readAccessibilityHierarchyRepresentationInt64(representation: AccessibilityHierarchyRepresentation): Long {
        return (representation.array[representation.index++] as Long?) ?: 0L
    }

    private fun readAccessibilityHierarchyRepresentationString(representation: AccessibilityHierarchyRepresentation): String {
        return (representation.array[representation.index++] as String?) ?: ""
    }

    private fun readAccessibilityHierarchyRepresentationBoolean(representation: AccessibilityHierarchyRepresentation): Boolean {
        val representation = representation.array[representation.index++]
        return (representation as Boolean?) ?: false
    }

    private fun convertIntToAccessibilityCategory(accessibilityCategory: Int): AccessibilityCategory {
        return when(accessibilityCategory) {
            2 -> AccessibilityCategory.Text
            3 -> AccessibilityCategory.Button
            4 -> AccessibilityCategory.Image
            5-> AccessibilityCategory.ImageButton
            6 -> AccessibilityCategory.Input
            7 -> AccessibilityCategory.Header
            8 -> AccessibilityCategory.Link
            9 -> AccessibilityCategory.CheckBox
            10 -> AccessibilityCategory.Radio
            11 -> AccessibilityCategory.KeyboardKey
            else -> AccessibilityCategory.View
        }
    }

    companion object {
        const val INVALID_ID = 0

        const val SCROLL_EVENT_TYPE_ON_SCROLL = 1
        const val SCROLL_EVENT_TYPE_ON_SCROLL_END = 2
        const val SCROLL_EVENT_TYPE_ON_DRAG_START = 3
        const val SCROLL_EVENT_TYPE_ON_DRAG_ENDING = 4

        const val POINT_MODE_DIRECTION_AGNOSTIC_RELATIVE = 1
        const val POINT_MODE_DIRECTION_AGNOSTIC_ABSOLUTE = 2
        const val POINT_MODE_VISUAL_RELATIVE = 3
        const val POINT_MODE_VISUAL_ABSOLUTE = 4
        const val POINT_MODE_VIEWPORT_RELATIVE = 5
        const val POINT_MODE_VIEWPORT_ABSOLUTE = 6

        const val SIZE_MODE_FRAME = 1
        const val SIZE_MODE_VIEWPORT = 2

        const val CHILDREN_SHALLOW_ALL = 1
        const val CHILDREN_SHALLOW_VISIBLE = 2

        const val ACTION_ENSURE_FRAME_IS_VISIBLE_WITHIN_PARENT_SCROLLS = 1
        const val ACTION_SCROLL_BY_ONE_PAGE = 2

        @JvmStatic
        fun horizontalFromEncodedLong(long: Long): Int {
            return (long shr 32 and 0xFFFFFFFF).toInt()
        }

        @JvmStatic
        fun verticalFromEncodedLong(long: Long): Int {
            return (long and 0xFFFFFFFF).toInt()
        }

        @JvmStatic
        fun encodeHorizontalVerticalPair(horizontal: Int, vertical: Int): Long {
            return (horizontal.toLong() shl 32) or vertical.toLong()
        }
    }

}
