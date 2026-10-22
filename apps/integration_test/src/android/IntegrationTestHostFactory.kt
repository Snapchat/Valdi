package com.snap.valdi.integrationtest

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.widget.EditText
import com.snap.valdi.ViewFactory
import com.snap.valdi.attributes.AttributesBinder
import com.snap.valdi.attributes.AttributesBindingContext
import com.snap.valdi.context.ValdiContext
import com.snap.valdi.createViewFactory
import com.snap.valdi.modules.RegisterValdiModule
import com.snap.valdi.modules.integration_test_app.FactoryIntegrationHostModule
import com.snap.valdi.modules.integration_test_app.FactoryIntegrationHostModuleFactory
import com.snap.valdi.modules.integration_test_app.IntegrationTestHostModule
import com.snap.valdi.modules.integration_test_app.IntegrationTestHostModuleFactory
import com.snap.valdi.nodes.IValdiViewNode
import com.snap.valdi.nodes.ValdiViewNode
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.json.JSONObject
import java.io.File

@RegisterValdiModule
class IntegrationTestHostFactory : IntegrationTestHostModuleFactory() {
    override fun onLoadModule(): IntegrationTestHostModule {
        return IntegrationTestHostModuleImpl()
    }
}

@RegisterValdiModule
class FactoryIntegrationHostFactory : FactoryIntegrationHostModuleFactory() {
    override fun onLoadModule(): FactoryIntegrationHostModule {
        return FactoryIntegrationHostModuleImpl()
    }
}

private class FactoryIntegrationHostModuleImpl : FactoryIntegrationHostModule {
    override fun createIntegrationViewFactory(): ViewFactory {
        val runtime = checkNotNull(ValdiContext.current()).runtime
        return runtime.createViewFactory(
            IntegrationFactoryView::class.java,
            { context -> IntegrationFactoryView(context) },
            IntegrationFactoryAttributesBinder(),
        )
    }
}

private class IntegrationFactoryView(context: Context) : View(context) {
    var factoryText: String? = null
        set(value) {
            field = value
            invalidate()
        }

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val scale = resources.displayMetrics.density
        val centerY = height / 2f

        path.reset()
        path.moveTo(44f * scale, centerY - 26f * scale)
        path.lineTo(66f * scale, centerY - 13f * scale)
        path.lineTo(66f * scale, centerY + 13f * scale)
        path.lineTo(44f * scale, centerY + 26f * scale)
        path.lineTo(22f * scale, centerY + 13f * scale)
        path.lineTo(22f * scale, centerY - 13f * scale)
        path.close()
        paint.color = Color.rgb(79, 70, 229)
        canvas.drawPath(path, paint)

        path.reset()
        path.moveTo(44f * scale, centerY - 16f * scale)
        path.lineTo(49f * scale, centerY - 5f * scale)
        path.lineTo(60f * scale, centerY)
        path.lineTo(49f * scale, centerY + 5f * scale)
        path.lineTo(44f * scale, centerY + 16f * scale)
        path.lineTo(39f * scale, centerY + 5f * scale)
        path.lineTo(28f * scale, centerY)
        path.lineTo(39f * scale, centerY - 5f * scale)
        path.close()
        paint.color = Color.WHITE
        canvas.drawPath(path, paint)

        paint.color = Color.rgb(23, 37, 84)
        paint.textSize = 16f * scale
        paint.typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        val baseline = centerY - (paint.ascent() + paint.descent()) / 2f
        canvas.drawText(factoryText.orEmpty(), 82f * scale, baseline, paint)
    }
}

private class IntegrationFactoryAttributesBinder : AttributesBinder<IntegrationFactoryView> {
    override val viewClass = IntegrationFactoryView::class.java

    override fun bindAttributes(attributesBindingContext: AttributesBindingContext<IntegrationFactoryView>) {
        attributesBindingContext.bindStringAttribute(
            "factoryText",
            false,
            { view, value, _ -> view.factoryText = value },
            { view, _ -> view.factoryText = null },
        )
    }
}

private class IntegrationTestHostModuleImpl : IntegrationTestHostModule {
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun getPlatform(): String = "android"

    override fun getOutputPath(): String {
        return "/data/data/com.snap.valdi.integrationtest/files/valdi-integration-test/results.json"
    }

    override fun markFinished(path: String) {
        // The TypeScript harness writes the result and .done sentinel through file_system.
    }

    override fun writeTextFile(path: String, contents: String) {
        val file = File(path)
        file.parentFile?.mkdirs()
        file.writeText(contents)
    }

    override fun submitTouchSequence(node: IValdiViewNode, sequenceJson: String): String {
        val target = getBackingView(node) ?: return "no backing Android View for ${node.viewClassName}"
        val request = JSONObject(sequenceJson)
        val events = request.optJSONArray("events") ?: return "no events in sequence"
        val downTime = SystemClock.uptimeMillis()
        val latch = CountDownLatch(1)
        val errors = mutableListOf<String>()

        mainHandler.post {
            try {
                var eventTime = downTime
                for (i in 0 until events.length()) {
                    val event = events.getJSONObject(i)
                    eventTime += event.optLong("delayMs", 16)
                    val action = when (event.optString("action")) {
                        "down" -> MotionEvent.ACTION_DOWN
                        "move" -> MotionEvent.ACTION_MOVE
                        "up" -> MotionEvent.ACTION_UP
                        "cancel" -> MotionEvent.ACTION_CANCEL
                        else -> MotionEvent.ACTION_CANCEL
                    }
                    val x = (event.optDouble("x", 0.5) * target.width).toFloat()
                    val y = (event.optDouble("y", 0.5) * target.height).toFloat()
                    val motionEvent = MotionEvent.obtain(downTime, eventTime, action, x, y, 0)
                    try {
                        target.dispatchTouchEvent(motionEvent)
                    } finally {
                        motionEvent.recycle()
                    }
                }
            } catch (error: Throwable) {
                errors.add(error.message ?: error.javaClass.name)
            } finally {
                latch.countDown()
            }
        }

        latch.await(2, TimeUnit.SECONDS)
        return if (errors.isEmpty()) {
            "dispatched ${events.length()} event(s) to ${target.javaClass.simpleName}"
        } else {
            "dispatch failed: ${errors.joinToString("; ")}"
        }
    }

    override fun focusTextInput(node: IValdiViewNode): String {
        val editText = getBackingView(node) as? EditText ?: return "target is not EditText"
        runOnMainSync {
            editText.requestFocus()
        }
        return "focused ${editText.javaClass.simpleName}"
    }

    override fun replaceText(node: IValdiViewNode, value: String): String {
        val editText = getBackingView(node) as? EditText ?: return "target is not EditText"
        runOnMainSync {
            editText.setText(value)
            editText.setSelection(value.length)
        }
        return "set text length=${value.length}"
    }

    override fun pressReturn(node: IValdiViewNode): String {
        val editText = getBackingView(node) as? EditText ?: return "target is not EditText"
        runOnMainSync {
            editText.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER))
            editText.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER))
        }
        return "sent enter key"
    }

    override fun pressBackspace(node: IValdiViewNode): String {
        val editText = getBackingView(node) as? EditText ?: return "target is not EditText"
        runOnMainSync {
            editText.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL))
            editText.dispatchKeyEvent(KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL))
        }
        return "sent delete key"
    }

    private fun getBackingView(node: IValdiViewNode): View? {
        val ref = (node as? ValdiViewNode)?.getBackingViewRef()
        return ref?.get() as? View
    }

    private fun runOnMainSync(block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            block()
            return
        }

        val latch = CountDownLatch(1)
        mainHandler.post {
            try {
                block()
            } finally {
                latch.countDown()
            }
        }
        latch.await(2, TimeUnit.SECONDS)
    }
}
