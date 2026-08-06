package com.snap.valdi.test.utils

import android.view.View
import android.view.ViewGroup
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.views.ValdiRootView
import com.snap.valdi.views.ValdiTextViewBase
import org.hamcrest.Matcher

object ValdiElementSearcher {

    @JvmStatic
    fun search(
        element: ValdiElementWithRootView,
        matcher: Matcher<ValdiElementWithRootView>
    ): ValdiElementWithRootView? {
        if (matcher.matches(element)) {
            return element
        }

        for (child in element.children()) {
            val matchedChild = search(child, matcher)
            if (matchedChild != null) {
                return matchedChild
            }
        }

        return null
    }

    @JvmStatic
    fun searchFromRootView(
        rootView: ValdiRootView,
        matcher: Matcher<ValdiElementWithRootView>
    ): ValdiElementWithRootView? {
        val rootViewNode = rootView.valdiContext?.getRootViewNode() ?: return null
        return search(ValdiElementWithRootView(rootView, rootViewNode), matcher)
    }

    /**
     * Search the backing view instance for the given ValdiElement.
     * This will only return a view if the node has a view instance.
     */
    @JvmStatic
    fun searchViewInstance(item: ValdiElementWithRootView): View? {
        return searchViewInstance(item.rootView, item.rootView, item.viewNode.id)
    }

    @JvmStatic
    private fun searchViewInstance(rootView: ValdiRootView, view: View, viewNodeId: Int): View? {
        val viewNode = ViewUtils.findViewNode(view)
        val composerContext = ViewUtils.findValdiContext(view)

        if (viewNode != null) {
            if (composerContext === rootView.valdiContext && viewNode.id == viewNodeId) {
                return (view as? ValdiTextViewBase)?.backingTextView ?: view
            }
        }

        if (view is ViewGroup) {
            for (index in 0 until view.childCount) {
                val textView = searchViewInstance(rootView, view.getChildAt(index), viewNodeId)
                if (textView != null) {
                    return textView
                }
            }
        }

        return null
    }
}
