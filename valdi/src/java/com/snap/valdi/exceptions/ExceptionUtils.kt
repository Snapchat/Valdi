package com.snap.valdi.exceptions

import java.lang.StringBuilder
import java.util.concurrent.CancellationException

/**
 * Mirrors the native `Valdi::kPromiseCanceledErrorCode`: the code stamped on the failure a canceled
 * promise delivers to its callbacks.
 */
const val PROMISE_CANCELED_ERROR_CODE = 101

/**
 * The native `Valdi::Error` code this throwable should cross the JNI boundary with, or 0 when it has
 * none. Keeps a cancellation distinguishable from a genuine failure on the other side, rather than
 * forcing consumers to match the message.
 *
 * Any [CancellationException] maps to [PROMISE_CANCELED_ERROR_CODE]: that covers
 * `PromiseCanceledException` and a coroutine cancellation escaping into a promise alike, both of
 * which are cancellations as far as native is concerned.
 */
fun Throwable.valdiErrorCode(): Int =
    when (this) {
        is ValdiException -> errorCode
        is CancellationException -> PROMISE_CANCELED_ERROR_CODE
        else -> 0
    }

private fun buildMessageWithCauses(throwable: Throwable, builder: StringBuilder) {
    val message = throwable.message
    builder.append(throwable::class.java.simpleName)
    builder.append(": '")
    builder.append(message ?: "Unknown Error")
    builder.append("'")

    val cause = throwable.cause
    if (cause != null) {
        builder.append(", Caused by: ")
        buildMessageWithCauses(cause, builder)
    }
}

fun Throwable.messageWithCauses(): String {
    val sb = StringBuilder()

    buildMessageWithCauses(this, sb)

    return sb.toString()
}