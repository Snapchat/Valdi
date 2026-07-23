package com.snap.valdi.attributes.impl

import android.graphics.Paint
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.animations.ValdiValueAnimation
import com.snap.valdi.attributes.impl.animations.ViewAnimator
import com.snap.valdi.attributes.impl.gradients.ValdiGradient
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.utils.CoordinateResolver
import com.snap.valdi.views.ShapeView

class ShapeViewAttributesBinder : AttributesBinder<ShapeView> {

    companion object {
        private val STROKE_START_KEY = "STROKE_START"
        private val STROKE_END_KEY = "STROKE_END"
    }

    override val viewClass: Class<ShapeView> = ShapeView::class.java

    private fun applyPath(view: ShapeView, value: Any?, animator: ValdiAnimator?) {
        view.setPathData(value as? ByteArray)
    }

    private fun resetPath(view: ShapeView, animator: ValdiAnimator?) {
        view.setPathData(null)
    }

    private fun applyStrokeWidth(view: ShapeView, strokeWidth: Float, animator: ValdiAnimator?) {
        view.setStrokeWidth(strokeWidth)
    }

    private fun resetStrokeWidth(view: ShapeView, animator: ValdiAnimator?) {
        view.resetStrokeWidth()
    }

    private fun applyStrokeColor(view: ShapeView, strokeColor: Int, animator: ValdiAnimator?) {
        view.setStrokeColor(strokeColor)
    }

    private fun resetStrokeColor(view: ShapeView, animator: ValdiAnimator?) {
        view.resetStrokeColor()
    }

    private fun applyFillColor(view: ShapeView, fillColor: Int, animator: ValdiAnimator?) {
        view.setFillColor(fillColor)
    }

    private fun resetFillColor(view: ShapeView, animator: ValdiAnimator?) {
        view.resetFillColor()
    }

    private fun applyFillGradient(view: ShapeView, value: Array<Any>, animator: ValdiAnimator?) {
        val valdiGradient = ValdiGradient.fromGradientData(value)

        if (valdiGradient.colors.isEmpty()) {
            view.resetFillGradient()
        } else if (valdiGradient.colors.size == 1) {
            view.resetFillGradient()
            view.setFillColor(valdiGradient.colors[0])
        } else {
            view.setFillGradient(valdiGradient)
        }
    }

    private fun resetFillGradient(view: ShapeView, animator: ValdiAnimator?) {
        view.resetFillGradient()
    }

    private fun applyStrokeCap(view: ShapeView, value: String, animator: ValdiAnimator?) {
        val strokeCap: Paint.Cap = when (value) {
            "butt" -> Paint.Cap.BUTT
            "round" -> Paint.Cap.ROUND
            "square" ->   Paint.Cap.SQUARE
            else -> throw AttributeError("Invalid value")
        }
        view.setStrokeCap(strokeCap)
    }

    private fun resetStrokeCap(view: ShapeView, animator: ValdiAnimator?) {
        view.resetStrokeCap()
    }

    private fun applyStrokeJoin(view: ShapeView, value: String, animator: ValdiAnimator?) {
        val strokeJoin: Paint.Join = when (value) {
            "bevel" ->  Paint.Join.BEVEL
            "miter" ->  Paint.Join.MITER
            "round" -> Paint.Join.ROUND
            else ->  throw AttributeError("Invalid value")
        }
        view.setStrokeJoin(strokeJoin)
    }

    private fun resetStrokeJoin(view: ShapeView, animator: ValdiAnimator?) {
        view.resetStrokeJoin()
    }

    private fun applyStrokeStart(view: ShapeView, value: Float, animator: ValdiAnimator?) {
        ViewUtils.cancelAnimation(view, STROKE_START_KEY)
        if (animator == null) {
            view.strokeStart = value
        } else {
            val startStrokeStart = view.strokeStart
            animator.addValueAnimation(STROKE_START_KEY, view, ValdiValueAnimation(ValdiValueAnimation.MinimumVisibleChange.SCALE_RATIO) { progress ->
                val resultValue = ViewAnimator.interpolate(progress, startStrokeStart, value)
                view.strokeStart = resultValue
            })
        }
    }

    private fun resetStrokeStart(view: ShapeView, animator: ValdiAnimator?) {
        applyStrokeStart(view, 0.0f, animator)
    }

    private fun applyStrokeEnd(view: ShapeView, value: Float, animator: ValdiAnimator?) {
        ViewUtils.cancelAnimation(view, STROKE_END_KEY)
        if (animator == null) {
            view.strokeEnd = value
        } else {
            val startStrokeEnd = view.strokeEnd
            animator.addValueAnimation(STROKE_END_KEY, view, ValdiValueAnimation(ValdiValueAnimation.MinimumVisibleChange.SCALE_RATIO) { progress ->
                val resultValue = ViewAnimator.interpolate(progress, startStrokeEnd, value)
                view.strokeEnd = resultValue
            })
        }
    }

    private fun resetStrokeEnd(view: ShapeView, animator: ValdiAnimator?) {
        applyStrokeEnd(view, 1.0f, animator)
    }

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<ShapeView>) {
        attributesBindingContext.bindUntypedAttribute("path", false, this::applyPath, this::resetPath)
        attributesBindingContext.bindFloatAttribute("strokeWidth", false, this::applyStrokeWidth, this::resetStrokeWidth)
        attributesBindingContext.bindColorAttribute("strokeColor", false, this::applyStrokeColor, this::resetStrokeColor)
        attributesBindingContext.bindColorAttribute("fillColor", false, this::applyFillColor, this::resetFillColor)
        attributesBindingContext.bindArrayAttribute("fillGradient", false, this::applyFillGradient, this::resetFillGradient)
        attributesBindingContext.bindStringAttribute("strokeCap", false, this::applyStrokeCap, this::resetStrokeCap)
        attributesBindingContext.bindStringAttribute("strokeJoin", false, this::applyStrokeJoin, this::resetStrokeJoin)
        attributesBindingContext.bindFloatAttribute("strokeStart", false, this::applyStrokeStart, this::resetStrokeStart)
        attributesBindingContext.bindFloatAttribute("strokeEnd", false, this::applyStrokeEnd, this::resetStrokeEnd)
    }

}
