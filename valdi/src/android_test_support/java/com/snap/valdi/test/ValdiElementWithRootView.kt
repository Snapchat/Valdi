package com.snap.valdi.test

import android.graphics.Rect
import android.graphics.RectF
import com.snap.valdi.nodes.IValdiViewNode
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.ValdiRootView

class ValdiElementWithRootView(
    val rootView: ValdiRootView,
    val viewNode: IValdiViewNode,
    val parent: ValdiElementWithRootView?,
    val coordinateResolver: CoordinateResolver
) {
    constructor(rootView: ValdiRootView, viewNode: IValdiViewNode) :
        this(rootView, viewNode, null, CoordinateResolver(rootView.context))

    val viewParent: ValdiElementWithRootView?
        get() {
            if (parent == null) {
                return null
            }

            if (!parent.isLayout) {
                return parent
            }

            return parent.viewParent
        }

    val isLayout: Boolean
        get() = viewNode.viewClassName == "Layout"

    fun withChildViewNode(viewNode: IValdiViewNode): ValdiElementWithRootView {
        return ValdiElementWithRootView(rootView, viewNode, this, coordinateResolver)
    }

    fun getVisualFrameFromRootView(): Rect {
        val absoluteFrameOfViewNode = RectF()
        viewNode.getVisualAbsoluteFrame(absoluteFrameOfViewNode)

        return Rect(
            absoluteFrameOfViewNode.left.toInt(),
            absoluteFrameOfViewNode.top.toInt(),
            absoluteFrameOfViewNode.right.toInt(),
            absoluteFrameOfViewNode.bottom.toInt()
        )
    }

    override fun toString(): String {
        return "Element ${viewNode.viewClassName} with id ${viewNode.id}"
    }

    fun children(): List<ValdiElementWithRootView> {
        return viewNode.getVisibleChildren().map { withChildViewNode(it) }
    }

    fun viewChildren(): List<ValdiElementWithRootView> {
        val collectedViewChildren = mutableListOf<ValdiElementWithRootView>()
        appendViewChildren(collectedViewChildren)
        return collectedViewChildren
    }

    private fun appendViewChildren(out: MutableList<ValdiElementWithRootView>) {
        for (child in children()) {
            if (child.isLayout) {
                child.appendViewChildren(out)
            } else {
                out.add(child)
            }
        }
    }
}
