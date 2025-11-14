package com.snap.valdi.views

import android.content.Context
import android.graphics.Canvas
import android.graphics.Point
import android.graphics.Rect
import android.os.Build
import android.util.AttributeSet
import android.view.View
import android.widget.OverScroller
import androidx.annotation.Keep
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.views.touches.ValdiGestureRecognizerState
import com.snap.valdi.views.touches.DragGestureRecognizer
import com.snap.valdi.views.touches.DragGestureRecognizerListener
import com.snap.valdi.views.touches.ScrollViewDragGestureRecognizer
import kotlin.math.roundToInt
import android.util.Xml.asAttributeSet
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.touches.ValdiGesturePointer

enum class KeyboardDismissMode(val value: String) {
    IMMEDIATE("immediate"),
    TOUCH_EXIT_BELOW("touch-exit-below"),
    TOUCH_EXIT_ABOVE("touch-exit-above");
    
    companion object {
        fun fromString(value: String): KeyboardDismissMode =
            values().find { it.value.equals(value, ignoreCase = true) } ?: IMMEDIATE
    }
}

/**
 * A scroll view container that uses Yoga for its layout and can be either
 * horizontal or vertical.
 */
@Keep
open class ValdiScrollView(context: Context) : ValdiView(context, attributeSet(context)),
        ValdiScrollableView, CustomChildViewAppender, Runnable, DragGestureRecognizerListener {

    companion object {
        fun attributeSet(context: Context): AttributeSet? {
            return if (Build.VERSION.SDK_INT == Build.VERSION_CODES.KITKAT) {
                try {
                    val parser = context.resources.getXml(com.snapchat.client.R.xml.valdi_scroll_view_kitkat)
                    parser.next()
                    parser.nextTag()
                    asAttributeSet(parser)
                } catch (e: Exception) {
                    null
                }
            } else {
                null
            }
        }

        val flingDecelerationRate = 0.998
        val flingDecelerationCoefficient = 1000 * Math.log(flingDecelerationRate)
        val flingDecelerationCorrection = 1 / -flingDecelerationCoefficient
    }

    // TODO(796) - deprecate, only used in android on legacy tray integration
    interface ScrollChangeListener {
        fun onScrollChange(newOffset: Int, oldOffset: Int)
    }

    var onScrollChangeListener: ScrollChangeListener? = null

    var scrollEnabled: Boolean = true
    var pagingEnabled: Boolean = false

    var scrollPerfLoggerBridge: IScrollPerfLoggerBridge? = null
        set(value) {
            if (field != value) {
                pauseScrollPerfLogger()
                field = value
            }
        }

    val contentView = ValdiView(context)

    val coordinateResolver = CoordinateResolver(context)

    var contentWidth: Int = 0
        private set
    var contentHeight: Int = 0
        private set

    var contentOffsetX: Int = 0
        private set
    var contentOffsetY: Int = 0
        private set

    var unclampedContentOffsetX: Int = 0
        private set
    var unclampedContentOffsetY: Int = 0
        private set

    var horizontalScroll: Boolean = false
        set(value) {
            if (field != value) {
                field = value

                dragGestureRecognizer.isHorizontalScroll = value

                requestLayout()
            }
        }

    private var edgeEffect: Boolean = true
        set(value) {
            field = value
            if (!edgeEffect) {
                forEachEdgeEffectWrapper { it.finish() }
            }
            updateWillNotDraw()
        }

    var bounces: Boolean
        get() = dragGestureRecognizer.bounces
        set(value) { dragGestureRecognizer.bounces = value }

    var bouncesFromDragAtStart: Boolean
        get() = dragGestureRecognizer.bouncesFromDragAtStart
        set(value) { dragGestureRecognizer.bouncesFromDragAtStart = value }

    var bouncesFromDragAtEnd: Boolean
        get() = dragGestureRecognizer.bouncesFromDragAtEnd
        set(value) { dragGestureRecognizer.bouncesFromDragAtEnd = value }

    var bouncesHorizontalWithSmallContent: Boolean
        get() = dragGestureRecognizer.bouncesHorizontalWithSmallContent
        set(value) { dragGestureRecognizer.bouncesHorizontalWithSmallContent = value }

    var bouncesVerticalWithSmallContent: Boolean
        get() = dragGestureRecognizer.bouncesVerticalWithSmallContent
        set(value) { dragGestureRecognizer.bouncesVerticalWithSmallContent = value }

    var cancelsTouchesOnScroll: Boolean
        get() = dragGestureRecognizer.cancelsTouchesOnScroll
        set(value) { dragGestureRecognizer.cancelsTouchesOnScroll = value }

    var dismissKeyboardOnDrag = false

    var dismissKeyboardMode = KeyboardDismissMode.IMMEDIATE

    private val scroller = OverScroller(context)

    private val contentInsetRect = Rect(0, 0, 0, 0)
    private var initialUnclampedContentOffsetX = 0
    private var initialUnclampedContentOffsetY = 0
    private var isAnimatingScroll = false
        set(value) {
            field = value
            dragGestureRecognizer.isAnimatingScroll = value
        }

    private var scrollPerfLoggerStarted = false

    private var isAnimatingFling = false
    private var isUpdatingScrollSpecs = false
    private val dragGestureRecognizer: ScrollViewDragGestureRecognizer
    private val leftEdgeEffect = EdgeEffectWrapper(context, Edge.LEFT)
    private val topEdgeEffect = EdgeEffectWrapper(context, Edge.TOP)
    private val rightEdgeEffect = EdgeEffectWrapper(context, Edge.RIGHT)
    private val bottomEdgeEffect = EdgeEffectWrapper(context, Edge.BOTTOM)

    init {
        addView(contentView)
        dragGestureRecognizer = ScrollViewDragGestureRecognizer(this, this)
        ViewUtils.addGestureRecognizer(this, dragGestureRecognizer)

        isHorizontalScrollBarEnabled = true
        isVerticalScrollBarEnabled = true
        isScrollbarFadingEnabled = true

        updateWillNotDraw()

        /**
         * Clipping of ScrollView contents is enabled by default. But, there could be perf issues
         * if clipping is enabled when the ScrollView has border-radius since this will result in
         * an expensive call to `clipPath()`. If the ScrollView has border-radius then you should
         * consider disabling clipping.
         */
        clipToBounds = true
    }

    override val clipToBoundsDefaultValue: Boolean
        get() = true

    override fun setHorizontalScrollBarEnabled(horizontalScrollBarEnabled: Boolean) {
        super.setHorizontalScrollBarEnabled(horizontalScrollBarEnabled)

        updateWillNotDraw()
    }

    override fun setVerticalScrollBarEnabled(verticalScrollBarEnabled: Boolean) {
        super.setVerticalScrollBarEnabled(verticalScrollBarEnabled)

        updateWillNotDraw()
    }

    private fun updateWillNotDraw() {
        val shouldDraw = edgeEffect || isHorizontalScrollBarEnabled || isVerticalScrollBarEnabled
        if (willNotDraw() != !shouldDraw) {
            setWillNotDraw(!shouldDraw)
        }
    }

    override fun onMovedToValdiContext(valdiContext: ValdiContext) {
        super.onMovedToValdiContext(valdiContext)

        updateScrollDirection()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        val width = measuredWidth
        val height = measuredHeight

        val scrollViewWidth = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY)
        val scrollViewHeight = MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY)
        contentView.measure(scrollViewWidth, scrollViewHeight)
    }

    override fun onLayout(p0: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val width = r - l
        val height = b - t

        forEachEdgeEffectWrapper { it.setSize(width, height) }

        contentView.layout(0, 0, width, height)

        super.onLayout(p0, l, t, r, b)
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)

        var needsInvalidate = false
        if (edgeEffect) {
            forEachEdgeEffectWrapper {
                if (!it.isFinished) {
                    if (it.draw(canvas, width, height)) {
                        needsInvalidate = true
                    }
                }
            }
        }

        if (needsInvalidate) {
            postInvalidateOnAnimation()
        }
    }

    private fun updateScrollDirection() {
        horizontalScroll = valdiViewNode?.isLayoutDirectionHorizontal == true
    }

    override fun addValdiChildView(childView: View, viewIndex: Int) {
        contentView.addView(childView, viewIndex)

        updateScrollDirection()
    }

    private fun cancelScrollAnimation() {
        scroller.abortAnimation()
        isAnimatingScroll = false
        isAnimatingFling = false
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        cancelScrollAnimation()
        pauseScrollPerfLogger()
    }

    private fun getMinContentOffsetX(): Int {
        return -contentInsetRect.left
    }

    private fun getMinContentOffsetY(): Int {
        return -contentInsetRect.top
    }

    private fun getMaxContentOffsetX(): Int {
        return Math.max(0, contentWidth - width + contentInsetRect.right)
    }

    private fun getMaxContentOffsetY(): Int {
        return Math.max(0, contentHeight - height + contentInsetRect.bottom)
    }

    private fun releaseEdgeEffect(edgeEffectWrapper: EdgeEffectWrapper?): Boolean {
        if (edgeEffectWrapper == null) {
            return false
        }
        if (edgeEffectWrapper.isFinished) {
            return false
        }
        edgeEffectWrapper.onRelease()
        return true
    }

    private fun pullEdgeEffect(edge: EdgeEffectWrapper, offsetDifference: Int, collidedSideSize: Int, otherSideSize: Int, otherSideTouchLocation: Int) {
        val distance = Math.abs(offsetDifference).toFloat() / collidedSideSize.toFloat()
        val displacement = otherSideTouchLocation.toFloat() / otherSideSize.toFloat()

        edge.onPull(distance, displacement)
    }

    private fun handleScroll(offsetX: Int, offsetY: Int, x: Int, y: Int, invertedVelocityX: Float, invertedVelocityY: Float) {
        var newUnclampedContentOffsetX = offsetToUnclampedContentOffsetX(offsetX)
        var newUnclampedContentOffsetY = offsetToUnclampedContentOffsetY(offsetY)

        val newContentOffsetX = newUnclampedContentOffsetX.coerceIn(getMinContentOffsetX(), getMaxContentOffsetX())
        val newContentOffsetY = newUnclampedContentOffsetY.coerceIn(getMinContentOffsetY(), getMaxContentOffsetY())

        var needInvalidate = false
        if (edgeEffect) {
            val offsetXDifference = newUnclampedContentOffsetX - newContentOffsetX
            val offsetYDifference = newUnclampedContentOffsetY - newContentOffsetY

            if (offsetXDifference != 0) {
                needInvalidate = true
                val edge = if (offsetXDifference > 0) rightEdgeEffect else leftEdgeEffect
                pullEdgeEffect(edge, offsetXDifference, width, height, y)
            } else {
                if (releaseEdgeEffect(leftEdgeEffect) || releaseEdgeEffect(rightEdgeEffect)) {
                    needInvalidate = true
                }
            }

            if (offsetYDifference != 0) {
                needInvalidate = true
                val edge = if (offsetYDifference > 0) bottomEdgeEffect else topEdgeEffect
                pullEdgeEffect(edge, offsetYDifference, height, width, x)
            } else {
                if (releaseEdgeEffect(topEdgeEffect) || releaseEdgeEffect(bottomEdgeEffect)) {
                    needInvalidate = true
                }
            }
        } else {
            newUnclampedContentOffsetX = newContentOffsetX
            newUnclampedContentOffsetY = newContentOffsetY
        }

        applyContentOffset(
            newContentOffsetX,
            newContentOffsetY,
            newUnclampedContentOffsetX,
            newUnclampedContentOffsetY,
            invertedVelocityX,
            invertedVelocityY
        )

        if (awakenScrollBars()) {
            // awakenScrollBar will invalidate already.
            needInvalidate = false
        }

        if (needInvalidate) {
            postInvalidateOnAnimation()
        }
    }

    override fun shouldBegin(
        gesture: DragGestureRecognizer,
        x: Int,
        y: Int,
        offsetX: Int,
        offsetY: Int,
        invertedVelocityX: Float,
        invertedVelocityY: Float,
        pointerCount: Int,
        pointerLocations: List<ValdiGesturePointer>
    ): Boolean {
        return scrollEnabled
    }

    override fun onRecognized(
        gesture: DragGestureRecognizer,
        state: ValdiGestureRecognizerState,
        x: Int,
        y: Int,
        offsetX: Int,
        offsetY: Int,
        invertedVelocityX: Float,
        invertedVelocityY: Float,
        pointerCount: Int,
        pointerLocations: List<ValdiGesturePointer>
    ) {
        cancelScrollAnimation()

        when (state) {
            ValdiGestureRecognizerState.BEGAN -> {
                initialUnclampedContentOffsetX = unclampedContentOffsetX
                initialUnclampedContentOffsetY = unclampedContentOffsetY
                if (dismissKeyboardOnDrag && dismissKeyboardMode == KeyboardDismissMode.IMMEDIATE) {
                    ViewUtils.resetFocusToRootViewOf(this)
                }
                resumeScrollPerfLogger()
                valdiViewNode?.notifyScroll(
                    ValdiViewNode.SCROLL_EVENT_TYPE_ON_DRAG_START,
                    contentOffsetX,
                    contentOffsetY,
                    unclampedContentOffsetX,
                    unclampedContentOffsetY,
                    invertedVelocityX,
                    invertedVelocityY
                )
                handleScroll(offsetX, offsetY, x, y, invertedVelocityX, invertedVelocityY)
            }
            ValdiGestureRecognizerState.CHANGED -> {
                handleScroll(offsetX, offsetY, x, y, invertedVelocityX, invertedVelocityY)

                if (dismissKeyboardOnDrag) {
                    when (dismissKeyboardMode) {
                        KeyboardDismissMode.TOUCH_EXIT_BELOW -> {
                            if (y > this.getHeight()) {
                                ViewUtils.resetFocusToRootViewOf(this)
                            }
                        }
                        KeyboardDismissMode.TOUCH_EXIT_ABOVE -> {
                            if (y < 0) {
                                ViewUtils.resetFocusToRootViewOf(this)
                            }
                        }
                        KeyboardDismissMode.IMMEDIATE -> {
                            // noop - this exited on ValdiGestureRecognizerState.BEGAN
                        }
                    }
                }
            }
            ValdiGestureRecognizerState.ENDED -> {

                // Should we invalidate the edgeEffect
                var shouldInvalidate = false
                forEachEdgeEffectWrapper {
                    if (releaseEdgeEffect(it)) {
                        shouldInvalidate = true
                    }
                }
                if (shouldInvalidate) {
                    postInvalidateOnAnimation()
                }

                // Check user's onDragEnding result
                var targetContentOffsetOverride = onDragEnding(invertedVelocityX, invertedVelocityY)

                // If paging is enabled, compute a pagination rounded target contentOffset
                if (pagingEnabled) {
                    targetContentOffsetOverride = computeTargetOffsetWithPaging(
                        targetContentOffsetOverride,
                        invertedVelocityX,
                        invertedVelocityY
                    )
                }

                // If we have a custom target contentOffset, animate to it
                if (targetContentOffsetOverride != null) {
                    setContentOffset(targetContentOffsetOverride.x, targetContentOffsetOverride.y, invertedVelocityX, invertedVelocityY, true)
                } else {
                    // Otherwise use the standard scrolling computations (use the gestures's fling state)
                    var newUnclampedContentOffsetX = offsetToUnclampedContentOffsetX(offsetX)
                    var newUnclampedContentOffsetY = offsetToUnclampedContentOffsetY(offsetY)

                    val maxOffsetX = getMaxContentOffsetX()
                    val maxOffsetY = getMaxContentOffsetY()

                    val newContentOffsetX = newUnclampedContentOffsetX.coerceIn(getMinContentOffsetX(), maxOffsetX)
                    val newContentOffsetY = newUnclampedContentOffsetY.coerceIn(getMinContentOffsetY(), maxOffsetY)

                    if (!edgeEffect) {
                        newUnclampedContentOffsetX = newContentOffsetX
                        newUnclampedContentOffsetY = newContentOffsetY
                    }

                    if (newContentOffsetX != newUnclampedContentOffsetX || newContentOffsetY != newUnclampedContentOffsetY) {
                        // We're overscrolling, animate back to where we're supposed to be
                        animateContentOffset(newContentOffsetX, newContentOffsetY, false)
                    } else {
                        applyContentOffset(
                            newContentOffsetX,
                            newContentOffsetY,
                            newUnclampedContentOffsetX,
                            newUnclampedContentOffsetY,
                            invertedVelocityX,
                            invertedVelocityY
                        )

                        initialUnclampedContentOffsetX = unclampedContentOffsetX
                        initialUnclampedContentOffsetY = unclampedContentOffsetY

                        val startX = initialUnclampedContentOffsetX
                        val startY = initialUnclampedContentOffsetY
                        val velocityX = -invertedVelocityX.toInt() * if (this.scaleX < 0f) { -1 } else { 1 }
                        val velocityY = -invertedVelocityY.toInt() * if (this.scaleY < 0f) { -1 } else { 1 }
                        if (horizontalScroll) {
                            scroller.fling(startX, startY, velocityX, velocityY, Int.MIN_VALUE, Int.MAX_VALUE, 0, maxOffsetY)
                        } else {
                            scroller.fling(startX, startY, velocityX, velocityY, 0, maxOffsetX, Int.MIN_VALUE, Int.MAX_VALUE)
                        }

                        isAnimatingFling = true
                        postScrollAnimation()
                    }
                }
            }
            else -> return
        }
    }

    private fun offsetToUnclampedContentOffsetX(offset: Int): Int {
        return if (horizontalScroll) initialUnclampedContentOffsetX - offset else initialUnclampedContentOffsetX
    }

    private fun offsetToUnclampedContentOffsetY(offset: Int): Int {
        return if (!horizontalScroll) initialUnclampedContentOffsetY - offset else initialUnclampedContentOffsetY
    }

    fun setContentInset(left: Int, top: Int, right: Int, bottom: Int) {
        contentInsetRect.set(left, top, right, bottom)
        requestLayout()
    }

    private fun computePostFlingPagedOffset(
        invertedVelocityX: Float,
        invertedVelocityY: Float
    ): Point {
        val flingRangeX: Float = measuredWidth.toFloat() / 2
        val flingRangeY: Float = measuredHeight.toFloat() / 2
        val flingRawDistanceX: Float = -invertedVelocityX * ValdiScrollView.flingDecelerationCorrection.toFloat()
        val flingRawDistanceY: Float = -invertedVelocityY * ValdiScrollView.flingDecelerationCorrection.toFloat()
        val flingClampedDistanceX: Float = flingRawDistanceX.coerceIn(-flingRangeX, +flingRangeX)
        val flingClampedDistanceY: Float = flingRawDistanceY.coerceIn(-flingRangeY, +flingRangeY)
        return Point(
            contentOffsetX + flingClampedDistanceX.toInt(),
            contentOffsetY + flingClampedDistanceY.toInt()
        )
    }

    private fun computeTargetOffsetWithPaging(
        targetContentOffsetOverride: Point?,
        invertedVelocityX: Float,
        invertedVelocityY: Float
    ): Point? {
        val pageSize: Int = if (horizontalScroll) measuredWidth else measuredHeight
        if (pageSize <= 0) {
            return targetContentOffsetOverride
        }

        var targetContentOffset = targetContentOffsetOverride
        if (targetContentOffset == null) {
            targetContentOffset = computePostFlingPagedOffset(invertedVelocityX, invertedVelocityY)
        }

        val maxContentOffset: Int = if (horizontalScroll) getMaxContentOffsetX() else getMaxContentOffsetY()
        val minContentOffset: Int = if (horizontalScroll) getMinContentOffsetX() else getMinContentOffsetY()

        val futureRawContentOffset: Int = if (horizontalScroll) targetContentOffset.x else targetContentOffset.y
        val futurePagedContentOffset: Int = Math.round(futureRawContentOffset.toFloat() / pageSize.toFloat()) * pageSize
        val futureClampedContentOffset: Int = futurePagedContentOffset.coerceIn(minContentOffset, maxContentOffset)

        if (horizontalScroll) {
            return Point(futureClampedContentOffset, 0)
        } else {
            return Point(0, futureClampedContentOffset)
        }
    }

    private fun onDragEnding(invertedVelocityX: Float, invertedVelocityY: Float): Point? {
        val viewNode = valdiViewNode ?: return null

        val pointOverride = viewNode.notifyScroll(
            ValdiViewNode.SCROLL_EVENT_TYPE_ON_DRAG_ENDING,
            contentOffsetX,
            contentOffsetY,
            unclampedContentOffsetX,
            unclampedContentOffsetY,
            invertedVelocityX,
            invertedVelocityY
        )

        // We use 0xFFFFFFFFFFFFFFFF to represent no scroll override
        if (pointOverride == Long.MIN_VALUE) {
            return null
        }

        val x = ValdiViewNode.horizontalFromEncodedLong(pointOverride)
        val y = ValdiViewNode.verticalFromEncodedLong(pointOverride)
        return Point(x, y)
    }

    private fun applyContentOffsetInternal(x: Int, y: Int, unclampedX: Int, unclampedY: Int) {
        contentOffsetX = x
        contentOffsetY = y

        unclampedContentOffsetX = unclampedX
        unclampedContentOffsetY = unclampedY

        contentView.scrollTo(x, y)
    }

    private fun applyContentOffset(x: Int, y: Int, unclampedX: Int, unclampedY: Int, invertedVelocityX: Float, invertedVelocityY: Float) {
        val oldContentOffsetX = contentOffsetX
        val oldContentOffsetY = contentOffsetY

        var newOffsetX = x
        var newOffsetY = y

        val viewNode = this.valdiViewNode
        if (viewNode != null && !isUpdatingScrollSpecs) {
            val updatedOffset = viewNode.notifyScroll(
                ValdiViewNode.SCROLL_EVENT_TYPE_ON_SCROLL,
                x,
                y,
                unclampedX,
                unclampedY,
                invertedVelocityX,
                invertedVelocityY
            )

            if (updatedOffset != Long.MIN_VALUE) {
                newOffsetX = ValdiViewNode.horizontalFromEncodedLong(updatedOffset)
                newOffsetY = ValdiViewNode.verticalFromEncodedLong(updatedOffset)
                initialUnclampedContentOffsetX += newOffsetX - x
                initialUnclampedContentOffsetY += newOffsetY - y
            }
        }

        applyContentOffsetInternal(newOffsetX, newOffsetY, unclampedX, unclampedY)

        // TODO(796) - deprecate, only used in android on legacy tray integration
        if (horizontalScroll) {
            onScrollChangeListener?.onScrollChange(contentOffsetX, oldContentOffsetX)
        } else {
            onScrollChangeListener?.onScrollChange(contentOffsetY, oldContentOffsetY)
        }
    }

    fun animateContentOffset(x: Int, y: Int, fast: Boolean) {
        initialUnclampedContentOffsetX = unclampedContentOffsetX
        initialUnclampedContentOffsetY = unclampedContentOffsetY
        val startX = initialUnclampedContentOffsetX
        val startY = initialUnclampedContentOffsetY
        val distanceX = x - startX
        val distanceY = y - startY
        if (fast) {
            scroller.startScroll(startX, startY, distanceX, distanceY) // default 250ms animation ("fast" for ios)
        } else {
            scroller.startScroll(startX, startY, distanceX, distanceY, 600) // approx match iOS "slow" duration
        }
        postScrollAnimation()
    }

    fun setContentOffset(x: Int, y: Int, invertedVelocityX: Float, invertedVelocityY: Float, animated: Boolean) {
        // Cancel any previously in-progress animation
        cancelScrollAnimation()
        // If we're not moving, just end the scroll here
        if (x == contentOffsetX && y == contentOffsetY && x == unclampedContentOffsetX && y == unclampedContentOffsetY) {
            notifyScrollEnded(invertedVelocityX, invertedVelocityY)
            return
        }
        // If we're about to start an animation
        if (animated) {
            animateContentOffset(x, y, true)
        } else {
            // If we just want to snap or stay in the specified position, just do it now
            applyContentOffset(x, y, x, y, invertedVelocityX, invertedVelocityY)
            notifyScrollEnded(invertedVelocityX, invertedVelocityY)
        }
    }

    private fun notifyScrollEnded( invertedVelocityX : Float, invertedVelocityY :Float) {
        val viewNode = this.valdiViewNode
        if (viewNode != null && !isUpdatingScrollSpecs) {
            viewNode.notifyScroll(
                ValdiViewNode.SCROLL_EVENT_TYPE_ON_SCROLL_END,
                contentOffsetX,
                contentOffsetY,
                unclampedContentOffsetX,
                unclampedContentOffsetY,
                0f,
                0f
            )
        }
        pauseScrollPerfLogger()
    }

    private fun resumeScrollPerfLogger() {
        if (!scrollPerfLoggerStarted) {
            scrollPerfLoggerStarted = true
            scrollPerfLoggerBridge?.resume()
        }
    }

    private fun pauseScrollPerfLogger() {
        if (scrollPerfLoggerStarted) {
            scrollPerfLoggerStarted = false
            scrollPerfLoggerBridge?.pause(cancelLogging = false)
        }
    }

    private fun postScrollAnimation() {
        isAnimatingScroll = true
        postOnAnimation(this)
    }

    override fun run() {
        updateScroll()
    }

    private inline fun forEachEdgeEffectWrapper(crossinline callback: (EdgeEffectWrapper) -> Unit) {
        callback(leftEdgeEffect)
        callback(topEdgeEffect)
        callback(rightEdgeEffect)
        callback(bottomEdgeEffect)
    }

    private fun getCollidedEdge(x: Int, y: Int): EdgeEffectWrapper {
        return if (horizontalScroll) {
            if (x == 0) leftEdgeEffect else rightEdgeEffect
        } else {
            if (y == 0) topEdgeEffect else bottomEdgeEffect
        }
    }

    private fun updateScroll() {
        if (scroller.computeScrollOffset()) {

            var newUnclampedContentOffsetX = offsetToUnclampedContentOffsetX(scroller.startX - scroller.currX)
            var newUnclampedContentOffsetY = offsetToUnclampedContentOffsetY(scroller.startY - scroller.currY)

            val newContentOffsetX = newUnclampedContentOffsetX.coerceIn(getMinContentOffsetX(), getMaxContentOffsetX())
            val newContentOffsetY = newUnclampedContentOffsetY.coerceIn(getMinContentOffsetY(), getMaxContentOffsetY())

            if (isAnimatingFling) {
                if (newContentOffsetX != newUnclampedContentOffsetX || newContentOffsetY != newUnclampedContentOffsetY) {
                    // We reached an edge, stop the scroll
                    if (edgeEffect) {
                        val velocity = scroller.currVelocity
                        if (velocity.isFinite()) {
                            getCollidedEdge(newContentOffsetX, newContentOffsetY).onAbsorb(velocity.roundToInt())
                            postInvalidateOnAnimation()
                        }
                    }
                    newUnclampedContentOffsetX = newContentOffsetX
                    newUnclampedContentOffsetY = newContentOffsetY
                    cancelScrollAnimation()
                }
            }

            applyContentOffset(newContentOffsetX, newContentOffsetY, newUnclampedContentOffsetX, newUnclampedContentOffsetY, 0.0f, 0.0f)

            if (scroller.isFinished) {
                notifyScrollEnded(0.0f, 0.0f)
                cancelScrollAnimation()
            } else {
                postScrollAnimation()
            }
        }
    }

    override fun prepareForRecycling() {
        contentWidth = 0
        contentHeight = 0
        applyContentOffsetInternal(0, 0, 0, 0)
        pauseScrollPerfLogger()
    }

    override fun onScrollSpecsChanged(contentOffsetX: Int, contentOffsetY: Int, contentWidth: Int, contentHeight: Int, animated: Boolean) {
        this.contentWidth = contentWidth
        this.contentHeight = contentHeight
        isUpdatingScrollSpecs = true
        if (contentOffsetX != this.contentOffsetX || contentOffsetY != this.contentOffsetY || contentOffsetX != this.unclampedContentOffsetX || contentOffsetY != this.unclampedContentOffsetY) {
            setContentOffset(contentOffsetX, contentOffsetY, 0.0f, 0.0f, animated)
        }
        isUpdatingScrollSpecs = false
        updateScrollDirection()
    }

    override fun computeHorizontalScrollRange(): Int {
        return contentWidth
    }

    override fun computeVerticalScrollRange(): Int {
        return contentHeight
    }

    override fun computeHorizontalScrollOffset(): Int {
        return contentOffsetX
    }

    override fun computeVerticalScrollOffset(): Int {
        return contentOffsetY
    }

    override fun computeHorizontalScrollExtent(): Int {
        return width
    }

    override fun computeVerticalScrollExtent(): Int {
        return height
    }

    override fun getTopFadingEdgeStrength(): Float {
        val offsetY = computeVerticalScrollOffset()
        val maxOffset = minOf(getVerticalFadingEdgeLength(), contentHeight - height)
        return fadeStrengthForOffset(offsetY, maxOffset)
    }

    override fun getBottomFadingEdgeStrength(): Float {
        val offsetY = computeVerticalScrollOffset()
        val remaining = contentHeight - height - offsetY
        val maxOffset = minOf(getVerticalFadingEdgeLength(), contentHeight - height)
        return fadeStrengthForOffset(remaining, maxOffset)
    }

    override fun getLeftFadingEdgeStrength(): Float {
        val offsetX = computeHorizontalScrollOffset()
        val maxOffset = minOf(getHorizontalFadingEdgeLength(), contentWidth - width)
        return fadeStrengthForOffset(offsetX, maxOffset)
    }

    override fun getRightFadingEdgeStrength(): Float {
        val offsetX = computeHorizontalScrollOffset()
        val remaining = contentWidth - width - offsetX
        val maxOffset = minOf(getHorizontalFadingEdgeLength(), contentWidth - width)
        return fadeStrengthForOffset(remaining, maxOffset)
    }

    private fun fadeStrengthForOffset(offset: Int, maxOffset: Int): Float {
        if (maxOffset <= 0) return 0f
        val t = (offset.toFloat() / maxOffset).coerceIn(0f, 1f)
        return easeInOut(t)
    }

    private fun easeInOut(t: Float): Float {
        return if (t < 0.5f) {
            2f * t * t
        } else {
            -1f + (4f - 2f * t) * t
        }
    }
}
