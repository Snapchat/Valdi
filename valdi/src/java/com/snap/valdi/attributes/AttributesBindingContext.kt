package com.snap.valdi.attributes

import android.view.View
import com.snap.valdi.actions.ValdiAction
import com.snap.valdi.attributes.conversions.ColorConversions
import com.snap.valdi.attributes.impl.animations.ValdiAnimator
import com.snap.valdi.attributes.impl.richtext.AttributedTextCpp
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.callable.ValdiFunctionActionAdapter
import com.snap.valdi.exceptions.AttributeError
import com.snap.valdi.exceptions.ValdiException
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snapchat.client.valdi_core.AssetOutputType
import com.snapchat.client.valdi_core.AttributeType
import com.snapchat.client.valdi_core.CompositeAttributePart
import com.snapchat.client.valdi.utils.CppObjectWrapper

/**
 * AttributesBindingContext offers convenience methods to bind attributes.
 * A bound attribute has callbacks which will be called when the attribute is being set or reset.
 * It is used by the AttributesBinders.
 */
@Suppress("UNCHECKED_CAST")
class AttributesBindingContext<T : View>(val native: AttributesBindingContextNative, val logger: Logger) {

    fun getBoundAttributeId(attributeName: String): Int {
        return native.boundAttributeIds[attributeName] ?: throw ValdiException("Attribute ${attributeName} was not bound")
    }

    inline fun bindUntypedAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Any?) -> Unit,
            crossinline reset: (view: T) -> Unit
    ) {
        val delegate = object: ObjectAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Any?, animator: ValdiAnimator?) {
                apply(view as T, value)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T)
            }
        }

        native.bindUntypedAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindUntypedAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Any?, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: ObjectAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Any?, animator: ValdiAnimator?) {
                apply(view as T, value, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindUntypedAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindTextAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Any?, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: ObjectAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Any?, animator: ValdiAnimator?) {
                val attributedValue = if (value is CppObjectWrapper) {
                    AttributedTextCpp(value)
                } else {
                    value
                }
                apply(view as T, attributedValue, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindTextAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindStringAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: String, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: ObjectAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Any?, animator: ValdiAnimator?) {
                apply(view as T, value as String, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindStringAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindFloatAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Float, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: FloatAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Float, animator: ValdiAnimator?) {
                apply(view as T, value, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindFloatAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindBooleanAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Boolean, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: BooleanAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Boolean, animator: ValdiAnimator?) {
                apply(view as T, value, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindBooleanAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindIntAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Int, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: IntAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Int, animator: ValdiAnimator?) {
                apply(view as T, value, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindIntAtribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindColorAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, color: Int, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: LongAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Long, animator: ValdiAnimator?) {
                apply(view as T, ColorConversions.fromRGBA(value), animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindColorAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    fun getValdiFunction(view: View, value: Any?): ValdiFunction {
        if (value is ValdiFunction) {
            return value
        } else if (value is String) {
            val action = ViewUtils.findValdiContext(view)?.actions?.getAction(value)
                    ?: throw AttributeError("Unable to get action $value")
            return getValdiFunction(view, action)
        } else if (value is ValdiAction) {
            return ValdiFunctionActionAdapter(value)
        } else {
            throw AttributeError("Invalid type for action attribute")

        }
    }

    inline fun bindFunctionAttribute(attribute: String,
                                     crossinline apply: (view: T, action: ValdiFunction) -> Unit,
                                     crossinline reset: (view: T) -> Unit
    ) {
        bindUntypedAttribute(attribute, false, { view, value ->
            ViewUtils.attachNativeHandleIfNeeded(view, attribute, value)

            val function = getValdiFunction(view, value)
            apply(view, function)
        }, { view ->
            ViewUtils.attachNativeHandleIfNeeded(view, attribute, null)
            reset(view)
        })
    }

    inline fun bindActionAttribute(
            attribute: String,
            crossinline apply: (view: T, action: ValdiAction) -> Unit,
            crossinline reset: (view: T) -> Unit
    ) {
        bindUntypedAttribute(attribute, false, { view, value ->
            ViewUtils.attachNativeHandleIfNeeded(view, attribute, value)

            if (value is ValdiAction) {
                apply(view, value)
            } else if (value is String) {
                val action = ViewUtils.findValdiContext(view)?.actions?.getAction(value)
                        ?: throw AttributeError("Unable to get action $value")
                apply(view, action)
            } else {
                throw AttributeError("Invalid type for action attribute")
            }
        }, { view ->
            ViewUtils.attachNativeHandleIfNeeded(view, attribute, null)
            reset(view)
        })
    }

    inline fun bindFunctionAndPredicateAttribute(
            attribute: String,
            additionalPart: CompositeAttributePart?,
            crossinline apply: (view: T, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) -> Unit,
            crossinline reset: (view: T) -> Unit
    ) {
        val compositeName = "${attribute}Composite"
        val predicateName = "${attribute}Predicate"
        val disabledName = "${attribute}Disabled"

        val parts = arrayListOf(
                CompositeAttributePart(attribute, AttributeType.UNTYPED, false, false),
                CompositeAttributePart(predicateName, AttributeType.UNTYPED, true, false),
                CompositeAttributePart(disabledName, AttributeType.BOOLEAN, true, false),
        )
        if (additionalPart != null) {
            parts.add(additionalPart)
        }

        bindCompositeAttribute(compositeName, parts, { view, value, _ ->
            value as? Array<*> ?: throw AttributeError("Expected array")

            if (value.size < 3) {
                throw AttributeError("Expected at least 3 array entries in bindFunctionAndPredicate")
            }

            val untypedFunction = value[0]
            val predicateFunction = value[1]
            val disabled = value[2] as? Boolean ?: false
            val additionalValue = if (value.size > 3) value[3] else null

            if (!disabled) {
                val function = getValdiFunction(view, untypedFunction)
                val predicate = if (predicateFunction != null) getValdiFunction(view, predicateFunction) else null

                apply(view, function, predicate, additionalValue)
            } else {
                reset(view)
            }
        }, { view, _ ->
            reset(view)
        })
    }

    inline fun bindFunctionAndPredicateAttribute(
            attribute: String,
            crossinline apply: (view: T, action: ValdiFunction, predicate: ValdiFunction?, additionalValue: Any?) -> Unit,
            crossinline reset: (view: T) -> Unit
    ) {
        bindFunctionAndPredicateAttribute(attribute, null, apply, reset)
    }

    inline fun bindPercentAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline applyPercent: (view: T, percent: Float, animator: ValdiAnimator?) -> Unit,
            crossinline applyRaw: (view: T, value: Float, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: PercentAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Float, valueIsPercent: Boolean, animator: ValdiAnimator?) {
                if (valueIsPercent) {
                    applyPercent(view as T, value, animator)
                } else {
                    applyRaw(view as T, value, animator)
                }
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindPercentAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindCornersAttribute(attribute: String, invalidateLayoutOnChange: Boolean,
                                    crossinline apply: (view: T,
                                                       topLeft: Float,
                                                       topLeftIsPercent: Boolean,
                                                       topRight: Float,
                                                       topRightIsPercent: Boolean,
                                                       bottomRight: Float,
                                                       bottomRightIsPercent: Boolean,
                                                       bottomLeft: Float,
                                                       bottomLeftIsPercent: Boolean,
                                                       animator: ValdiAnimator?) -> Unit,
                                    crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit) {
        val delegate = object: CornersAttributeHandlerDelegate() {
            override fun onApply(view: View, topLeft: Float, topLeftIsPercent: Boolean, topRight: Float, topRightIsPercent: Boolean, bottomRight: Float, bottomRightIsPercent: Boolean, bottomLeft: Float, bottomLeftIsPercent: Boolean, animator: ValdiAnimator?) {
                apply(view as T, topLeft, topLeftIsPercent, topRight, topRightIsPercent, bottomRight, bottomRightIsPercent, bottomLeft, bottomLeftIsPercent, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindCornersAttribute(attribute, invalidateLayoutOnChange, delegate)
    }

    inline fun bindArrayAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline apply: (view: T, value: Array<Any>, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        bindUntypedAttribute(attribute, invalidateLayoutOnChange, { view, value, animator ->
            if (value !is Array<*>) {
                throw AttributeError("Not an array")
            }

            apply(view, value as Array<Any>, animator)
        }, reset)
    }

    inline fun bindCompositeAttribute(
            attribute: String,
            parts: ArrayList<CompositeAttributePart>,
            crossinline apply: (view: T, value: Any?, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        val delegate = object: ObjectAttributeHandlerDelegate() {
            override fun onApply(view: View, value: Any?, animator: ValdiAnimator?) {
                apply(view as T, value, animator)
            }

            override fun onReset(view: View, animator: ValdiAnimator?) {
                reset(view as T, animator)
            }
        }

        native.bindCompositeAttribute(attribute, parts, delegate)
    }

    inline fun <AttributeType>bindDeserializableAttribute(
            attribute: String,
            invalidateLayoutOnChange: Boolean,
            crossinline deserializer: (value: Any?) -> AttributeType,
            crossinline apply: (view: T, value: AttributeType, animator: ValdiAnimator?) -> Unit,
            crossinline reset: (view: T, animator: ValdiAnimator?) -> Unit
    ) {
        bindUntypedAttribute(attribute, invalidateLayoutOnChange, { view, value, animator ->
            apply(view, value as AttributeType, animator)
        }, reset)

        registerPreprocessor(attribute) {
            deserializer(it)
        }
    }

    inline fun registerPreprocessor(attribute: String, crossinline preprocessor: (value: Any?) -> Any?) {
        registerPreprocessor(attribute, false, preprocessor)
    }

    inline fun registerPreprocessor(attribute: String, enableCache: Boolean, crossinline preprocessor: (value: Any?) -> Any?) {
        val delegate = object: AttributePreprocessor() {
            override fun preprocess(input: Any?): Any? {
                return preprocessor(input)
            }
        }

        native.registerPreprocessor(attribute, enableCache, delegate)
    }

    fun bindScrollAttributes() {
        native.bindScrollAttributes()
    }

    fun bindAssetAttributes(outputType: AssetOutputType) {
        native.bindAssetAttributes(outputType)
    }

    /**
     * Registers a MeasureDelegate that will be used to measure elements.
     * Without a MeasureDelegate, the element will always have an intrinsic size of 0, 0.
     * Its size would need to be configured through flexbox attributes. View implementations
     * that have an intrinsic size based on their content, like a label for instance, should
     * register a MeasureDelegate. The MeasureDelegate can be called in arbitrary threads.
     */
    fun setMeasureDelegate(measureDelegate: MeasureDelegate) {
        native.setMeasureDelegate(measureDelegate)
    }

    /**
     * Registers a MeasureDelegate that uses a placeholder view instance to measure elements.
     * The given lazy provider should return a view that will be configured with attributes,
     * measure() will then be called on it to resolve the element size, and the view will then
     * be restored to its original view. The returned view might be re-used many times to measure
     * elements of this view class. setMeasureDelegate() should be preferred over
     * setPlaceholderViewMeasureDelegate() since it can typically be implemented in a more
     * efficient way. Measuring elements configured with a placeholderViewMeasureDelegate will
     * always be measured in the main thread.
     */
    fun setPlaceholderViewMeasureDelegate(provider: Lazy<T>) {
        native.setPlaceholderViewMeasureDelegate(provider)
    }

    companion object {
        const val ATTRIBUTE_TYPE_UNTYPED = 1
        const val ATTRIBUTE_TYPE_INT = 1
        const val ATTRIBUTE_TYPE_BOOLEAN = 1
        const val ATTRIBUTE_TYPE_DOUBLE = 1
        const val ATTRIBUTE_TYPE_STRING = 1
        const val ATTRIBUTE_TYPE_COMPOSITE = 1
        const val ATTRIBUTE_TYPE_COLOR = 1
        const val ATTRIBUTE_TYPE_BORDER = 1
        const val ATTRIBUTE_TYPE_PERCENT = 1
    }

}
