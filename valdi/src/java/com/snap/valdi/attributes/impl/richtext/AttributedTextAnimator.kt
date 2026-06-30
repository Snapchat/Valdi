package com.snap.valdi.attributes.impl.richtext

import android.text.Spannable
import android.text.Spanned
import android.view.animation.AnimationUtils
import com.snap.valdi.nodes.IValdiViewNode
import com.snap.valdi.utils.InternedString
import java.util.ArrayDeque
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow

private class TextAnimationStoredProgress {
    val startTimes = HashMap<String, Long>()
}

class AttributedTextAnimationTimeline {
    private data class TimelineState(
        var existingAnimationStartMillis: Long? = null,
        var newAnimationStartMillis: Long? = null
    )

    private val timelineStates = hashMapOf<String, TimelineState>()

    fun resetFrameState() {
        timelineStates.clear()
    }

    fun recordExistingAnimationScheduledStartTime(timelineKey: String, scheduledStartTimeMillis: Long) {
        val timelineState = timelineStates.getOrPut(timelineKey) { TimelineState() }
        timelineState.existingAnimationStartMillis =
            max(timelineState.existingAnimationStartMillis ?: Long.MIN_VALUE, scheduledStartTimeMillis)
    }

    fun startTimeForNewAnimation(timelineKey: String, currentTimeMillis: Long, timeOffsetMillis: Long): Long {
        val timelineState = timelineStates.getOrPut(timelineKey) { TimelineState() }
        val existingStartMillis = timelineState.existingAnimationStartMillis
        val newStartMillis = timelineState.newAnimationStartMillis ?: (
            existingStartMillis?.let { max(currentTimeMillis, it + timeOffsetMillis) } ?: currentTimeMillis
        )
        timelineState.newAnimationStartMillis = newStartMillis
        return newStartMillis
    }
}

class AttributedTextAnimator {
    companion object {
        private val TEXT_ANIMATION_START_TIMES_STORAGE_KEY by lazy { InternedString.create("valdi.textAnimationStartTimes") }
    }

    private data class TimelineState(
        var existingAnimationStartMillis: Long? = null,
        var newAnimationBaseDelayMillis: Long? = null,
        var newAnimationStartMillis: Long? = null
    )

    private data class PendingAnimation(
        val animation: AttributedTextAnimation,
        val startDelayMillis: Long
    )

    private val animations = hashMapOf<String, AttributedTextAnimation>()
    private val activeAnimations = ArrayDeque<AttributedTextAnimation>()
    private val activeKeys = hashSetOf<String>()
    private val timelineStates = hashMapOf<String, TimelineState>()
    private val pendingAnimations = arrayListOf<PendingAnimation>()
    private val partAnimations = arrayListOf<AttributedTextAnimation?>()
    private var isSyncing = false
    private var syncTimeMillis = 0L
    var groupedTimeline: AttributedTextAnimationTimeline? = null
    var basePartIndex: Int = 0
    var viewNode: IValdiViewNode? = null
        set(value) {
            field = value
            storedProgress = value?.getStoredObject(TEXT_ANIMATION_START_TIMES_STORAGE_KEY) as? TextAnimationStoredProgress
        }
    private var storedProgress: TextAnimationStoredProgress? = null

    fun beginSync() {
        isSyncing = true
        syncTimeMillis = currentAnimationTimeMillis()
        activeKeys.clear()
        timelineStates.clear()
        pendingAnimations.clear()
        partAnimations.clear()
    }

    fun prepareGroupedFrame() {
        val timeline = groupedTimeline ?: return
        animations.values.forEach { animation ->
            if (animation.active) {
                timeline.recordExistingAnimationScheduledStartTime(
                    animation.timelineKey,
                    animation.startTimeMillis + delayMillisFor(animation.startTransform)
                )
            }
        }
    }

    fun endSync() {
        check(isSyncing) { "AttributedTextAnimator.endSync() called without beginSync()" }
        if (groupedTimeline != null) {
            prepareGroupedFrame()
        }
        if (pendingAnimations.isNotEmpty()) {
            for (pendingAnimation in pendingAnimations) {
                val animation = pendingAnimation.animation
                val groupedTimeline = this.groupedTimeline
                if (groupedTimeline != null) {
                    animation.startTimeMillis = groupedTimeline.startTimeForNewAnimation(
                        animation.timelineKey,
                        syncTimeMillis,
                        timeOffsetMillisFor(animation.startTransform)
                    )
                } else {
                    val timelineState = timelineStates.getOrPut(animation.timelineKey) { TimelineState() }
                    if (timelineState.newAnimationBaseDelayMillis == null) {
                        timelineState.newAnimationBaseDelayMillis = pendingAnimation.startDelayMillis
                    }
                    if (timelineState.newAnimationStartMillis == null) {
                        val nextStartMillis = timelineState.existingAnimationStartMillis?.let {
                            it + timeOffsetMillisFor(animation.startTransform)
                        } ?: syncTimeMillis
                        timelineState.newAnimationStartMillis = max(syncTimeMillis, nextStartMillis)
                    }

                    animation.startTimeMillis =
                        timelineState.newAnimationStartMillis!! - timelineState.newAnimationBaseDelayMillis!!
                }
                animation.scheduledStartTimeMillis = animation.startTimeMillis + pendingAnimation.startDelayMillis
                animation.progress = 0f
                animation.active = true
                applyProgress(animation, 0f)
                storeAnimationStartTimeIfNeeded(animation)
                activeAnimations.add(animation)
            }
        }

        animations.keys.retainAll(activeKeys)
        activeAnimations.removeAll { it.key !in activeKeys }
        isSyncing = false
    }

    fun animationForPart(partIndex: Int, transform: TextAnimationTransform, start: Int, end: Int): AttributedTextAnimation? {
        check(isSyncing) { "AttributedTextAnimator.animationForPart() called outside beginSync()/endSync()" }

        val startDelayMillis = delayMillisFor(transform)
        if (isNoOpStartTransform(transform) || (durationMillisFor(transform) == 0L && startDelayMillis == 0L)) {
            return null
        }

        val key = keyFor(partIndex, transform)
        activeKeys.add(key)

        val existingAnimation = animations[key]
        if (existingAnimation != null) {
            existingAnimation.rangeStart = start
            existingAnimation.rangeEnd = end
            recordPartAnimation(partIndex, existingAnimation)
            recordExistingAnimationScheduledStartTime(existingAnimation)
            return existingAnimation
        }

        val animation = AttributedTextAnimation(
            key = key,
            timelineKey = timelineKeyFor(transform),
            startTransform = transform
        )
        animation.rangeStart = start
        animation.rangeEnd = end
        val storedStartTimeMillis = storedStartTimeMillisFor(key, transform)
        if (storedStartTimeMillis != null) {
            animation.startTimeMillis = storedStartTimeMillis
            animation.scheduledStartTimeMillis = storedStartTimeMillis + startDelayMillis
            val progress = progress(animation, syncTimeMillis)
            if (progress >= 1f) {
                storeAnimationStartTimeIfNeeded(animation)
                return null
            }

            animation.progress = progress
            animation.active = true
            applyProgress(animation, easeOut(progress))
            animations[key] = animation
            activeAnimations.add(animation)
            recordExistingAnimationScheduledStartTime(animation)
            recordPartAnimation(partIndex, animation)
            return animation
        }

        animations[key] = animation
        pendingAnimations.add(PendingAnimation(animation, startDelayMillis))
        recordPartAnimation(partIndex, animation)
        return animation
    }

    fun animationForPartIndex(partIndex: Int): AttributedTextAnimation? {
        return partAnimations.getOrNull(partIndex)
    }

    fun clear(spannable: Spannable? = null) {
        if (spannable != null) {
            animations.values.forEach { animation ->
                spannable.removeSpan(animation)
            }
        }
        isSyncing = false
        activeKeys.clear()
        timelineStates.clear()
        pendingAnimations.clear()
        partAnimations.clear()
        activeAnimations.clear()
        animations.clear()
    }

    fun saveStoredAnimationStartTimes() {
        animations.values.forEach { animation ->
            storeAnimationStartTimeIfNeeded(animation)
        }
    }

    fun hasAnimationRuns(): Boolean {
        return activeAnimations.isNotEmpty()
    }

    fun update(spannable: Spannable): Boolean {
        val activeBefore = activeAnimations.size
        val currentTimeMillis = currentAnimationTimeMillis()
        repeat(activeBefore) {
            val animation = activeAnimations.removeFirst()
            val previousProgress = animation.progress
            val progress = progress(animation, currentTimeMillis)
            val didProgressChange = progress != previousProgress
            if (didProgressChange) {
                val easedProgress = easeOut(progress)
                applyProgress(animation, easedProgress)
                animation.progress = progress
            }
            animation.active = progress < 1f
            if (!animation.active) {
                storeAnimationStartTimeIfNeeded(animation)
                spannable.removeSpan(animation)
            } else if (didProgressChange) {
                invalidateAnimation(spannable, animation)
                activeAnimations.addLast(animation)
            } else {
                activeAnimations.addLast(animation)
            }
        }
        return activeAnimations.isNotEmpty()
    }

    private fun invalidateAnimation(spannable: Spannable, animation: AttributedTextAnimation) {
        val start = animation.rangeStart
        val end = animation.rangeEnd
        if (start < 0 || end <= start || end > spannable.length) {
            return
        }

        spannable.removeSpan(animation)
        spannable.setSpan(
            animation,
            start,
            end,
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE
        )
    }

    private fun recordPartAnimation(partIndex: Int, animation: AttributedTextAnimation) {
        while (partAnimations.size <= partIndex) {
            partAnimations.add(null)
        }
        partAnimations[partIndex] = animation
    }

    private fun recordExistingAnimationScheduledStartTime(animation: AttributedTextAnimation) {
        val timelineState = timelineStates.getOrPut(animation.timelineKey) { TimelineState() }
        timelineState.existingAnimationStartMillis =
            max(timelineState.existingAnimationStartMillis ?: Long.MIN_VALUE, animation.scheduledStartTimeMillis)
    }

    private fun storedStartTimeMillisFor(key: String, transform: TextAnimationTransform): Long? {
        if (transform.key == null) {
            return null
        }
        return storedProgress?.startTimes?.get(key)
    }

    private fun storeAnimationStartTimeIfNeeded(animation: AttributedTextAnimation) {
        if (animation.startTransform.key == null) {
            return
        }
        val viewNode = viewNode ?: return
        val currentStoredProgress = storedProgress ?: TextAnimationStoredProgress().also {
            storedProgress = it
            viewNode.setStoredObject(TEXT_ANIMATION_START_TIMES_STORAGE_KEY, it)
        }
        currentStoredProgress.startTimes[animation.key] = animation.startTimeMillis
    }

    private fun keyFor(partIndex: Int, transform: TextAnimationTransform): String {
        val transformKey = transform.key
        return if (transformKey != null) {
            "$transformKey:$partIndex"
        } else {
            partIndex.toString()
        }
    }

    private fun timelineKeyFor(transform: TextAnimationTransform): String {
        return transform.key ?: "group:${transform.groupIndex}"
    }

    private fun progress(animation: AttributedTextAnimation, currentTimeMillis: Long): Float {
        val startTransform = animation.startTransform
        if (isNoOpStartTransform(startTransform)) {
            return 1f
        }

        val startDelayMillis = delayMillisFor(startTransform)
        val delayedElapsedMillis = currentTimeMillis - animation.startTimeMillis - startDelayMillis
        val durationMillis = durationMillisFor(startTransform)
        if (durationMillis == 0L) {
            return if (delayedElapsedMillis >= 0L) 1f else 0f
        }
        if (delayedElapsedMillis <= 0L) {
            return 0f
        }

        return min(max(delayedElapsedMillis.toFloat() / durationMillis.toFloat(), 0f), 1f)
    }

    private fun applyProgress(animation: AttributedTextAnimation, progress: Float) {
        val start = animation.startTransform
        animation.translationY = start.translationY * (1f - progress)
        animation.scale = start.scale + (1f - start.scale) * progress
        animation.opacity = start.opacity + (1f - start.opacity) * progress
    }

    private fun easeOut(progress: Float): Float {
        return (1f - (1f - progress).toDouble().pow(3.0)).toFloat()
    }

    private fun delayMillisFor(transform: TextAnimationTransform): Long {
        val delaySeconds = transform.timeOffsetBetweenParts * (basePartIndex + transform.partIndexInGroup)
        return max((delaySeconds * 1000.0).toLong(), 0L)
    }

    private fun timeOffsetMillisFor(transform: TextAnimationTransform): Long {
        return max((transform.timeOffsetBetweenParts * 1000.0).toLong(), 0L)
    }

    private fun durationMillisFor(transform: TextAnimationTransform): Long {
        return max((transform.duration * 1000.0).toLong(), 0L)
    }

    private fun isNoOpStartTransform(transform: TextAnimationTransform): Boolean {
        return transform.translationY == 0f &&
                transform.scale == 1f &&
                transform.opacity == 1f
    }

    private fun currentAnimationTimeMillis(): Long {
        return AnimationUtils.currentAnimationTimeMillis()
    }
}
