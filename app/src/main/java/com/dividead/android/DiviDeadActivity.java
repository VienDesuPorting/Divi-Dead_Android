package com.dividead.android;

import org.libsdl.app.SDLActivity;

/**
 * Divi-Dead main activity.
 * 
 * This extends SDLActivity which handles:
 * - Loading the native library (libdividead.so)
 * - Setting up the SDL2 video/audio/input subsystems
 * - Calling the native SDL_main() function
 * - Managing the Android lifecycle (pause/resume/destroy)
 * 
 * The native code is in app/jni/src/ and is built into libdividead.so
 * by CMakeLists.txt.
 * 
 * Touch gestures are handled natively in src/touch_input.c:
 * - Tap → Select/Confirm
 * - Swipe up → Open menu
 * - Swipe down → Gallery
 * - Swipe left/right → Navigate
 * - Long press → Cancel
 */
public class DiviDeadActivity extends SDLActivity {
    
    /**
     * This method is called by SDL before starting the native application.
     * Override to pass custom arguments to SDL_main().
     * 
     * For Divi-Dead, we pass the path to SG.DL1 so the engine knows
     * where to find game data files.
     * 
     * @return array of arguments to pass to SDL_main()
     */
    @Override
    protected String[] getArguments() {
        // The engine's main() expects argv[1] to be a path to a DL1 file.
        // SDL_GetBasePath() returns the app's assets directory on Android,
        // so we pass "SG.DL1" and the engine resolves it relative to assets/.
        return new String[]{"SG.DL1"};
    }
    
    /**
     * This method is called by SDL before loading the native library.
     * It should return the name of the library without the "lib" prefix
     * and ".so" suffix.
     * 
     * @return the native library name
     */
    @Override
    protected String getMainLibraryName() {
        return "dividead";
    }
    
    /**
     * This method is called by SDL before starting the native application.
     * Override to perform any initialization before the native code runs.
     */
    @Override
    protected void beforeStartNative() {
        // Ensure the native library is loaded
        try {
            System.loadLibrary("dividead");
        } catch (UnsatisfiedLinkError e) {
            // SDLActivity will handle this error
        }
    }
}
