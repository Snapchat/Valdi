package com.snap.valdi.views

import android.graphics.Rect
import android.os.Bundle
import android.os.Build
import android.os.SystemClock
import android.view.View
import android.view.MotionEvent
import android.widget.TextView
import android.widget.Button
import android.widget.ImageView
import android.widget.ImageButton
import android.widget.EditText
import android.widget.CheckBox
import android.widget.RadioButton
import android.inputmethodservice.Keyboard.Key
import android.util.Log
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import androidx.core.view.ViewCompat
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.nodes.IValdiViewNode.AccessibilityNode
import com.snap.valdi.nodes.IValdiViewNode.AccessibilityCategory

/**
 * ValdiAccessibilityDelegateHierarchy is a ValdiAccessibilityDelegate implementation
 *
 * It provides the capability to fetch, read, parse
 * and apply the valdi's view/layout hierarchy onto android AccessibilityNodes
 *
 * It implements all the necessary hooks needed by the ValdiAccessibilityDelegate
 * to be able to make sense of the valdi's view/layout hierarchy (locating/finding/fetching views)
 *
 * Note: The "host" view is the ValdiRootView,
 * and all its ViewNode with accessibility are considered as a "virtual view" to be navigateable by accessibility
 */
class ValdiAccessibilityDelegateHierarchy(host: View, val valdiContext: ValdiContext) : ValdiAccessibilityDelegate(host) {

    private var accessibilityNodes = mutableMapOf<Int, AccessibilityNode>()

    private var hostLocationOnScreen = IntArray(2)

    /**
     * Called by the android system when the user hover the finger over the UI
     * This is used to find the ID of the view that was just hovered over
     */
    override fun getVirtualViewAt(x: Float, y: Float): Int {
        val rootViewNode = valdiContext.getRootViewNode()
        if (rootViewNode == null) {
            return VIRTUAL_VIEW_ID_HOST
        }
        val hitTest = rootViewNode.hitTestAccessibility(x.toInt(), y.toInt())
        if (hitTest == null) {
            return VIRTUAL_VIEW_ID_HOST
        }
        val accessibilityNode = accessibilityNodes[hitTest.id]
        if (accessibilityNode == null) {
            return VIRTUAL_VIEW_ID_HOST
        }

        // The ViewNode tree only knows about top level custom-views
        // The hit target might be a subview so we need to look for it
        // There are no utilities to do this so we must hittest ourselves
        if (accessibilityNode.customView != null) {
            val view = accessibilityNode.customView
            // Translate the touch target to be relative to the custom-view
            val xOffset = getXOffsetRelativeToHost(view)
            val yOffset = getYOffsetRelativeToHost(view)
            val hitChild = hitTestCustomView(accessibilityNode.customView, x.toInt() - xOffset, y.toInt() - yOffset)
            if (hitChild > 0) {
                return hitChild
            }
        }
        return accessibilityNode.id
    }

    private fun getXOffsetRelativeToHost(view: View): Int {
        return if (view == host) {
            0
        } else {
            view.left + getXOffsetRelativeToHost(view.parent as View)
        }
    }

    private fun getYOffsetRelativeToHost(view: View): Int {
        return if (view == host) {
            0
        } else {
            view.top + getYOffsetRelativeToHost(view.parent as View)
        }
    }

    private fun translateX(view: View, x: Int): Int {
        return x - view.left
    }

    private fun translateY(view: View, y: Int): Int {
        return y - view.top
    }

    private fun hitTestCustomView(view: View, x: Int, y: Int): Int {
        if (view is ViewGroup) {
            val childCount = view.childCount
            var childBounds = Rect()
            for (i in 0 until childCount) {
                var child = view.getChildAt(i)
                child.getHitRect(childBounds)
                if (childBounds.contains(x, y)) {
                    val hitChild = hitTestCustomView(child, translateX(child, x), translateY(child, y))
                    if (hitChild > 0) {
                        return hitChild
                    }

                    return child.id
                }
            }
        }
        return -1
    }

    /**
     * Called by the android system when the user tries to focus a virtualView
     * that is located nearby another virtualView, this can be used to decide which view to focus
     */
    override fun getVirtualViewNearby(virtualViewId: Int, direction: Int): Int {
        return when (direction) {
            View.FOCUS_FORWARD -> pickVirtualViewId(virtualViewId, { node -> node.id })
            View.FOCUS_BACKWARD -> pickVirtualViewId(virtualViewId, { node -> -node.id })
            View.FOCUS_LEFT -> pickVirtualViewId(virtualViewId, { node -> node.boundsInRoot.centerX() })
            View.FOCUS_RIGHT -> pickVirtualViewId(virtualViewId, { node -> -node.boundsInRoot.centerX() })
            View.FOCUS_DOWN -> pickVirtualViewId(virtualViewId, { node -> node.boundsInRoot.centerY() })
            View.FOCUS_UP -> pickVirtualViewId(virtualViewId, { node -> -node.boundsInRoot.centerY() })
            else -> VIRTUAL_VIEW_ID_INVALID
        }
    }

    /**
     * Sort the virtual views by an arbitrary sort priority,
     * so that we can pick the closest virtual view to the reference virtual view
     */
    private fun pickVirtualViewId(virtualViewId: Int, sorting: (AccessibilityNode) -> Int): Int {
        val reference = accessibilityNodes[virtualViewId]
        if (reference == null) {
            return VIRTUAL_VIEW_ID_INVALID
        }
        val list = accessibilityNodes.values.sortedBy(sorting)
        val currentIndex = list.indexOf(reference)
        if (currentIndex < 0) {
            return VIRTUAL_VIEW_ID_INVALID
        }
        val nextIndex = currentIndex + 1
        if (nextIndex >= list.count()) {
            return VIRTUAL_VIEW_ID_INVALID
        }
        return list[nextIndex].id
    }

    /**
     * Called when the user performs an accessibility on the host view
     * (in valdi the host is a regular non-accessible view)
     */
    override fun onPopulateEventForHost(event: AccessibilityEvent) {
        event.setClassName(CLASS_NAME_VIEW)
    }

    /**
     * Called when an accessibility action is performed on the host
     * (in valdi the host is a regular non-accessible view)
     */
    override fun onPerformActionForHost(action: Int, arguments: Bundle?): Boolean {
        return false
    }

    /**
     * Configure the host view's node
     * (should just list all child nodes, the host node should be useless by itself)
     */
    override fun onPopulateNodeForHost(nodeInfo: AccessibilityNodeInfoCompat) {
        nodeInfo.setClassName(CLASS_NAME_VIEW)
        accessibilityNodes.clear()
        val rootViewNode = valdiContext.getRootViewNode()
        if (rootViewNode == null) {
            return
        }
        host.getLocationOnScreen(hostLocationOnScreen)
        val accessibilityHierarchy = rootViewNode.getAccessibilityHierarchy()
        for (accessibilityNode in accessibilityHierarchy) {
            nodeInfo.addChild(host, accessibilityNode.id)
        }
        indexAccessibilityHierarchy(nodeInfo, accessibilityHierarchy)
    }

    /**
     * Recursively index all the virtual accessibility nodes into a map by ID
     */
    private fun indexAccessibilityHierarchy(nodeInfo: AccessibilityNodeInfoCompat, nodes: List<AccessibilityNode>) {
        for (node in nodes) {
            accessibilityNodes[node.id] = node
            if (node.children.size > 0) {

                indexAccessibilityHierarchy(nodeInfo, node.children)
            }
        }
    }

    /**
     * Called when the user performs an accessibility action on a virtual view
     */
    override fun onPopulateEventForVirtualView(virtualViewId: Int, event: AccessibilityEvent) {
        // Read the virtual node info
        val accessibilityNode = accessibilityNodes[virtualViewId]

        // If no information is available for some reason, feed a dummy event
        if (accessibilityNode == null) {
            event.setClassName(CLASS_NAME_VIEW)
            event.setContentDescription("Unknown")
            return
        }

        if (accessibilityNode.customView != null) {
            ViewCompat.onPopulateAccessibilityEvent(accessibilityNode.customView, event)
            return
        }
        // Apply the configuration flags based on the accessibility category
        event.setClassName(computeClassName(accessibilityNode.accessibilityCategory))
        // Compute the content description
        event.setContentDescription(computeContentDescription(
            accessibilityNode.accessibilityLabel,
            accessibilityNode.accessibilityHint,
            accessibilityNode.accessibilityValue,
        ))
        // Apply the states flags
        event.setEnabled(!accessibilityNode.accessibilityStateDisabled)
    }

    /**
     * Called once per virtual accessibility node after the getVisibleVirtualViews() has returned
     */
    override fun onPopulateNodeForVirtualView(virtualViewId: Int, nodeInfo: AccessibilityNodeInfoCompat) {
        // Read the virtual node info
        val accessibilityNode = accessibilityNodes[virtualViewId]

        // If no information is available for some reason, feed a dummy node
        if (accessibilityNode == null) {
            nodeInfo.setFocusable(false)
            nodeInfo.setClassName(CLASS_NAME_VIEW)
            nodeInfo.setContentDescription("Unknown")
            nodeInfo.setBoundsInParent(Rect())
            nodeInfo.setBoundsInScreen(Rect())
            return
        }

        if (accessibilityNode.customView != null) {
            ViewCompat.onInitializeAccessibilityNodeInfo(accessibilityNode.customView, nodeInfo)
            return
        }

        // Apply the configuration flags based on the accessibility category
        val className = computeClassName(accessibilityNode.accessibilityCategory)
        nodeInfo.className = className
        if (className == CLASS_NAME_TEXT_VIEW || className == CLASS_NAME_EDIT_TEXT) {
            nodeInfo.text = accessibilityNode.accessibilityValue
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            nodeInfo.setHeading(computeFlagHeading(accessibilityNode.accessibilityCategory))
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            nodeInfo.setEditable(computeFlagEditable(accessibilityNode.accessibilityCategory))
        }
        // Compute the content description
        nodeInfo.setContentDescription(computeContentDescription(
            accessibilityNode.accessibilityLabel,
            accessibilityNode.accessibilityHint,
            accessibilityNode.accessibilityValue,
        ))

        // Apply the states flags
        nodeInfo.setEnabled(!accessibilityNode.accessibilityStateDisabled)
        if (accessibilityNode.accessibilityStateLiveRegion) {
            nodeInfo.setLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE)
        }
        // Optionally apply the check flag
        if (computeFlagCheckable(accessibilityNode.accessibilityCategory)) {
            nodeInfo.setCheckable(true)
            nodeInfo.setChecked(accessibilityNode.accessibilityStateSelected)
        } else {
            nodeInfo.setSelected(accessibilityNode.accessibilityStateSelected)
        }
        // Compute the final bounds and set on the node
        val boundsInRoot = computeBoundsInRootForcedIntoViewport(accessibilityNode.boundsInRoot)
        val boundsInParent = Rect(boundsInRoot)
        val boundsInScreen = Rect(boundsInRoot)
        boundsInScreen.offset(
            hostLocationOnScreen[0],
            hostLocationOnScreen[1]
        )
        nodeInfo.setBoundsInParent(boundsInParent)
        nodeInfo.setBoundsInScreen(boundsInScreen)
        // Mark parent/child relationship
        for (child in accessibilityNode.children) {
            nodeInfo.addChild(host, child.id)
        }
        val parent = accessibilityNode.parent
        if (parent != null) {
            nodeInfo.setParent(host, parent.id)
        } else {
            nodeInfo.setParent(host)
        }
        // All node are actionables, since they may be inside of views that have triggerable events
        nodeInfo.addAction(AccessibilityNodeInfoCompat.ACTION_SCROLL_FORWARD)
        nodeInfo.addAction(AccessibilityNodeInfoCompat.ACTION_SCROLL_BACKWARD)
        nodeInfo.addAction(AccessibilityNodeInfoCompat.ACTION_CLICK)
        nodeInfo.addAction(AccessibilityNodeInfoCompat.ACTION_LONG_CLICK)

        nodeInfo.viewIdResourceName = accessibilityNode.accessibilityId
    }

    /**
     * Called when the user performs an accessibility action on a virtual view
     */
    override fun onPerformActionForVirtualView(virtualViewId: Int, action: Int, arguments: Bundle?): Boolean {
        val accessibilityNode = accessibilityNodes[virtualViewId]
        if (accessibilityNode == null) {
            return false
        }

        // The node we get here may be returned by getVirtualViewAt and may be a sub view
        // of the custom view.
        if (accessibilityNode.viewNode == null) {
            // This is a custom view, we need to do something else
            if (accessibilityNode.customView != null) {
                return ViewCompat.performAccessibilityAction(accessibilityNode.customView, action, arguments)
            }
            // Something has gone horribly wrong
            return false
        }

        if (action == AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS) {
            accessibilityNode.viewNode?.performEnsureFrameIsVisibleWithinParentScrolls(true)
            return true
        }
        if (action == AccessibilityNodeInfoCompat.ACTION_FOCUS) {
            accessibilityNode.viewNode?.performEnsureFrameIsVisibleWithinParentScrolls(true)
            return true
        }
        if (action == AccessibilityNodeInfoCompat.ACTION_SCROLL_FORWARD && accessibilityNode.viewNode != null) {
            return accessibilityNode.viewNode.performScrollByOnePage(
                ValdiRootView.ScrollDirection.Forward,
                true
            )
        }
        if (action == AccessibilityNodeInfoCompat.ACTION_SCROLL_BACKWARD && accessibilityNode.viewNode != null) {
            return accessibilityNode.viewNode.performScrollByOnePage(
                ValdiRootView.ScrollDirection.Backward,
                true
            )
        }
        if (action == AccessibilityNodeInfoCompat.ACTION_CLICK) {
            val uptimeNowMs = SystemClock.uptimeMillis()
            val uptimeDownMs = uptimeNowMs - EVENT_DELAY_TAP
            val uptimeUpMs = uptimeNowMs
            performTouchEvent(accessibilityNode, MotionEvent.ACTION_DOWN, uptimeDownMs, uptimeDownMs)
            performTouchEvent(accessibilityNode, MotionEvent.ACTION_UP, uptimeDownMs, uptimeUpMs)
            return true
        }
        if (action == AccessibilityNodeInfoCompat.ACTION_LONG_CLICK) {
            val uptimeNowMs = SystemClock.uptimeMillis()
            val uptimeDownMs = uptimeNowMs - EVENT_DELAY_LONG_PRESS
            val uptimeUpMs = uptimeNowMs
            performTouchEvent(accessibilityNode, MotionEvent.ACTION_DOWN, uptimeDownMs, uptimeDownMs)
            performTouchEvent(accessibilityNode, MotionEvent.ACTION_UP, uptimeDownMs, uptimeUpMs)
            return true
        }

        if (action == AccessibilityNodeInfoCompat.ACTION_SET_TEXT) {
            if (arguments != null) {
                accessibilityNode.viewNode?.setTextAccessibility(arguments.getCharSequence(AccessibilityNodeInfoCompat.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE))
                return true
            }
        }
        return false
    }

    /**
     * Fake a touch event on the root view to trigger valdi gestures
     */
    private fun performTouchEvent(accessibilityNode: AccessibilityNode, action: Int, downMs: Long, currentMs: Long) {
        val exactCenterX = accessibilityNode.boundsInRoot.exactCenterX()
        val exactCenterY = accessibilityNode.boundsInRoot.exactCenterY()
        val event = MotionEvent.obtain(
            downMs,
            currentMs,
            action,
            exactCenterX,
            exactCenterY,
            0
        )
        host.dispatchTouchEvent(event)
        event.recycle()
    }

    /**
     * Called when the keyboard changes target
     */
    override fun onVirtualViewKeyboardFocusChanged(virtualViewId: Int, hasFocus: Boolean) {
        // No-op
    }

    /**
     * Utilities
     */

    private fun computeContentDescription(label: String, hint: String, value: String): String {
        val stringBuilder = StringBuilder()
        appendContentDescription(stringBuilder, label)
        appendContentDescription(stringBuilder, hint)
        appendContentDescription(stringBuilder, value)
        return stringBuilder.toString()
    }

    private fun appendContentDescription(stringBuilder: StringBuilder, value: String) {
        if (!value.isEmpty()) {
            if (stringBuilder.length > 0) {
                stringBuilder.append(", ")
            }
            stringBuilder.append(value)
        }
    }

    private fun computeBoundsInRootForcedIntoViewport(boundsInRoot: Rect): Rect {
        val boundsInRootLeft = boundsInRoot.left
        if (boundsInRootLeft < 0) {
            boundsInRoot.offset(-boundsInRootLeft, 0)
        }
        val boundsInRootTop = boundsInRoot.top
        if (boundsInRootTop < 0) {
            boundsInRoot.offset(0, -boundsInRootTop)
        }
        val boundsInRootRight = boundsInRoot.right
        if (boundsInRootRight > host.width) {
            boundsInRoot.offset(host.width - boundsInRootRight, 0)
        }
        val boundsInRootBottom = boundsInRoot.bottom
        if (boundsInRootBottom > host.height) {
            boundsInRoot.offset(0, host.height - boundsInRootBottom)
        }
        return boundsInRoot
    }
    private fun computeClassName(accessibilityCategory: AccessibilityCategory): String {
        return when (accessibilityCategory) {
            AccessibilityCategory.Auto -> CLASS_NAME_VIEW
            AccessibilityCategory.View -> CLASS_NAME_VIEW
            AccessibilityCategory.Text -> CLASS_NAME_TEXT_VIEW
            AccessibilityCategory.Button -> CLASS_NAME_BUTTON
            AccessibilityCategory.Image -> CLASS_NAME_IMAGE_VIEW
            AccessibilityCategory.ImageButton -> CLASS_NAME_IMAGE_BUTTON
            AccessibilityCategory.Input ->  CLASS_NAME_EDIT_TEXT
            AccessibilityCategory.Header -> CLASS_NAME_TEXT_VIEW
            AccessibilityCategory.Link -> CLASS_NAME_BUTTON
            AccessibilityCategory.CheckBox ->  CLASS_NAME_CHECK_BOX
            AccessibilityCategory.Radio ->  CLASS_NAME_RADIO_BUTTON
            AccessibilityCategory.KeyboardKey -> CLASS_NAME_KEYBOARD_KEY
        }
    }

    private fun computeFlagEditable(accessibilityCategory: AccessibilityCategory): Boolean {
        return when (accessibilityCategory) {
            AccessibilityCategory.Input -> true
            else -> false
        }
    }

    private fun computeFlagHeading(accessibilityCategory: AccessibilityCategory): Boolean {
        return when (accessibilityCategory) {
            AccessibilityCategory.Header -> true
            else -> false
        }
    }

    private fun computeFlagCheckable(accessibilityCategory: AccessibilityCategory): Boolean {
        return when (accessibilityCategory) {
            AccessibilityCategory.Radio -> true
            AccessibilityCategory.CheckBox -> true
            else -> false
        }
    }

    companion object {
        private val CLASS_NAME_VIEW = View::class.java.name
        private val CLASS_NAME_TEXT_VIEW = TextView::class.java.name
        private val CLASS_NAME_IMAGE_VIEW = ImageView::class.java.name
        private val CLASS_NAME_BUTTON = Button::class.java.name
        private val CLASS_NAME_IMAGE_BUTTON = ImageButton::class.java.name
        private val CLASS_NAME_EDIT_TEXT = EditText::class.java.name
        private val CLASS_NAME_CHECK_BOX = CheckBox::class.java.name
        private val CLASS_NAME_RADIO_BUTTON = RadioButton::class.java.name
        private val CLASS_NAME_KEYBOARD_KEY = Key::class.java.name

        /**
         * Delay between up/down events for a simulated tap gesture
         * This can be quite short, the system doesn't mind
         */
        private val EVENT_DELAY_TAP = 1

        /**
         * Must be able to trigger an Android classic's long press and a SnapDrawing long press
         * - must be longer than kMaxTapHoldSeconds in TapGestureRecognizer.hpp
         * - must be longer than what AndroidDetectorGestureRecognizer considers a "onLongPress"
         */
        private val EVENT_DELAY_LONG_PRESS = 600
    }

}
