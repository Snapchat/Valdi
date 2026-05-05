package com.snap.valdi

import com.snap.valdi.snapdrawing.SnapDrawingOptions
import com.snap.valdi.utils.QoSClass
import com.snapchat.client.valdi_core.JavaScriptEngineType;

// Values of 0 means use default
data class ValdiTweaks(
        val javaScriptEngineType: JavaScriptEngineType = JavaScriptEngineType.AUTO,
        val maxJsStackSize: Int = 0,
        val maxJsStackSizePercentToNative: Int = 0,
        val disableBoxShadow: Boolean = false,
        val disableAnimations: Boolean = false,
        val disableSlowClipping: Boolean = false,
        val useNativeHandlersManager: Boolean = false,
        val forceDarkMode: Boolean = false,
        val maxImageCacheSizeInBytes : Long = 0,
        val enableSkia: Boolean = false,
        val enableLeakTracker: Boolean = false,
        val debugTouchEvents: Boolean = false,
        val keepDebuggerServiceOnPause: Boolean = false,
        val fatalExceptionSleepTimeBeforeRethrowing: Long? = null,
        val disableLegacyMeasureBehavior: Boolean = false,
        val renderBackend: RenderBackend = RenderBackend.DEFAULT,
        val snapDrawingOptions: SnapDrawingOptions = SnapDrawingOptions(),
        val jsThreadQoS: QoSClass = QoSClass.MAX,
        val isTestEnvironment: Boolean = false,
        val anrTimeoutMs: Int = 0,
        val enableKeychainEncryptorBypass: Boolean = false,
        /**
         * Whether to enable a Device specific workaround that disables hardware acceleration
         * on large views when the app goes to background. This is to workaround a crash that occurs
         * on some devices.
         */
         val enableHardwareLayerWorkaround: Boolean = false,
         val maxViewOperationsProcessingTimeMs: Int = 0,
         val disableHitTestSyncDeadline: Boolean = false,
         val enableTextAlignmentForRTL: Boolean = true,
         val updatePointScaleOnResume: Boolean = false,
         /**
          * When true, reverts [TouchDispatcher.hitTest] to the legacy behavior that builds the
          * screen-space rect from `getLocationOnScreen(0,0) + view.width/height` without mapping
          * all four corners through the parent-transform chain. Use as an escape hatch if the
          * transform-aware hit-test causes regressions.
          */
         val disableTransformAwareHitTest: Boolean = false,
         /**
          * When true, clearing the Valdi `selection` attribute does not move the caret (matches iOS).
          * When false, legacy behavior resets the caret to index 0.
          */
         val editTextResetSelectionMatchesIos: Boolean = false,
)
