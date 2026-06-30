package com.snap.valdi

import android.content.Context
import android.view.View
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.exceptions.ValdiFatalException
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.AttributesBindingContextNative
import com.snap.valdi.utils.ViewRefSupport
import com.snap.valdi.views.ValdiChildFrameManagingView

class DeferredViewFactory<T: View>(val viewClass: Class<T>,
                                   val factory: (context: Context) -> T,
                                   val attributesBinder: AttributesBinder<T>?,
                                   val viewRefSupport: ViewRefSupport,
                                   val context: Context): ViewFactoryPrivate {

    override fun createView(valdiContext: Any?): ViewRef {
        return try {
            ViewRef(factory(context), true, viewRefSupport)
        } catch (throwable: Throwable) {
            ValdiFatalException.handleFatal(throwable, "View factory of class '$viewClass' failed to create view")
        }
    }

    override fun bindAttributes(bindingContextHandle: Long) {
        return try {
            val native = AttributesBindingContextNative(viewClass, bindingContextHandle)
            attributesBinder!!.bindAttributes(AttributesBindingContext(native, viewRefSupport.logger))
        } catch (throwable: Throwable) {
            ValdiFatalException.handleFatal(throwable, "View factory of class '$viewClass' failed to bind attributes")
        }
    }

    override fun managesChildFrames(): Boolean {
        return ValdiChildFrameManagingView::class.java.isAssignableFrom(viewClass)
    }

}
