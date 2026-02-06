package com.snap.valdi

import android.content.ComponentCallbacks
import android.content.Context
import android.content.res.Configuration
import android.graphics.Bitmap
import android.view.View
import android.view.ViewConfiguration
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.impl.AnimatedImageViewAttributesBinder
import com.snap.valdi.attributes.impl.ValdiRootViewAttributesBinder
import com.snap.valdi.attributes.impl.ValdiIndexPickerAttributesBinder
import com.snap.valdi.attributes.impl.ValdiDatePickerAttributesBinder
import com.snap.valdi.attributes.impl.ValdiImageViewAttributesBinder
import com.snap.valdi.attributes.impl.ValdiVideoViewAttributesBinder
import com.snap.valdi.attributes.impl.ValdiTextViewAttributesBinder
import com.snap.valdi.attributes.impl.ValdiTimePickerAttributesBinder
import com.snap.valdi.attributes.impl.EditTextAttributesBinder
import com.snap.valdi.attributes.impl.EditTextMultilineAttributesBinder
import com.snap.valdi.attributes.impl.ScrollViewAttributesBinder
import com.snap.valdi.attributes.impl.ShapeViewAttributesBinder
import com.snap.valdi.attributes.impl.TextViewAttributesBinder
import com.snap.valdi.attributes.impl.ViewAttributesBinder
import com.snap.valdi.attributes.impl.ViewGroupAttributesBinder
import com.snap.valdi.attributes.impl.fonts.DefaultFonts
import com.snap.valdi.attributes.impl.fonts.DefaultTypefaceResLoader
import com.snap.valdi.attributes.impl.fonts.FontDataProvider
import com.snap.valdi.attributes.impl.fonts.FontDescriptor
import com.snap.valdi.attributes.impl.fonts.FontManager
import com.snap.valdi.attributes.impl.fonts.TypefaceResLoader
import com.snap.valdi.attributes.impl.richtext.FontAttributes
import com.snap.valdi.attributes.impl.richtext.RichTextConverter
import com.snap.valdi.bundle.IValdiCustomModuleProvider
import com.snap.valdi.bundle.ResourceResolver
import com.snap.valdi.context.ContextManager
import com.snap.valdi.drawables.BoxShadowRendererPool
import com.snap.valdi.exceptions.GlobalExceptionHandler
import com.snap.valdi.exceptions.HostUncaughtExceptionHandler
import com.snap.valdi.imageloading.ValdiImageLoaderPostprocessor
import com.snap.valdi.imageloading.DefaultValdiImageLoader
import com.snap.valdi.jsmodules.ValdiStringsModule
import com.snap.valdi.jsmodules.ValdiJSRuntime
import com.snap.valdi.jsmodules.ValdiJSWorker
import com.snap.valdi.jsmodules.JSThreadDispatcher
import com.snap.valdi.logger.DefaultLogger
import com.snap.valdi.logger.Logger
import com.snap.valdi.modules.ValdiApplicationModule
import com.snap.valdi.modules.ValdiDateFormattingModule
import com.snap.valdi.modules.ValdiDeviceModule
import com.snap.valdi.modules.ValdiNativeModules
import com.snap.valdi.modules.ValdiNumberFormattingModule
import com.snap.valdi.modules.DrawingModuleImpl
import com.snap.valdi.nativebridge.ValdiViewManager
import com.snap.valdi.nativebridge.MainThreadDispatcher
import com.snap.valdi.nativebridge.RuntimeNative
import com.snap.valdi.network.CompositeRequestManager
import com.snap.valdi.network.DefaultHTTPRequestManager
import com.snap.valdi.snapdrawing.SnapDrawingRenderMode
import com.snap.valdi.store.KeychainUtils
import com.snap.valdi.utils.*
import com.snap.valdi.utils.executors.ExecutorFactory
import com.snap.valdi.snapdrawing.SnapDrawingOptions
import com.snap.valdi.snapdrawing.SnapDrawingRuntime
import com.snap.valdi.snapdrawing.SnapDrawingRuntimeCPP
import com.snap.valdi.snapdrawing.SnapDrawingThreadedFrameScheduler
import com.snap.valdi.ValdiRuntimeManager
import com.snap.valdi.views.AnimatedImageView
import com.snapchat.client.valdi_core.HTTPRequestManager
import com.snapchat.client.valdi_core.JavaScriptEngineType;
import com.snapchat.client.valdi_core.ModuleFactoriesProvider
import com.snapchat.client.valdi.NativeBridge
import com.snapchat.client.valdi.utils.NativeHandleWrapper
import java.io.File
import java.lang.ref.WeakReference
import java.util.concurrent.atomic.AtomicBoolean

class ValdiRuntimeManager(context: Context,
                                customLogger: Logger? = null,
                                val tweaks: ValdiTweaks? = null,
                                val requestManager: HTTPRequestManager? = null,
                                executorFactory: ExecutorFactory? = null,
                                typefaceResLoader: TypefaceResLoader? = null,
                                hostUncaughtExceptionHandler: HostUncaughtExceptionHandler? = null,
                                tracer: Tracer? = null,
                                val customModuleProvider: IValdiCustomModuleProvider? = null
) : LifecycleObserver, ComponentCallbacks, FontManager.Listener {

    val logger: Logger

    val nativeHandle: Long
        get() = handle.nativeHandle

    private val nativeBridge = NativeBridge()
    private val viewManager: ValdiViewManager
    private val contextManager: ContextManager

    private val handle: NativeHandleWrapper

    private var observingProcessLifecycle = false

    private var mainRuntimeHolder: ValdiRuntime? = null

    private val mainRuntimeLazy = lazy {
        synchronized(mainRuntimeListeners) {
            mainRuntimeHolder = doCreateRuntime(customModuleProvider, true)
            mainRuntimeHolder!!.also { notifyMainRuntimeListeners(it) }
        }
    }

    private var forceDarkMode = false

    private var isIntegrationTestEnvironment = false

    /**
     * `baseContext` will be a ContextWrapper if context passed to the constructor is a ContextWrapper.
     * `context` will always be the Application context.
     * Ideally `baseContext` is the only Context reference. However, it's unclear if there are any
     * call sites that require the Application context specifically, so we provide both for now.
     */
    val baseContext: Context = context
    val context: Context = context.applicationContext

    val mainRuntime: ValdiRuntime by mainRuntimeLazy

    val fontManager = FontManager(context, typefaceResLoader ?: DefaultTypefaceResLoader())

    val snapDrawingRuntime: SnapDrawingRuntime?
        get() = snapDrawingRuntimeField

    private val snapDrawingMaxRenderTargetSizeLazy = lazy {
        if (tweaks?.enableHardwareLayerWorkaround == true) {
            snapDrawingRuntime?.getMaxRenderTargetSize() ?: -1
        } else {
            -1
        }
    }
    val snapDrawingMaxRenderTargetSize: Long by snapDrawingMaxRenderTargetSizeLazy

    var debugMessagePresenter: DebugMessagePresenter?
        get() = viewManager.debugMessagePresenter
        set(value) {
            viewManager.debugMessagePresenter = value
        }

    var exceptionReporter: ExceptionReporter?
        get() = viewManager.exceptionReporter
        set(value) {
            viewManager.exceptionReporter = value
        }

    val coordinateResolver = CoordinateResolver(context)

    private val compositeRequestManager = CompositeRequestManager()

    val defaultRenderBackend: RenderBackend

    val useLegacyMeasureBehaviorByDefault: Boolean

    val snapDrawingAvailable: Boolean
        get() = snapDrawingRuntime != null

    val bitmapPool: BitmapPool
    val viewRefSupport: ViewRefSupport
    val imageLoaderPostprocessor: ValdiImageLoaderPostprocessor

    private val displayScale = context.resources.displayMetrics.density

    private val snapDrawingRuntimeField: SnapDrawingRuntimeCPP?

    private val keychain: KeychainUtils

    private val snapDrawingRenderBackendPrepared = AtomicBoolean(false)
    private val androidRenderBackendPrepared = AtomicBoolean(false)
    private val pendingRegisterFontsOperation = mutableListOf<Runnable>()

    private var runtimeStartupSpan: AsyncSpan? = null

    private var mainRuntimeListeners: MutableList<(runtime: IValdiRuntime)->Unit> = mutableListOf()

    init {
        this.logger = customLogger ?: DefaultLogger()

        val buildOptions = BuildOptions.get
        if (buildOptions.loggingEnabled) {
            logger.info("Initializing Valdi with build options: $buildOptions")
        }
        if (buildOptions.tracingEnabled) {
            VALDI_TRACER = tracer ?: OSTracer()
            runtimeStartupSpan = asyncTraceBegin("ValdiRuntime:runtimeStartup")
        }

        if (hostUncaughtExceptionHandler != null) {
            GlobalExceptionHandler.setHostUncaughtExceptionHandler(hostUncaughtExceptionHandler)
        }
        if (tweaks?.fatalExceptionSleepTimeBeforeRethrowing != null) {
            GlobalExceptionHandler.setSleepTimeBeforeRethrowing(tweaks.fatalExceptionSleepTimeBeforeRethrowing)
        }

        this.bitmapPool = BitmapPool(context, Bitmap.Config.ARGB_8888, logger)
        this.viewRefSupport = ViewRefSupport(logger, bitmapPool, coordinateResolver)

        if (tweaks?.useNativeHandlersManager == true) {
            NativeHandlesManager.start()
        }
        ValdiLeakTracker.enabled = tweaks?.enableLeakTracker == true

        viewManager = ValdiViewManager(context, logger,
            tweaks?.disableAnimations ?: false, viewRefSupport,
            tweaks?.maxViewOperationsProcessingTimeMs ?: 0)

        contextManager = ContextManager(nativeBridge, logger)

        val cacheRootDirFile = File(context.filesDir, "valdi_cache")

        val cacheRootDir = cacheRootDirFile.toString()

        forceDarkMode = tweaks?.forceDarkMode ?: false

        isIntegrationTestEnvironment = tweaks?.isTestEnvironment ?: false

        val anrTimeoutMs = tweaks?.anrTimeoutMs ?: 0

        keychain = KeychainUtils(context, tweaks?.enableKeychainEncryptorBypass ?: false, logger)

        this.imageLoaderPostprocessor = ValdiImageLoaderPostprocessor(context, bitmapPool)

        val resourceResolver = ResourceResolver(
            context,
            coordinateResolver,
            imageLoaderPostprocessor,
            logger,
            null)

        //TODO(1120): We need a better decision mechanism here
        var displayMetrics = context.resources.displayMetrics
        var debugTouchEvents = false
        var maxCacheSizeInBytes = 2 * displayMetrics.widthPixels.toLong() * displayMetrics.heightPixels.toLong()
        if (tweaks != null) {
            maxCacheSizeInBytes = tweaks.maxImageCacheSizeInBytes ?: maxCacheSizeInBytes
            debugTouchEvents = tweaks.debugTouchEvents
        }
        val javaScriptEngineType = tweaks?.javaScriptEngineType ?: JavaScriptEngineType.AUTO

        val viewConfiguration = ViewConfiguration.get(context)

        val longPressTimeoutMs = ViewConfiguration.getLongPressTimeout()
        val doubleTapTimeoutMs = ViewConfiguration.getDoubleTapTimeout()
        val scrollFriction = ViewConfiguration.getScrollFriction()
        val dragTouchSlopPixels = viewConfiguration.scaledTouchSlop
        val touchTolerancePixels = dragTouchSlopPixels / 2

        val runtimeManagerHandle = trace({ "Valdi.createNativeRuntimeManager"}) {
            NativeBridge.createRuntimeManager(
                    MainThreadDispatcher(logger),
                    SnapDrawingThreadedFrameScheduler(),
                    viewManager,
                    logger,
                    contextManager,
                    resourceResolver,
                    context.assets,
                    keychain,
                    cacheRootDir,
                    context.packageName,
                    displayScale,
                    longPressTimeoutMs,
                    doubleTapTimeoutMs,
                    dragTouchSlopPixels,
                    touchTolerancePixels,
                    scrollFriction,
                    debugTouchEvents,
                    tweaks?.keepDebuggerServiceOnPause ?: false,
                    maxCacheSizeInBytes,
                    javaScriptEngineType,
                    (tweaks?.jsThreadQoS ?: QoSClass.MAX).value,
                    anrTimeoutMs
            )
        }

        handle = object : NativeHandleWrapper(runtimeManagerHandle) {
            override fun destroyHandle(handle: Long) {
                NativeBridge.deleteRuntimeManager(handle)
            }
        }

        enqueueRegisterGlobalAttributesBinderOperation(Runnable {
            // Register well-known widgets
            registerDefaultGlobalAttributesBinder()
        })

        val httpRequestManager = requestManager ?: DefaultHTTPRequestManager(context)

        compositeRequestManager.addRequestManager("http", httpRequestManager)
        compositeRequestManager.addRequestManager("https", httpRequestManager)
        NativeBridge.setRuntimeManagerRequestManager(handle.nativeHandle, compositeRequestManager)

        registerAssetLoader(DefaultValdiImageLoader(context, imageLoaderPostprocessor, httpRequestManager))
        registerAssetLoader(ValdiRawImageResourceLoader(lazy { ExecutorsUtil.newSingleThreadCachedExecutor() }, context))

        fontManager.listener = this

        if (buildOptions.snapDrawingAvailable) {
            defaultRenderBackend = tweaks?.renderBackend ?: RenderBackend.DEFAULT
            snapDrawingRuntimeField = SnapDrawingRuntimeCPP(lazy {
                NativeRef(NativeBridge.getSnapDrawingRuntimeHandle(runtimeManagerHandle))
            }, displayScale, viewRefSupport, context)

            val logger = this.logger
            val snapDrawingRuntime = this.snapDrawingRuntimeField
            registerGlobalViewFactory(AnimatedImageView::class.java, {
                AnimatedImageView(SnapDrawingOptions(renderMode = SnapDrawingRenderMode.TEXTURE_VIEW), logger, snapDrawingRuntime, it)
            }, AnimatedImageViewAttributesBinder(context))
        } else {
            defaultRenderBackend = RenderBackend.ANDROID
            snapDrawingRuntimeField = null
        }

        useLegacyMeasureBehaviorByDefault = tweaks?.disableLegacyMeasureBehavior != true

        enqueueLoadOperation(Runnable {
            trace({ "Valdi.registerDefaultFonts"}) {
                DefaultFonts.register(fontManager)
            }
        })

        startObservingProcessLifecycle()
    }

    private fun registerDefaultGlobalAttributesBinder() {
        trace({ "Valdi.createGlobalAttributesBinders"}) {
            val boxShadowRendererPool = BoxShadowRendererPool(context, logger)

            val viewAttributesBinder = ViewAttributesBinder(context,
                    logger,
                    boxShadowRendererPool,
                    tweaks?.disableBoxShadow
                            ?: false, tweaks?.disableSlowClipping ?: false)

            val textConverter = RichTextConverter(fontManager)
            val editTextAttributesBinder = EditTextAttributesBinder(context, textConverter, FontAttributes.default)

            arrayOf(
                    viewAttributesBinder,
                    ViewGroupAttributesBinder(),
                    ValdiRootViewAttributesBinder(),
                    ScrollViewAttributesBinder(coordinateResolver, logger),
                    ShapeViewAttributesBinder(),
                    ValdiImageViewAttributesBinder(context),
                    ValdiVideoViewAttributesBinder(context),
                    TextViewAttributesBinder(context, textConverter, FontAttributes.default),
                    ValdiTextViewAttributesBinder(context),
                    editTextAttributesBinder,
                    EditTextMultilineAttributesBinder(context, editTextAttributesBinder),
                    ValdiIndexPickerAttributesBinder(context, logger),
                    ValdiDatePickerAttributesBinder(context, logger),
                    ValdiTimePickerAttributesBinder(context, logger)
            ).forEach { registerGlobalAttributesBinder(it) }
        }
    }

    fun emitInitMetrics() {
        NativeBridge.emitRuntimeManagerInitMetrics(handle.nativeHandle)
    }

    fun emitUserSessionReadyMetrics() {
        asyncTraceEnd(runtimeStartupSpan)
        NativeBridge.emitUserSessionReadyMetrics(handle.nativeHandle)
    }

    fun shouldUseSnapDrawingRenderBackend(requestedRenderBackend: RenderBackend): Boolean {
        if (!snapDrawingAvailable) {
            return false
        }

        if (requestedRenderBackend != RenderBackend.DEFAULT) {
            return requestedRenderBackend == RenderBackend.SNAP_DRAWING
        }

        return when (defaultRenderBackend) {
            RenderBackend.DEFAULT -> /* No preference set at all, use Android */ false
            RenderBackend.ANDROID -> false
            RenderBackend.SNAP_DRAWING -> true
        }
    }

    fun prepareRenderBackend(renderBackend: RenderBackend, preloadingMode: PreloadingMode) {
        val useSnapDrawing = shouldUseSnapDrawingRenderBackend(renderBackend)

        if (preloadingMode == PreloadingMode.AGGRESSIVE) {
            if (useSnapDrawing) {
                preloadSnapDrawing()
            } else {
                preloadAndroid()
            }
        }

        if (useSnapDrawing) {
            synchronized(pendingRegisterFontsOperation) {
                while (pendingRegisterFontsOperation.isNotEmpty()) {
                    enqueueLoadOperation(pendingRegisterFontsOperation.removeAt(pendingRegisterFontsOperation.size - 1))
                }
            }
        }
    }

    fun ensureSnapDrawingReady() {
        flushPendingLoadOperations()
        prepareRenderBackend(RenderBackend.SNAP_DRAWING, PreloadingMode.DISABLED)
    }

    private fun preloadSnapDrawing() {
        if (!snapDrawingRenderBackendPrepared.compareAndSet(false, true)) {
            return
        }

        NativeBridge.prepareRenderBackend(handle.nativeHandle, true)
    }

    private fun preloadAndroid() {
        if (!androidRenderBackendPrepared.compareAndSet(false, true)) {
            return
        }
        NativeBridge.prepareRenderBackend(handle.nativeHandle, false)
        fontManager.preloadAll()
    }

    fun createNativeModules(jsThreadDispatcher: JSThreadDispatcher): ValdiNativeModules {
        return ValdiNativeModules(
                ValdiApplicationModule(context, isIntegrationTestEnvironment),
                ValdiDeviceModule(jsThreadDispatcher, context, forceDarkMode),
                ValdiDateFormattingModule(context),
                ValdiNumberFormattingModule(context),
                DrawingModuleImpl(coordinateResolver, fontManager, logger),
                // We use `baseContext` here to ensure ContextWrapper is used if one was provided.
                // This allows us to implement custom behavior when accessing string resources.
                ValdiStringsModule(baseContext)
        )
    }

    fun createRuntime(customModuleProvider: IValdiCustomModuleProvider?): IValdiRuntime {
        return doCreateRuntime(customModuleProvider, isMain = false)
    }

    private fun doCreateRuntime(customModuleProvider: IValdiCustomModuleProvider?, isMain: Boolean): ValdiRuntime {
        val customResourceResolver = customModuleProvider?.let { ResourceResolver(context, coordinateResolver, this.imageLoaderPostprocessor, logger, customModuleProvider) }
        val nativeHandle = NativeBridge.createRuntime(handle.nativeHandle, customResourceResolver)
        val runtime = ValdiRuntime(RuntimeNative(nativeBridge, nativeHandle), context, logger, this, viewRefSupport, isMain)
        NativeBridge.setRuntimeAttachedObject(nativeHandle, runtime)

        trace({ "Valdi.createNativeModules"}) {
            val nativeModules = createNativeModules(runtime)
            runtime.nativeModules = nativeModules
            nativeModules.register(runtime)
        }

        registerRuntime(runtime)
        NativeBridge.onRuntimeReady(handle.nativeHandle, runtime.native.nativeHandle)

        return runtime
    }

    /**
     * Destroy the RuntimeManager and release all its native resources.
     */
    fun destroy() {
        stopObservingProcessLifecycle()

        if (mainRuntimeLazy.isInitialized()) {
            mainRuntimeLazy.value.destroy()
        }
        handle.destroy()
        snapDrawingRuntimeField?.clearCache()
    }

    /**
     * Register an attribute binder, which will allow Valdi to apply attributes to the viewClass
     * that this AttributesBinder handles. There can be only one AttributesBinder per view class.
     * Registering an AttributesBinder for a class that was bound already effectively replaces the
     * AttributesBinder. Valdi will register a base set of AttributesBinder to the standard android
     * View's classes. You should use this if you have a custom view class and you want to be able
     * to apply custom attributes to it. Because attributes binding happens lazily the first time
     * the view is requested, this method is effectively really cheap and can/should be done very
     * early in the app startup process.
     */
    fun <T : View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {
        viewManager.registerGlobalAttributesBinder(attributesBinder)
    }

    fun <T : View> registerGlobalAttributesBinderForClass(cls: Class<AttributesBinder<T>>) {
        val ctor = cls.getDeclaredConstructor(Context::class.java)
        val attributesBinder = ctor.newInstance(this.context)

        registerGlobalAttributesBinder(attributesBinder)
    }

    /**
      Register attributes binder from attributes binder that were annotated with
      @RegisterAttributesBinder .
     */
    fun registerGlobalAttributesBindersFromContext(context: Context) {
        enqueueRegisterGlobalAttributesBinderOperation(Runnable {
            val assetManager = context.assets

            val attributesBinderList = assetManager.list("valdi_attributes_binders") ?: return@Runnable

            for (clsName in attributesBinderList) {
                val cls = Class.forName(clsName)
                registerGlobalAttributesBinderForClass(cls as Class<AttributesBinder<View>>)
            }
        })
    }

    /**
     * Register a global view factory which will be used to create instances of the given view class.
     * By default, view instances are created by reflection by retrieving the constructor taking an
     * Android Context objects. View classes that cannot be instantiated using this method should
     * be registered as view factories explicitly. Note that view factories cannot be removed or
     * overriden once they have been registered, so the view factories that are registered should
     * have a lifecycle that is equal or longer than the ValdiRuntimeManager itself.
     */
    fun <T: View> registerGlobalViewFactory(viewClass: Class<T>,
                                            factory: (context: Context) -> T,
                                            attributesBinder: AttributesBinder<T>?) {
        val viewFactory = DeferredViewFactory(viewClass, factory, attributesBinder, viewRefSupport, context)
        viewManager.registerGlobalViewFactory(viewFactory)
    }

    /**
     * Registers a ValdiAssetLoader instance into the runtime, which will be used to load Android
     * Bitmap, Byte or Video assets for the URL schemes that this ValdiAssetLoader supports.
     * If there are multiple ValdiAssetLoader registered that can load the same URL scheme,
     * the ValdiAssetLoader which was registered last will take priority.
     */
    fun registerAssetLoader(assetLoader: ValdiAssetLoader) {
        val supportedSchemes = assetLoader.getSupportedURLSchemes().toTypedArray()
        val supportedOutputTypes = assetLoader.getSupportedOutputTypes()
        NativeBridge.registerAssetLoader(
                handle.nativeHandle,
                assetLoader,
                supportedSchemes,
                supportedOutputTypes)
    }

    /**
     * Unregister a previously registered ValdiAssetLoader from the runtime.
     */
    fun unregisterAssetLoader(assetLoader: ValdiAssetLoader) {
        NativeBridge.unregisterAssetLoader(handle.nativeHandle, assetLoader)
    }

    // For backwards compatibility
    fun registerImageLoader(imageLoader: ValdiAssetLoader) {
        registerAssetLoader(imageLoader)
    }

    fun unregisterImageLoader(imageLoader: ValdiAssetLoader) {
        unregisterAssetLoader(imageLoader)
    }

    fun <T: View>getAttributesBinderForClass(cls: Class<T>): AttributesBinder<T>? {
        return viewManager.getAttributesBinderForClass(cls)
    }

    fun registerClassReplacement(sourceClass: Class<*>, newClass: Class<*>) {
        NativeBridge.registerViewClassReplacement(handle.nativeHandle, sourceClass.name, newClass.name)
    }

    /**
     * Kick off a preload operation for the given view class name.
     * It will instantiate "count" instances of that view class and
     * put them into the view pool, which can make inflation faster
     * later on if those views are needed for rendering.
     * The preloader creates those views in the main thread but doesn't
     * spend more than 10ms per frame to create them.
     */
    fun preloadViews(cls: Class<*>, count: Int) {
        NativeBridge.preloadViews(handle.nativeHandle, cls.name, count)
    }

    /**
     * Force all the registered attributes to be bound. This is useful if you want
     * to bind them eagerly in a background thread to avoid the lazy binding to happen
     * during rendering in the main thread.
     */
    fun forceBindAllAttributes() {
        viewManager.getAllRegisteredClassNames().forEach {
            NativeBridge.forceBindAttributes(handle.nativeHandle, it)
        }
    }

    fun getAllRuntimes(): List<ValdiRuntime> {
        val runtimes = NativeBridge.getAllRuntimeAttachedObjects(handle.nativeHandle) as Array<Any>

        return runtimes.map { it as ValdiRuntime }
    }

    fun clearViewPools() {
        runOnMainThreadIfNeeded {
            NativeBridge.clearViewPools(handle.nativeHandle)
        }
    }

    fun <T: View> createViewFactory(viewClass: Class<T>, factory: (context: Context) -> T, attributesBinder: AttributesBinder<T>?): ViewFactory {
        val handle = NativeBridge.createViewFactory(
                handle.nativeHandle,
                viewClass.name,
                DeferredViewFactory(viewClass, factory, attributesBinder, viewRefSupport, context),
                attributesBinder != null
        )

        return CppViewFactory(handle)
    }

    private fun startObservingProcessLifecycle() {
        runOnMainThreadIfNeeded {
            if (observingProcessLifecycle) {
                return@runOnMainThreadIfNeeded
            }
            observingProcessLifecycle = true
            // Using ProcessLifecycleOwner is a lint violation, but this class is only used in development
            ProcessLifecycleOwner.get().lifecycle.addObserver(this)
            context.registerComponentCallbacks(this)
        }
    }

    private fun stopObservingProcessLifecycle() {
        runOnMainThreadIfNeeded {
            if (!observingProcessLifecycle) {
                return@runOnMainThreadIfNeeded
            }
            observingProcessLifecycle = false

            // Using ProcessLifecycleOwner is a lint violation, but this class is only used in development
            ProcessLifecycleOwner.get().lifecycle.removeObserver(this)
            context.unregisterComponentCallbacks(this)
        }
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun onResume() {
        applicationDidResume()
    }

    @OnLifecycleEvent(Lifecycle.Event.ON_PAUSE)
    fun onPause() {
        applicationWillPause()
    }

    fun dumpMemoryStatistics() : JavaScriptMemoryStatistics? {
        val array = (NativeBridge.dumpMemoryStatistics(this.nativeHandle) as Array<*>).map { it as Long }
        val totalMemory  = array[0]
        val totalObjects = array[1]

        if (totalMemory == -1L || totalObjects == -1L) {
            return null;
        }

        return JavaScriptMemoryStatistics(totalMemory, totalObjects)
    }

    fun applicationDidResume() {
        runOnMainThreadIfNeeded {
            val density = context.resources.displayMetrics.density
            val scaledDensity = context.resources.displayMetrics.scaledDensity
            val dynamicTypeScale = scaledDensity / density
            NativeBridge.applicationSetConfiguration(handle.nativeHandle, dynamicTypeScale)
            NativeBridge.applicationDidResume(handle.nativeHandle)
        }
    }

    fun applicationWillPause() {
        runOnMainThreadIfNeeded {
            NativeBridge.applicationWillPause(handle.nativeHandle)
            snapDrawingRuntimeField?.clearCache()
        }
    }

    /**
     * Enqueue an asynchronous load operation that will be executed in the worker thread.
     * The operation will be flushed in the main thread if it does not complete before the next
     * render request for a component comes in. This function can be used to schedule work
     * that must be completed before rendering, but could otherwise be running in parallel
     * from other initialization tasks.
     */
    fun enqueueLoadOperation(operation: Runnable) {
        NativeBridge.enqueueLoadOperation(nativeHandle, operation)
    }

    /**
      Enqueue a load operation that will register global attributes binders.
      This can be used to guarantee that the operation gets executed before any
      attributes is about to be bound. The runtime will do a best effort at executing
      the operation in a worker thread, but if attributes need to be resolved before the
      worker thread had time to flush that operation, it will flush it in the thread
      that needs the attributes.
     */
    fun enqueueRegisterGlobalAttributesBinderOperation(operation: Runnable) {
        val future = SingleRunnable(operation)
        viewManager.appendAttributesBinderFuture(future)

        enqueueLoadOperation(future)
    }

    internal fun flushPendingLoadOperations() {
        NativeBridge.flushLoadOperations(nativeHandle)
    }

    /**
     * Set the Thread QoS class of the main JS threads used in all Runtime instances.
     */
    fun setJsThreadQoSClass(qoSClass: QoSClass) {
        NativeBridge.setRuntimeManagerJsThreadQoS(nativeHandle, qoSClass.value)
    }

    fun setUserSession(userId: String?) {
        NativeBridge.setUserSession(handle.nativeHandle, userId)
    }

    fun addRequestManager(scheme: String, requestManager: HTTPRequestManager) {
        compositeRequestManager.addRequestManager(scheme, requestManager)
    }

    /**
     * Register a ModuleFactoriesProvider which will provide additional modules to every created Runtime.
     */
    fun registerModuleFactoriesProvider(moduleFactoriesProvider: ModuleFactoriesProvider) {
        NativeBridge.registerModuleFactoriesProvider(nativeHandle, moduleFactoriesProvider)
    }

    /**
     * Register a type converter for the given native class and a path to the converter function at the TypeScript level.
     */
    fun registerTypeConverter(cls: Class<*>, functionPath: String) {
        NativeBridge.registerTypeConverter(nativeHandle, cls.name, functionPath)
    }

    /**
     * Enqueue a task to be executed on the worker thread.
     */
    fun enqueueWorkerTask(runnable: Runnable) {
        NativeBridge.enqueueWorkerTask(nativeHandle, runnable)
    }

    /**
     * Acquire a Worker runtime on the given executor
     */
    fun getWorker(executor: String, block: (ValdiJSRuntime) -> Unit) {
        val existingWorker = synchronized(workerExecutorCache) {
            workerExecutorCache.get(executor)?.get()
        }
        if (existingWorker != null) {
            existingWorker!!.runOnJsThread { block(existingWorker!!) }
        } else {
            mainRuntime.getJSRuntime {jsRuntime ->
                val newWorker = ValdiJSWorker(jsRuntime.getNativeObject().createWorker())
                synchronized(workerExecutorCache) {
                    workerExecutorCache.put(executor, WeakReference(newWorker))
                }
                newWorker.runOnJsThread { block(newWorker) }
            }
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {

    }

    override fun onLowMemory() {
        bitmapPool.clear()
        NativeBridge.applicationIsInLowMemory(handle.nativeHandle)
        snapDrawingRuntimeField?.clearCache()
    }

    private fun makeRegisterFontOperation(descriptor: FontDescriptor, isFallback: Boolean, dataProvider: FontDataProvider): Runnable {
        return Runnable {
            val snapDrawingRuntime = this.snapDrawingRuntime
            if (descriptor.family == null || snapDrawingRuntime == null) {
                return@Runnable
            }

            val assetFileDescriptor = dataProvider.openAssetFileDescriptor()
            if (assetFileDescriptor != null) {
                assetFileDescriptor.use {
                    snapDrawingRuntime.registerTypeface(
                            descriptor.family,
                            descriptor.weight,
                            descriptor.style,
                            isFallback,
                            null,
                            it.parcelFileDescriptor.fd)
                }
            } else {
                val bytes = dataProvider.getBytes()
                snapDrawingRuntime.registerTypeface(
                        descriptor.family,
                        descriptor.weight,
                        descriptor.style,
                        isFallback,
                        bytes,
                        0)
            }
        }
    }

    override fun onTypefaceRegistered(descriptor: FontDescriptor, isFallback: Boolean, dataProvider: FontDataProvider) {
        val registerFontOperation = makeRegisterFontOperation(descriptor, isFallback, dataProvider)

        synchronized(pendingRegisterFontsOperation) {
            if (snapDrawingRenderBackendPrepared.get()) {
                // If we have already prepared the android render backend, immediately schedule the
                // register font operation
                enqueueLoadOperation(registerFontOperation)
            } else {
                // Otherwise, we wait until prepare() is called for SnapDrawing
                pendingRegisterFontsOperation.add(registerFontOperation)
            }
        }
    }

    override fun onFontRequested(descriptor: FontDescriptor) {
        // Some fonts get registered through a pending load operation, we call flush here
        // to ensure that those operations get processed.
        flushPendingLoadOperations()
    }

    fun addMainRuntimeListener(cb: (runtime: IValdiRuntime)->Unit) {
        synchronized(mainRuntimeListeners) {
            mainRuntimeListeners.add(cb)
            mainRuntimeHolder?.let { notifyMainRuntimeListeners(it) }
        }
    }

    private fun notifyMainRuntimeListeners(runtime: ValdiRuntime) {
        mainRuntimeListeners.forEach { listener ->
            listener(runtime)
        }
        mainRuntimeListeners.clear()
    }

    companion object {
        private val runtimes = mutableListOf<WeakReference<ValdiRuntime>>()
        private fun registerRuntime(runtime: ValdiRuntime) {
            synchronized(runtimes) {
                runtimes.removeAll { it.get() == null }
                runtimes.add(WeakReference(runtime))
            }
        }

        @JvmStatic
        fun allRuntimes(): List<ValdiRuntime> {
            return synchronized(runtimes) { runtimes.mapNotNull { it.get() } }
        }

        private val workerExecutorCache = mutableMapOf<String, WeakReference<ValdiJSWorker>>()
    }
}
