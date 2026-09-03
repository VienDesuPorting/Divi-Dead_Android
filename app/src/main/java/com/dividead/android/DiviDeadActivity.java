package com.dividead.android;

import org.libsdl.app.SDLActivity;
import android.view.View;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.media.MediaPlayer;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;
import java.io.IOException;

public class DiviDeadActivity extends SDLActivity {
    
    private MediaPlayer videoPlayer;
    private SurfaceView videoSurface;
    private boolean videoPlaying = false;
    private boolean videoSkipped = false;
    
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
            hideSystemUI();
        }
    }
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
    }
    
    private void hideSystemUI() {
        View decorView = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            );
        }
    }
    
    /**
     * Play a video file using Android MediaPlayer.
     * Called from native code via JNI.
     * 
     * @param path Full path to the video file
     * @param skipAllowed Whether the user can skip the video (1=yes, 0=no)
     * @return 1 if video played successfully, 0 if failed, 2 if skipped by user
     */
    public int playVideo(final String path, final int skipAllowed) {
        videoSkipped = false;
        videoPlaying = true;
        
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    // Create a SurfaceView for video
                    videoSurface = new SurfaceView(DiviDeadActivity.this);
                    videoSurface.setLayoutParams(new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
                    
                    // Add to the root view, on top of SDL surface
                    addContentView(videoSurface, new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
                    
                    videoSurface.getHolder().addCallback(new SurfaceHolder.Callback() {
                        @Override
                        public void surfaceCreated(SurfaceHolder holder) {
                            try {
                                videoPlayer = new MediaPlayer();
                                videoPlayer.setDataSource(path);
                                videoPlayer.setDisplay(holder);
                                videoPlayer.setLooping(false);
                                videoPlayer.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
                                    @Override
                                    public void onCompletion(MediaPlayer mp) {
                                        videoPlaying = false;
                                    }
                                });
                                videoPlayer.setOnErrorListener(new MediaPlayer.OnErrorListener() {
                                    @Override
                                    public boolean onError(MediaPlayer mp, int what, int extra) {
                                        videoPlaying = false;
                                        return true;
                                    }
                                });
                                videoPlayer.prepare();
                                videoPlayer.start();
                            } catch (Exception e) {
                                e.printStackTrace();
                                videoPlaying = false;
                            }
                        }
                        
                        @Override
                        public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {}
                        
                        @Override
                        public void surfaceDestroyed(SurfaceHolder holder) {
                            if (videoPlayer != null) {
                                videoPlayer.release();
                                videoPlayer = null;
                            }
                        }
                    });
                    
                    // Tap to skip
                    if (skipAllowed == 1) {
                        videoSurface.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                if (videoPlayer != null && videoPlayer.isPlaying()) {
                                    videoPlayer.stop();
                                }
                                videoSkipped = true;
                                videoPlaying = false;
                            }
                        });
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                    videoPlaying = false;
                }
            }
        });
        
        // Wait for video to finish (or be skipped)
        while (videoPlaying) {
            try {
                Thread.sleep(50);
            } catch (InterruptedException e) {
                break;
            }
        }
        
        // Cleanup: remove SurfaceView and release MediaPlayer
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (videoPlayer != null) {
                    videoPlayer.release();
                    videoPlayer = null;
                }
                if (videoSurface != null) {
                    // Remove from parent
                    if (videoSurface.getParent() != null) {
                        ((FrameLayout) videoSurface.getParent()).removeView(videoSurface);
                    }
                    videoSurface = null;
                }
            }
        });
        
        return videoSkipped ? 2 : 1;
    }
}
