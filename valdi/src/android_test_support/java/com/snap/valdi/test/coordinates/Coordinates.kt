package com.snap.valdi.test.coordinates

import android.graphics.Rect
import android.view.View
import androidx.test.espresso.action.CoordinatesProvider

class Coordinates(
    val view: View,
    val viewNodeFrame: Rect,
    val locationX: Int,
    val locationY: Int
) {

    fun atHorizontalStart(): Coordinates {
        return atOffsetRatio(0.0f, 0.0f)
    }

    fun atHorizontalCenter(): Coordinates {
        return atOffsetRatio(0.5f, 0.0f)
    }

    fun atHorizontalEnd(): Coordinates {
        return atOffsetRatio(1.0f, 0.0f)
    }

    fun atVerticalStart(): Coordinates {
        return atOffsetRatio(0.0f, 0.0f)
    }

    fun atVerticalCenter(): Coordinates {
        return atOffsetRatio(0.0f, 0.5f)
    }

    fun atVerticalEnd(): Coordinates {
        return atOffsetRatio(0.0f, 1f)
    }

    fun atOffsetRatio(offsetRatioX: Float, offsetRatioY: Float): Coordinates {
        return atOffset((viewNodeFrame.width() * offsetRatioX).toInt(), (viewNodeFrame.height() * offsetRatioY).toInt())
    }

    fun atOffset(offsetX: Int, offsetY: Int): Coordinates {
        return Coordinates(view, viewNodeFrame, locationX + offsetX, locationY + offsetY)
    }

    fun limitingToVisibleParts(): Coordinates {
        // TODO(simon): Finish this
        return this
    }

    fun toCoordinatesProvider(): CoordinatesProvider {
        val location = intArrayOf(0, 0)
        view.getLocationOnScreen(location)
        location[0] += locationX
        location[1] += locationY

        return CoordinatesProvider {
            floatArrayOf(location[0].toFloat(), location[1].toFloat())
        }
    }
}
