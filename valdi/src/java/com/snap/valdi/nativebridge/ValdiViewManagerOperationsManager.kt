package com.snap.valdi.nativebridge

import android.os.SystemClock
import android.os.Handler
import android.os.Looper
import android.view.Choreographer
import android.view.View
import com.snap.valdi.ViewRef
import com.snap.valdi.attributes.AttributeHandlerDelegate
import com.snap.valdi.attributes.BooleanAttributeHandlerDelegate
import com.snap.valdi.attributes.CornersAttributeHandlerDelegate
import com.snap.valdi.attributes.FloatAttributeHandlerDelegate
import com.snap.valdi.attributes.IntAttributeHandlerDelegate
import com.snap.valdi.attributes.LongAttributeHandlerDelegate
import com.snap.valdi.attributes.ObjectAttributeHandlerDelegate
import com.snap.valdi.attributes.PercentAttributeHandlerDelegate
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.exceptions.ValdiFatalException
import com.snap.valdi.exceptions.messageWithCauses
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.info
import com.snap.valdi.utils.error
import com.snap.valdi.utils.trace
import com.snap.valdi.utils.runOnMainThreadDelayed
import com.snap.valdi.views.ValdiRootView
import java.nio.ByteBuffer
import java.nio.ByteOrder

class ValdiViewManagerOperationsManager(
    private val logger: Logger,
    private val maxViewOperationsProcessingTimeMs: Int,
) {
    private var pendingOperations = ArrayDeque<ValdiViewManagerOperations>()
    private var currentAnimator: ValdiAnimator? = null

    private var lastOperation = 0

    // Controls wether to throttle when maxViewOperationsProcessingTimeMs
    // is exceeded. maxViewOperationsProcessingTimeMs = 0 means no throttling
    private val throttlingEnabled = maxViewOperationsProcessingTimeMs > 0

    // Indicates there are remaining updates to be completed later
    private var throttled = false

    fun appendViewOperations(byteBuffer: ByteBuffer, attachedValues: Array<Any>?) {
        val operations = ValdiViewManagerOperations(byteBuffer.order(ByteOrder.LITTLE_ENDIAN), attachedValues ?: emptyArray())
        pendingOperations.add(operations)
    }

    // replace all direct buffers because these are released after calling
    // flushViewOperations
    private fun retainPendingOperations() {
        for (i in pendingOperations.indices) {
            val operation = pendingOperations[i]
            if (operation.byteBuffer.isDirect) {
                val src = operation.byteBuffer
                val bufferCopy = ByteBuffer
                    .allocate(src.remaining())
                    .order(ByteOrder.LITTLE_ENDIAN)
                bufferCopy.put(src)
                bufferCopy.flip()
                pendingOperations[i] =
                ValdiViewManagerOperations(bufferCopy, operation.attachedValues)
            }
        }
    }

    private fun shouldThrottle(t: Long, deadline: Long): Boolean =
        throttlingEnabled && t > deadline

    fun flushViewOperations(sync: Boolean) {
        if (this.throttled && !pendingOperations.isEmpty() && !sync) {
            // return if there is already a pending completion scheduled
            // buf we need to make sure all the buffers are safe
            retainPendingOperations()
            return
        }
        doFlushViewOperations(sync, false)
    }
    
    fun doFlushViewOperations(sync: Boolean, resume: Boolean) {
        // We have a limited time window to process as many pending operations as we can
        // After that we will return to the system looper and schedule another pass to
        // complete the rest. This is to avoid spending too much time in a function and
        // being marked as ANR.
        var processingStartTime = SystemClock.elapsedRealtime()
        val deadline = processingStartTime + maxViewOperationsProcessingTimeMs

        // Exit the loop if the deadline is passed. We will process at least one
        // operation becase in the beginning, processingStartTime is guaranteed
        // to be less than the deadline.
        while (!this.pendingOperations.isEmpty() &&
                (sync || !shouldThrottle(processingStartTime, deadline))) {
            val operations = this.pendingOperations.first()
            val buffer = operations.byteBuffer
            val attachedValues = operations.attachedValues

            // Exit the loop if the deadline is passed
            while (buffer.hasRemaining() &&
                   (sync || !shouldThrottle(processingStartTime, deadline))) {
                val header = buffer.int
                val operation = header and 0xFF
                val hasValue = ((header shr 8) and 0xFF) != 0

                when (operation) {
                    OP_BEGIN_RENDERING_VIEW -> {
                        beginRenderingView(buffer, attachedValues, hasValue)
                    }
                    OP_END_RENDERING_VIEW -> {
                        endRenderingView(buffer, attachedValues, hasValue, resume)
                    }
                    OP_MOVED_TO_TREE -> {
                        movedToTree(buffer, attachedValues, hasValue)
                    }
                    OP_MOVE_TO_PARENT -> {
                        moveViewToParent(buffer, attachedValues, hasValue)
                    }
                    OP_ADDED_TO_POOL -> {
                        addedToPool(buffer, attachedValues, hasValue)
                    }
                    OP_SET_FRAME -> {
                        setFrame(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_SET_SCROLLABLE_SPECS -> {
                        setScrollableSpecs(buffer, attachedValues, hasValue)
                    }
                    OP_SET_ACTIVE_ANIMATOR -> {
                        currentAnimator = updateActiveAnimator(buffer, attachedValues, hasValue)
                    }
                    OP_SET_LOADED_ASSET -> {
                        setLoadedAsset(buffer, attachedValues, hasValue)
                    }
                    OP_RESET_ATTRIBUTE -> {
                        resetAttribute(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_BOOL -> {
                        applyAttributeBool(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_INT -> {
                        applyAttributeInt(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_LONG -> {
                        applyAttributeLong(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_FLOAT -> {
                        applyAttributeFloat(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_OBJECT -> {
                        applyAttributeObject(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_PERCENT -> {
                        applyAttributePercent(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    OP_APPLY_ATTRIBUTE_CORNERS -> {
                        applyAttributeCorners(buffer, attachedValues, hasValue, currentAnimator)
                    }
                    else -> throw ValdiException("Invalid View Operation ${operation} (last operation: ${lastOperation})")
                }

                lastOperation = operation
                
                // update the timestamp
                processingStartTime = SystemClock.elapsedRealtime()
            }
            if (!buffer.hasRemaining() && operations == this.pendingOperations.firstOrNull()) {
                this.pendingOperations.removeAt(0)
                this.currentAnimator = null // reset animator after completing a buffer
            }
        }

        if (!this.pendingOperations.isEmpty()) {
            // If there are still pending operations, it means we failed to
            // process all pending operations within the time window. We need to
            // schedule another pass on the main thread to finish the rest.
            this.throttled = true
            retainPendingOperations()
            Choreographer.getInstance().postFrameCallback({
                this.throttled = false
                this.doFlushViewOperations(false, true)
            })
        }
    }

    private fun getRootView(buffer: ByteBuffer, attachedValues: Array<Any>): ValdiRootView? {
        val ref = attachedValues[buffer.int] as ViewRef
        val rootView = ref.get() as? ValdiRootView
        if (rootView == null) {
            logger.error("ValdiRootView is null")
        }

        return rootView
    }

    private fun beginRenderingView(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean) {
        getRootView(buffer, attachedValues)?.valdiUpdatesBegan()
    }

    private fun endRenderingView(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, async: Boolean) {
        val layoutDidBecomeDirty = hasValue
        if (async) {
            getRootView(buffer, attachedValues)?.valdiUpdatesEndedAsync(layoutDidBecomeDirty)
        } else {
            getRootView(buffer, attachedValues)?.valdiUpdatesEnded(layoutDidBecomeDirty)
        }
    }

    private fun handleApplyFailure(delegate: AttributeHandlerDelegate?, viewRef: ViewRef?, exception: Throwable) {
        if (exception !is AttributeError) {
            ValdiFatalException.handleFatal(exception, "Fatal error while processing attribute")
        }

        val view = viewRef?.get()
        if (delegate != null && view != null) {
            val viewNode = ViewUtils.findViewNode(view)
            if (viewNode != null) {
                viewNode.notifyApplyAttributeFailed(delegate.attributeId, exception.messageWithCauses())
                return
            }
        }

        logger.error("Failed to apply attribute on view ${view}: ${exception.message}")
    }

    private fun setLoadedAsset(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean) {
        val ref = attachedValues[buffer.int] as ViewRef
        val shouldDrawFlipped = buffer.int != 0
        val loadedAsset: Any? = if (hasValue) attachedValues[buffer.int] else null

        trace({"Valdi.applyImageAsset"}) {
            try {
                ref.onLoadedAssetChanged(loadedAsset, shouldDrawFlipped)
            } catch (e: ValdiException) {
                logger.error("Failed to set loaded asset: ${e.message}")
            }
        }
    }

    private inline fun <T: AttributeHandlerDelegate> handleAttribute(buffer: ByteBuffer,
                                                                     attachedValues: Array<Any>,
                                                                     receiver: (delegate: T, viewRef: ViewRef) -> Unit) {
        var delegate: T? = null
        var viewRef: ViewRef? = null

        try {
            delegate = attachedValues[buffer.int] as T
            viewRef = attachedValues[buffer.int] as ViewRef

            receiver(delegate, viewRef)
        } catch (exception: Throwable) {
            handleApplyFailure(delegate, viewRef, exception)
        }
    }

    private inline fun <T: AttributeHandlerDelegate> handleApplyAttribute(buffer: ByteBuffer,
                                                                          attachedValues: Array<Any>,
                                                                          receiver: (delegate: T, viewRef: ViewRef) -> Unit) {
        handleAttribute<T>(buffer, attachedValues) { delegate, viewRef ->
            trace({ delegate.applyAttributeTrace }) {
                receiver(delegate, viewRef)
            }
        }
    }

    private fun getViewFromViewRef(viewRef: ViewRef): View {
        return viewRef.get() ?: throw AttributeError("View instance is null")
    }

    private fun resetAttribute(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleAttribute<AttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            trace({ delegate.resetAttributeTrace }) {
                delegate.onReset(getViewFromViewRef(viewRef), currentAnimator)
            }
        }
    }

    private fun applyAttributeBool(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<BooleanAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val value = buffer.int != 0
            delegate.onApply(getViewFromViewRef(viewRef), value, currentAnimator)
        }
    }

    private fun applyAttributeInt(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<IntAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val value = buffer.int
            delegate.onApply(getViewFromViewRef(viewRef), value, currentAnimator)
        }
    }

    private fun applyAttributeLong(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<LongAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val value = buffer.long
            delegate.onApply(getViewFromViewRef(viewRef), value, currentAnimator)
        }
    }

    private fun applyAttributeFloat(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<FloatAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val value = buffer.float
            delegate.onApply(getViewFromViewRef(viewRef), value, currentAnimator)
        }
    }

    private fun applyAttributeObject(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<ObjectAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val value = attachedValues[buffer.int]
            delegate.onApply(getViewFromViewRef(viewRef), value, currentAnimator)
        }
    }

    private fun applyAttributePercent(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<PercentAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val value = buffer.float
            val isPercent = buffer.int != 0
            delegate.onApply(getViewFromViewRef(viewRef), value, isPercent, currentAnimator)
        }
    }

    private fun applyAttributeCorners(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        handleApplyAttribute<CornersAttributeHandlerDelegate>(buffer, attachedValues) { delegate, viewRef ->
            val borderFlags = buffer.int
            val topLeftValue = buffer.float
            val topRightValue = buffer.float
            val bottomRightValue = buffer.float
            val bottomLeftValue = buffer.float

            val topLeftIsPercent = (borderFlags and (1 shl 0)) != 0
            val topRightIsPercent = (borderFlags and (1 shl 1)) != 0
            val bottomRightIsIsPercent = (borderFlags and (1 shl 2)) != 0
            val bottomleftIsPercent = (borderFlags and (1 shl 3)) != 0

            delegate.onApply(getViewFromViewRef(viewRef),
                topLeftValue,
                topLeftIsPercent,
                topRightValue,
                topRightIsPercent,
                bottomRightValue,
                bottomRightIsIsPercent,
                bottomLeftValue,
                bottomleftIsPercent,
                currentAnimator)
        }
    }

    private fun updateActiveAnimator(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean): ValdiAnimator? {
        val newAnimator = if (hasValue) {
            attachedValues[buffer.int] as ValdiAnimator
        } else {
            null
        }

        this.currentAnimator = newAnimator

        return newAnimator
    }

    private fun movedToTree(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean) {
        val view = attachedValues[buffer.int] as ViewRef
        val userData = attachedValues[buffer.int] as ValdiContext
        val viewNodeId = buffer.int

        view.onMovedToContext(userData, viewNodeId)
    }

    private fun moveViewToParent(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean) {
        val view = attachedValues[buffer.int] as ViewRef
        if (hasValue) {
            val parentView = attachedValues[buffer.int] as ViewRef
            val viewIndex = buffer.int
            parentView.insertChild(view, viewIndex)
        } else {
            val shouldClearViewNode = buffer.int != 0
            view.removeFromParent(shouldClearViewNode)
        }
    }

    private fun addedToPool(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean) {
        val view = attachedValues[buffer.int] as ViewRef
        view.willEnqueueToPool()
    }

    private fun setFrame(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean, currentAnimator: ValdiAnimator?) {
        val isRightToLeft = hasValue
        val view = attachedValues[buffer.int] as ViewRef
        view.onFrameChanged(buffer.int, buffer.int, buffer.int, buffer.int, isRightToLeft, currentAnimator)
    }

    private fun setScrollableSpecs(buffer: ByteBuffer, attachedValues: Array<Any>, hasValue: Boolean) {
        val view = attachedValues[buffer.int] as ViewRef
        view.onScrollSpecsChanged(buffer.int, buffer.int, buffer.int, buffer.int, buffer.int != 0)
    }

    companion object {
        private const val OP_BEGIN_RENDERING_VIEW = 1
        private const val OP_END_RENDERING_VIEW = 2
        private const val OP_MOVED_TO_TREE = 3
        private const val OP_MOVE_TO_PARENT = 4
        private const val OP_ADDED_TO_POOL = 5
        private const val OP_SET_FRAME = 6
        private const val OP_SET_SCROLLABLE_SPECS = 7
        private const val OP_SET_ACTIVE_ANIMATOR = 8
        private const val OP_SET_LOADED_ASSET = 9
        private const val OP_RESET_ATTRIBUTE = 10
        private const val OP_APPLY_ATTRIBUTE_BOOL = 11
        private const val OP_APPLY_ATTRIBUTE_INT = 12
        private const val OP_APPLY_ATTRIBUTE_LONG = 13
        private const val OP_APPLY_ATTRIBUTE_FLOAT = 14
        private const val OP_APPLY_ATTRIBUTE_OBJECT = 15
        private const val OP_APPLY_ATTRIBUTE_PERCENT = 16
        private const val OP_APPLY_ATTRIBUTE_CORNERS = 17
    }
}
