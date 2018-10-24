package org.godotengine.godot;

import android.app.Activity;

public class CrashThrow extends Godot.SingletonBase {
    protected Activity appActivity;

    public CrashThrow(Activity p_activity) {
        appActivity = p_activity;
        registerClass("CrashThrow", new String[] {
            "Throw"
        });
    }

    static public Godot.SingletonBase initialize(Activity activity) {
        return new CrashThrow(activity);
    }

    public void Throw(String p_message) throws Exception {
        throw new Exception(p_message);
    }
}
