package com.snap.valdi.views

import android.content.Context
import android.view.View
import android.view.ViewGroup
import com.snap.valdi.attributes.impl.richtext.AttributedTextAnimationTimeline
import com.snap.valdi.attributes.impl.richtext.TextViewHelper
import java.lang.ref.WeakReference
import java.util.Collections
import java.util.WeakHashMap

class ValdiTextAnimationGroup(context: Context) : ValdiView(context) {
    private val participants = Collections.newSetFromMap(WeakHashMap<TextViewHelper, Boolean>())
    private val participantsByView = WeakHashMap<View, WeakReference<TextViewHelper>>()
    private var orderedParticipants: List<TextViewHelper> = emptyList()
    private var participantBaseIndexesDirty = false
    private var animationFrameCallbackPosted = false
    private var animationFrameCallback: Runnable? = null

    internal val textAnimationTimeline = AttributedTextAnimationTimeline()

    fun registerParticipant(participant: TextViewHelper) {
        participants.add(participant)
        participantsByView[participant.textAnimationView] = WeakReference(participant)
        participant.applyTextAnimationTimeline(textAnimationTimeline, 0)
        markParticipantBaseIndexesDirty()
    }

    fun unregisterParticipant(participant: TextViewHelper) {
        participants.remove(participant)
        participantsByView.remove(participant.textAnimationView)
        participant.clearTextAnimationGroupRegistration()
        markParticipantBaseIndexesDirty()
    }

    fun markParticipantBaseIndexesDirty() {
        participantBaseIndexesDirty = true
        requestLayout()
    }

    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {
        super.onLayout(changed, l, t, r, b)
        ensureParticipantBaseIndexes()
    }

    private fun ensureParticipantBaseIndexes() {
        if (participantBaseIndexesDirty) {
            rebuildOrderedParticipantsAndApplyBaseIndexes()
        }
    }

    private fun rebuildOrderedParticipantsAndApplyBaseIndexes() {
        orderedParticipants = collectOrderedParticipants()
        participantBaseIndexesDirty = false

        var basePartIndex = 0
        for (participant in orderedParticipants) {
            participant.applyTextAnimationTimeline(textAnimationTimeline, basePartIndex)
            basePartIndex += participant.textAnimationPartCount
        }
    }

    fun startTextAnimationFrameLoopIfNeeded() {
        if (animationFrameCallbackPosted) {
            return
        }

        animationFrameCallbackPosted = true
        postOnAnimation(animationFrameCallback())
    }

    override fun prepareForRecycling() {
        cancelAnimationFrameLoop()
        participants.forEach { it.clearTextAnimationGroupRegistration() }
        participants.clear()
        participantsByView.clear()
        orderedParticipants = emptyList()
        participantBaseIndexesDirty = false
        textAnimationTimeline.resetFrameState()
    }

    private fun animationFrameCallback(): Runnable {
        val existingFrameCallback = animationFrameCallback
        if (existingFrameCallback != null) {
            return existingFrameCallback
        }

        return Runnable {
            animationFrameCallbackPosted = false
            ensureParticipantBaseIndexes()
            val orderedParticipants = orderedParticipants
            textAnimationTimeline.resetFrameState()
            orderedParticipants.forEach { it.prepareGroupedTextAnimationFrame() }
            val hasActiveAnimations = orderedParticipants.fold(false) { active, participant ->
                participant.updateGroupedTextAnimationFrame() || active
            }
            if (hasActiveAnimations) {
                startTextAnimationFrameLoopIfNeeded()
            }
        }.also {
            animationFrameCallback = it
        }
    }

    private fun cancelAnimationFrameLoop() {
        if (animationFrameCallbackPosted) {
            animationFrameCallback?.let { removeCallbacks(it) }
            animationFrameCallbackPosted = false
        }
    }

    private fun collectOrderedParticipants(): List<TextViewHelper> {
        val orderedParticipants = arrayListOf<TextViewHelper>()
        for (index in 0 until childCount) {
            collectParticipants(getChildAt(index), orderedParticipants)
        }
        return orderedParticipants
    }

    private fun collectParticipants(view: View, output: MutableList<TextViewHelper>) {
        if (view is ValdiTextAnimationGroup) {
            return
        }

        val helper = participantsByView[view]?.get()
        if (helper != null && participants.contains(helper)) {
            output.add(helper)
        }

        if (view is ViewGroup) {
            for (index in 0 until view.childCount) {
                collectParticipants(view.getChildAt(index), output)
            }
        }
    }
}
