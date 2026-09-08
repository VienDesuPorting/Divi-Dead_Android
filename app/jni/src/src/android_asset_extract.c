/*
 * android_asset_extract.c - Extract assets to internal storage on first launch.
 *
 * On Android, reading from assets/ via SDL_RWFromFile works but is slow
 * for large PAK files (SG.DL1=112MB, WV.DL1=315MB) because each read
 * involves JNI calls to the Android AssetManager.
 *
 * This module extracts ALL files from assets/ to the app's internal
 * storage (filesDir) on first launch. Subsequent reads use the
 * filesystem directly, which is much faster.
 *
 * The extraction happens during the splash screen display.
 */

#ifdef __ANDROID__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <SDL2/SDL.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#define TAG "DiviDead"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* Maximum file size to extract (512MB for WV.DL1) */
#define MAX_EXTRACT_SIZE (512 * 1024 * 1024)

/* Get the app's internal files directory */
static char internal_dir[512] = {0};

static const char *get_internal_dir(void) {
    if (internal_dir[0]) return internal_dir;
    
    /* Use SDL_GetPrefPath which returns the internal storage path */
    char *pref = SDL_GetPrefPath("su.viende.dividead", "dividead");
    if (pref) {
        strncpy(internal_dir, pref, sizeof(internal_dir) - 1);
        SDL_free(pref);
        /* Remove trailing slash */
        int len = strlen(internal_dir);
        if (len > 0 && internal_dir[len-1] == '/') internal_dir[len-1] = 0;
        return internal_dir;
    }
    
    /* Fallback: use /data/data/su.viende.dividead/files */
    strcpy(internal_dir, "/data/data/su.viende.dividead/files");
    return internal_dir;
}

/* Get the Android AssetManager via JNI */
static AAssetManager *get_asset_manager(void) {
    /* SDL2 stores the AssetManager; we can access it via JNI */
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (!env) return NULL;
    
    /* Get SDLActivity */
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) return NULL;
    
    jclass cls = (*env)->GetObjectClass(env, activity);
    
    /* Call getAssets() on the activity */
    jmethodID mid_get_assets = (*env)->GetMethodID(env, cls, "getAssets", "()Landroid/content/res/AssetManager;");
    if (!mid_get_assets) {
        (*env)->DeleteLocalRef(env, cls);
        (*env)->DeleteLocalRef(env, activity);
        return NULL;
    }
    
    jobject java_asset_manager = (*env)->CallObjectMethod(env, activity, mid_get_assets);
    if (!java_asset_manager) {
        (*env)->DeleteLocalRef(env, cls);
        (*env)->DeleteLocalRef(env, activity);
        return NULL;
    }
    
    AAssetManager *mgr = AAssetManager_fromJava(env, java_asset_manager);
    
    (*env)->DeleteLocalRef(env, java_asset_manager);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
    
    return mgr;
}

/* Extract a single file from assets to internal storage.
 * Returns 0 on success, -1 on failure. */
static int extract_file(AAssetManager *mgr, const char *asset_path, const char *dest_path) {
    AAsset *asset = AAssetManager_open(mgr, asset_path, AASSET_MODE_STREAMING);
    if (!asset) {
        LOGE("extract: can't open asset '%s'", asset_path);
        return -1;
    }
    
    off_t size = AAsset_getLength(asset);
    if (size <= 0) {
        AAsset_close(asset);
        LOGE("extract: empty asset '%s'", asset_path);
        return -1;
    }
    
    LOGI("extract: '%s' -> '%s' (%ld bytes)", asset_path, dest_path, (long)size);
    
    /* Create parent directory if needed */
    char dir[512];
    strncpy(dir, dest_path, sizeof(dir) - 1);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        mkdir(dir, 0755);
    }
    
    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        AAsset_close(asset);
        LOGE("extract: can't create '%s'", dest_path);
        return -1;
    }
    
    char buf[65536];
    int total = 0;
    int n;
    while ((n = AAsset_read(asset, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, n, f);
        total += n;
    }
    
    fclose(f);
    AAsset_close(asset);
    
    LOGI("extract: done '%s' (%d bytes)", dest_path, total);
    return (total > 0) ? 0 : -1;
}

/* Check if a file exists in internal storage */
static int file_exists_internal(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

/* List all files in assets/ recursively and extract them.
 * For now, we know the file list, so we extract them explicitly. */
static int extract_all_assets(void) {
    const char *dir = get_internal_dir();
    AAssetManager *mgr = get_asset_manager();
    if (!mgr) {
        LOGE("extract_all: can't get AssetManager");
        return -1;
    }
    
    LOGI("extract_all: internal dir = %s", dir);
    
    /* Create subdirectories */
    char path[1024];
    snprintf(path, sizeof(path), "%s/LANG", dir);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/OGG", dir);
    mkdir(path, 0755);
    
    /* List of files to extract */
    const char *files[] = {
        "SG.DL1", "WV.DL1", "CS_ROGO.MPG", "OPEN.AVI", "CLICK.WAV", "ICMP.DAT",
        "LANG/ENGLISH.TXT",
        "OGG/OPENING.MID.OGG",
        "OGG/BGM_1.MID.OGG",
        "OGG/BGM_2.MID.OGG",
        "OGG/BGM_3.MID.OGG",
        "OGG/BGM_4.MID.OGG",
        "OGG/BGM_5.MID.OGG",
        "OGG/BGM_6.MID.OGG",
        "OGG/BGM_7.MID.OGG",
        "OGG/BGM_8.MID.OGG",
        "OGG/BGM_9.MID.OGG",
        "OGG/BGM_124.MID.OGG",
        "OGG/INSIDE.MID.OGG",
        "OGG/OUTSIDE.MID.OGG",
        NULL
    };
    
    int ok = 0, fail = 0;
    for (int i = 0; files[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        
        /* Skip if already extracted */
        if (file_exists_internal(path)) {
            LOGI("extract_all: '%s' already exists, skipping", files[i]);
            ok++;
            continue;
        }
        
        if (extract_file(mgr, files[i], path) == 0) {
            ok++;
        } else {
            fail++;
            /* Non-critical files (music, video) can be missing */
        }
    }
    
    LOGI("Assets: %d files OK", ok);
    
    /* Write a marker file to indicate extraction is complete */
    snprintf(path, sizeof(path), "%s/.extracted", dir);
    FILE *marker = fopen(path, "w");
    if (marker) {
        fprintf(marker, "1\n");
        fclose(marker);
    }
    
    return (fail > 0) ? -1 : 0;
}

/* Check if assets have been extracted */
int android_assets_extracted(void) {
    const char *dir = get_internal_dir();
    char path[1024];
    snprintf(path, sizeof(path), "%s/.extracted", dir);
    return file_exists_internal(path);
}

/* Extract all assets to internal storage.
 * Called during splash screen display.
 * Returns 0 on success. */
int android_extract_assets(void) {
    /* Don't skip even if .extracted exists - check each file individually.
     * This ensures new files (like OPEN.AVI) are extracted even after
     * the first extraction was done. */
    return extract_all_assets();
}

/* Get the internal storage path for a game data file.
 * Returns the full path, or NULL if not available. */
const char *android_get_data_path(const char *filename) {
    static char path[1024];
    const char *dir = get_internal_dir();
    snprintf(path, sizeof(path), "%s/%s", dir, filename);
    
    if (file_exists_internal(path)) {
        return path;
    }
    return NULL;
}

#endif /* __ANDROID__ */
