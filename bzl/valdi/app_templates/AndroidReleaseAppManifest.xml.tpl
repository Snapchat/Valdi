<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:tools="http://schemas.android.com/tools"
    package="@VALDI_APP_PACKAGE@"
>
    <uses-sdk
        android:minSdkVersion="23"
        android:targetSdkVersion="34"
    />

    <application
        android:allowBackup="true"
        android:label="@VALDI_APP_NAME@"
        android:supportsRtl="true"
        android:theme="@style/Theme.AppCompat.Light.NoActionBar"
        @VALDI_APPLICATION_ATTRIBUTES@
    >
        <activity
            android:name="@VALDI_APP_PACKAGE@.StartActivity"
            android:screenOrientation="portrait"
            android:exported="true"
            @VALDI_ACTIVITY_ATTRIBUTES@
        >
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />

                <category android:name="android.intent.category.LAUNCHER" />
                <category android:name="android.intent.category.DEFAULT" />
            </intent-filter>
        </activity>
    </application>

    <uses-permission android:name="android.permission.WAKE_LOCK" />
    <uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.VIBRATE" />
</manifest>
