package com.snap.valdi.utils

import android.graphics.RectF
import android.view.View
import com.snap.valdi.attributes.impl.animations.transition.ValdiTransitionInfo
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.drawables.utils.BorderRadii
import com.snap.valdi.drawables.utils.DrawableInfoProvider
import com.snap.valdi.drawables.utils.MaskPathRenderer
import com.snap.valdi.views.touches.GestureRecognizers
import com.snapchat.client.valdi.utils.NativeHandleWrapper

class ValdiObjects: DrawableInfoProvider {
    var valdiContext: ValdiContext? = null
        set(value) {
            if (field !== value) {
                field = value
                invalidateViewNode()
            }
        }

    var valdiViewNodeId = 0
        set(value) {
            if (field != value) {
                field = value
                invalidateViewNode()
            }
        }

    val valdiViewNode: ValdiViewNode?
        get() {
            if (innerViewNode == null && hasValdiViewNode) {
                innerViewNode = valdiContext?.getTypedViewNodeForId(valdiViewNodeId)
            }

            return innerViewNode
        }

    val hasValdiViewNode: Boolean
        get() = valdiViewNodeId != 0

    var valdiTransitionInfo: ValdiTransitionInfo? = null
    var gestureRecognizers: GestureRecognizers? = null
    var nativeHandles: HashMap<String, NativeHandleWrapper>? = null
    var touchAreaInsets: RectF? = null
    override var borderRadii: BorderRadii? = null
    var isRightToLeft = false
    var calculatedX = 0
    var calculatedY = 0
    var calculatedWidth = 0
    var calculatedHeight = 0

    override var maskPathRenderer: MaskPathRenderer? = null
    var maskImageGradient: com.snap.valdi.attributes.impl.gradients.ValdiGradient? = null

    private var innerViewNode: ValdiViewNode? = null
    private var didFinishLayoutForKey: MutableMap<String, (view: View) -> Unit>? = null

    private fun invalidateViewNode() {
        val innerViewNode = this.innerViewNode
        if (innerViewNode != null) {
            this.innerViewNode = null
            innerViewNode.destroy()
        }
    }

    fun setDidFinishLayoutForKey(key: String, didFinishLayout: (view: View) -> Unit) {
        if (didFinishLayoutForKey == null) {
            didFinishLayoutForKey = mutableMapOf()
        }
        didFinishLayoutForKey!![key] = didFinishLayout
    }

    fun removeDidFinishLayoutForKey(key: String) {
        val didFinishLayoutForKey = this.didFinishLayoutForKey ?: return
        didFinishLayoutForKey.remove(key)

        if (didFinishLayoutForKey.isEmpty()) {
            this.didFinishLayoutForKey = null
        }
    }

    fun didApplyLayout(view: View) {
        val didFinishLayoutForKey = this.didFinishLayoutForKey ?: return
        didFinishLayoutForKey.values.forEach { it(view) }
    }

}
