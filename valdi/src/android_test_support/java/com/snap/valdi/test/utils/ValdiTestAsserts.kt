package com.snap.valdi.test.utils

import junit.framework.AssertionFailedError
import org.hamcrest.Matcher
import org.hamcrest.StringDescription

fun <T> assertThat(description: () -> String, value: T, matcher: Matcher<T>) {
    if (!matcher.matches(value)) {
        val errorMessage = StringDescription()

        errorMessage
            .appendText(description())
            .appendText("\nExpected: ")
            .appendDescriptionOf(matcher)
            .appendText("\n     Got: ")

        matcher.describeMismatch(value, errorMessage)
        errorMessage.appendText("\n")

        throw AssertionFailedError(errorMessage.toString())
    }
}
