package com.snap.valdi.modules

import android.annotation.TargetApi
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
import android.os.LocaleList
import android.os.SystemClock
import android.util.DisplayMetrics
import android.view.WindowManager
import com.snap.valdi.callable.ValdiFunction
import com.snap.valdi.jsmodules.JSThreadDispatcher
import com.snap.valdi.utils.ValdiMarshaller
import com.snap.valdi.utils.dispatchOnMainThread
import com.snap.valdi.context.ValdiContext
import java.util.Date
import java.util.Locale
import java.util.TimeZone

class ValdiDeviceModule(
        private val jsThreadDispatcher: JSThreadDispatcher,
        val context: Context,
        private val forceDarkMode: Boolean
) : ValdiBridgeModule(), ValdiBridgeObserverListener {

    var performHapticFeedbackFunction: ValdiFunction? = null
        set(value) {
            synchronized(this) {
                field = value
            }
        }
        get() {
            return synchronized(this) { field }
        }
    private val systemVersion: String
    private val model: String

    // Mutable properties read and written on the JS thread.
    private var displayWidth: Double = 0.0
    private var displayHeight: Double = 0.0
    private var displayScale: Double = 1.0
    private var displayTopInset = 0.0
    private var displayBottomInset = 0.0
    private var systemUsesDarkMode = false
    private var isDarkMode = forceDarkMode

    private var windowWidth: Double = 0.0
    private var windowHeight: Double = 0.0

    private var insetsObserver = ValdiBridgeObserver()
    private var displaySizeObserver = ValdiBridgeObserver()
    private var darkModeObserver = ValdiBridgeObserver()

    init {
        systemVersion = Build.VERSION.SDK_INT.toString()
        model = Build.MODEL

        updateDisplaySize()

        darkModeObserver.setListener(this)
    }

    private fun updateDisplaySize() {
        val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as? WindowManager
            ?: throw IllegalStateException("WindowManager should never be null")

        val displayMetrics = DisplayMetrics()
        windowManager.defaultDisplay.getRealMetrics(displayMetrics)
            
        val scale = displayMetrics.density.toDouble()
        displayScale = scale
        displayWidth = displayMetrics.widthPixels.toDouble() / scale
        displayHeight = displayMetrics.heightPixels.toDouble() / scale

        updateWindowMetrics()
    }

    private fun updateWindowMetrics() {
        val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as? WindowManager
            ?: throw IllegalStateException("WindowManager should never be null")

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val windowMetrics = windowManager.currentWindowMetrics

            windowWidth = windowMetrics.bounds.width().toDouble() / displayScale
            windowHeight = windowMetrics.bounds.height().toDouble() / displayScale
        } else {
            val displayMetrics = DisplayMetrics()
            windowManager.defaultDisplay.getRealMetrics(displayMetrics)

            windowWidth = displayMetrics.widthPixels.toDouble() / displayScale
            windowHeight = displayMetrics.heightPixels.toDouble() / displayScale
        }
    }

    fun notifyInsetsChanged(topInset: Double, bottomInset: Double) {
        jsThreadDispatcher.runOnJsThread(Runnable {
            updateDisplaySize()

            displayTopInset = topInset / displayScale
            displayBottomInset = bottomInset / displayScale

            ValdiMarshaller.use {
                insetsObserver.notifyObservers(it)
            }
        })
    }

    fun notifyDisplaySizeChanged() {
        jsThreadDispatcher.runOnJsThread(Runnable {
            updateDisplaySize()

            ValdiMarshaller.use {
                displaySizeObserver.notifyObservers(it)
            }
        })
    }
    // Pass useDarkMode=null to flush current resolved
    // dark mode value
    fun notifyDarkModeChanged(useDarkMode: Boolean?) {
        jsThreadDispatcher.runOnJsThread(Runnable {
            if (useDarkMode != null) {
                systemUsesDarkMode = useDarkMode
            }
            isDarkMode = systemUsesDarkMode || forceDarkMode

            ValdiMarshaller.use {
                it.pushBoolean(isDarkMode)
                darkModeObserver.notifyObservers(it)
            }
        })
    }

    override fun bridgeObserverAddedNewCallback(observer: ValdiBridgeObserver) {
        if (observer == darkModeObserver) {
            // immediately notify new subscriber with current value in case of a late subscription
            ValdiMarshaller.use {
                it.pushBoolean(isDarkMode)
                darkModeObserver.notifyObservers(it)
            }
        }
    }

    private fun performHapticFeedback(marshaller: ValdiMarshaller) {
        val function = this.performHapticFeedbackFunction
        if (function == null) {
            marshaller.pushUndefined()
            return
        }

        if (!function.perform(marshaller)) {
            marshaller.pushUndefined()
        }
    }

    private fun getSystemType(marshaller: ValdiMarshaller) {
        marshaller.pushString("android")
    }

    private fun getSystemVersion(marshaller: ValdiMarshaller) {
        marshaller.pushString(systemVersion)
    }

    private fun getModel(marshaller: ValdiMarshaller) {
        marshaller.pushString(model)
    }

    private fun getDeviceLocales(marshaller: ValdiMarshaller) {
        val locales = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            context.getResources().getConfiguration().locales.toList()
        } else {
            listOf(context.getResources().getConfiguration().locale)
        }
        val count = locales.size
        val list = marshaller.pushList(count)
        for (idx in 0..count - 1) {
            val locale = locales.get(idx)
            val localeString = locale.toLanguageTag()
            marshaller.pushString(localeString)
            marshaller.setListItem(list, idx)
        }
    }

    private fun getWindowWidth(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(windowWidth)
    }

    private fun getWindowHeight(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(windowHeight)
    }

    private fun getDisplayWidth(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(displayWidth)
    }

    private fun getDisplayHeight(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(displayHeight)
    }

    private fun getDisplayScale(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(displayScale)
    }

    private fun getDynamicTypeScale(marshaller: ValdiMarshaller) {
        val metrics = context.resources.displayMetrics
        val density = metrics.density
        val scale = if (density > 0f) metrics.scaledDensity.toDouble() / density.toDouble() else 1.0
        marshaller.pushDouble(scale)
    }

    private fun getDisplayLeftInset(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(0.0)
    }

    private fun getDisplayTopInset(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(displayTopInset)
    }

    private fun getDisplayRightInset(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(0.0)
    }

    private fun getDisplayBottomInset(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(displayBottomInset)
    }

    private fun setBackButtonObserver(marshaller: ValdiMarshaller) {
        val rootView = ValdiContext.current()?.rootView ?: return

        if (marshaller.isNullOrUndefined(0)) {
            rootView.setBackButtonObserverWithFunction(null)
        } else {
            rootView.setBackButtonObserverWithFunction(marshaller.getFunction(0))
        }

        marshaller.pushUndefined()
    }

    private fun getLocaleUsesMetricSystem(marshaller: ValdiMarshaller) {
        // The LocaleData.MeasurementSystem API is only available from API level 28
        // Since there are only a handful of countries still using the imperial
        // system, it's easy enough to implement manually
        val country = Locale.getDefault().getCountry()
        val usesMetricSystem = when (country.uppercase()) {
            // Technically the UK only uses metric for distance and some volume measures
            // but I'm confident this query will only be used for distance measures.
            "US", "GB", "MM", "LR" -> false
            else -> true
        }

        marshaller.pushBoolean(usesMetricSystem)
    }

    private fun getTimeZoneName(marshaller: ValdiMarshaller) {
        val timezone = TimeZone.getDefault()
        marshaller.pushString(timezone.getID())
    }

    private fun getTimeZoneFromMarshaller(marshaller: ValdiMarshaller): TimeZone {
        val timezone = if (marshaller.isString(0)) {
            TimeZone.getTimeZone(marshaller.getString(0))
        } else {
            TimeZone.getDefault()
        }
        return timezone;
    }

    private fun getTimeZoneRawSecondsFromGMT(marshaller: ValdiMarshaller) {
        val timezone = getTimeZoneFromMarshaller(marshaller)
        val offsetMillis = timezone.getRawOffset()
        val offsetSeconds = offsetMillis / 1000.0
        marshaller.pushDouble(offsetSeconds)
    }

    private fun getTimeZoneDstSecondsFromGMT(marshaller: ValdiMarshaller) {
        val timezone = getTimeZoneFromMarshaller(marshaller)
        val offsetMillis = timezone.getOffset(Date().getTime())
        val offsetSeconds = offsetMillis / 1000.0
        marshaller.pushDouble(offsetSeconds)
    }

    private fun copyToClipBoard(marshaller: ValdiMarshaller) {
        val clipContent = marshaller.getString(0)
        dispatchOnMainThread {
            val clipboard = this.context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("", clipContent)
            clipboard.setPrimaryClip(clip)
        }
    }

    private fun getUptimeMs(marshaller: ValdiMarshaller) {
        marshaller.pushDouble(SystemClock.uptimeMillis().toDouble())
    }

    override fun getModulePath(): String {
        return "valdi_core/src/DeviceBridge"
    }

    override fun loadModule(): Any {
        return mapOf(
                "copyToClipBoard" to makeBridgeMethod (this::copyToClipBoard),
                "getSystemType" to makeBridgeMethod(this::getSystemType),
                "getSystemVersion" to makeBridgeMethod(this::getSystemVersion),
                "getModel" to makeBridgeMethod(this::getModel),
                "getDeviceLocales" to makeBridgeMethod(this::getDeviceLocales),
                "getDisplayWidth" to makeBridgeMethod(this::getDisplayWidth),
                "getDisplayHeight" to makeBridgeMethod(this::getDisplayHeight),
                "getDisplayScale" to makeBridgeMethod(this::getDisplayScale),
                "getDynamicTypeScale" to makeBridgeMethod(this::getDynamicTypeScale),
                "getWindowWidth" to makeBridgeMethod(this::getWindowWidth),
                "getWindowHeight" to makeBridgeMethod(this::getWindowHeight),
                "getDisplayLeftInset" to makeBridgeMethod(this::getDisplayLeftInset),
                "getDisplayRightInset" to makeBridgeMethod(this::getDisplayRightInset),
                "getDisplayBottomInset" to makeBridgeMethod(this::getDisplayBottomInset),
                "getDisplayTopInset" to makeBridgeMethod(this::getDisplayTopInset),
                "setBackButtonObserver" to makeBridgeMethod(this::setBackButtonObserver),
                "observeDisplayInsetChange" to insetsObserver,
                "observeDisplaySizeChange" to displaySizeObserver,
                "performHapticFeedback" to makeBridgeMethod(this::performHapticFeedback),
                "getLocaleUsesMetricSystem" to makeBridgeMethod(this::getLocaleUsesMetricSystem),
                "getTimeZoneName" to makeBridgeMethod(this::getTimeZoneName),
                "getTimeZoneRawSecondsFromGMT" to makeBridgeMethod(this::getTimeZoneRawSecondsFromGMT),
                "getTimeZoneDstSecondsFromGMT" to makeBridgeMethod(this::getTimeZoneDstSecondsFromGMT),
                "getUptimeMs" to makeBridgeMethod(this::getUptimeMs),
                "observeDarkMode" to darkModeObserver
        )
    }

    @TargetApi(Build.VERSION_CODES.N)
    private fun LocaleList.toList(): List<Locale> = mutableListOf<Locale>().also {
        for (i in 0..size() - 1) {
            it.add(get(i))
        }
    }.toList()
}
