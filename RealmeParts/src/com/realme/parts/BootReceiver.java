/*
 * Copyright (C) 2018 The Asus-SDM660 Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

package com.realme.parts;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;
import android.content.SharedPreferences;
import android.os.SELinux;
import android.util.Log;
import android.widget.Toast;

import com.realme.parts.R;

import com.realme.parts.preferences.VibratorStrengthPreference;
import com.realme.parts.kcal.DisplayCalibration;
import com.realme.parts.doze.DozeUtils;

import java.io.IOException;
import java.util.List;

public class BootReceiver extends BroadcastReceiver implements Utils {

    private final String VIBRATION_STRENGTH_PATH = "/sys/class/leds/vibrator/vmax_mv";
    private final String HEADPHONE_GAIN_PATH = "/sys/kernel/sound_control/headphone_gain";
    private final String MICROPHONE_GAIN_PATH = "/sys/kernel/sound_control/mic_gain";

    private static final String PREF_SELINUX_MODE = "selinux_mode";

    private Context settingsContext = null;
    private static final String TAG = "SettingsOnBoot";
    private boolean mSetupRunning = false;
    private Context mContext;

    public void onReceive(Context context, Intent intent) {

        mContext = context;
    ActivityManager activityManager =
            (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
    List<ActivityManager.RunningAppProcessInfo> procInfos =
            activityManager.getRunningAppProcesses();
    for(int i = 0; i < procInfos.size(); i++) {
        if(procInfos.get(i).processName.equals("com.google.android.setupwizard")) {
            mSetupRunning = true;
        }
    }

    if(!mSetupRunning) {
        try {
            settingsContext = context.createPackageContext("com.android.settings", 0);
        } catch (Exception e) {
            Log.e(TAG, "Package not found", e);
        }
        SharedPreferences sharedpreferences = context.getSharedPreferences("selinux_pref",
                Context.MODE_PRIVATE);
        if (sharedpreferences.contains(PREF_SELINUX_MODE)) {
            boolean currentIsSelinuxEnforcing = SELinux.isSELinuxEnforced();
            boolean isSelinuxEnforcing =
                    sharedpreferences.getBoolean(PREF_SELINUX_MODE,
                            currentIsSelinuxEnforcing);
            if (isSelinuxEnforcing) {
                if (!currentIsSelinuxEnforcing) {
                    try {
                        SuShell.runWithSuCheck("setenforce 1");
                        showToast(context.getString(R.string.selinux_enforcing_toast_title),
                                context);
                    } catch (SuShell.SuDeniedException e) {
                        showToast(context.getString(R.string.cannot_get_su), context);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            } else {
                if (currentIsSelinuxEnforcing) {
                    try {
                        SuShell.runWithSuCheck("setenforce 0");
                        showToast(context.getString(R.string.selinux_permissive_toast_title),
                                context);
                    } catch (SuShell.SuDeniedException e) {
                        showToast(context.getString(R.string.cannot_get_su), context);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        }
    }

        int gain = Settings.Secure.getInt(context.getContentResolver(),
                DeviceSettings.PREF_HEADPHONE_GAIN, 4);
        FileUtils.setValue(HEADPHONE_GAIN_PATH, gain + " " + gain);
        FileUtils.setValue(MICROPHONE_GAIN_PATH, Settings.Secure.getInt(context.getContentResolver(),
                DeviceSettings.PREF_MICROPHONE_GAIN, 0));
        FileUtils.setValue(DeviceSettings.EARPIECE_GAIN_PATH, Settings.Secure.getInt(context.getContentResolver(),
                DeviceSettings.PREF_EARPIECE_GAIN, 0));
        // Dirac
        context.startService(new Intent(context, DiracService.class));

        // KCAL
        context.startService(new Intent(context, DisplayCalibration.class));
    }

        private void showToast(String toastString, Context context) {
    Toast.makeText(context, toastString, Toast.LENGTH_SHORT)
            .show();
    }
}
