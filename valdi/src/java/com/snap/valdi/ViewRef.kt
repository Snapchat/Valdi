package com.snap.valdi

import com.snap.valdi.attributes.impl.animations.ValdiValueAnimation

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Path
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import androidx.annotation.Keep
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.exceptions.messageWithCauses
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.extensions.removeFromParentView
import com.snap.valdi.utils.error
import com.snap.valdi.utils.TypedRef
import com.snap.valdi.utils.ViewRefSupport
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.snapdrawing.SnapDrawingRootHandle
import com.snap.valdi.utils.BitmapHandler
import com.snap.valdi.views.ValdiAssetReceiver
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.ValdiRecyclableView
import com.snap.valdi.views.ValdiContextMovedListener
import com.snap.valdi.views.ValdiRootView
import com.snap.valdi.views.ValdiScrollableView
import com.snap.valdi.views.ValdiTouchEventResult
import com.snap.valdi.views.ValdiTouchTarget
import com.snap.valdi.views.ValdiView
import com.snap.valdi.views.CustomChildViewAppender
import java.io.ByteArrayOutputStream
import java.lang.Exception

@Keep
class ViewRef(view: View, strong: Boolean, val support: ViewRefSupport): TypedRef<View>(view, strong) {

    fun onMovedToContext(valdiContext: ValdiContext, viewNodeId: Int) {
        val view = get() ?: return

        ViewUtils.setValdiContext(view, valdiContext)
        ViewUtils.setViewNodeId(view, viewNodeId)

        if (view is ValdiContextMovedListener) {
            view.onMovedToValdiContext(valdiContext)
        }
    }

    fun insertChild(childViewRef: ViewRef, viewIndex: Int) {
        val view = get() ?: return
        val childView = childViewRef.get() ?: return

        childView.removeFromParentView()

        if (view is CustomChildViewAppender) {
            view.addValdiChildView(childView, viewIndex)
            return
        }

        if (view !is ViewGroup) {
            support.logger.error("Cannot add $childView into parentView $view, parentView needs to be a ViewGroup")
            return
        }

        view.addView(childView, viewIndex)
    }

    fun removeFromParent(shouldClearViewNode: Boolean) {
        val view = get() ?: return

        if (shouldClearViewNode) {
            ViewUtils.setViewNodeId(view, 0)
        }
        view.removeFromParentView()
    }

    private fun doInvalidateLayout(view: View) {
        if (view is ValdiRootView) {
            view.onValdiLayoutInvalidated()
        } else {
            view.requestLayout()
        }
    }

    @Keep
    fun invalidateLayout() {
        val view = get() ?: return

        doInvalidateLayout(view)
    }

    private fun measureAndLayout(view: View, x: Int, y: Int, width: Int, height: Int, animating: Boolean) {
        ViewUtils.setCalculatedFrame(view, x, y, width, height)

        if (animating || view.parent == null) {
            val widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY)
            val heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY)

            view.measure(widthMeasureSpec, heightMeasureSpec)
            view.layout(x, y, width + x, height + y)

            ViewUtils.notifyDidApplyLayout(view)
        } else {
            view.requestLayout()
        }
    }

    fun onFrameChanged(x: Int, y: Int, width: Int, height: Int, isRightToLeft: Boolean, animator: Any?) {
        val view = get() ?: return

        val typedAnimator = animator as? ValdiAnimator

        val wasAnimating = ViewUtils.cancelAnimation(view, "frame")
        ViewUtils.setIsRightToLeft(view, isRightToLeft)

        if (typedAnimator == null) {
            measureAndLayout(view, x, y, width, height, false)
        } else {
            val fromX: Int
            val fromY: Int
            val fromWidth: Int
            val fromHeight: Int
            if (!typedAnimator.beginFromCurrentState || !wasAnimating) {
                fromX = ViewUtils.getCalculatedX(view)
                fromY = ViewUtils.getCalculatedY(view)
                fromWidth = ViewUtils.getCalculatedWidth(view)
                fromHeight = ViewUtils.getCalculatedHeight(view)
            } else {
                fromX = view.left
                fromY = view.top
                fromWidth = view.width
                fromHeight = view.height
            }

            val valueAnimation = ValdiValueAnimation(ValdiValueAnimation.MinimumVisibleChange.PIXEL) { progress ->
                val fractionX = fromX + ((x - fromX) * progress).toInt()
                val fractionY = fromY + ((y - fromY) * progress).toInt()
                val fractionWidth = fromWidth + ((width - fromWidth) * progress).toInt()
                val fractionHeight = fromHeight + ((height - fromHeight) * progress).toInt()

                measureAndLayout(view, fractionX, fractionY, fractionWidth, fractionHeight, true)
            }
            typedAnimator.addValueAnimation("frame", view, valueAnimation)
        }
    }

    fun onScrollSpecsChanged(contentOffsetX: Int,
                       contentOffsetY: Int,
                       contentWidth: Int,
                       contentHeight: Int,
                       animated: Boolean) {
        val view = get() ?: return

        if (view !is ValdiScrollableView) {
            return
        }
        view.onScrollSpecsChanged(contentOffsetX, contentOffsetY, contentWidth, contentHeight, animated)
    }

    @Keep
    fun measure(width: Int, widthMode: Int, height: Int, heightMode: Int): Long {
        val view = get() ?: return 0L

        if (view is ValdiView) {
            // If this view is a ValdiView, then its layout is fully managed by Yoga. This means that the intrinsic size is managed by Yoga as well, so we don't have more information to tell it.
            return 0L
        }
        val widthMeasureSpec = makeMeasureSpec(width, widthMode)
        val heightMeasureSpec = makeMeasureSpec(height, heightMode)

        view.measure(widthMeasureSpec, heightMeasureSpec)

        val result = ValdiViewNode.encodeHorizontalVerticalPair(view.measuredWidth, view.measuredHeight)
        view.requestLayout()
        return result
    }

    @Keep
    fun layout() {
        val view = get() as? ValdiRootView ?: return
        view.applyValdiLayout()
    }

    @Keep
    fun requestFocus() {
        val view = get() ?: return
        ViewUtils.resetFocusToRootViewOf(view)
    }

    @Keep
    fun cancelAllAnimations() {
        val view = get() ?: return
        ViewUtils.removeAllAnimations(view)
    }

    @Keep
    fun isRecyclable(): Boolean {
        return get() is ValdiRecyclableView
    }

    fun onLoadedAssetChanged(asset: Any?, shouldFlip: Boolean) {
        val view = get() as? ValdiAssetReceiver ?: return

        view.onAssetChanged(asset, shouldFlip)
    }

    fun willEnqueueToPool() {
        val view = get() ?: return

        if (view is ValdiRecyclableView) {
            view.prepareForRecycling()
        }

        ViewUtils.willEnqueueViewToPool(view)
    }

    private fun drawViewInCanvas(canvas: Canvas, bitmap: Bitmap, view: View, transform: Matrix?, shouldScale: Boolean) {
        val bitmapWidth = bitmap.width
        val bitmapHeight = bitmap.height
        val viewWidth = view.width
        val viewHeight = view.height

        canvas.setBitmap(bitmap)

        if (transform != null) {
            canvas.concat(transform)
        }

        if (shouldScale) {
            canvas.scale(bitmapWidth.toFloat() / viewWidth.toFloat(), bitmapHeight.toFloat() / viewHeight.toFloat())
        }

        view.draw(canvas)

        canvas.setBitmap(null)
    }

    @Keep
    fun raster(bitmap: Any?, left: Int, top: Int, right: Int, bottom: Int, rasterScaleX: Float, rasterScaleY: Float, transform: Any?): String? {
        try {
            val view = get() ?: return "Cannot resolve view"

            if (view.parent == null) {
                measureAndLayout(view, left, top, right - left, bottom - top, false)
            }

            val renderCanvas = Canvas()

            val transformMatrix = Matrix()

            val resolvedRasterScaleX = this.support.coordinateResolver.fromPixel(rasterScaleX)
            val resolvedRasterScaleY = this.support.coordinateResolver.fromPixel(rasterScaleY)

            transformMatrix.preScale(resolvedRasterScaleX.toFloat(), resolvedRasterScaleY.toFloat())
            (transform as? FloatArray)?.let { values ->
                Matrix().apply {
                    setValues(values)
                    transformMatrix.preConcat(this)
                }
            }

            transformMatrix.preTranslate(left.toFloat(), top.toFloat())

            drawViewInCanvas(renderCanvas, bitmap as Bitmap, view, transformMatrix, false)

            return null
        } catch (exc: Exception) {
            this.support.logger.log(
                LogLevel.ERROR,
                exc,
                "Failed to raster view"
            )
            return exc.messageWithCauses()
        }
    }

    @Keep
    fun snapshot(callback: ValdiFunction) {
        var bitmap: BitmapHandler? = null

        fun handleError() {
            ValdiMarshaller.use {
                it.pushUndefined()
                callback.perform(it)
            }
            bitmap?.release()
        }

        val view = get() ?: return handleError()

        val width = view.width
        val height = view.height

        if (width < 1 || height < 1) {
            return handleError()
        }

        try {
            val renderCanvas = this.support.getRenderCanvas()

            bitmap = this.support.bitmapPool.get(width, height) ?: return handleError()

            drawViewInCanvas(renderCanvas, bitmap.getBitmap(), view, null, true)

            val output = ByteArrayOutputStream()

            this.support.getSnapshotExecutor().submit {
                bitmap.getBitmap().compress(Bitmap.CompressFormat.PNG, 0, output)
                ValdiMarshaller.use {
                    it.pushByteArray(output.toByteArray())
                    callback.perform(it)
                }
                bitmap?.release()
            }
        } catch (exc: Exception) {
            this.support.logger.log(LogLevel.ERROR, exc, "Failed to take Snapshot of view with size ${width}x${height}")
            handleError()
        }
    }

    @Keep
    fun onTouchEvent(
            initialEventAgoMs: Long,
            touchEventType: Int,
            x: Float,
            y: Float,
            originalTouchEvent: Any?
            ): Boolean {
        val touchEventTypeValue = SnapDrawingRootHandle.TouchEventType.fromRawInt(touchEventType) ?: return false
        if (originalTouchEvent !is MotionEvent) {
            return false
        }

        val motionEventAction = touchEventTypeValue.toMotionEventAction()

        val adjustedMotionEvent = MotionEvent.obtain(
                originalTouchEvent.downTime,
                originalTouchEvent.eventTime + initialEventAgoMs,
                motionEventAction,
                x,
                y,
                originalTouchEvent.metaState
        )

        return try {
            processTouchEvent(adjustedMotionEvent)
        } finally {
            adjustedMotionEvent.recycle()
        }
    }

    @Keep
    fun customizedHitTest(x: Float, y: Float): Int {
        val view = get() ?: return CUSTOMIZED_HIT_TEST_RESULT_USE_DEFAULT
        if (!view.isEnabled || view.alpha == 0f || view.visibility == View.INVISIBLE) {
            return CUSTOMIZED_HIT_TEST_RESULT_NOT_HIT
        }

        val touchTarget = view as? ValdiTouchTarget ?: return CUSTOMIZED_HIT_TEST_RESULT_USE_DEFAULT

        val motionEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_DOWN, x, y, 0)
        try {
            return when (touchTarget.hitTest(motionEvent)) {
                null -> CUSTOMIZED_HIT_TEST_RESULT_USE_DEFAULT
                true -> CUSTOMIZED_HIT_TEST_RESULT_HIT
                false -> CUSTOMIZED_HIT_TEST_RESULT_NOT_HIT
            }
        } finally {
            motionEvent.recycle()
        }
    }

    private fun processTouchEvent(event: MotionEvent): Boolean {
        val touchTarget = get() as? ValdiTouchTarget ?: return false
        val touchEventResult = touchTarget.processTouchEvent(event)
        if (touchEventResult === ValdiTouchEventResult.ConsumeEventAndCancelOtherGestures) {
            return true
        }
        return false
    }

    @Keep
    fun getViewClassName(): String {
        return get()?.javaClass?.name ?: ""
    }

    companion object {
        const val CUSTOMIZED_HIT_TEST_RESULT_USE_DEFAULT = 0
        const val CUSTOMIZED_HIT_TEST_RESULT_HIT = 1
        const val CUSTOMIZED_HIT_TEST_RESULT_NOT_HIT = 2

        @JvmStatic
        private fun viewMeasureSpecFromYogaMeasureMode(mode: Int): Int {
            return if (mode == 2) {
                View.MeasureSpec.AT_MOST
            } else {
                if (mode == 1) {
                    View.MeasureSpec.EXACTLY
                } else {
                    View.MeasureSpec.UNSPECIFIED
                }
            }
        }

        @JvmStatic
        fun makeMeasureSpec(value: Int, mode: Int): Int {
            return View.MeasureSpec.makeMeasureSpec(value, viewMeasureSpecFromYogaMeasureMode(mode))
        }
    }

}
