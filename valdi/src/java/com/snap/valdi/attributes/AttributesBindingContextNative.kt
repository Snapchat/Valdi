package com.snap.valdi.attributes

import android.view.View
import com.snapchat.client.valdi_core.AssetOutputType
import com.snapchat.client.valdi_core.CompositeAttributePart
import com.snapchat.client.valdi.NativeBridge

class AttributesBindingContextNative(viewClass: Class<*>,
                                     private val nativeHandle: Long) {

    val boundAttributeIds = hashMapOf<String, Int>()

    private val viewClassName = viewClass.simpleName

    private fun doBindAttribute(type: Int,
                                name: String,
                                invalidateLayoutOnChange: Boolean,
                                delegate: AttributeHandlerDelegate,
                                compositeParts: Any?) {
        val attributeId = NativeBridge.bindAttribute(nativeHandle, type, name, invalidateLayoutOnChange, delegate, compositeParts)
        boundAttributeIds[name] = attributeId

        delegate.attributeId = attributeId
        delegate.viewClassName = viewClassName
        delegate.attributeName = name
    }

    fun bindUntypedAttribute(name: String,
                             invalidateLayoutOnChange: Boolean,
                             delegate: ObjectAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_UNTYPED, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindStringAttribute(name: String,
                            invalidateLayoutOnChange: Boolean,
                            delegate: ObjectAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_STRING, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindFloatAttribute(name: String,
                           invalidateLayoutOnChange: Boolean,
                           delegate: FloatAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_DOUBLE, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindBooleanAttribute(name: String,
                             invalidateLayoutOnChange: Boolean,
                             delegate: BooleanAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_BOOLEAN, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindIntAtribute(name: String,
                        invalidateLayoutOnChange: Boolean,
                        delegate: IntAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_INT, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindColorAttribute(name: String,
                           invalidateLayoutOnChange: Boolean,
                           delegate: LongAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_COLOR, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindPercentAttribute(name: String,
                             invalidateLayoutOnChange: Boolean,
                             delegate: PercentAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_PERCENT, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindCornersAttribute(name: String,
                             invalidateLayoutOnChange: Boolean,
                             delegate: CornersAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_BORDER, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindTextAttribute(name: String,
                          invalidateLayoutOnChange: Boolean,
                          delegate: ObjectAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_TEXT, name, invalidateLayoutOnChange, delegate, null)
    }

    fun bindCompositeAttribute(name: String,
                               parts: List<CompositeAttributePart>,
                               delegate: ObjectAttributeHandlerDelegate) {
        doBindAttribute(ATTRIBUTE_TYPE_COMPOSITE, name, false, delegate, parts.toTypedArray())
    }

    fun bindTransformAttributes(delegate: ObjectAttributeHandlerDelegate) {
        NativeBridge.bindTransformAttributes(nativeHandle, delegate)
    }

    fun registerPreprocessor(attributeName: String, enableCache: Boolean, preprocessor: AttributePreprocessor) {
        NativeBridge.registerAttributePreprocessor(nativeHandle, attributeName, enableCache, preprocessor)
    }

    fun bindScrollAttributes() {
        NativeBridge.bindScrollAttributes(nativeHandle)
    }

    fun bindAssetAttributes(outputType: AssetOutputType) {
        NativeBridge.bindAssetAttributes(nativeHandle, outputType.ordinal)
    }

    fun setMeasureDelegate(measureDelegate: MeasureDelegate) {
        NativeBridge.setMeasureDelegate(nativeHandle, measureDelegate)
    }

    fun setPlaceholderViewMeasureDelegate(placeholderViewProvider: Lazy<View?>) {
        NativeBridge.setPlaceholderViewMeasureDelegate(nativeHandle, placeholderViewProvider)
    }

    companion object {
        private const val ATTRIBUTE_TYPE_UNTYPED = 1
        private const val ATTRIBUTE_TYPE_INT = 2
        private const val ATTRIBUTE_TYPE_BOOLEAN = 3
        private const val ATTRIBUTE_TYPE_DOUBLE = 4
        private const val ATTRIBUTE_TYPE_STRING = 5
        private const val ATTRIBUTE_TYPE_COLOR = 6
        private const val ATTRIBUTE_TYPE_BORDER = 7
        private const val ATTRIBUTE_TYPE_PERCENT = 8
        private const val ATTRIBUTE_TYPE_TEXT = 9
        private const val ATTRIBUTE_TYPE_COMPOSITE = 10
    }

}
