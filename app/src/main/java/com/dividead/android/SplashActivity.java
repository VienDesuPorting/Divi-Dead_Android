package com.dividead.android;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.graphics.Color;
import android.net.Uri;
import android.content.Context;
import android.view.Gravity;

/**
 * Splash screen activity.
 * 
 * Shows a logo image centered on screen with social media buttons at the bottom.
 * Automatically transitions to DiviDeadActivity after a delay.
 * 
 * Images needed in res/drawable/:
 *   - splash_logo.png (main logo, centered)
 *   - btn_yt.png (YouTube button)
 *   - btn_tg.png (Telegram button)
 *   - btn_vk.png (VK button)
 *   - btn_web.png (Website button)
 */
public class SplashActivity extends Activity {

    private static final int SPLASH_DELAY_MS = 3000;  // 3 seconds
    private static final String PREFS_NAME = "dividead";
    private static final String PREF_FIRST_RUN = "first_run";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Fullscreen, no title
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                             WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // Create layout programmatically (no XML needed)
        RelativeLayout root = new RelativeLayout(this);
        root.setBackgroundColor(Color.BLACK);

        // --- Logo image (centered) ---
        int logoId = getResources().getIdentifier("splash_logo", "drawable", getPackageName());
        ImageView logo = new ImageView(this);
        if (logoId != 0) {
            logo.setImageResource(logoId);
        }
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        
        RelativeLayout.LayoutParams logoParams = new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.WRAP_CONTENT,
            RelativeLayout.LayoutParams.WRAP_CONTENT);
        logoParams.addRule(RelativeLayout.CENTER_IN_PARENT);
        logo.setLayoutParams(logoParams);
        root.addView(logo);

        // --- Social buttons (bottom center) ---
        LinearLayout buttonBar = new LinearLayout(this);
        buttonBar.setOrientation(LinearLayout.HORIZONTAL);
        buttonBar.setGravity(Gravity.CENTER);
        
        int btnSize = dpToPx(48);
        int btnSpacing = dpToPx(24);
        LinearLayout.LayoutParams btnParams = new LinearLayout.LayoutParams(btnSize, btnSize);
        btnParams.setMargins(btnSpacing, 0, btnSpacing, 0);

        // Add social buttons (only if resources exist)
        addButton(buttonBar, "btn_yt", "https://www.youtube.com/@viendesu", btnParams);
        addButton(buttonBar, "btn_tg", "https://t.me/pufkein", btnParams);
        addButton(buttonBar, "btn_vk", "https://vk.com/viendesu", btnParams);
        addButton(buttonBar, "btn_web", "https://viende.su", btnParams);

        RelativeLayout.LayoutParams barParams = new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.WRAP_CONTENT,
            RelativeLayout.LayoutParams.WRAP_CONTENT);
        barParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        barParams.addRule(RelativeLayout.CENTER_HORIZONTAL);
        barParams.bottomMargin = dpToPx(48);
        buttonBar.setLayoutParams(barParams);
        root.addView(buttonBar);

        setContentView(root);

        // Hide system UI
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

        // Transition to game after delay
        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(SplashActivity.this, DiviDeadActivity.class);
                startActivity(intent);
                finish();
                
                // Custom fade animation
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            }
        }, SPLASH_DELAY_MS);
    }

    private void addButton(LinearLayout parent, String imageName, String url, LinearLayout.LayoutParams params) {
        int id = getResources().getIdentifier(imageName, "drawable", getPackageName());
        if (id == 0) return;  // Image not found, skip
        
        ImageView btn = new ImageView(this);
        btn.setImageResource(id);
        btn.setScaleType(ImageView.ScaleType.FIT_CENTER);
        btn.setOnClickListener(v -> {
            Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            startActivity(browserIntent);
        });
        parent.addView(btn, params);
    }

    private int dpToPx(int dp) {
        float density = getResources().getDisplayMetrics().density;
        return (int) (dp * density + 0.5f);
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Re-hide system UI
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
}
