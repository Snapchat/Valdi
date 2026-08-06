package com.snap.valdi.test.actions

import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import com.snap.valdi.test.ValdiElementWithRootView
import com.snap.valdi.test.ElementAction
import com.snap.valdi.test.coordinates.Coordinates

class ElementActionAdapter(
    val actionName: String,
    val viewActionFactory: (coordinates: Coordinates) -> ViewAction
) : ElementAction {

    override fun getDescription(): String {
        return actionName
    }

    override fun perform(controller: UiController, item: ValdiElementWithRootView) {
        val absoluteFrame = item.getVisualFrameFromRootView()

        val coordinates = Coordinates(item.rootView, absoluteFrame, absoluteFrame.left, absoluteFrame.top)
        val viewAction = viewActionFactory(coordinates)

        viewAction.perform(controller, item.rootView)
    }
}
