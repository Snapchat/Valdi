package com.snap.valdi.views.touches

import android.graphics.Matrix
import android.graphics.Rect
import android.os.Build
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import com.snap.valdi.extensions.ViewUtils
import com.snap.valdi.logger.Logger
import com.snap.valdi.views.ValdiRootView

interface TouchDispatcher {

    val rootView: ViewGroup
    var captureAllHitTargets: Boolean

    var disallowInterceptTouchEventMode: DisallowInterceptTouchEventMode

    var cancelsTouchTargetsWhenGestureRequestsExclusivity: Boolean

    var adjustCoordinates: Boolean

    fun dispatchTouch(event: MotionEvent): Boolean

    fun getCurrentGesturesState(): GesturesState

    fun gestureRecognizerWantsDeferredUpdate(gestureRecognizer: ValdiGestureRecognizer): Boolean

    fun isEmpty(): Boolean

    companion object {
        fun hitTest(view: View, event: MotionEvent, outLocation: IntArray, rect: Rect, corners: FloatArray = FloatArray(8)): Boolean {
            // Single findValdiContext traversal — used for both the tweak flag and
            // newMultiTouchExperience, avoiding two separate parent-chain walks.
            val valdiContext = ViewUtils.findValdiContext(view)
            val tweaks = valdiContext?.runtimeOrNull?.manager?.tweaks
            val rootView = if (view is ValdiRootView) view else valdiContext?.rootView

            if (tweaks?.disableTransformAwareHitTest == true) {
                // Legacy path: origin + raw dimensions. Incorrect when an ancestor has a
                // non-identity matrix (e.g. scaleY=-1), but kept as an escape hatch.
                view.getLocationOnScreen(outLocation)
                rect.left = outLocation[0]
                rect.top = outLocation[1]
                rect.right = outLocation[0] + view.width
                rect.bottom = outLocation[1] + view.height
            } else {
                // Transform-aware path: map all 4 corners through the full parent-transform
                // chain so the bounding rect is correct even when ancestors have non-identity
                // matrices (e.g. scaleY=-1 on an inverted scroll view).
                // corners and outLocation are caller-allocated to avoid per-call allocation.
                val w = view.width.toFloat()
                val h = view.height.toFloat()
                corners[0] = 0f;  corners[1] = 0f
                corners[2] = w;   corners[3] = 0f
                corners[4] = 0f;  corners[5] = h
                corners[6] = w;   corners[7] = h
                // outLocation is reused as rootLocation scratch space inside mapLocalPointsToScreen;
                // it is safe to do so because we overwrite outLocation[0/1] below after the call.
                mapLocalPointsToScreen(view, corners, outLocation)

                var minX = corners[0]; var maxX = corners[0]
                var minY = corners[1]; var maxY = corners[1]
                for (i in corners.indices step 2) {
                    if (corners[i] < minX) minX = corners[i]
                    if (corners[i] > maxX) maxX = corners[i]
                    if (corners[i + 1] < minY) minY = corners[i + 1]
                    if (corners[i + 1] > maxY) maxY = corners[i + 1]
                }

                outLocation[0] = minX.toInt()
                outLocation[1] = minY.toInt()
                rect.set(minX.toInt(), minY.toInt(), maxX.toInt(), maxY.toInt())
            }

            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && rootView?.useNewMultiTouchExperience == true) {
                rect.contains(event.getRawX(event.actionIndex).toInt(), event.getRawY(event.actionIndex).toInt())
            } else {
                rect.contains(event.rawX.toInt(), event.rawY.toInt())
            }
        }

        /**
         * Maps [points] (interleaved x,y pairs in the view's local coordinate space) through the
         * full chain of parent transforms up to screen coordinates, replicating the same sequence
         * of steps that [View.getLocationOnScreen] performs for a single point.
         */
        private fun mapLocalPointsToScreen(view: View, points: FloatArray, rootLocation: IntArray) {
            var current: View = view
            // Exits when current.parent is no longer a View (reached the window root).
            while (true) {
                if (!current.matrix.isIdentity) {
                    current.matrix.mapPoints(points)
                }
                val parent = current.parent as? View ?: break
                val dx = (current.left - parent.scrollX).toFloat()
                val dy = (current.top - parent.scrollY).toFloat()
                for (i in points.indices step 2) {
                    points[i] += dx
                    points[i + 1] += dy
                }
                current = parent
            }
            // current is now the root view; add its window-to-screen offset.
            // getLocationOnScreen(0,0) = current.matrix.map(0,0) + windowOffset.
            // We already applied current.matrix to points in the loop, so we must add only
            // windowOffset to avoid double-counting the root view's matrix translation.
            // For any 2D affine matrix, map(0,0) = (MTRANS_X, MTRANS_Y).
            current.getLocationOnScreen(rootLocation)
            var windowOffsetX = rootLocation[0].toFloat()
            var windowOffsetY = rootLocation[1].toFloat()
            if (!current.matrix.isIdentity) {
                val matrixValues = FloatArray(9)
                current.matrix.getValues(matrixValues)
                windowOffsetX -= matrixValues[Matrix.MTRANS_X]
                windowOffsetY -= matrixValues[Matrix.MTRANS_Y]
            }
            for (i in points.indices step 2) {
                points[i] += windowOffsetX
                points[i + 1] += windowOffsetY
            }
        }

        fun create(
            rootView: ViewGroup,
            disallowInterceptTouchEventMode: DisallowInterceptTouchEventMode,
            logger: Logger?,
            debugTouchEvents: Boolean = false,
            cancelsTouchTargetsWhenGestureRequestsExclusivity: Boolean = false,
            captureAllHitTargets: Boolean = false,
            adjustCoordinates: Boolean = false,
        ) : TouchDispatcher {
            return if (newMultiTouchExperience(rootView)) {
                TouchDispatcherNewExperience(
                    rootView,
                    disallowInterceptTouchEventMode,
                    logger,
                    debugTouchEvents,
                    cancelsTouchTargetsWhenGestureRequestsExclusivity,
                    captureAllHitTargets,
                    adjustCoordinates,
                )
            } else {
                TouchDispatcherImpl(
                    rootView,
                    disallowInterceptTouchEventMode,
                    logger,
                    debugTouchEvents,
                    cancelsTouchTargetsWhenGestureRequestsExclusivity,
                    captureAllHitTargets,
                    adjustCoordinates,
                )
            }
        }

        fun newMultiTouchExperience(view: View): Boolean {
            val rootView = if (view is ValdiRootView) {
                view
            } else {
                ViewUtils.findValdiContext(view)?.rootView
            }
            return rootView?.useNewMultiTouchExperience ?: false
        }
    }
}