package com.snap.valdi.test.utils

import android.view.View
import com.snap.valdi.test.ValdiElementWithRootView
import java.lang.ref.WeakReference

class CapturedValdiElementWithRootView {

    // Use WeakReference in values to allow ValdiElementWithRootView (which holds rootView) to be GC'd
    // This prevents memory leaks when test page objects hold onto matchers that reference this cache
    private val matchedElementsByView = hashMapOf<Int, WeakReference<ValdiElementWithRootView>>()

    fun get(startingView: View): ValdiElementWithRootView? {
        return synchronized(matchedElementsByView) {
            matchedElementsByView[System.identityHashCode(startingView)]?.get()
        }
    }

    override fun toString(): String {
        val desc = synchronized(matchedElementsByView) {
            matchedElementsByView.mapNotNull { entry ->
                val element = entry.value.get()
                if (element != null) {
                    "View hash ${entry.key} with element $element"
                } else null
            }.joinToString(", ")
        }

        return "Matched elements: [$desc]"
    }

    fun set(matchedView: View, element: ValdiElementWithRootView?) {
        val key = System.identityHashCode(matchedView)
        synchronized(matchedElementsByView) {
            if (element == null) {
                matchedElementsByView.remove(key)
            } else {
                matchedElementsByView[key] = WeakReference(element)
            }
        }
    }
}
