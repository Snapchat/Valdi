package com.snap.valdi.attributes

import com.snap.valdi.attributes.impl.richtext.AttributedTextAnimationTimeline
import org.junit.Assert.assertEquals
import org.junit.Test

class AttributedTextAnimationTimelineTest {
    @Test
    fun reusesStartTimeForNewAnimationsInSameTimeline() {
        val timeline = AttributedTextAnimationTimeline()

        val firstStartTime = timeline.startTimeForNewAnimation("intro", 10_000L, 80L)
        val secondStartTime = timeline.startTimeForNewAnimation("intro", 10_400L, 80L)

        assertEquals(10_000L, firstStartTime)
        assertEquals(10_000L, secondStartTime)
    }

    @Test
    fun offsetsAfterExistingAnimations() {
        val timeline = AttributedTextAnimationTimeline()
        timeline.recordExistingAnimationScheduledStartTime("intro", 12_000L)

        val startTime = timeline.startTimeForNewAnimation("intro", 10_000L, 80L)

        assertEquals(12_080L, startTime)
    }

    @Test
    fun resetClearsTimelineState() {
        val timeline = AttributedTextAnimationTimeline()
        timeline.recordExistingAnimationScheduledStartTime("intro", 12_000L)
        timeline.resetFrameState()

        val startTime = timeline.startTimeForNewAnimation("intro", 10_000L, 80L)

        assertEquals(10_000L, startTime)
    }
}
