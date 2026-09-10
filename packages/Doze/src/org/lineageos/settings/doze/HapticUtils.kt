/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.doze

import android.content.Context
import android.os.SystemProperties
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.Log
import java.io.File
import java.io.FileWriter

object HapticUtils {
    private const val TAG = "HapticUtils"

    const val PROP_VENDOR = "persist.vendor.vibrator.touch_style"
    const val PROP_SYS = "persist.sys.vibrator.touch_style"
    const val PROC_NODE = "/proc/vibrator/touch_style"

    const val STYLE_CRISP = 0
    const val STYLE_GENTLE = 1

    fun isSupported(): Boolean {
        return File(PROC_NODE).exists() || File("/odm/etc/wave_lib").exists()
    }

    fun getTouchStyle(): Int {
        val vendorVal = SystemProperties.get(PROP_VENDOR, "")
        if (vendorVal.isNotEmpty()) {
            return vendorVal.toIntOrNull() ?: STYLE_CRISP
        }
        return SystemProperties.get(PROP_SYS, "0").toIntOrNull() ?: STYLE_CRISP
    }

    fun setTouchStyle(context: Context, style: Int) {
        val valStr = style.toString()
        try {
            SystemProperties.set(PROP_SYS, valStr)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to set $PROP_SYS", e)
        }
        try {
            SystemProperties.set(PROP_VENDOR, valStr)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to set $PROP_VENDOR", e)
        }

        try {
            FileWriter(PROC_NODE).use { it.write(valStr) }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to write $PROC_NODE", e)
        }

        // Live tactile feedback on selection
        val vibrator = context.getSystemService(Vibrator::class.java)
        vibrator?.let {
            if (it.hasVibrator()) {
                it.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_CLICK))
            }
        }
    }
}
