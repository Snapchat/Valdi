package com.snap.valdi.context

import android.content.Context
import android.os.Looper
import android.view.View
import androidx.annotation.Keep
import com.snap.valdi.ValdiRuntime
import com.snap.valdi.ViewFactory
import com.snap.valdi.actions.ValdiActionHandlerHolder
import com.snap.valdi.actions.ValdiActions
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.logger.Logger
import com.snap.valdi.exceptions.ValdiFatalException
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.modules.drawing.Size
import com.snap.valdi.nativebridge.ContextNative
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.nodes.IValdiViewNode
import com.snap.valdi.utils.*
import com.snap.valdi.utils.InternedStringCPP
import com.snap.valdi.views.ValdiRootView
import com.snapchat.client.valdi.NativeBridge
import java.lang.ref.WeakReference
import java.lang.reflect.Constructor

/**
 * A Valdi context uniquely identifies a view tree inflated by Valdi. It is
 * backed by a native instance.
 */
@Keep
class ValdiContext(val native: ContextNative,
                      contextId: ValdiContextId,
                      componentPath: String,
                      moduleName: String,
                      moduleOwnerName: String,
                      val logger: Logger): IValdiContext {

    var performGcOnDestroy = false

    override val contextId: ValdiContextId

    val componentPath: String
    val moduleName: String
    val moduleOwnerName: String

    val bundleName: String
        get() = moduleName

    var usesSnapDrawingRenderBackend = false
        internal set

    internal var rootViewHandler: IRootViewHandler? = null

    val actions: ValdiActions
        get() {
            return synchronized(this) {
                if (innerActions == null) {
                    innerActions = ValdiActions(ValdiActionHandlerHolder())
                }

                innerActions!!
            }
        }

    /**
     * The actionHandler is the object that will respond to actions/touch events.
     */
    var actionHandler: Any?
        get() = actions.actionHandlerHolder.actionHandler
        set(value) { actions.actionHandlerHolder.actionHandler = value }

    val runtime: ValdiRuntime
        get() {
            return native.getRuntime()!!
        }

    val runtimeOrNull: ValdiRuntime?
        get() {
            return native.getRuntime()
        }

    var componentContext: WeakReference<Any>? = null

    /**
     * The owner for this context
     */
    var owner: ValdiViewOwner? = null

    /**
     * Whether the legacy measure behavior should be used, which means that MeasureSpec.AT_MOST
     * would get treated as MeasureSpec.UNSPECIFIED that gets artificially capped to the given
     * max size. Components that set width/height to 100% on their root element will end up
     * taking the entire available space when when the non legacy behavior is enabled
     */
    var useLegacyMeasureBehavior = false

    /**
     * Returns the rootView belonging to this ValdiContext
     *
     * When setting this property, it will set the root view to use on this Valdi Context.
     * The view hierarchy will be populated and added to the given rootView.
     * Passing null will teardown the view hierarchy from the previous root view.
     */
    override var rootView: ValdiRootView?
        get() = rootViewHandler?.rootView
        set(value) {
            if (destroyed) {
                throw ValdiFatalException("Attempting to a attach a root view to a destroyed ValdiContext")
            }

            val previousRootView = this.rootView
            if (previousRootView == value) {
                return
            }

            val previousRootViewValdiContext = previousRootView?.valdiContext
            if (previousRootViewValdiContext != null && previousRootViewValdiContext != this) {
                throw ValdiFatalException("Attempting to a attach a root view that belongs to another ValdiContext")
            }

            rootViewHandler?.rootView = value

            if (previousRootView != null) {
                ViewUtils.setValdiContext(previousRootView, null)
                ViewUtils.setViewNodeId(previousRootView, 0)
            }

            if (value != null) {
                ViewUtils.setValdiContext(value, this)
                value.contextIsReady(this)
            }
        }

    /**
     * The ViewModel that will be applied to the view tree.
     * A render() will occur when setting the view model.
     */
    override var viewModel: Any?
        set(value) {
            innerViewModel = value
            native.setViewModel(ValdiMarshallable.from(value))
        }
        get() = innerViewModel

    /**
     * Allows to turn on/off view inflation.
     * When off, only the root view is kept, all the child views
     * will be removed from the tree.
     */
    var viewInflationEnabled: Boolean
        get() = viewInflationEnabledInner
        set(value) {
            if (value != viewInflationEnabledInner) {
                viewInflationEnabledInner = value

                runOnMainThreadIfNeeded {
                    native.withHandle { NativeBridge.setViewInflationEnabled(it, viewInflationEnabledInner) }
                }
            }
        }

    private var innerActions: ValdiActions? = null
    private var innerViewModel: Any? = null
    private var nextRendersCallbacks: MutableList<() -> Unit>? = null
    private var layoutDirtyCallbacks: MutableList<() -> Unit>? = null
    private var delayDestroy = false

    private var disposables: MutableList<Disposable>? = null
    private var attachedObjects: MutableMap<String, Any?>? = null
    private var destroyed = false
    private var viewInflationEnabledInner = true

    init {
        this.contextId = contextId
        this.componentPath = componentPath
        this.moduleName = moduleName
        this.moduleOwnerName = moduleOwnerName
    }

    /**
     * Returns the view given its id. If you manually specified a view id using id="someId",
     * you will be able to get that view using getView("someId").
     * Views are indexed, so this operation is O(1)
     */
    fun getView(id: String): View? {
        return native.getView(id)?.get()
    }

    /**
     * Perform a FlexBox layout calculation using the given max width and height.
     * Returns the measured width/height contained in as an encoded Long
     */
    fun measureLayout(maxWidth: Int, widthMode: Int, maxHeight: Int, heightMode: Int, isRTL: Boolean): Long {
        val yogaWidthMode = measureSpecModeToYogaSpecMode(widthMode)
        val yogaHeightMode = measureSpecModeToYogaSpecMode(heightMode)
        return native.measureLayout(maxWidth, yogaWidthMode, maxHeight, yogaHeightMode, isRTL)
    }

    /**
     * Perform a FlexBox layout calculation using the given width and height measure specs.
     */
    override fun measureLayout(widthMeasureSpec: Int, heightMeasureSpec: Int, isRTL: Boolean): Size {
        val width = View.MeasureSpec.getSize(widthMeasureSpec)
        val widthMode = View.MeasureSpec.getMode(widthMeasureSpec)
        val height = View.MeasureSpec.getSize(heightMeasureSpec)
        val heightMode = View.MeasureSpec.getMode(heightMeasureSpec)
        val size = measureLayout(width, widthMode, height, heightMode, isRTL)

        return Size(
                ValdiViewNode.horizontalFromEncodedLong(size).toDouble(),
                ValdiViewNode.verticalFromEncodedLong(size).toDouble()
        )
    }

    override fun setLayoutSpecs(width: Int, height: Int, isRTL: Boolean) {
        native.setLayoutSpecs(width, height, isRTL)
    }

    override fun setVisibleViewport(x: Int, y: Int, width: Int, height: Int) {
        native.setVisibleViewport(x, y, width, height)
    }

    override fun unsetVisibleViewport() {
        native.unsetVisibleViewport()
    }

    internal fun onCreate() {
        native.onCreate()
    }

    internal fun onDestroy() {
        val componentPath = this.componentPath

        var disposables: MutableList<Disposable>?

        synchronized(this) {
            destroyed = true

            native.markDestroyed()
            owner = null
            innerViewModel = null
            nextRendersCallbacks = null
            layoutDirtyCallbacks = null
            actionHandler = null
            innerActions = null

            disposables = this.disposables
            this.disposables = null
            this.attachedObjects = null
            this.rootViewHandler?.rootView = null
            this.rootViewHandler = null
        }

        try {
            disposables?.forEach { it.dispose() }
        } catch (throwable: Throwable) {
            ValdiFatalException.handleFatal(throwable, "Failed to invoke disposables after ValdiContext $componentPath was destroyed")
        }
    }

    /**
     * Destroy the ValdiContext, which release all native and JavaScript (if any) resources for this context.
     * Calling this effectively make the ValdiContext unusable. You should call this when you no longer
     * need the context/view.
     */
    override fun destroy() {
        if (this.delayDestroy || Looper.myLooper() !== Looper.getMainLooper()) {
            dispatchOnMainThread {
                doDestroy()
            }
        } else {
            doDestroy()
        }
    }

    override fun isDestroyed(): Boolean {
        synchronized(this) {
            return destroyed
        }
    }

    /**
     * Add a disposable which will be disposed whenever the context is destroyed.
     * This method is thread safe.
     */
    fun addDisposable(disposable: Disposable) {
        val added = synchronized(this) {
            if (destroyed) {
                false
            } else {
                if (this.disposables == null) {
                    this.disposables = mutableListOf()
                }
                this.disposables!!.add(disposable)

                true
            }
        }

        if (!added) {
            disposable.dispose()
        }
    }

    /**
     * Attach an opaque object given a key.
     * This is useful if you want to store and retrieve arbitrary data
     * from your context.
     */
    fun setAttachedObject(key: String, obj: Any?) {
        synchronized(this) {
            if (attachedObjects == null) {
                attachedObjects = hashMapOf()
            }
            attachedObjects!![key] = obj
        }
    }

    /**
     * Returns the attached opaque object for the given key.
     */
    fun getAttachedObject(key: String): Any? {
        return synchronized(this) {
            attachedObjects?.get(key)
        }
    }

    private fun doDestroy() {
        val componentContext = this.componentContext
        val runtime = this.runtimeOrNull

        native.destroy()
        onDestroy()

        if (runtime == null) {
            return
        }

        if (runtime.performGcOnContextDestroy) {
            runtime.performGcNow(true)
        }

        if (ValdiLeakTracker.enabled) {
            val componentToString = componentContext?.get()?.toString()
            if (componentToString != null) {
                ValdiLeakTracker.trackRef(componentContext, "ComponentContext $componentToString", runtime)
            }
        }
    }

    fun setViewModelNoUpdate(viewModel: Any?) {
        innerViewModel = viewModel
    }

    fun valueChangedForAttribute(viewNode: ValdiViewNode, attributeName: InternedString, attributeValue: Any?) {
        val runtime = this.runtimeOrNull ?: return
        NativeBridge.valueChangedForAttribute(runtime.native.nativeHandle, viewNode.nativeHandle, (attributeName as InternedStringCPP).nativeHandle, attributeValue)
    }

    /**
     * Register a custom AttributesBinder, where the views are created through reflection using a constructor
     * taking a single Context parameter. The created views from this factory will be enqueued into a local view
     * pool instead of the global view pool.
     */
    fun <T: View>registerAttributesBinder(attributesBinder: AttributesBinder<T>) {
        val ctor = attributesBinder.viewClass.getDeclaredConstructor(Context::class.java) as Constructor<T>

        registerViewFactory(attributesBinder.viewClass, { context -> ctor.newInstance(context) }, attributesBinder)
    }

    /**
     * Register a custom view factory for the given view class. The created views from
     * this factory will be enqueued into a local view pool instead of the global view pool.
     * If provided, the attributesBinder callback will be used to bind any additional attributes.
     */
    fun <T: View>registerViewFactory(viewClass: Class<T>, factory: (context: Context) -> T, attributesBinder: AttributesBinder<T>?) {
        val manager = this.runtimeOrNull?.manager ?: return

        val viewFactory = manager.createViewFactory(viewClass, factory, attributesBinder)
        registerViewFactory(viewFactory)
        viewFactory.getHandle().destroy()
    }

    /**
     * Register a custom view factory that was already created. The created views from
     * this factory will be enqueued into a local view pool instead of the global view pool.
     */
    fun registerViewFactory(viewFactory: ViewFactory) {
        native.withHandle { NativeBridge.registerLocalViewFactory(it, viewFactory.getHandle().nativeHandle) }
    }

    /**
     * Enqueue a callback to call on the next render
     */
    override fun enqueueNextRenderCallback(onNextRenderCallback: () -> Unit) {
        if (nextRendersCallbacks == null) {
            nextRendersCallbacks = mutableListOf()
        }
        nextRendersCallbacks?.add(onNextRenderCallback)
    }

    /**
     * Registers a callback to be called whenever the Valdi layout becomes dirty. This happens
     * whenever a node is moved/removed/added or when a layout attribute is changed during a render.
     * A dirty layout means that the measured size of the Valdi component might change.
     */
    fun onLayoutDirty(layoutDirtyCallback: () -> Unit) {
        if (layoutDirtyCallbacks == null) {
            layoutDirtyCallbacks = mutableListOf()
        }
        layoutDirtyCallbacks?.add(layoutDirtyCallback)
    }

    fun setDisableViewReuse(disableViewReuse: Boolean) {
    }

    fun setKeepViewAliveOnDestroy(keepViewAliveOnDestroy: Boolean) {
        native.withHandle { NativeBridge.setKeepViewAliveOnDestroy(it, keepViewAliveOnDestroy) }
    }

    fun performJsAction(name: String, parameters: Array<Any?>) {
        native.callJSFunction(name, parameters)
    }

    fun setDelayDestroy(delayDestroy: Boolean) {
        this.delayDestroy = delayDestroy
    }

    fun setParentContext(parentContext: ValdiContext) {
        native.withHandle { handle ->
            val parentRef = parentContext.native.nativeWrapper
            val parentHandle = parentRef.pin()
            try {
                NativeBridge.setParentContext(handle, parentHandle)
            } finally {
                if (parentHandle != 0L) {
                    parentRef.unpin()
                }
            }
        }
    }

    private fun getRetainedViewNode(viewId: String?, viewNodeId: Int): ValdiViewNode? {
        val viewNodeHandle = native.withHandle(0L) { NativeBridge.getRetainedViewNodeInContext(it, viewId, viewNodeId) }
        if (viewNodeHandle == 0L) {
            return null
        }

        return ValdiViewNode(viewNodeHandle, this)
    }

    override fun getViewNode(viewId: String): IValdiViewNode? {
        return getRetainedViewNode(viewId, 0)
    }

    fun getTypedViewNodeForId(viewNodeId: Int): ValdiViewNode? {
        return getRetainedViewNode(null, viewNodeId)
    }

    override fun getViewNodeForId(viewNodeId: Int): IValdiViewNode? {
        return getTypedViewNodeForId(viewNodeId)
    }

    override fun getRootViewNode(): IValdiViewNode? {
        return getRetainedViewNode(null, ROOT_VIEW_NODE_ID_SENTINEL)
    }

    override fun setRetainsLayoutSpecsOnInvalidateLayout(retainsLayoutSpecsOnInvalidateLayout: Boolean) {
        native.withHandle { NativeBridge.setRetainsLayoutSpecsOnInvalidateLayout(it, retainsLayoutSpecsOnInvalidateLayout) }
    }

    private fun makeValdiFunction(callback: () -> Unit): ValdiFunction {
        return object: ValdiFunction {
            override fun perform(marshaller: ValdiMarshaller): Boolean {
                callback()
                return false
            }
        }
    }

    /**
     * Asynchronously wait until all the pending updates of this ValdiContext have completed, and
     * call the given callback when done. If there are no pending updates in this ValdiContext,
     * the callback will be called immediately
     */
    override fun waitUntilAllUpdatesCompleted(callback: () -> Unit) {
        native.withHandle { NativeBridge.waitUntilAllUpdatesCompleted(it, makeValdiFunction(callback), false) }
    }

    override fun onNextLayout(callback: () -> Unit) {
        native.withHandle { NativeBridge.onNextLayout(it, makeValdiFunction(callback)) }
    }

    override fun scheduleExclusiveUpdate(callback: Runnable) {
        native.withHandle { NativeBridge.scheduleExclusiveUpdate(it, callback) }
    }

    /**
     * Synchronously wait until all the pending updates of this ValdiContext have completed.
     * If shouldFlushRenderRequests is true, this will cause the calling thread to process renders
     * scheduled into the context instead of waiting for the main thread to process them.
     */
    fun waitUntilAllUpdatesCompletedSync(shouldFlushRenderRequests: Boolean = false) {
        native.withHandle { NativeBridge.waitUntilAllUpdatesCompleted(it, null, shouldFlushRenderRequests) }
    }

    // Called any time a context has been rendered.
    internal fun onRender() {
        if (nextRendersCallbacks != null) {
            val callbacks = nextRendersCallbacks
            nextRendersCallbacks = null

            try {
                callbacks?.let { it.forEach { it() } }
            } catch (throwable: Throwable) {
                ValdiFatalException.handleFatal(throwable, "Failed to invoke onRender callbacks of ValdiContext $componentPath")
            }
        }
    }

    internal fun onLayoutDidBecomeDirty() {
        layoutDirtyCallbacks?.forEach { it() }
    }

    private fun measureSpecModeToYogaSpecMode(specMode: Int): Int {
        return when (specMode) {
            View.MeasureSpec.EXACTLY -> YOGA_MODE_EXACTLY
            View.MeasureSpec.AT_MOST -> if (useLegacyMeasureBehavior) YOGA_MODE_UNSPECIFIED else YOGA_MODE_AT_MOST
            else -> YOGA_MODE_UNSPECIFIED
        }
    }

    companion object {
        private const val ROOT_VIEW_NODE_ID_SENTINEL = -1
        private const val YOGA_MODE_UNSPECIFIED = 0
        private const val YOGA_MODE_EXACTLY = 1
        private const val YOGA_MODE_AT_MOST = 2

        /**
         * Get the current ValdiContext. This will only be available during JavaScript to Java calls.
         */
        @JvmStatic
        fun current(): ValdiContext? {
            return NativeBridge.getCurrentContext() as? ValdiContext
        }
    }


}
