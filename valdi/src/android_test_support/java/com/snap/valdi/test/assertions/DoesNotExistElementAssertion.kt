package com.snap.valdi.test.assertions

import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAssertion
import com.snap.valdi.test.utils.assertThat
import org.hamcrest.Matchers

class DoesNotExistElementAssertion : ElementAssertion {

    override fun check(element: ValdiElementWithRootView?) {
        assertThat({ "ViewNode is not present in the hierarchy" }, element != null, Matchers.`is`(false))
    }
}
