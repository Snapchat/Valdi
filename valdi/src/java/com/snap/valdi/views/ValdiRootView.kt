package com.snap.valdi.views

import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.ViewTreeObserver
import android.view.KeyEvent
import android.graphics.Rect
import androidx.core.view.ViewCompat
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.context.ValdiViewOwner
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.extensions.removeFromParentView
import com.snap.valdi.keyboard.KeyboardManager
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.utils.ValdiLeakTracker
import com.snap.valdi.utils.Disposable
import com.snap.valdi.utils.getValdiHandler
import com.snap.valdi.utils.runOnMainThreadIfNeeded
import com.snap.valdi.utils.trace
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.views.touches.DisallowInterceptTouchEventMode
import com.snap.valdi.views.touches.GesturesState
import com.snap.valdi.views.touches.TouchDispatcher
import com.snap.valdi.views.touches.backbutton.BackButtonListener
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.callable.performSync
import com.snapchat.client.valdi.NativeBridge
import java.util.ArrayDeque
import java.lang.ref.WeakReference
import kotlin.math.min

open class ValdiRootView: ValdiView, Disposable {

    enum class ScrollDirection(val value: Int) {
        TopToBottom(0),
        BottomToTop(1),
        LeftToRight(2),
        RightToLeft(3),
        HorizontalForward(4),
        HorizontalBackward(5),
        VerticalForward(6),
        VerticalBackward(7),
        Forward(8),
        Backward(9),
    }

    constructor(context: Context) : super(context)
    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs)

    /**
     * Whether to use the new multi touch handling, see [TouchDispatcher].
     * It should be set once before the ValdiRootView starts working (typically immediately after creation).
     */
    var useNewMultiTouchExperience = false

    // Implements fixes when using onRotate.
    var enableRotateGestureRecognizeV2 = false

    // Implements fixes when using onPinch.
    var enablePinchGestureRecognizeV2 = false

    var enableV2GestureDetectorReset = false

    var disableLeakTracking = false

    var adjustCoordinates: Boolean = false

    /**
     * Whether view inflation should stay enabled when the view is invisible.
     * When disabled, all the subviews are removed to free up memory when the view is not active.
     */
    var enableViewInflationWhenInvisible = true
        set(value) {
            if (field != value) {
                field = value
                updateViewInflationState(isAttachedToWindow)
            }
        }

    /**
     * Whether captured ValdiTouchTarget should be removed from gesture candidates
     * when a gesture starts and requests exclusivity of touches. When this flag is disabled,
     * only the candidate gesture recognizers will be cancelled when a gesture requests exclusivity,
     * the captured views that implement ValdiTouchTarget will remain as candidates and they can
     * still handle touches. When this flag is enabled, captured views will stop being considered
     * as candidates, in addition to other gestures being cancelled. This will become
     * the default value in the future.
     */
    var cancelsTouchTargetsWhenGestureRequestsExclusivity = false
        set(value) {
            field = value
            touchDispatcher?.cancelsTouchTargetsWhenGestureRequestsExclusivity = value
        }

    /*
     * A helper class to setup a shadow accessibility node tree.
     * These methods will only execute when TalkBack is enabled.
     */
    private var accessibilityDelegate: ValdiAccessibilityDelegate? = null

    override val clipToBoundsDefaultValue: Boolean
        get() = false

    var touchDispatcher: TouchDispatcher? = null
        private set

    var onBackButtonListener: BackButtonListener? = null

    val keyboardManager by lazy {
        KeyboardManager(this)
    }

    var destroyed = false
        private set

    var destroyValdiContextOnFinalize = false

    /**
     * Defines how the Valdi touch system will prevent touches from being intercepted by their parent.
     */
    var disallowInterceptTouchEventMode: DisallowInterceptTouchEventMode =
        DisallowInterceptTouchEventMode.DISALLOW_WHEN_GESTURE_RECOGNIZED
        set(value) {
            field = value
            touchDispatcher?.disallowInterceptTouchEventMode = value
        }

    /**
     * Return the current gestures state within the Valdi gestures system.
     * This state is updated after every dispatchTouchEvent() call.
     */
    val currentGesturesState: GesturesState
        get() {
            return touchDispatcher?.getCurrentGesturesState() ?: GesturesState.INACTIVE
        }

    private var isLeakTracked = false
    private var contextReadyCallbacks: MutableList<(ValdiContext) -> Unit>? = null
    private var activeVisibility = View.INVISIBLE
    private var valdiUpdatesCount = 0
    private val onNextDrawCallbackHandles = ArrayDeque<Long>()
    private var onNextDrawPreDrawListener: ViewTreeObserver.OnPreDrawListener? = null
    private var onNextDrawPreDrawObserver: ViewTreeObserver? = null
    private var onNextDrawAttachListener: View.OnAttachStateChangeListener? = null

    val performingUpdates: Boolean
        get() = valdiUpdatesCount > 0

    var snapDrawingContainerView: View? = null
        set(value) {
            if (field !== value) {
                val oldValue = field
                field = value

                oldValue?.removeFromParentView()
                if (value != null) {
                    addView(value)
                }
            }
        }

    var rootViewTouchListener: ((rootView: ValdiView, event: MotionEvent) -> Unit)? = null

    var captureAllHitTargets: Boolean = false
        set(value) {
            field = value
            touchDispatcher?.captureAllHitTargets = value
        }
    init {
        clipChildren = false
        isFocusable = true
        isFocusableInTouchMode = true
        if (layoutParams == null) {
            // By default, ValdiView have a null layoutParams, which is interpreted as MATCH_PARENT by Android.
            // Because we can't set a layoutParams to null, we make it explicitly as MATCH_PARENT always.
            layoutParams = ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        }
    }

    /**
     * Destroys the ValdiContext, which releases all its resources.
     */
    fun destroy() {
        runOnMainThreadIfNeeded {
            destroyed = true
            discardPendingOnNextDrawCallbacks()

            getValdiContext {
                ViewUtils.setViewNodeId(this, 0)
                ViewUtils.setValdiContext(this, null)
                setOnSystemUiVisibilityChangeListener(null)
                it.destroy()
            }
        }
    }

    /**
     * Enqueue a callback to call on the next render
     */
    fun enqueueNextRenderCallback(onNextRenderCallback: () -> Unit) {
        getValdiContext { it.enqueueNextRenderCallback(onNextRenderCallback) }
    }

    /**
     * Registers a callback to be called whenever the Valdi layout becomes dirty. This happens
     * whenever a node is moved/removed/added or when a layout attribute is changed during a render.
     * A dirty layout means that the measured size of the Valdi component might change.
     */
    fun onLayoutDirty(layoutDirtyCallback: () -> Unit) {
        getValdiContext { it.onLayoutDirty(layoutDirtyCallback) }
    }

    /**
     * Enqueues a callback that will be executed once a complete layout pass has finished.
     * Will be called immediately if the layout is up to date.
     */
    fun onNextLayout(callback: () -> Unit) {
        getValdiContext { it.onNextLayout(callback) }
    }

    fun setActionHandlerUntyped(actionHandler: Any?) {
        getValdiContext { it.actionHandler = actionHandler }
    }

    fun setViewModelUntyped(viewModel: Any?) {
        getValdiContext { it.viewModel = viewModel }
    }

    fun setOwner(owner: ValdiViewOwner?) {
        getValdiContext { it.owner = owner }
    }

    /**
     * Set the visible viewport that should be used when computing the viewports from
     * the root node. This can be used to only render a smaller area of the root node.
     * When not set, the root node will be considered as completely visible.
     */
    fun setVisibleViewport(x: Int, y: Int, width: Int, height: Int) {
        getValdiContext { it.setVisibleViewport(x, y, width, height) }
    }

    /**
     * Unset a previously set visible viewport. The root node will be considered
     * as completely visible.
     */
    fun unsetVisibleViewport() {
        getValdiContext { it.unsetVisibleViewport() }
    }

    /**
     * Set whether the layout sepcs should be kept when the layout is invalidated. When this option
     * is set, the underlying Valdi Context will be able to immediately recalculate the layout on
     * layout invalidation without asking platform about the new layout specs. You can use this when
     * the root view's size is not dependent on calculating the size of the Valdi tree.
     */
    fun setRetainsLayoutSpecsOnInvalidateLayout(retainsLayoutSpecsOnInvalidateLayout: Boolean) {
        getValdiContext { it.setRetainsLayoutSpecsOnInvalidateLayout(retainsLayoutSpecsOnInvalidateLayout) }
    }

    fun setBackButtonObserverWithFunction(function: ValdiFunction?) {
        if (function == null) {
            onBackButtonListener = null
        } else {
            onBackButtonListener = object : BackButtonListener {
                override fun onBackButtonPressed(): Boolean {
                    return ValdiMarshaller.use {
                        val hasValue = function.performSync(it, false)
                        if (hasValue) {
                            it.getBoolean(-1)
                        } else {
                            false
                        }
                    }
                }
            }
        }
    }

    override fun canScrollHorizontally(direction: Int): Boolean {
        if (this.snapDrawingContainerView != null) {
            if (direction > 0) {
                return canScrollAtPoint(width / 2, height / 2, ScrollDirection.LeftToRight)
            } else {
                return canScrollAtPoint(width / 2, height / 2, ScrollDirection.RightToLeft)
            }
        }
        return super.canScrollHorizontally(direction)
    }

    override fun canScrollVertically(direction: Int): Boolean {
        if (this.snapDrawingContainerView != null) {
            if (direction > 0) {
                return canScrollAtPoint(width / 2, height / 2, ScrollDirection.TopToBottom)
            } else {
                return canScrollAtPoint(width / 2, height / 2, ScrollDirection.BottomToTop)
            }
        }
        return super.canScrollVertically(direction)
    }

    fun canScrollAtPoint(x: Int, y: Int, direction: ScrollDirection): Boolean {
        val viewNode = ViewUtils.findViewNode(this)
        if (viewNode != null) {
            return viewNode.canScrollAtPoint(x, y, direction)
        }
        return false
    }

    /**
     * Returns the ValdiContext whenever it becomes available.
     */
    fun getValdiContext(callback: (ValdiContext) -> Unit) {
        runOnMainThreadIfNeeded {
            val valdiContext = this.valdiContext
            if (valdiContext != null) {
                callback(valdiContext)
            } else {
                if (contextReadyCallbacks == null) {
                    contextReadyCallbacks = mutableListOf()
                }
                contextReadyCallbacks?.add(callback)
            }
        }
    }

    /**
     * Returns the ValdiViewNode whenever it becomes available.
     */
    fun getValdiViewNode(callback: (ValdiViewNode) -> Unit) {
        getValdiContext {
            val viewNode = valdiViewNode ?: return@getValdiContext
            callback(viewNode)
        }
    }

    override fun dispatchTouchEvent(event: MotionEvent?): Boolean {
        if (event == null) {
            return super.dispatchTouchEvent(event)
        }


        val snapDrawingView = this.snapDrawingContainerView
        if (snapDrawingView != null) {
            val adjustedEvent = MotionEvent.obtain(event)
            adjustedEvent.setLocation(adjustedEvent.x - x, adjustedEvent.y - y)
            try {
                val handled = snapDrawingView.dispatchTouchEvent(adjustedEvent)
                rootViewTouchListener?.invoke(this, event);
                return handled;
            } finally {
                adjustedEvent.recycle()
            }
        } else {
            if (touchDispatcher == null) {
                val debugTouchEvents = valdiContext?.runtimeOrNull?.manager?.tweaks?.debugTouchEvents ?: false
                touchDispatcher = TouchDispatcher.create(
                    this,
                    disallowInterceptTouchEventMode,
                    valdiContext?.runtimeOrNull?.logger,
                    debugTouchEvents=debugTouchEvents,
                    cancelsTouchTargetsWhenGestureRequestsExclusivity=cancelsTouchTargetsWhenGestureRequestsExclusivity,
                    captureAllHitTargets = this.captureAllHitTargets,
                    adjustCoordinates = this.adjustCoordinates,
                )
            }
            val handled = touchDispatcher!!.dispatchTouch(event)
            rootViewTouchListener?.invoke(this, event);
            return handled
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()

        clearOnNextDrawPreDrawListener()
        updateViewInflationState(false)

        if (ValdiLeakTracker.enabled && !disableLeakTracking) {
            val runtime = valdiContext?.runtime
            if (runtime != null) {
                ValdiLeakTracker.trackRef(WeakReference(this), this::class.java.name, runtime)
                isLeakTracked = true
            }
        }
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()

        if (onNextDrawCallbackHandles.isNotEmpty()) {
            scheduleOnNextDrawDispatch()
        }
        updateViewInflationState(true)

        if (isLeakTracked) {
            isLeakTracked = false
            ValdiLeakTracker.untrackRef(this)
        }
    }

    private fun isRTL(): Boolean {
        return layoutDirection == View.LAYOUT_DIRECTION_RTL
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val widthMode = MeasureSpec.getMode(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)
        val heightMode = MeasureSpec.getMode(heightMeasureSpec)

        var calculatedWidth: Int
        var calculatedHeight: Int

        if (widthMode != MeasureSpec.EXACTLY || heightMode != MeasureSpec.EXACTLY) {
            val valdiContext = this.valdiContext
            if (valdiContext != null) {
                val calculatedSize = valdiContext.measureLayout(width, widthMode, height, heightMode, isRTL())
                calculatedWidth = ValdiViewNode.horizontalFromEncodedLong(calculatedSize)
                calculatedHeight = ValdiViewNode.verticalFromEncodedLong(calculatedSize)
            } else {
                calculatedWidth = 0
                calculatedHeight = 0
            }

            if (widthMode == MeasureSpec.AT_MOST) {
                calculatedWidth = min(calculatedWidth, width)
            } else if (widthMode == MeasureSpec.EXACTLY) {
                calculatedWidth = width
            }

            if (heightMode == MeasureSpec.AT_MOST) {
                calculatedHeight = min(calculatedHeight, height)
            } else if (heightMode == MeasureSpec.EXACTLY) {
                calculatedHeight = height
            }
        } else {
            calculatedWidth = width
            calculatedHeight = height
        }

        this.snapDrawingContainerView?.measure(
            MeasureSpec.makeMeasureSpec(calculatedWidth, MeasureSpec.EXACTLY),
            MeasureSpec.makeMeasureSpec(calculatedHeight, MeasureSpec.EXACTLY)
        )

        setMeasuredDimension(calculatedWidth, calculatedHeight)
    }

    override fun onLayout(p0: Boolean, l: Int, t: Int, r: Int, b: Int) {
        val context = this.valdiContext
        if (context != null) {
            val width = r - l
            val height = b - t
            context.setLayoutSpecs(width, height, isRTL())
        }
        applyValdiLayout()

        snapDrawingContainerView?.layout(0, 0, r - l, b - t)
        ViewUtils.notifyDidApplyLayout(this)
    }

    fun applyValdiLayout() {
        trace({"Valdi.dispatchMeasure"}) {
            ViewUtils.measureValdiChildren(this)
        }
        trace({ "Valdi.dispatchLayout" }) {
            ViewUtils.applyLayoutToValdiChildren(this)
        }
    }

    override fun onVisibilityChanged(changedView: View, visibility: Int) {
        super.onVisibilityChanged(changedView, visibility)

        activeVisibility = visibility

        updateViewInflationState()
    }

    override fun requestLayout() {
        if (valdiUpdatesCount == 0) {
            super.requestLayout()
        }
    }

    fun onValdiLayoutInvalidated() {
        super.requestLayout()
    }

    internal fun enqueueOnNextDrawCallback(callbackHandle: Long) {
        if (callbackHandle == 0L) {
            return
        }

        if (destroyed) {
            NativeBridge.discardCallback(callbackHandle)
            return
        }

        val shouldInvalidate = onNextDrawCallbackHandles.isEmpty()
        onNextDrawCallbackHandles.addLast(callbackHandle)
        if (shouldInvalidate) {
            scheduleOnNextDrawDispatch()
            postInvalidateOnAnimation()
        }
    }

    internal fun valdiUpdatesBegan() {
        valdiUpdatesCount++
    }

    internal fun valdiUpdatesEnded(layoutDidBecomeDirty: Boolean) {
        valdiUpdatesCount--
        if (valdiUpdatesCount == 0 && !isLayoutRequested) {
            applyValdiLayout()
        }
    }
    internal fun valdiUpdatesEndedAsync(layoutDidBecomeDirty: Boolean) {
        valdiUpdatesCount--
        if (valdiUpdatesCount == 0 && !isLayoutRequested) {
            // This is called outside of normal update cycle
            // so we can't directly call applyValdiLayout().
            // Otherwise we can get java.lang.IllegalStateException
            // due to calling layout in the middle of RecyclerView’s
            // layout stage
            post {
                requestLayout()
            }
        }
    }

    private fun discardPendingOnNextDrawCallbacks() {
        clearOnNextDrawPreDrawListener()
        clearOnNextDrawAttachListener()
        while (onNextDrawCallbackHandles.isNotEmpty()) {
            NativeBridge.discardCallback(requireNotNull(onNextDrawCallbackHandles.pollFirst()))
        }
    }

    private fun scheduleOnNextDrawDispatch() {
        if (onNextDrawPreDrawListener != null || onNextDrawAttachListener != null) {
            return
        }

        if (!isAttachedToWindow) {
            val attachListener = object : View.OnAttachStateChangeListener {
                override fun onViewAttachedToWindow(v: View) {
                    clearOnNextDrawAttachListener()
                    scheduleOnNextDrawDispatch()
                }

                override fun onViewDetachedFromWindow(v: View) = Unit
            }
            onNextDrawAttachListener = attachListener
            addOnAttachStateChangeListener(attachListener)
            return
        }

        val observer = viewTreeObserver
        if (!observer.isAlive) {
            getValdiHandler().post {
                if (onNextDrawCallbackHandles.isNotEmpty()) {
                    scheduleOnNextDrawDispatch()
                }
            }
            return
        }

        val listener = object : ViewTreeObserver.OnPreDrawListener {
            override fun onPreDraw(): Boolean {
                clearOnNextDrawPreDrawListener()
                getValdiHandler().postAtFrontOfQueue {
                    if (onNextDrawCallbackHandles.isNotEmpty()) {
                        performPendingOnNextDrawCallbacks()
                    }
                }
                return true
            }
        }
        onNextDrawPreDrawListener = listener
        onNextDrawPreDrawObserver = observer
        observer.addOnPreDrawListener(listener)
    }

    private fun clearOnNextDrawPreDrawListener() {
        val listener = onNextDrawPreDrawListener ?: return
        onNextDrawPreDrawListener = null
        val observer = onNextDrawPreDrawObserver
        onNextDrawPreDrawObserver = null
        if (observer?.isAlive == true) {
            observer.removeOnPreDrawListener(listener)
            return
        }

        val currentObserver = viewTreeObserver
        if (currentObserver.isAlive) {
            currentObserver.removeOnPreDrawListener(listener)
        }
    }

    private fun clearOnNextDrawAttachListener() {
        val listener = onNextDrawAttachListener ?: return
        onNextDrawAttachListener = null
        removeOnAttachStateChangeListener(listener)
    }

    private fun performPendingOnNextDrawCallbacks() {
        val callbackHandles = ArrayList<Long>(onNextDrawCallbackHandles.size)
        while (onNextDrawCallbackHandles.isNotEmpty()) {
            callbackHandles.add(requireNotNull(onNextDrawCallbackHandles.pollFirst()))
        }

        for (callbackHandle in callbackHandles) {
            NativeBridge.performCallback(callbackHandle)
        }
    }

    private fun updateViewInflationState() {
        updateViewInflationState(isAttachedToWindow)
    }

    private fun updateViewInflationState(isAttachedToWindow: Boolean) {
        valdiContext?.viewInflationEnabled = enableViewInflationWhenInvisible || (isAttachedToWindow && activeVisibility == View.VISIBLE)
    }

    internal fun contextIsReady(valdiContext: ValdiContext) {
        updateViewInflationState()

        if (contextReadyCallbacks != null) {
            val callbacks = contextReadyCallbacks
            contextReadyCallbacks  = null
            callbacks?.let { it.forEach { it(valdiContext) } }
        }

        accessibilityDelegate = ValdiAccessibilityDelegateHierarchy(this, valdiContext)
        ViewCompat.setAccessibilityDelegate(this, accessibilityDelegate)

        requestLayout()
    }

    protected fun finalize() {
        runOnMainThreadIfNeeded {
            discardPendingOnNextDrawCallbacks()
            if (destroyValdiContextOnFinalize) {
                valdiContext?.destroy()
            }
        }
    }

    override fun dispose() {
        destroy()
    }

    fun invalidateAccessibilityTree() {
        // This will only kick off a tree rebuild if TalkBack is enabled.
        accessibilityDelegate?.invalidateRoot()
    }

    override fun invalidate() {
        // This signal is used to update the shadow tree for the traditional Android UI
        invalidateAccessibilityTree()
        super.invalidate()
    }

    override fun dispatchHoverEvent(event: MotionEvent): Boolean {
        // Required for accessibility touch traversal.
        val accessibilityDispatched = accessibilityDelegate?.dispatchHoverEvent(event) ?: false
        return accessibilityDispatched || super.dispatchHoverEvent(event)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        // Required for accessibility keyboard traversal.
        val accessibilityDispatched = accessibilityDelegate?.dispatchKeyEvent(event) ?: false
        return accessibilityDispatched || super.dispatchKeyEvent(event)
    }

    override fun onFocusChanged(focused: Boolean, direction: Int, previouslyFocusedRect: Rect?) {
        if (focused) {
            this.keyboardManager.hideKeyboard(this)
        }
        super.onFocusChanged(focused, direction, previouslyFocusedRect)
        accessibilityDelegate?.onFocusChanged(focused, direction, previouslyFocusedRect)
    }

}
