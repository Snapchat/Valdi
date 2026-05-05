package com.snap.valdi.extensions

import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Picture
import android.graphics.RectF
import android.graphics.drawable.Drawable
import android.graphics.drawable.PictureDrawable
import android.os.Build
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.ViewManager
import com.snap.valdi.attributes.impl.animations.transition.ValdiTransitionInfo
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.views.ValdiRootView
import com.snap.valdi.drawables.ValdiGradientDrawable
import com.snap.valdi.drawables.utils.MaskPathRenderer
import com.snap.valdi.drawables.utils.BorderRadii
import com.snap.valdi.drawables.utils.DrawableInfoProvider
import com.snap.valdi.drawables.utils.ValdiGradientDrawablePool
import com.snap.valdi.keyboard.KeyboardManager
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.nodes.IValdiViewNode
import com.snap.valdi.utils.ValdiObjects
import com.snap.valdi.utils.InternedString
import com.snap.valdi.utils.Ref
import com.snap.valdi.views.ValdiClippableView
import com.snap.valdi.views.ValdiForegroundHolder
import com.snap.valdi.views.touches.ValdiGestureRecognizer
import com.snap.valdi.views.touches.GestureRecognizers
import com.snapchat.client.valdi.UndefinedValue
import com.snapchat.client.valdi.utils.NativeHandleWrapper

object ViewUtils {

    var enableTextAlignmentForRTL: Boolean = true

    private fun getOptionalValdiObjects(view: View): ValdiObjects? {
        return view.tag as? ValdiObjects
    }

    private fun getOrCreateValdiObjects(view: View): ValdiObjects {
        var objects = view.tag as? ValdiObjects
        if (objects == null) {
            objects = ValdiObjects()
            view.tag = objects

            if (view is ValdiClippableView) {
                view.clipper.borderRadiiProvider = objects
            }
        }

        return objects
    }

    fun setValdiContext(view: View, context: ValdiContext?) {
        getOrCreateValdiObjects(view).valdiContext = context
    }

    /**
     * Find the ValdiContext to which this View belongs to.
     */
    fun findValdiContext(view: View): ValdiContext? {
        return getOptionalValdiObjects(view)?.valdiContext
    }

    fun getTransitionInfo(view: View): ValdiTransitionInfo? {
        return getOptionalValdiObjects(view)?.valdiTransitionInfo
    }

    fun getOrCreateTransitionInfo(view: View): ValdiTransitionInfo {
        val valdiObjects = getOrCreateValdiObjects(view)
        val info = valdiObjects.valdiTransitionInfo
        return if (info == null) {
            val newInfo = ValdiTransitionInfo()
            valdiObjects.valdiTransitionInfo = newInfo
            newInfo
        } else {
            info
        }
    }

    fun setViewNodeId(view: View, viewNodeId: Int) {
        getOrCreateValdiObjects(view).valdiViewNodeId = viewNodeId
    }

    fun findViewNode(view: View): ValdiViewNode? {
        return getOptionalValdiObjects(view)?.valdiViewNode
    }

    fun notifyAttributeChanged(view: View, attributeName: InternedString, attributeValue: Any?) {
        val viewNode = findViewNode(view) ?: return
        val context = findValdiContext(view) ?: return

        context.valueChangedForAttribute(viewNode, attributeName, attributeValue)
    }

    /**
     * Notifies FlexBox that the intrinsic content size of that view changed
     * and that layout should be recalculated.
     */
    fun invalidateLayout(view: View) {
        val root = getRootView(view) ?: return

        if (!root.performingUpdates) {
            findViewNode(view)?.invalidateLayout()
        }
    }

    fun getTouchAreaInsets(view: View): RectF? {
        return getOptionalValdiObjects(view)?.touchAreaInsets
    }

    fun setTouchAreaInsets(view: View, left: Float, top: Float, right: Float, bottom: Float) {
        val objects = getOrCreateValdiObjects(view)
        if (objects.touchAreaInsets == null) {
            objects.touchAreaInsets = RectF(left, top, right, bottom)
        } else {
            objects.touchAreaInsets!!.set(left, top, right, bottom)
        }
    }

    fun removeTouchAreaInsets(view: View) {
        getOptionalValdiObjects(view)?.touchAreaInsets = null
    }

    fun setCalculatedFrame(view: View, x: Int, y: Int, width: Int, height: Int) {
        val valdiObjects = getOrCreateValdiObjects(view)
        valdiObjects.calculatedX = x
        valdiObjects.calculatedY = y
        valdiObjects.calculatedWidth = width
        valdiObjects.calculatedHeight = height
    }

    fun getCalculatedX(view: View): Int {
        return getOptionalValdiObjects(view)?.calculatedX ?: 0
    }

    fun getCalculatedY(view: View): Int {
        return getOptionalValdiObjects(view)?.calculatedY ?: 0
    }

    fun getCalculatedWidth(view: View): Int {
        return getOptionalValdiObjects(view)?.calculatedWidth ?: 0
    }

    fun getCalculatedHeight(view: View): Int {
        return getOptionalValdiObjects(view)?.calculatedHeight ?: 0
    }

    fun measureValdiChildren(viewGroup: ViewGroup) {
        for (index in 0 until viewGroup.childCount) {
            val childView = viewGroup.getChildAt(index)
            if (!childView.isLayoutRequested) {
                continue
            }

            val valdiObjects = getOptionalValdiObjects(childView) ?: continue
            if (!valdiObjects.hasValdiViewNode) {
                continue
            }

            val widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(valdiObjects.calculatedWidth, View.MeasureSpec.EXACTLY)
            val heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(valdiObjects.calculatedHeight, View.MeasureSpec.EXACTLY)

            childView.measure(widthMeasureSpec, heightMeasureSpec)
        }
    }

    fun notifyDidApplyLayout(view: View) {
        val valdiObjects = getOptionalValdiObjects(view) ?: return
        valdiObjects.didApplyLayout(view)
    }

    fun setDidFinishLayoutForKey(view: View, key: String, didFinishLayout: (view: View) -> Unit) {
        getOrCreateValdiObjects(view).setDidFinishLayoutForKey(key, didFinishLayout)
    }

    fun removeDidFinishLayoutForKey(view: View, key: String) {
        getOptionalValdiObjects(view)?.removeDidFinishLayoutForKey(key)
    }

    private fun applyLayoutToChild(childView: View, left: Int, top: Int, right: Int, bottom: Int) {
        childView.layout(left, top, right, bottom)
        notifyDidApplyLayout(childView)
    }

    fun applyLayoutToValdiChildren(viewGroup: ViewGroup) {
        for (index in 0 until viewGroup.childCount) {
            val childView = viewGroup.getChildAt(index)
            if (!childView.isLayoutRequested) {
                continue
            }

            val valdiObjects = getOptionalValdiObjects(childView) ?: continue
            if (!valdiObjects.hasValdiViewNode) {
                continue
            }

            val x = valdiObjects.calculatedX
            val y = valdiObjects.calculatedY
            val width = valdiObjects.calculatedWidth
            val height = valdiObjects.calculatedHeight

            applyLayoutToChild(childView, x, y, x + width, y + height)
        }
    }

    fun cancelAnimation(view: View, key: Any): Boolean {
        val transitionInfo = getTransitionInfo(view) ?: return false
        return transitionInfo.cancelValueAnimator(key)
    }

    fun willEnqueueViewToPool(view: View) {
        removeAllAnimations(view)
        setValdiContext(view, null)
        setViewNodeId(view, 0)

        getOrCreateValdiObjects(view).maskPathRenderer = null
    }

    // Apply the final value for all ongoing value animators and 
    fun removeAllAnimations(view: View) {
        // Set animation listener to null first to avoid triggering onAnimationEnd on the listener
        view.animation?.setAnimationListener(null)
        view.animation?.cancel()
        view.clearAnimation()
        // ^^^ this seems to deal with the plain android.view.animation Animation system...
        //     Since we use our own, do we even need this anymore?

        val transitionInfo = getTransitionInfo(view)
        transitionInfo?.finishAllValueAnimators()
    }

    fun getGestureRecognizers(view: View, createIfNeeded: Boolean = false): GestureRecognizers? {
        val valdiObjects: ValdiObjects? = if (createIfNeeded) {
            getOrCreateValdiObjects(view)
        } else {
            getOptionalValdiObjects(view)
        }

        if (createIfNeeded && valdiObjects?.gestureRecognizers == null) {
            valdiObjects?.gestureRecognizers = GestureRecognizers()
        }

        return valdiObjects?.gestureRecognizers
    }

    fun getOrCreateGestureRecognizers(view: View): GestureRecognizers {
        return getGestureRecognizers(view, createIfNeeded = true)!!
    }

    fun removeGestureRecognizers(view: View) {
        val valdiObjects = getOptionalValdiObjects(view) ?: return
        valdiObjects.gestureRecognizers?.removeAllGestureRecognizers()
    }

    fun addGestureRecognizer(view: View, gestureRecognizer: ValdiGestureRecognizer) {
        getOrCreateGestureRecognizers(view).addGestureRecognizer(gestureRecognizer)
    }

    fun attachNativeHandleIfNeeded(view: View, name: String, nativeHandle: Any?) {
        val valdiObjects = getOrCreateValdiObjects(view)

        if (nativeHandle is NativeHandleWrapper) {
            val existingNativeHandle = valdiObjects.nativeHandles?.get(name)
            if (existingNativeHandle !== nativeHandle) {
                existingNativeHandle?.destroy()

                if (valdiObjects.nativeHandles == null) {
                    valdiObjects.nativeHandles = hashMapOf()
                }
                valdiObjects.nativeHandles!![name] = nativeHandle
            }
        } else {
            valdiObjects.nativeHandles?.remove(name)?.destroy()
        }
    }

    private fun releaseDrawable(drawable: ValdiGradientDrawable): Boolean {
        drawable.mutationEnd()

        if (drawable.mutationInProgress() || !drawable.isEmpty()) {
            return false
        }
        ValdiGradientDrawablePool.releaseDrawable(drawable)
        return true
    }

    fun acquireForegroundDrawable(view: ValdiForegroundHolder): ValdiGradientDrawable {
        var foreground = view.valdiForeground as? ValdiGradientDrawable
        if (foreground == null) {
            val borderRadiiProvider: DrawableInfoProvider = getOrCreateValdiObjects(view as View)
            foreground = ValdiGradientDrawablePool.createDrawable(view.getContext(), borderRadiiProvider).also {
                // Attach
                view.valdiForeground = it
                if (it.callback == null) {
                    it.callback = view
                }

                view.invalidate()
            }
        }

        foreground.mutationStart()
        return foreground
    }

    fun releaseForegroundDrawable(view: ValdiForegroundHolder, drawable: ValdiGradientDrawable) {
        if (releaseDrawable(drawable)) {
            view.valdiForeground = null
            drawable.callback = null
        }
    }

    inline fun mutateForegroundDrawable(view: ValdiForegroundHolder, receiver: (drawable: ValdiGradientDrawable) -> Unit) {
        acquireForegroundDrawable(view).also {
            receiver(it)
            releaseForegroundDrawable(view, it)
        }
    }

    fun acquireBackgroundDrawable(view: View): ValdiGradientDrawable {
        var background = view.background as? ValdiGradientDrawable
        if (background == null) {
            val drawableInfoProvider: DrawableInfoProvider = getOrCreateValdiObjects(view)
            background = ValdiGradientDrawablePool.createDrawable(view.context, drawableInfoProvider).also {
                view.background = it
            }
        }
        background.mutationStart()
        return background
    }

    fun releaseBackgroundDrawable(view: View, drawable: ValdiGradientDrawable) {
        if (releaseDrawable(drawable)) {
            view.background = null
        }
    }

    inline fun mutateBackgroundDrawable(view: View, receiver: (drawable: ValdiGradientDrawable) -> Unit) {
        acquireBackgroundDrawable(view).also {
            receiver(it)
            releaseBackgroundDrawable(view, it)
        }
    }

    fun getBorderRadii(view: View): BorderRadii? {
        return getOptionalValdiObjects(view)?.borderRadii
    }

    fun setBorderRadii(view: View,
                       borderRadii: BorderRadii?) {
        val valdiObjects = getOrCreateValdiObjects(view)

        var resolvedBorderRadii = borderRadii
        if (resolvedBorderRadii != null && !resolvedBorderRadii.hasNonNullValue) {
            resolvedBorderRadii = null
        }

        val oldBorderRadii = valdiObjects.borderRadii
        if (oldBorderRadii == resolvedBorderRadii) {
            return
        }

        valdiObjects.borderRadii = resolvedBorderRadii

        view.invalidate()
    }

    fun verifyDrawable(view: ValdiForegroundHolder,
                       drawable: Drawable): Boolean {
        return view.valdiForeground === drawable
    }

    fun isRightToLeft(view: View): Boolean {
        return getOptionalValdiObjects(view)?.isRightToLeft ?: false
    }

    fun setIsRightToLeft(view: View, isRightToLeft: Boolean) {
        getOrCreateValdiObjects(view).isRightToLeft = isRightToLeft

        if (!enableTextAlignmentForRTL) {
            return;
        }

        // Skip ValdiRootView as its layout direction is controlled by the application.
        if (view is ValdiRootView) {
            return
        }

        // Set the native Android layout direction on TextViews so text alignment respects
        // the Valdi-computed direction. We only need this on views that render text, since
        // Yoga already handles the layout positioning for all views.
        // 
        // TextView is the base class for EditText, AppCompatEditText, AppCompatTextView, etc.
        if (view is android.widget.TextView) {
            val targetDirection = if (isRightToLeft) {
                View.LAYOUT_DIRECTION_RTL
            } else {
                View.LAYOUT_DIRECTION_LTR
            }
            
            // Only set if changed to avoid potential invalidation overhead
            if (view.layoutDirection != targetDirection) {
                view.layoutDirection = targetDirection
            }
        }
    }

    /**
    Given a d/x value within the ViewNode's coordinate space, returns
    the d/x value in either direction agnostic coordinates or view coordinates.
     */
    fun resolveDeltaX(view: View, deltaX: Double): Double {
        return if (isRightToLeft(view)) {
            deltaX * -1
        } else {
            deltaX
        }
    }

    /**
    Given a d/x value within the ViewNode's coordinate space, returns
    the d/x value in either direction agnostic coordinates or view coordinates.
     */
    fun resolveDeltaX(view: View, deltaX: Int): Int {
        return if (isRightToLeft(view)) {
            deltaX * -1
        } else {
            deltaX
        }
    }

    /**
    Given a d/x value within the ViewNode's coordinate space, returns
    the d/x value in either direction agnostic coordinates or view coordinates.
     */
    fun resolveDeltaX(view: View, deltaX: Float): Float {
        return if (isRightToLeft(view)) {
            deltaX * -1
        } else {
            deltaX
        }
    }

    fun drawForeground(view: ValdiForegroundHolder, canvas: Canvas?) {
        if (canvas == null) {
            return
        }

        val foreground = view.valdiForeground
        if (foreground != null) {
            foreground.setBounds(0, 0, view.getWidth(), view.getHeight())
            foreground.draw(canvas)
        }
    }

    inline fun onDraw(view: View, canvas: Canvas, doDraw: (canvas: Canvas) -> Unit) {
        val maskPathRenderer = getOptionalMaskPathRenderer(view)
        if (maskPathRenderer != null && !maskPathRenderer.isEmpty) {
            maskPathRenderer.prepareMask(view.width, view.height, canvas)
            doDraw(canvas)
            maskPathRenderer.applyMask(canvas)
        } else {
            doDraw(canvas)
        }
    }

    inline fun dispatchDraw(view: View, canvas: Canvas, doDispatchDraw: (canvas: Canvas) -> Unit) {
        onDraw(view, canvas, doDispatchDraw)

        if (view is ValdiForegroundHolder) {
            drawForeground(view, canvas)
        }
    }

    fun getOptionalMaskPathRenderer(view: View): MaskPathRenderer? {
        return getOptionalValdiObjects(view)?.maskPathRenderer
    }

    fun getMaskPathRenderer(view: View): MaskPathRenderer {
        val valdiObjects = getOrCreateValdiObjects(view)
        var maskPathRenderer = valdiObjects.maskPathRenderer
        if (maskPathRenderer == null) {
            maskPathRenderer = MaskPathRenderer()
            valdiObjects.maskPathRenderer = maskPathRenderer
        }

        return maskPathRenderer
    }

    private const val MAX_OVERLAPPING_RENDERING_SIZE = 4096

    fun hasOverlappingRendering(view: View): Boolean {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.P ||
                Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP ||
                Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1 ||
                Build.VERSION.SDK_INT == Build.VERSION_CODES.M) {
            // Workaround for a Samsung/LG Android 9 crash. We disable overlapping rendering if the size of the view
            // goes beyond 4096, which is half the max texture size for all the device that crash.
            // Overlapping rendering enabled forces Android to emit hardware layers for views that have opacity
            // set between 0 and 1 (exclusive). In some cases, memory allocation for the layer will fail and the Android
            // rendering pipeline does not recover from it.
            return Math.max(view.width, view.height) < MAX_OVERLAPPING_RENDERING_SIZE
        }

        return true
    }

    /**
     * Resolves the IValdiViewNode instance from the given Ref object
     */
    fun getViewNodeFromRef(ref: Ref): IValdiViewNode? {
        // UndefinedValue may be returned by the Valdi C++ bridge when a component ref is in an
        // undefined state (not yet initialized or already destroyed). It doesn't implement Ref
        // despite being passed as one, so calling ref.get() would throw IncompatibleClassChangeError.
        if ((ref as Any) is UndefinedValue) return null
        val item = ref.get()

        return when (item) {
            is View -> findViewNode(item)
            is IValdiViewNode -> item
            else -> null
        }
    }

    /**
     * Resolves the View instance from the given Ref object
     */
    @Deprecated("This will stop working with SnapDrawing. Use getViewNodeFromRef() instead")
    fun getViewFromRef(ref: Ref): View? {
        if ((ref as Any) is UndefinedValue) return null
        val item = ref.get()

        return when (item) {
            is View -> item
            is ValdiViewNode -> item.getBackingViewRef()?.get() as? View
            else -> null
        }
    }

    /**
     * Return a Drawable representation of the given Ref.
     */
    fun refToDrawable(ref: Ref): Drawable? {
        if ((ref as Any) is UndefinedValue) return null
        val item = ref.get()

        return when (item) {
            is View -> viewToDrawable(item)
            is IValdiViewNode -> item.snapshot()
            else -> null
        }
    }

    fun viewToDrawable(view: View): Drawable {
        val picture = Picture()
        val canvas = picture.beginRecording(view.width, view.height)
        view.draw(canvas)
        picture.endRecording()
        return PictureDrawable(picture)
    }

    private fun getRootView(view: View): ValdiRootView? {
        if (view is ValdiRootView) {
            return view as ValdiRootView
        }
        return findValdiContext(view)?.rootView
    }

    fun getKeyboardManager(view: View): KeyboardManager? {
        return getRootView(view)?.keyboardManager
    }

    fun resetFocusToRootViewOf(view: View) {
        getRootView(view)?.requestFocus()
    }

    /**
     * Traverses up the view hierarchy to calculate the cumulative X and Y scale factors.
     */
    fun getCumulativeScale(view: View): Pair<Float, Float> {
        var scaleX = 1.0f
        var scaleY = 1.0f
        var currentView: View? = view
        while (currentView != null) {
            scaleX *= currentView.scaleX
            scaleY *= currentView.scaleY
            currentView = currentView.parent as? View
        }
        return Pair(scaleX, scaleY)
    }

    fun getCumulativeRotation(view: View): Float {
        var rotation = 0.0f
        var currentView: View? = view
        while (currentView != null) {
            rotation+= currentView.rotation
            currentView = currentView.parent as? View
        }
        return rotation
    }

    fun adjustedCoordinates(view: View, event: MotionEvent): FloatArray {
        val (scaleX, scaleY) = getCumulativeScale(view)
        val rotation = getCumulativeRotation(view)
        val location = IntArray(2)
        view.getLocationOnScreen(location)

        val matrix = Matrix()
        matrix.postScale(1 / scaleX, 1 / scaleY)
        matrix.postRotate(-rotation)

        val coordinates = floatArrayOf(event.rawX - location[0], event.rawY - location[1])
        matrix.mapPoints(coordinates)
        return coordinates
    }
    /**
     * Obtains an adjusted MotionEvent that accounts for the view's scale and rotation.
     * Note: make sure you recycle the MotionEvent after use.
     */
    fun obtainAdjustedMotionEvent(view: View, event: MotionEvent): MotionEvent {
        val adjustedCoordinates = adjustedCoordinates(view, event)
        val adjustedEvent = MotionEvent.obtain(event)
        adjustedEvent.setLocation(adjustedCoordinates[0], adjustedCoordinates[1])
        return adjustedEvent
    }
}

/**
 * Remove the view from its parent view.
 */
fun View.removeFromParentView() {
    val parent = this.parent as? ViewManager
    parent?.removeView(this)
}
