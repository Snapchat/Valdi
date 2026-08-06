package com.snap.valdi.test

import androidx.test.espresso.UiController

interface ElementAction {

    fun getDescription(): String

    fun perform(controller: UiController, item: ValdiElementWithRootView)
}
