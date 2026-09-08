package su.viende.dividead;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.graphics.Color;
import android.net.Uri;
import android.view.Gravity;

public class SplashActivity extends Activity {

    private static final int SPLASH_DELAY_MS = 3000;
    
    // Change this to "en" for English Telegram channel
    private static final String LINK_VARIANT = "ru";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
        } else {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                                 WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }

        RelativeLayout root = new RelativeLayout(this);
        root.setBackgroundColor(Color.BLACK);

        float density = getResources().getDisplayMetrics().density;
        int screenWidth = getResources().getDisplayMetrics().widthPixels;
        int screenHeight = getResources().getDisplayMetrics().heightPixels;

        // --- Logo (centered, max 60% width / 50% height) ---
        int logoId = getResources().getIdentifier("splash_logo", "drawable", getPackageName());
        ImageView logo = new ImageView(this);
        if (logoId != 0) {
            logo.setImageResource(logoId);
        }
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        logo.setAdjustViewBounds(true);

        int maxLogoW = (int)(screenWidth * 0.6);
        int maxLogoH = (int)(screenHeight * 0.5);
        RelativeLayout.LayoutParams logoParams = new RelativeLayout.LayoutParams(maxLogoW, maxLogoH);
        logoParams.addRule(RelativeLayout.CENTER_IN_PARENT);
        logo.setLayoutParams(logoParams);
        root.addView(logo);

        // --- Social buttons ---
        LinearLayout buttonBar = new LinearLayout(this);
        buttonBar.setOrientation(LinearLayout.HORIZONTAL);
        buttonBar.setGravity(Gravity.CENTER);

        int btnSize = (int)(38 * density);
        int btnSpacing = (int)(20 * density);
        LinearLayout.LayoutParams btnParams = new LinearLayout.LayoutParams(btnSize, btnSize);
        btnParams.setMargins(btnSpacing, 0, btnSpacing, 0);

        // Telegram link depends on LINK_VARIANT
        String tgLink = "ru".equals(LINK_VARIANT) 
            ? "https://t.me/visual_novels_for_android"
            : "https://t.me/visual_novels_android_eng";

        addButton(buttonBar, "btn_yt", "https://www.youtube.com/@viendesu", btnParams);
        addButton(buttonBar, "btn_tg", tgLink, btnParams);
        addButton(buttonBar, "btn_vk", "https://vk.com/viendesu", btnParams);
        addButton(buttonBar, "btn_web", "https://viende.su", btnParams);

        RelativeLayout.LayoutParams barParams = new RelativeLayout.LayoutParams(
            RelativeLayout.LayoutParams.WRAP_CONTENT,
            RelativeLayout.LayoutParams.WRAP_CONTENT);
        barParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
        barParams.addRule(RelativeLayout.CENTER_HORIZONTAL);
        barParams.bottomMargin = (int)(40 * density);
        buttonBar.setLayoutParams(barParams);
        root.addView(buttonBar);

        setContentView(root);
        hideSystemUI();

        new Handler().postDelayed(new Runnable() {
            @Override
            public void run() {
                Intent intent = new Intent(SplashActivity.this, DiviDeadActivity.class);
                startActivity(intent);
                finish();
                overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out);
            }
        }, SPLASH_DELAY_MS);
    }

    private void addButton(LinearLayout parent, String imageName, String url, LinearLayout.LayoutParams params) {
        int id = getResources().getIdentifier(imageName, "drawable", getPackageName());
        if (id == 0) return;

        ImageView btn = new ImageView(this);
        btn.setImageResource(id);
        btn.setScaleType(ImageView.ScaleType.FIT_CENTER);
        btn.setOnClickListener(v -> {
            Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            startActivity(browserIntent);
        });
        parent.addView(btn, params);
    }

    @SuppressWarnings("deprecation")
    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    protected void onResume() {
        super.onResume();
        hideSystemUI();
    }
}
