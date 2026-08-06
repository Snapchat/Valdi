package com.snap.valdi.test.assertions

import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAssertion
import com.snap.valdi.test.utils.assertThat
import org.hamcrest.Matchers

abstract class NonNullableElementAssertion : ElementAssertion {
    protected abstract fun doCheck(element: ValdiElementWithRootView)

    final override fun check(element: ValdiElementWithRootView?) {
        assertThat({ "Composer Element is present in the hierarchy" }, element != null, Matchers.`is`(element != null))
        doCheck(element!!)
    }
}
