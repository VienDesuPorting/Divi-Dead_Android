package com.dividead.android;

import org.libsdl.app.SDLActivity;

/**
 * Divi-Dead main activity.
 * 
 * Extends SDLActivity which handles:
 * - Loading native libraries (SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, dividead)
 * - Setting up SDL2 video/audio/input
 * - Calling the native SDL_main() function
 * - Android lifecycle management
 * 
 * Touch gestures are handled natively in src/touch_input.c.
 */
public class DiviDeadActivity extends SDLActivity {
    
    /**
     * Returns the list of native libraries to load.
     * Order matters: SDL2 first, then its extensions, then our engine.
     */
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
    
    /**
     * Returns arguments to pass to the native SDL_main().
     * The engine expects argv[1] to be a path to a DL1 file.
     */
    @Override
    protected String[] getArguments() {
        return new String[]{"SG.DL1"};
    }
}
