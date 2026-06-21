package com.snap.valdi.nativebridge

import android.content.Context
import android.view.View
import com.snap.valdi.ViewFactoryPrivate
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.exceptions.ValdiFatalException
import com.snap.valdi.ViewRef
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.attributes.AttributesBindingContextNative
import com.snap.valdi.utils.ViewRefSupport
import com.snap.valdi.utils.error
import com.snap.valdi.views.ValdiChildFrameManagingView
import com.snap.valdi.views.ValdiView
import java.lang.reflect.Constructor

class ReflectionViewFactory(val context: Context, val viewRefSupport: ViewRefSupport, val cls: Class<View>, val attributesBinder: AttributesBinder<View>?): ViewFactoryPrivate {

    private var constructor: Constructor<View>? = null
    private var didResolveConstructor: Boolean = false

    private fun getConstructor(): Constructor<View>? {
        synchronized(this) {
            if (!didResolveConstructor) {
                didResolveConstructor = true

                try {
                    constructor = cls.getDeclaredConstructor(Context::class.java)
                } catch (exc: NoSuchMethodException) {
                    viewRefSupport.logger.error("Unable to resolve constructor for View class $cls, will fallback to ValdiView")
                }
            }

            return constructor
        }
    }

    override fun createView(valdiContext: Any?): ViewRef {
        var view: View? = null

        if (view == null) {
            try {
                view = getConstructor()?.newInstance(context)
            } catch (t: Throwable) {
                ValdiFatalException.handleFatal(t, "Global view factory failed to create view for class named '${cls.name}'")
            }
        }

        // Uncomment to track view leaks
        //        val runtime = valdiContext?.runtime
        //        if (runtime != null) {
        //            ValdiLeakTracker.trackRef(WeakReference(view), "View $cls", runtime)
        //        }

        return ViewRef(view ?: ValdiView(context), true, viewRefSupport)
    }

    override fun bindAttributes(bindingContextHandle: Long) {
        try {
            val native = AttributesBindingContextNative(cls, bindingContextHandle)
            attributesBinder?.bindAttributes(AttributesBindingContext(native, viewRefSupport.logger))
        } catch (throwable: Throwable) {
            ValdiFatalException.handleFatal(throwable, "View factory of class '$cls' failed to bind attributes")
        }
    }

    override fun managesChildFrames(): Boolean {
        return ValdiChildFrameManagingView::class.java.isAssignableFrom(cls)
    }

}
