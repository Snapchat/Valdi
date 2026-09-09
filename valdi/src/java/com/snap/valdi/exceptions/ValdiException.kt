package com.snap.valdi.exceptions

import androidx.annotation.Keep

/**
 * Base class for Valdi exceptions.
 * Valdi exceptions should be thrown only for recoverable errors.
 */
@Keep
open class ValdiException(message: String, cause: Throwable?, val errorCode: Int = 0):
    RuntimeException(message, cause) {

    @Keep
    constructor(message: String): this(message, null)

    /**
     * Carries the code from the native `Valdi::Error` this was converted from, so consumers can
     * branch on it instead of matching the message. 0 when the error had no code set.
     */
    @Keep
    constructor(message: String, errorCode: Int): this(message, null, errorCode)

}
