package com.snap.valdi.views

import android.content.Context
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.os.Build
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import androidx.annotation.Keep
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.utils.CanvasClipper
import com.snap.valdi.views.touches.DragGestureRecognizer
import com.snap.valdi.ValdiRuntimeManager

/**
 * View used by all container views in Valdi. Will contain the ValdiContext, which
 * uniquely identifies a Valdi view tree.
 */
@Keep
open class ValdiView : ViewGroup, ValdiRecyclableView, ValdiClippableView, ValdiForegroundHolder {
    constructor(context: Context) : super(context)
    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs)

    override var clipToBounds = false
        set(value) {
            if (field != value) {
                field = value
                updateLayer()
            }
        }

    override val clipToBoundsDefaultValue: Boolean
        get() = false

    /**
     * The ValdiContext to which this ValdiView belongs to.
     */
    val valdiContext: ValdiContext?
        get() = ViewUtils.findValdiContext(this)

    val valdiViewNode: ValdiViewNode?
        get() = ViewUtils.findViewNode(this)

    private val clipperBounds = Rect(0, 0, 0, 0)
    override val clipper = CanvasClipper()

    override var valdiForeground: Drawable?
        get() = valdiForegroundField
        set(value) {
            valdiForegroundField = value
        }
    private var valdiForegroundField: Drawable? = null

    init {
        clipChildren = false
    }

    fun hasDragGestureRecognizer(): Boolean {
        val gestureRecognizer = ViewUtils.getGestureRecognizers(this, false) ?: return false
        return gestureRecognizer.hasGestureRecognizer(DragGestureRecognizer::class.java)
    }

    fun getDragGestureRecognizer(): DragGestureRecognizer? {
        val gestureRecognizer = ViewUtils.getGestureRecognizers(this, false) ?: return null
        return gestureRecognizer.getGestureRecognizer(DragGestureRecognizer::class.java)
    }

    override fun setClipChildren(clipChildren: Boolean) {
        invalidate()
        super.setClipChildren(clipChildren)
    }

    override fun setBackground(background: Drawable?) {
        invalidate()
        super.setBackground(background)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        updateLayer()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        ViewUtils.measureValdiChildren(this)

        setMeasuredDimension(width, height)
    }

    override fun onLayout(p0: Boolean, l: Int, t: Int, r: Int, b: Int) {
        ViewUtils.applyLayoutToValdiChildren(this)
    }

    open fun onMovedToValdiContext(valdiContext: ValdiContext) {

    }

    internal fun movedToValdiContext(valdiContext: ValdiContext) {
        onMovedToValdiContext(valdiContext)
    }

    private fun isLargeView(): Boolean {
        // use the main runtime manager if the current view does not have one
        val viewloadeManager = valdiContext?.runtimeOrNull?.manager ?: ValdiRuntimeManager.allRuntimes().firstOrNull()?.manager
        val maxRenderTargetSize = viewloadeManager?.snapDrawingMaxRenderTargetSize ?: -1
        // Some devices (such as Samsung A05) have max render target size 16383 (not a power of 2 size)
        // This particular size triggers a crash, Ticket: 3560
        // This is because Android rounds sizes up to the nearest multiplier of 64. For size limit 16383,
        // anything > 16320 will cause Android to attemp a hardware layer of size 16384, exceeding the
        // hardware limit.
        if (maxRenderTargetSize == 16383L) {
            return width > 16320L || height > 16320L
        } else {
            return false;       // -1 means max size is n/a
        }
    }

    private fun needsSoftwareLayer(): Boolean {
        if (isLargeView()) {
            return true
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return false
        }

        if (!clipToBounds || !clipper.hasNonNullCornerRadius) {
            return false
        }

        // On Android 8 and below, clipping path is broken if the view or a parent
        // has a rotation set.
        var current = this as? ValdiView
        while (current != null) {
            if (current.rotation != 0f) {
                return true
            }

            current = current.parent as? ValdiView
        }

        return false
    }

    private fun updateLayer() {
        val desiredLayerType = if (needsSoftwareLayer()) View.LAYER_TYPE_SOFTWARE else View.LAYER_TYPE_NONE
        if (desiredLayerType != layerType) {
            setLayerType(desiredLayerType, null)
        }
    }

    override fun hasOverlappingRendering(): Boolean {
        return ViewUtils.hasOverlappingRendering(this)
    }

    override fun setRotation(rotation: Float) {
        super.setRotation(rotation)

        updateLayer()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        updateLayer()
        super.onSizeChanged(w, h, oldw, oldh)
    }

    override fun verifyDrawable(who: Drawable): Boolean {
        return ViewUtils.verifyDrawable(this, who) || super.verifyDrawable(who)
    }

    // maskImage wraps entire draw (background + content + children)
    override fun draw(canvas: Canvas) {
        val maskImageGradient = ViewUtils.getOptionalImageMaskGradient(this)
        if (maskImageGradient != null && width > 0 && height > 0) {
            val saveCount = canvas.saveLayer(0f, 0f, width.toFloat(), height.toFloat(), null)
            super.draw(canvas)
            ViewUtils.drawImageMaskGradient(canvas, width.toFloat(), height.toFloat(), maskImageGradient)
            canvas.restoreToCount(saveCount)
        } else {
            super.draw(canvas)
        }
    }

    // clipping support

    override fun dispatchDraw(canvas: Canvas) {
        if (clipToBounds) {
            clipperBounds.right = width
            clipperBounds.bottom = height

            clipper.clip(canvas, clipperBounds)
        }

        ViewUtils.dispatchDraw(this, canvas) {
            super.dispatchDraw(canvas)
        }
    }
}
