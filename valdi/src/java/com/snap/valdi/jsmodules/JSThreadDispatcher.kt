package com.snap.valdi.jsmodules

import java.lang.Runnable

interface JSThreadDispatcher {

    fun runOnJsThread(runnable: Runnable)

    /**
     * @param attribution stable, non-empty identifier for the scheduling callsite. Keep it
     * low-cardinality and do not include user or content identifiers.
     */
    fun runOnJsThread(attribution: String, runnable: Runnable)

}
