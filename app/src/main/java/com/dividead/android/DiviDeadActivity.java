package com.dividead.android;

import org.libsdl.app.SDLActivity;
import android.view.View;
import android.os.Build;

public class DiviDeadActivity extends SDLActivity {
    
    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "SDL2_ttf",
            "dividead"
        };
    }
    
    @Override
    protected String[] getArguments() {
        return new String[]{"SG.DL1"};
    }
    
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            /* Enable immersive mode: hide status bar and navigation bar */
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            );
        }
    }
}
