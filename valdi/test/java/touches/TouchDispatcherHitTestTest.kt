package com.snap.valdi.views.touches

import android.content.Context
import android.graphics.Rect
import android.view.MotionEvent
import android.view.View
import android.widget.FrameLayout
import androidx.test.core.app.ApplicationProvider.getApplicationContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * Tests for [TouchDispatcher.hitTest]'s transform-aware path introduced to fix incorrect
 * screen-rect computation when ancestor views have non-identity matrices (e.g. scaleY=-1
 * on an inverted scroll list).
 *
 * Note: Some tests that exercise matrix effects via [android.view.View.getLocationOnScreen]
 * carry an inherent Robolectric limitation — the shadow may not fully replicate all matrix
 * transforms through [android.view.View.transformFromViewToWindowSpace]. Those tests still
 * document the expected contract and guard against regressions on implementations that do
 * support the full transform chain.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [28], manifest = Config.NONE)
class TouchDispatcherHitTestTest {

    private fun makeDownEvent(x: Float, y: Float): MotionEvent =
        MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_DOWN, x, y, 0)

    private fun allocScratch() = Pair(IntArray(2), Rect())

    // -----------------------------------------------------------------------------------------
    // Baseline: identity hierarchy
    // -----------------------------------------------------------------------------------------

    @Test
    fun `hitTest identity hierarchy - point inside child returns true`() {
        val (_, child) = buildTwoLevelHierarchy(
            rootSize = 1000 to 1000,
            childBounds = Rect(100, 100, 300, 300),
        )
        val (loc, rect) = allocScratch()
        val event = makeDownEvent(200f, 200f) // centre of child

        assertTrue(TouchDispatcher.hitTest(child, event, loc, rect))
        event.recycle()
    }

    @Test
    fun `hitTest identity hierarchy - point outside child returns false`() {
        val (_, child) = buildTwoLevelHierarchy(
            rootSize = 1000 to 1000,
            childBounds = Rect(100, 100, 300, 300),
        )
        val (loc, rect) = allocScratch()
        val event = makeDownEvent(50f, 50f) // above-left of child

        assertFalse(TouchDispatcher.hitTest(child, event, loc, rect))
        event.recycle()
    }

    // -----------------------------------------------------------------------------------------
    // scaleY=-1 on an intermediate ancestor (the PublicGroups inverted-scroll scenario)
    // -----------------------------------------------------------------------------------------

    /**
     * When a parent has scaleY=-1 the child's visual rect is reflected around the parent's
     * vertical centre. [TouchDispatcher.hitTest] must map all four corners through the full
     * transform chain and take their bounding rect, not just use getLocationOnScreen(0,0)
     * + width/height.
     *
     * Hierarchy:  root(0..1000 x 0..600)
     *               └── parent(0..1000 x 0..600)  scaleY=-1
     *                     └── child(0..1000 x 400..600)  — at bottom of parent
     *
     * After scaleY=-1 on parent the child visually appears near the TOP of the screen (y ~0..200).
     */
    @Test
    fun `hitTest scaleY=-1 parent - child hit-test reflects to correct screen rect`() {
        val context = getApplicationContext<Context>()
        val root = FrameLayout(context)
        val parent = FrameLayout(context)
        val child = View(context)

        root.addView(parent)
        parent.addView(child)

        root.layout(0, 0, 1000, 600)
        parent.layout(0, 0, 1000, 600)
        child.layout(0, 400, 1000, 600) // child in the bottom 200px of parent

        parent.scaleY = -1f // mirror parent; child visually moves to y 0..200

        val (loc, rect) = allocScratch()

        // Touch inside reflected position (y ~100) should hit.
        val hitEvent = makeDownEvent(500f, 100f)
        assertTrue(TouchDispatcher.hitTest(child, hitEvent, loc, rect))
        hitEvent.recycle()

        // Touch at original (un-reflected) position (y ~500) should miss.
        val missEvent = makeDownEvent(500f, 500f)
        assertFalse(TouchDispatcher.hitTest(child, missEvent, loc, rect))
        missEvent.recycle()
    }

    // -----------------------------------------------------------------------------------------
    // Root-view matrix: the double-counting bug fixed in mapLocalPointsToScreen
    // -----------------------------------------------------------------------------------------

    /**
     * Exercises the non-identity root-view matrix code path introduced to fix double-counting.
     *
     * **The bug**: when the root view (the topmost [View] in the hierarchy) has a non-identity
     * matrix (e.g. during a window slide-in animation), [android.graphics.Matrix.mapPoints]
     * already applies the matrix translation to [points] inside the traversal loop. Previously,
     * [android.view.View.getLocationOnScreen] was then added verbatim — but its return value
     * also includes the root matrix's translation (via Android's internal
     * `transformFromViewToWindowSpace`), double-counting it.
     *
     * **The fix**: subtract [android.graphics.Matrix.MTRANS_X]/[android.graphics.Matrix.MTRANS_Y]
     * from the [getLocationOnScreen] result before adding it, so only the pure window-to-screen
     * offset is applied.
     *
     * **Robolectric limitation**: Robolectric's [View.getLocationOnScreen] shadow does **not**
     * apply the view's matrix; it always returns the raw sum of `left`/`top` values up the
     * hierarchy. Because the shadow returns `(0,0)` for an unattached root, subtracting
     * `MTRANS_X/Y` would over-correct, neutralising the translation. This test therefore only
     * verifies that the non-identity matrix code path executes without error and still produces
     * a valid, consistent rect. The full correctness guarantee (translation counted exactly once)
     * must be verified with an instrumented device test or by manual inspection on a real device.
     */
    @Test
    fun `hitTest root view with translation - non-identity matrix path runs without error`() {
        val (root, child) = buildTwoLevelHierarchy(
            rootSize = 1000 to 1000,
            childBounds = Rect(100, 100, 300, 300),
        )
        root.translationX = 50f
        root.translationY = 50f

        val (loc, rect) = allocScratch()

        // In Robolectric, getLocationOnScreen ignores the root matrix and returns (0,0).
        // mapLocalPointsToScreen applies the matrix via mapPoints (+50,+50) and then subtracts
        // MTRANS_X/Y from rootLocation (0,0), netting out the translation. The effective rect
        // therefore equals the child's local bounds: (100..300, 100..300).
        // Regardless of this Robolectric-specific outcome, the important invariant is that no
        // exception is thrown and the rect is non-empty (i.e. the code path is live).
        val event = makeDownEvent(200f, 200f) // inside child's local bounds in both interpretations
        assertTrue(TouchDispatcher.hitTest(child, event, loc, rect))
        event.recycle()
    }

    // -----------------------------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------------------------

    /**
     * Builds a two-level [FrameLayout] → [View] hierarchy laid out inside the Robolectric
     * environment and returns (root, child).
     */
    private fun buildTwoLevelHierarchy(
        rootSize: Pair<Int, Int>,
        childBounds: Rect,
    ): Pair<FrameLayout, View> {
        val context = getApplicationContext<Context>()
        val root = FrameLayout(context)
        val child = View(context)
        root.addView(child)
        root.layout(0, 0, rootSize.first, rootSize.second)
        child.layout(childBounds.left, childBounds.top, childBounds.right, childBounds.bottom)
        return root to child
    }
}
