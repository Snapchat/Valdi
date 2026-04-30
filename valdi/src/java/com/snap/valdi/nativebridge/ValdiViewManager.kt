package com.snap.valdi.nativebridge

import android.content.Context
import androidx.annotation.Keep
import android.view.View
import com.snap.valdi.DebugMessagePresenter
import com.snap.valdi.DeferredViewFactory
import com.snap.valdi.ExceptionReporter
import com.snap.valdi.ViewRef
import com.snap.valdi.actions.ValdiNativeAction
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.AttributesBindingContextNative
import com.snap.valdi.attributes.MeasureDelegate
import com.snap.valdi.attributes.ViewLayoutAttributesCpp
import com.snap.valdi.attributes.impl.animations.ValdiAnimatorFactory
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.exceptions.ValdiFatalException
import com.snap.valdi.nodes.ValdiViewNode
import com.snap.valdi.nodes.ValdiViewNodeRef
import com.snap.valdi.logger.LogLevel
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.*
import java.nio.ByteBuffer

/**
 * The ViewManager is used by C++ code to manage views. C++ calls it when it needs to create a View,
 * move a View to a different parent View or for binding attributes.
 *
 * Methods in this class are used by native code. Don't remove them!
 */
@SuppressWarnings("unused")
@Suppress("UNCHECKED_CAST")
class ValdiViewManager(
    val context: Context,
    val logger: Logger,
    val disableAnimations: Boolean,
    val viewRefSupport: ViewRefSupport,
    private val maxViewOperationsProcessingTimeMs: Int,
) {

    var debugMessagePresenter: DebugMessagePresenter?
        get() {
            return synchronized(this) {
                innerDebugMessagePresenter
            }
        }
        set(newValue) {
            synchronized(this) {
                innerDebugMessagePresenter = newValue
            }
        }

    var exceptionReporter: ExceptionReporter?
        get() {
            return synchronized(this) {
                innerExceptionReporter
            }
        }
        set(newValue) {
            synchronized(this) {
                innerExceptionReporter = newValue
            }
        }

    private var innerDebugMessagePresenter: DebugMessagePresenter? = null
    private var innerExceptionReporter: ExceptionReporter? = null

    private val pendingAttributesBinderFutures = mutableListOf<Runnable>()

    private val attributesBinderForClass = hashMapOf<Class<*>, AttributesBinder<*>>()
    private val globalFactoryForClass = hashMapOf<Class<*>, DeferredViewFactory<*>>()

    private val operationsManager = ThreadLocal<ValdiViewManagerOperationsManager?>()

    fun appendAttributesBinderFuture(future: Runnable) {
        synchronized(pendingAttributesBinderFutures) {
            pendingAttributesBinderFutures.add(future)
        }
    }

    private fun flushNextAttributesBinderFuture(): Boolean {
        val operation: Runnable
        synchronized(pendingAttributesBinderFutures) {
            if (pendingAttributesBinderFutures.isEmpty()) {
                return false
            }
            operation = pendingAttributesBinderFutures.first()
        }

        operation.run()

        synchronized(pendingAttributesBinderFutures) {
            pendingAttributesBinderFutures.remove(operation)
        }

        return true
    }

    private fun flushAttributesBinderFutures() {
        while (flushNextAttributesBinderFuture()) {
            // no-op
        }
    }

    fun <T: View>getAttributesBinderForClass(cls: Class<T>): AttributesBinder<T>? {
        flushAttributesBinderFutures()

        return synchronized(attributesBinderForClass) {
            attributesBinderForClass[cls] as? AttributesBinder<T>
        }
    }

    fun <T: View> getGlobalViewFactory(cls: Class<T>): DeferredViewFactory<T>? {
        return synchronized(globalFactoryForClass) {
            globalFactoryForClass[cls] as? DeferredViewFactory<T>
        }
    }

    fun <T: View> registerGlobalViewFactory(viewFactory: DeferredViewFactory<T>) {
        synchronized(globalFactoryForClass) {
            globalFactoryForClass[viewFactory.viewClass] = viewFactory
        }

        if (viewFactory.attributesBinder != null) {
            registerGlobalAttributesBinder(viewFactory.attributesBinder)
        }
    }

    fun <T: View> registerGlobalAttributesBinder(attributesBinder: AttributesBinder<T>) {
        synchronized(attributesBinderForClass) {
            attributesBinderForClass[attributesBinder.viewClass] = attributesBinder
        }
    }

    fun getAllRegisteredClassNames(): List<String> {
        return synchronized(attributesBinderForClass) {
            attributesBinderForClass.keys.map {
                it.name
            }
        }
    }

    private fun onFatal(exc: Throwable): Nothing {
        ValdiFatalException.handleFatal(exc, "ViewManager call failed")
    }

    private inline fun <T> handleUncaughtException(receiver: () -> T): T {
        return try {
            receiver()
        } catch (exc: Throwable) {
            onFatal(exc)
        }
    }

    @Keep
    fun createViewFactory(cls: Class<*>): Any {
        return handleUncaughtException {
            val typedClass = cls as Class<View>
            val globalViewFactory = getGlobalViewFactory(typedClass)
            if (globalViewFactory === null) {
                val attributesBinder = getAttributesBinderForClass(typedClass)
                ReflectionViewFactory(context, viewRefSupport, typedClass, attributesBinder)
            } else {
                globalViewFactory
            }
        }
    }

    @Keep
    fun callAction(context: ValdiContext, actionName: String, parameters: Array<Any?>?) {
        handleUncaughtException {
            var action = context.actions.getAction(actionName)

            if (action == null) {
                action = ValdiNativeAction(context.actions.actionHandlerHolder, actionName, context.logger)
                context.actions.actionByName[actionName] = action
            }

            if (parameters == null) {
                action.perform(arrayOf())
            } else {
                action.perform(parameters)
            }
        }
    }

    @Keep
    fun bindAttributes(cls: Class<*>, bindingContextHandle: Long) {
        handleUncaughtException {
            val binder = getAttributesBinderForClass(cls as Class<View>) ?: return

            val native = AttributesBindingContextNative(cls, bindingContextHandle)

            binder.bindAttributes(AttributesBindingContext(native, logger))
        }
    }

    @Keep
    fun createAnimator(animationType: Int, controlPoints: Array<Any>?, duration: Double, beginFromCurrentState: Boolean, crossfade: Boolean, stiffness: Double, damping: Double): Any? {
        return handleUncaughtException {
            if (disableAnimations) {
                null
            } else {
                ValdiAnimatorFactory.createAnimation(
                        animationType,
                        controlPoints,
                        (1000 * duration).toLong(),
                        beginFromCurrentState,
                        stiffness,
                        damping
                )
            }
        }
    }

    @Keep
    fun createViewNodeWrapper(valdiContext: ValdiContext, viewNodeHandle: Long, wrapInPlatformReference: Boolean): Any? {
        val viewNode =  ValdiViewNode(viewNodeHandle, valdiContext)
        return if (wrapInPlatformReference) {
            ValdiViewNodeRef(viewNode)
        } else {
            viewNode
        }
    }

    @Keep
    fun presentDebugMessage(level: Int, str: String) {
        handleUncaughtException {
            val presenterLevel = when (level) {
                LogLevel.DEBUG -> DebugMessagePresenter.Level.INFO
                LogLevel.INFO -> DebugMessagePresenter.Level.INFO
                LogLevel.WARN -> DebugMessagePresenter.Level.ERROR
                LogLevel.ERROR -> DebugMessagePresenter.Level.ERROR
                else -> DebugMessagePresenter.Level.INFO
            }
            debugMessagePresenter?.presentDebugMessage(presenterLevel, str)
        }
    }

    @Keep
    fun onNonFatal(errorCode: Int, moduleName: String, errorMessage: String, stackTrace: String) {
        handleUncaughtException {
            exceptionReporter?.reportNonFatal(errorCode, errorMessage, moduleName.ifEmpty { null }, stackTrace.ifEmpty { null })
        }
    }

    @Keep
    fun onJsCrash(moduleName: String, errorMessage: String, stackTrace: String, isANR: Boolean) {
        handleUncaughtException {
            exceptionReporter?.reportCrash(errorMessage, moduleName.ifEmpty { null }, stackTrace.ifEmpty { null }, isANR)
        }
    }

    @Keep
    fun measure(measureDelegate: Any?,
                attributesHandle: Long,
                width: Int,
                widthMode: Int,
                height: Int,
                heightMode: Int,
                isRightToLeft: Boolean): Long {
        handleUncaughtException {
            val attributes = ViewLayoutAttributesCpp(attributesHandle)
            val widthMeasureSpec = ViewRef.makeMeasureSpec(width, widthMode)
            val heightMeasureSpec = ViewRef.makeMeasureSpec(height, heightMode)

            val size = (measureDelegate as MeasureDelegate).onMeasure(attributes, widthMeasureSpec, heightMeasureSpec, isRightToLeft)

            return size.value
        }
    }

    @Keep
    fun getMeasurerPlaceholderView(provider: Any): ViewRef? {
        handleUncaughtException {
            val placeholderView = (provider as Lazy<View?>).value ?: return null

            return ViewRef(placeholderView, true, viewRefSupport)
        }
    }

    @Keep
    fun performViewOperations(untypedByteBuffer: Any?, attachedValues: Array<Any>?, sync: Boolean) {
        handleUncaughtException {
            var viewOperationsManager = this.operationsManager.get()
            if (viewOperationsManager == null) {
                viewOperationsManager = ValdiViewManagerOperationsManager(this.logger, this.maxViewOperationsProcessingTimeMs)
                this.operationsManager.set(viewOperationsManager)
            }
            if (untypedByteBuffer != null) {
                viewOperationsManager.appendViewOperations(untypedByteBuffer as ByteBuffer, attachedValues)
            }
            viewOperationsManager.flushViewOperations(sync)
        }
    }
}
