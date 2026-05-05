package com.snap.valdi.attributes.impl

import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiScrollView
import com.snap.valdi.views.KeyboardDismissMode
import com.snap.valdi.views.IScrollPerfLoggerBridge
import com.snap.valdi.utils.CoordinateResolver
import com.snapchat.client.valdi_core.AttributeType
import com.snapchat.client.valdi_core.CompositeAttributePart

/**
 * Binds attributes for the ScrollView's view class
 */
class ScrollViewAttributesBinder(private val coordinateResolver: CoordinateResolver, private val logger: Logger): AttributesBinder<ValdiScrollView> {

    private class ScrollPerfLoggerBridgeWrapper(val scrollPerfLoggerBridge: IScrollPerfLoggerBridge)

    override val viewClass: Class<ValdiScrollView>
        get() = ValdiScrollView::class.java

    fun applyScrollEnabled(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.scrollEnabled = value
    }

    fun resetScrollEnabled(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.scrollEnabled = true
    }

    fun applyPagingEnabled(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.pagingEnabled = value
    }

    fun resetPagingEnabled(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.pagingEnabled = false
    }

    fun applyScrollPerfLoggerBridge(view: ValdiScrollView, value: Any?) {
        val wrapper = value as? ScrollPerfLoggerBridgeWrapper ?: throw AttributeError("scrollPerfLoggerBridge needs to be a IScrollPerfLoggerBridge, not ${value}")
        view.scrollPerfLoggerBridge = wrapper.scrollPerfLoggerBridge
    }

    fun resetScrollPerfLoggerBridge(view: ValdiScrollView) {
        view.scrollPerfLoggerBridge = null
    }

    fun applyShowsHoriztonalScrollIndicator(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.isHorizontalScrollBarEnabled = value
    }

    fun resetShowsHorizontalScrollIndicator(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.isHorizontalScrollBarEnabled = true
    }

    fun applyShowsVerticalScrollIndicator(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.isVerticalScrollBarEnabled = value
    }

    fun resetShowsVerticalScrollIndicator(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.isVerticalScrollBarEnabled = true
    }

    private fun applyBouncesVerticalWithSmallContent(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.bouncesVerticalWithSmallContent = value
    }

    private fun resetBouncesVerticalWithSmallContent(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.bouncesVerticalWithSmallContent = false
    }

    private fun applyBouncesHorizontalWithSmallContent(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.bouncesHorizontalWithSmallContent = value
    }

    private fun resetBouncesHorizontalWithSmallContent(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.bouncesHorizontalWithSmallContent = false
    }

    private fun applyDismissKeyboardOnDrag(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.dismissKeyboardOnDrag = value
    }

    private fun resetDismissKeyboardOnDrag(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.dismissKeyboardOnDrag = false
    }

    private fun applyDismissKeyboardOnDragMode(view: ValdiScrollView, value: String, animator: ValdiAnimator?) {
        view.dismissKeyboardMode = KeyboardDismissMode.fromString(value);
    }

    private fun resetDismissKeyboardOnDragMode(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.dismissKeyboardMode = KeyboardDismissMode.IMMEDIATE;
    }

    fun applyTranslatesForKeyboard(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        // no-op for now // TODO(664) - deprecate
    }

    fun resetTranslatesForKeyboard(view: ValdiScrollView, animator: ValdiAnimator?) {
        // no-op for now // TODO(664) - deprecate
    }

    fun applyCancelsTouchesOnScroll(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.cancelsTouchesOnScroll = value
    }

    fun resetCancelsTouchesOnScroll(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.cancelsTouchesOnScroll = true
    }

    fun applyBouncesFromDragAtStart(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.bouncesFromDragAtStart = value
    }

    fun resetBouncesFromDragAtStart(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.bouncesFromDragAtStart = true
    }

    fun applyBouncesFromDragAtEnd(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.bouncesFromDragAtEnd = value
    }

    fun resetBouncesFromDragAtEnd(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.bouncesFromDragAtEnd = true
    }

    fun applyFadingEdgeLength(view: ValdiScrollView, value: Float, animator: ValdiAnimator?) {
        val fadingEdgeLength = coordinateResolver.toPixel(value)
        view.setCustomFadingEdgeLength(fadingEdgeLength)
    }

    fun resetFadingEdgeLength(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.setCustomFadingEdgeLength(0)
        view.setHorizontalFadingEdgeEnabled(false)
        view.setVerticalFadingEdgeEnabled(false)
        view.setFadingEdgeLength(0)
    }

    fun applyFadingEdgeStart(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.fadingEdgeStartEnabled = value
    }

    fun resetFadingEdgeStart(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.fadingEdgeStartEnabled = true
    }

    fun applyFadingEdgeEnd(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.fadingEdgeEndEnabled = value
    }

    fun resetFadingEdgeEnd(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.fadingEdgeEndEnabled = true
    }

    fun applyDecelerationRate(view: ValdiScrollView, value: String, animator: ValdiAnimator?) {
        // no-op for now
    }

    fun resetDecelerationRate(view: ValdiScrollView, animator: ValdiAnimator?) {
        // no-op for now
    }

    fun applyBounces(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.bounces = value
    }

    fun resetBounces(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.bounces = true
    }

    fun applyAndroidOnlyEnableExtendedFadingEdge(view: ValdiScrollView, value: Boolean, animator: ValdiAnimator?) {
        view.androidOnlyEnableExtendedFadingEdge = value
    }

    fun resetAndroidOnlyEnableExtendedFadingEdge(view: ValdiScrollView, animator: ValdiAnimator?) {
        view.androidOnlyEnableExtendedFadingEdge = false
    }

    fun preprocessScrollPerfLoggerBridge(value: Any?): Any {
        if (value is IScrollPerfLoggerBridge) {
            return ScrollPerfLoggerBridgeWrapper(value)
        }

        if (value is Map<*, *>) {
            val scrollPerfLoggerBridge = value["\$nativeInstance"] as? IScrollPerfLoggerBridge
            if (scrollPerfLoggerBridge != null) {
                return ScrollPerfLoggerBridgeWrapper(scrollPerfLoggerBridge)
            }
        }

        throw AttributeError("Could not unwrap IScrollPerfLoggerBridge instance")
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ValdiScrollView>) {
        attributesBindingContext.bindScrollAttributes()
        attributesBindingContext.bindBooleanAttribute("scrollEnabled", true, this::applyScrollEnabled, this::resetScrollEnabled)
        attributesBindingContext.bindBooleanAttribute("pagingEnabled", false, this::applyPagingEnabled, this::resetPagingEnabled)
        attributesBindingContext.bindBooleanAttribute("showsHorizontalScrollIndicator", false, this::applyShowsHoriztonalScrollIndicator, this::resetShowsHorizontalScrollIndicator)
        attributesBindingContext.bindBooleanAttribute("showsVerticalScrollIndicator", false, this::applyShowsVerticalScrollIndicator, this::resetShowsVerticalScrollIndicator)
        attributesBindingContext.bindBooleanAttribute("bouncesVerticalWithSmallContent", false, this::applyBouncesVerticalWithSmallContent, this::resetBouncesVerticalWithSmallContent)
        attributesBindingContext.bindBooleanAttribute("bouncesHorizontalWithSmallContent", false, this::applyBouncesHorizontalWithSmallContent, this::resetBouncesHorizontalWithSmallContent)
        attributesBindingContext.bindBooleanAttribute("dismissKeyboardOnDrag", false, this::applyDismissKeyboardOnDrag, this::resetDismissKeyboardOnDrag)
        attributesBindingContext.bindStringAttribute("dismissKeyboardOnDragMode", false, this::applyDismissKeyboardOnDragMode, this::resetDismissKeyboardOnDragMode)
        attributesBindingContext.bindBooleanAttribute("translatesForKeyboard", false, this::applyTranslatesForKeyboard, this::resetTranslatesForKeyboard)
        attributesBindingContext.bindBooleanAttribute("bouncesFromDragAtStart", false, this::applyBouncesFromDragAtStart, this::resetBouncesFromDragAtStart)
        attributesBindingContext.bindBooleanAttribute("bouncesFromDragAtEnd", false, this::applyBouncesFromDragAtEnd, this::resetBouncesFromDragAtEnd)
        attributesBindingContext.bindBooleanAttribute("cancelsTouchesOnScroll", false, this::applyCancelsTouchesOnScroll, this::resetCancelsTouchesOnScroll)
        attributesBindingContext.bindUntypedAttribute("scrollPerfLoggerBridge", false, this::applyScrollPerfLoggerBridge, this::resetScrollPerfLoggerBridge)
        attributesBindingContext.bindFloatAttribute("fadingEdgeLength", false, this::applyFadingEdgeLength, this::resetFadingEdgeLength)
        attributesBindingContext.bindBooleanAttribute("fadingEdgeStart", false, this::applyFadingEdgeStart, this::resetFadingEdgeStart)
        attributesBindingContext.bindBooleanAttribute("fadingEdgeEnd", false, this::applyFadingEdgeEnd, this::resetFadingEdgeEnd)
        attributesBindingContext.bindStringAttribute("decelerationRate", false, this::applyDecelerationRate, this::resetDecelerationRate)
        attributesBindingContext.bindBooleanAttribute("bounces", false, this::applyBounces, this::resetBounces)
        attributesBindingContext.bindBooleanAttribute("androidOnlyEnableExtendedFadingEdge", false, this::applyAndroidOnlyEnableExtendedFadingEdge, this::resetAndroidOnlyEnableExtendedFadingEdge)

        attributesBindingContext.registerPreprocessor("scrollPerfLoggerBridge", false, this::preprocessScrollPerfLoggerBridge)
    }
}
