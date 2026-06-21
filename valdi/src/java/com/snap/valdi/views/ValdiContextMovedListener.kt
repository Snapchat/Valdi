package com.snap.valdi.views

import com.snap.valdi.context.ValdiContext

/**
 * Implemented by views that need to react when their ValdiContext and ViewNode are assigned.
 */
interface ValdiContextMovedListener {
    fun onMovedToValdiContext(valdiContext: ValdiContext)
}
