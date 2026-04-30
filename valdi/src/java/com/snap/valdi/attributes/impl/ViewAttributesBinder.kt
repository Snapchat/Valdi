package com.snap.valdi.attributes.impl

import com.snap.valdi.attributes.impl.animations.ValdiValueAnimation
import android.content.Context
import android.graphics.Color
import android.view.View
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.conversions.ColorConversions
import com.snap.valdi.attributes.impl.animations.ColorAnimator
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.animations.ViewAnimator
import com.snap.valdi.attributes.impl.gestures.GestureAttributes
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.drawables.BoxShadowRendererPool
import com.snap.valdi.drawables.utils.BorderRadii
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.utils.JSConversions
import com.snap.valdi.utils.error
import com.snap.valdi.views.ValdiClippableView
import com.snap.valdi.views.ValdiForegroundHolder
import com.snapchat.client.valdi_core.AttributeType
import com.snapchat.client.valdi_core.CompositeAttributePart

/**
 * Binds attributes for the View class
 */
class ViewAttributesBinder(private val context: Context,
                           val logger: Logger,
                           val boxShadowRendererPool: BoxShadowRendererPool,
                           val disableBoxShadow: Boolean = false,
                           val disableSlowClipping: Boolean = false) : AttributesBinder<View> {

    companion object {
        private val ALPHA_KEY = "ALPHA_KEY"
        private val BACKGROUND_COLOR_KEY = "BACKGROUND_COLOR_KEY"
        private val BORDER_RADIUS_KEY = "BORDER_RADIUS_KEY"
        private val BORDER_KEY = "BORDER_KEY"
        private val TRANSLATION_X_KEY = "TRANSLATION_X_KEY"
        private val TRANSLATION_Y_KEY = "TRANSLATION_Y_KEY"
        private val SCALE_X_KEY = "SCALE_X_KEY"
        private val SCALE_Y_KEY = "SCALE_Y_KEY"
        private val ROTATION_KEY = "ROTATION_KEY"
        private val BOX_SHADOW_KEY = "BOX_SHADOW_KEY"

        private val RGBA_COLOR_BLACK = 0x000000ffL
    }

    private val coordinateResolver = CoordinateResolver(context)

    private val gestureAttributes = GestureAttributes(coordinateResolver)

    private val accessibilityIdByName = hashMapOf<String, Int>()

    override val viewClass: Class<View>
        get() = View::class.java

    fun applyAlpha(view: View, value: Float, animator: ValdiAnimator?) {
        ViewUtils.cancelAnimation(view, ALPHA_KEY)
        if (animator == null) {
            view.alpha = value
            return
        }

        animator.addValueAnimation(ALPHA_KEY, view, ViewAnimator.animateAlpha(view, value))
    }

    fun resetAlpha(view: View, animator: ValdiAnimator?) {
        ViewUtils.cancelAnimation(view, ALPHA_KEY)
        if (animator == null) {
            view.alpha = 1f
            return
        }

        animator.addValueAnimation(ALPHA_KEY, view, ViewAnimator.animateAlpha(view, 1f))
    }

    fun applyBackground(view: View, value: Array<Any>, animator: ValdiAnimator?) {
        val valdiGradient = ValdiGradient.fromGradientData(value)

        if (valdiGradient.colors.isEmpty()) {
            resetBackgroundInternal(view, animator)
        } else if (valdiGradient.colors.size == 1) {
            applyBackgroundColor(
                view,
                valdiGradient.colors[0],
                animator
            )
        } else {
            ViewUtils.mutateBackgroundDrawable(view) { background ->
                background.setFillGradient(
                    valdiGradient.getDrawableGradientType(),
                    valdiGradient.colors,
                    valdiGradient.locations,
                    valdiGradient.getDrawableOrientation()
                )
            }
        }
    }

    fun applyBackgroundColor(view: View, value: Int, animator: ValdiAnimator?) {
        ViewUtils.cancelAnimation(view, BACKGROUND_COLOR_KEY)
        val hadBackground = view.background != null

        val background = ViewUtils.acquireBackgroundDrawable(view)
        if (animator == null) {
            background.setFillColor(value)
            ViewUtils.releaseBackgroundDrawable(view, background)
            return
        }

        if (!hadBackground) {
            background.setFillColor(Color.TRANSPARENT)
        }

        animator.addValueAnimation(
                BACKGROUND_COLOR_KEY,
                view,
                ColorAnimator.animateGradientDrawable(background, value)) {
            ViewUtils.releaseBackgroundDrawable(view, background)
        }
    }

    fun resetBackground(view: View, animator: ValdiAnimator?) {
        resetBackgroundInternal(view, animator)
    }

    fun resetBackgroundInternal(view: View, animator: ValdiAnimator?) {
        applyBackgroundColor(view, Color.TRANSPARENT, animator)
    }

    fun setBorderRadius(view: View,
                          topLeft: Float,
                          topLeftIsPercent: Boolean,
                          topRight: Float,
                          topRightIsPercent: Boolean,
                          bottomRight: Float,
                          bottomRightIsPercent: Boolean,
                          bottomLeft: Float,
                        bottomLeftIsPercent: Boolean, animator: ValdiAnimator?) {
        val borderRadii = BorderRadii.fromPointValues(
                topLeft,
                topRight,
                bottomRight,
                bottomLeft,
                topLeftIsPercent,
                topRightIsPercent,
                bottomRightIsPercent,
                bottomLeftIsPercent,
                coordinateResolver)

        applyBorderRadius(view, borderRadii, animator)
    }

    fun applyBorderRadius(view: View,
                       borderRadii: BorderRadii?,
                       animator: ValdiAnimator?) {
        if (animator == null) {
            val borderAnimator = ViewUtils.getTransitionInfo(view)?.getValueAnimator(BORDER_RADIUS_KEY)
            if (borderAnimator != null) {
                val endValue = borderAnimator.valueAnimation.additionalData as BorderRadii

                if (endValue == borderRadii) {
                    // If we are animating to those values, ignore this call and let the animation
                    // tick by itself
                    return
                }
            }
        }

        ViewUtils.cancelAnimation(view, BORDER_RADIUS_KEY)

        if (animator == null) {
            ViewUtils.setBorderRadii(view, borderRadii)
            return
        }

        animator.addValueAnimation(BORDER_RADIUS_KEY, view, ViewAnimator.animateBorderRadius(view, borderRadii))
    }

    fun resetBorderRadius(view: View, animator: ValdiAnimator?) {
        applyBorderRadius(view, null, animator)
    }

    private fun applyBorderComposite(view: View, value: Any?, animator: ValdiAnimator?) {
        if (value !is Array<*>) {
            throw AttributeError("Expecting an array for the composite attribute")
        }

        val borderWidth = value[0] as? Double ?: 0.0
        val borderColor = value[1] as? Long ?: RGBA_COLOR_BLACK
        setBorder(view, borderWidth, borderColor, animator)
    }

    private fun setBorder(view: View, borderWidth: Double, borderColor: Long, animator: ValdiAnimator?) {
        if (view !is ValdiForegroundHolder) {
            throw AttributeError("The View needs to implement the ValdiForegroundHolder interface in order to use the 'border' attribute")
        }
        ViewUtils.cancelAnimation(view, BORDER_KEY)

        val resolvedBorderWidth = coordinateResolver.toPixel(borderWidth)
        val color = ColorConversions.fromRGBA(borderColor)

        val foreground = ViewUtils.acquireForegroundDrawable(view)
        if (animator == null) {
            foreground.setStroke(resolvedBorderWidth, color)
            ViewUtils.releaseForegroundDrawable(view, foreground)
            return
        }

        animator.addValueAnimation(
                BORDER_KEY,
                view,
                ViewAnimator.animateBorder(view, foreground, resolvedBorderWidth, color)) {
            ViewUtils.releaseForegroundDrawable(view, foreground)
        }
    }

    fun applyBorder(view: View, value: Array<Any>, animator: ValdiAnimator?) {
        val color = if (value.size > 1) JSConversions.asLong(value[1]) else 0
        setBorder(view, JSConversions.asDouble(value[0]), color, animator)
    }

    fun resetBorder(view: View, animator: ValdiAnimator?) {
        setBorder(view, 0.0, 0, animator)
    }

    fun applyTouchEnabled(view: View, value: Boolean, animator: ValdiAnimator?) {
        view.isEnabled = value
    }

    fun resetTouchEnabled(view: View, animator: ValdiAnimator?) {
        view.isEnabled = true
    }

    fun applyBoxShadow(view: View, value: Any?, animator: ValdiAnimator?) {
        if (disableBoxShadow) {
            return
        }

        if (value !is Array<*>) {
            resetBoxShadow(view, animator)
            return
        }

        if (value.size < 5) {
            throw AttributeError("boxShadow components should have 5 entries")
        }

        val widthOffset = coordinateResolver.toPixel(value[1] as? Double ?: 0.0)
        val heightOffset = coordinateResolver.toPixel(value[2] as? Double ?: 0.0)
        val blur = coordinateResolver.toPixel(value[3] as? Double ?: 0.0)
        val color = ColorConversions.fromRGBA(value[4] as? Long ?: 0)

        ViewUtils.mutateBackgroundDrawable(view) { background ->
            background.setBoxShadow(widthOffset, heightOffset, blur.toFloat(), color, boxShadowRendererPool)
        }
    }

    fun resetBoxShadow(view: View, animator: ValdiAnimator?) {
        if (disableBoxShadow) {
            return
        }
        ViewUtils.mutateBackgroundDrawable(view) { background ->
            background.setBoxShadow(0, 0, 0f, Color.TRANSPARENT, boxShadowRendererPool)
        }
    }

    private inline fun setTransformElement(view: View,
                                           value: Float,
                                           animator: ValdiAnimator?,
                                           animationKey: Any,
                                           crossinline getValue: View.() -> Float,
                                           crossinline setValue: View.(Float) -> Unit,
                                           minimumVisibleChange: Float) {
        if (animator == null) {
            ViewUtils.cancelAnimation(view, animationKey)
            setValue(view, value)
        } else {
            animator.addValueAnimation(animationKey,
                    view,
                    ViewAnimator.animateTransformElement(value, view, getValue, setValue, minimumVisibleChange=minimumVisibleChange))
        }
    }

    private fun reapplyTranslationXIfNeeded(view: View, value: Float) {
        val translationX = ViewUtils.resolveDeltaX(view, value)
        val valueAnimator = ViewUtils.getTransitionInfo(view)?.getValueAnimator(TRANSLATION_X_KEY)

        if (valueAnimator == null) {
            view.translationX = translationX
        } else if (valueAnimator.valueAnimation.additionalData != translationX) {
            ViewUtils.cancelAnimation(view, TRANSLATION_X_KEY)
            view.translationX = translationX
        }
    }

    fun applyTranslationX(view: View, value: Float, animator: ValdiAnimator?) {
        val resolvedValue = coordinateResolver.toPixelF(value)
        val resolvedTranslationX = ViewUtils.resolveDeltaX(view, resolvedValue)

        if (value != 0.0f) {
            ViewUtils.setDidFinishLayoutForKey(view, "translationX") {
                reapplyTranslationXIfNeeded(it, resolvedValue)
            }
        } else {
            ViewUtils.removeDidFinishLayoutForKey(view, "translationX")
        }

        setTransformElement(view,
                resolvedTranslationX,
                animator,
                TRANSLATION_X_KEY,
                { translationX },
                { translationX = it },
                ValdiValueAnimation.MinimumVisibleChange.PIXEL)
    }

    fun resetTranslationX(view: View, animator: ValdiAnimator?) {
        applyTranslationX(view, 0.0f, animator)
    }

    fun applyTranslationY(view: View, value: Float, animator: ValdiAnimator?) {
        val resolvedValue = coordinateResolver.toPixelF(value)
        setTransformElement(view,
            resolvedValue,
            animator,
            TRANSLATION_Y_KEY,
            { translationY },
            { translationY = it },
            ValdiValueAnimation.MinimumVisibleChange.PIXEL)
    }

    fun resetTranslationY(view: View, animator: ValdiAnimator?) {
        applyTranslationY(view, 0.0f, animator)
    }

    fun applyScaleX(view: View, value: Float, animator: ValdiAnimator?) {
        setTransformElement(view,
            value,
            animator,
            SCALE_X_KEY,
            { scaleX },
            { scaleX = it },
            ValdiValueAnimation.MinimumVisibleChange.SCALE_RATIO)
    }

    fun resetScaleX(view: View, animator: ValdiAnimator?) {
        applyScaleX(view, 1.0f, animator)
    }

    fun applyScaleY(view: View, value: Float, animator: ValdiAnimator?) {
        setTransformElement(view,
            value,
            animator,
            SCALE_Y_KEY,
            { scaleY },
            { scaleY = it },
            ValdiValueAnimation.MinimumVisibleChange.SCALE_RATIO)
    }

    fun resetScaleY(view: View, animator: ValdiAnimator?) {
        applyScaleY(view, 1.0f, animator)
    }

    fun applyRotation(view: View, value: Float, animator: ValdiAnimator?) {
        val radianValue = ViewUtils.resolveDeltaX(view, value)
        val resolvedValue = Math.toDegrees(radianValue.toDouble()).toFloat()

        setTransformElement(view,
            resolvedValue,
            animator,
            ROTATION_KEY,
            { rotation },
            { rotation = it },
            ValdiValueAnimation.MinimumVisibleChange.ROTATION_DEGREES_ANGLE)
    }

    fun resetRotation(view: View, animator: ValdiAnimator?) {
        applyRotation(view, 0.0f, animator)
    }

    private fun getResourceIdForGeneratedValdiId(value: String): Int {
        val splitIndex = value.indexOf('/')
        if (splitIndex < 0) {
            // Value is not using the Ids API
            return View.NO_ID
        }

        return synchronized(accessibilityIdByName) {
            var resourceId = accessibilityIdByName[value]
            if (resourceId == null) {
                resourceId = context.resources.getIdentifier(value.replace("/", "__"), "id", context.packageName)
                if (resourceId == 0) {
                    resourceId = View.NO_ID
                }

                accessibilityIdByName[value] = resourceId
            }

            resourceId
        }
    }

    private fun applyAccessibilityId(view: View, value: String, animator: ValdiAnimator?) {
        view.id = getResourceIdForGeneratedValdiId(value)
    }

    private fun resetAccessibilityId(view: View, animator: ValdiAnimator?) {
        view.id = View.NO_ID
    }

    fun applySlowClipping(view: View, slowClipping: Boolean, animator: ValdiAnimator?) {
        if (view !is ValdiClippableView) {
            logger.error("slowClipping can only be set on a clippable view, ${view.javaClass.name} isn't")
            return
        }

        val clipsToBounds = if (!this.disableSlowClipping) slowClipping else view.clipToBoundsDefaultValue

        if (view.clipToBounds != clipsToBounds) {
            view.clipToBounds = clipsToBounds
            view.invalidate()
        }
    }

    fun resetSlowClipping(view: View, animator: ValdiAnimator?) {
        if (view !is ValdiClippableView) {
            return
        }

        if (view.clipToBounds != view.clipToBoundsDefaultValue) {
            view.clipToBounds = view.clipToBoundsDefaultValue
            view.invalidate()
        }
    }

    fun applyFilterTouchesWhenObscured(view: View, value: Boolean, animator: ValdiAnimator?) {
        view.filterTouchesWhenObscured = value
    }

    fun resetFilterTouchesWhenObscured(view: View, animator: ValdiAnimator?) {
        view.filterTouchesWhenObscured = false
    }

    fun applyTouchAreaExtension(view: View, value: Any?, animator: ValdiAnimator?) {
        val components = value as? Array<*> ?: return

        var leftInset = 0f
        var topInset = 0f
        var rightInset = 0f
        var bottomInset = 0f

        val providedInset = components[0] as? Double
        val providedLeftInset = components[1] as? Double
        val providedTopInset = components[2] as? Double
        val providedRightInset = components[3] as? Double
        val providedBottomInset = components[4] as? Double

        if (providedInset != null) {
            val inset = coordinateResolver.toPixelF(providedInset)
            leftInset = inset
            topInset = inset
            rightInset = inset
            bottomInset = inset
        }

        if (providedLeftInset != null) {
            leftInset = coordinateResolver.toPixelF(providedLeftInset)
        }
        if (providedTopInset != null) {
            topInset = coordinateResolver.toPixelF(providedTopInset)
        }
        if (providedRightInset != null) {
            rightInset = coordinateResolver.toPixelF(providedRightInset)
        }
        if (providedBottomInset != null) {
            bottomInset = coordinateResolver.toPixelF(providedBottomInset)
        }

        ViewUtils.setTouchAreaInsets(view, leftInset, topInset, rightInset, bottomInset)
    }

    fun resetTouchAreaExtension(view: View, animator: ValdiAnimator?) {
        ViewUtils.removeTouchAreaInsets(view)
    }

    private fun updateMaskPathData(view: View, value: ByteArray?, animator: ValdiAnimator?) {
        val maskPathRenderer = ViewUtils.getMaskPathRenderer(view)
        maskPathRenderer.setPathData(value)

        view.invalidate()
    }

    fun applyMaskPath(view: View, value: Any?, animator: ValdiAnimator?) {
        updateMaskPathData(view, value as? ByteArray, animator)
    }

    fun resetMaskPath(view: View, animator: ValdiAnimator?) {
        updateMaskPathData(view, null, animator)
    }

    private fun updateMaskOpacity(view: View, opacity: Float, animator: ValdiAnimator?) {
        val maskPathRenderer = ViewUtils.getMaskPathRenderer(view)
        maskPathRenderer.setOpacity(opacity)

        view.invalidate()
    }

    fun applyMaskOpacity(view: View, value: Float, animator: ValdiAnimator?) {
        updateMaskOpacity(view, value, animator)
    }

    fun resetMaskOpacity(view: View, animator: ValdiAnimator?) {
        updateMaskOpacity(view, 1.0f, animator)
    }

    fun applyOnTouchDelayDuration(view: View, value: Float, animator: ValdiAnimator?) {
        // no-op for now
    }

    fun resetOnTouchDelayDuration(view: View, animator: ValdiAnimator?) {
        // no-op for now
    }

    // blurStyle is an iOS-only attribute on `<blur>` / `BlurView` (mapped to
    // SCValdiBlurView on iOS). Android does not have a dedicated BlurView class —
    // `<blur>` falls through to ValdiView (see the FIXME on BlurView in
    // valdi_tsx/src/NativeTemplateElements.d.ts). Register the attribute here as a
    // no-op so the C++ attribute applier finds a handler and does not emit
    // `Could not find attribute 'blurStyle' in class com.snap.valdi.views.ValdiView`
    // for every mounted <blur> on Android.
    fun applyBlurStyle(view: View, value: String, animator: ValdiAnimator?) {
        // no-op on Android — no native BlurView implementation yet.
    }

    fun resetBlurStyle(view: View, animator: ValdiAnimator?) {
        // no-op on Android — no native BlurView implementation yet.
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<View>) {
        attributesBindingContext.bindArrayAttribute("background", false, this::applyBackground, this::resetBackground)
        attributesBindingContext.bindColorAttribute("backgroundColor", false, this::applyBackgroundColor, this::resetBackground)
        attributesBindingContext.bindFloatAttribute("opacity", false, this::applyAlpha, this::resetAlpha)
        attributesBindingContext.bindCornersAttribute("borderRadius", false, this::setBorderRadius, this::resetBorderRadius)
        attributesBindingContext.bindArrayAttribute("border", false, this::applyBorder, this::resetBorder)
        attributesBindingContext.bindBooleanAttribute("touchEnabled", false, this::applyTouchEnabled, this::resetTouchEnabled)
        attributesBindingContext.bindUntypedAttribute("boxShadow", false, this::applyBoxShadow, this::resetBoxShadow)
        attributesBindingContext.bindStringAttribute("accessibilityId", false, this::applyAccessibilityId, this::resetAccessibilityId)
        attributesBindingContext.bindBooleanAttribute("slowClipping", false, this::applySlowClipping, this::resetSlowClipping)
        attributesBindingContext.bindBooleanAttribute("filterTouchesWhenObscured", false, this::applyFilterTouchesWhenObscured, this::resetFilterTouchesWhenObscured)

        attributesBindingContext.bindCompositeAttribute("borderComposite", arrayListOf(
                CompositeAttributePart("borderWidth", AttributeType.DOUBLE, true, false),
                CompositeAttributePart("borderColor", AttributeType.COLOR, true, false)
        ), this::applyBorderComposite, this::resetBorder)

        attributesBindingContext.bindFloatAttribute("translationX", false, this::applyTranslationX, this::resetTranslationX)
        attributesBindingContext.bindFloatAttribute("translationY", false, this::applyTranslationY, this::resetTranslationY)
        attributesBindingContext.bindFloatAttribute("scaleX", false, this::applyScaleX, this::resetScaleX)
        attributesBindingContext.bindFloatAttribute("scaleY", false, this::applyScaleY, this::resetScaleY)
        attributesBindingContext.bindFloatAttribute("rotation", false, this::applyRotation, this::resetRotation)

        attributesBindingContext.bindUntypedAttribute("maskPath", false, this::applyMaskPath, this::resetMaskPath)
        attributesBindingContext.bindFloatAttribute("maskOpacity", false, this::applyMaskOpacity, this::resetMaskOpacity)

        attributesBindingContext.bindCompositeAttribute("touchAreaExtensionComposite", arrayListOf(
                CompositeAttributePart("touchAreaExtension", AttributeType.DOUBLE, true, false),
                CompositeAttributePart("touchAreaExtensionLeft", AttributeType.DOUBLE, true, false),
                CompositeAttributePart("touchAreaExtensionTop", AttributeType.DOUBLE, true, false),
                CompositeAttributePart("touchAreaExtensionRight", AttributeType.DOUBLE, true, false),
                CompositeAttributePart("touchAreaExtensionBottom", AttributeType.DOUBLE, true, false)
        ), this::applyTouchAreaExtension, this::resetTouchAreaExtension)

        attributesBindingContext.bindFunctionAndPredicateAttribute("onTap", gestureAttributes::applyOnTap, gestureAttributes::resetOnTap)
        attributesBindingContext.bindFunctionAndPredicateAttribute("onDoubleTap", gestureAttributes::applyOnDoubleTap, gestureAttributes::resetOnDoubleTap)
        attributesBindingContext.bindFunctionAndPredicateAttribute("onLongPress",
                CompositeAttributePart("longPressDuration", AttributeType.DOUBLE, true, false),
                gestureAttributes::applyOnLongPress,
                gestureAttributes::resetOnLongPress)
        attributesBindingContext.bindFunctionAndPredicateAttribute("onDrag", gestureAttributes::applyOnDrag, gestureAttributes::resetOnDrag)
        attributesBindingContext.bindFunctionAndPredicateAttribute("onPinch", gestureAttributes::applyOnPinch, gestureAttributes::resetOnPinch)
        attributesBindingContext.bindFunctionAndPredicateAttribute("onRotate", gestureAttributes::applyOnRotate, gestureAttributes::resetOnRotate)

        attributesBindingContext.bindFunctionAttribute("onTouchStart", gestureAttributes::applyOnTouchStart, gestureAttributes::resetOnTouchStart)
        attributesBindingContext.bindFunctionAttribute("onTouch", gestureAttributes::applyOnTouch, gestureAttributes::resetOnTouch)
        attributesBindingContext.bindFunctionAttribute("onTouchEnd", gestureAttributes::applyOnTouchEnd, gestureAttributes::resetOnTouchEnd)
        attributesBindingContext.bindFloatAttribute("onTouchDelayDuration", false, this::applyOnTouchDelayDuration, this::resetOnTouchDelayDuration)

        attributesBindingContext.bindFunctionAttribute("hitTest", gestureAttributes::applyHitTest, gestureAttributes::resetHitTest)

        // iOS-only BlurView attribute; accepted as no-op on Android — see applyBlurStyle above.
        attributesBindingContext.bindStringAttribute("blurStyle", false, this::applyBlurStyle, this::resetBlurStyle)
    }
}
