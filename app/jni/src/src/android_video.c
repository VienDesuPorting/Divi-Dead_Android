/* Video playback via Android MediaPlayer (JNI) */

#ifdef __ANDROID__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <android/log.h>
#include <jni.h>

#define TAG "DiviDead"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

int android_play_video(const char *path, int skip) {
    LOGI("VIDEO: playing '%s' (skip=%d)", path, skip);
    
    if (!path || !*path) {
        LOGE("VIDEO: null path");
        return 0;
    }
    
    /* Check if file exists */
    if (access(path, F_OK) != 0) {
        LOGI("VIDEO: file not found '%s'", path);
        return 0;
    }
    
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    if (!env) {
        LOGE("VIDEO: can't get JNIEnv");
        return 0;
    }
    
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) {
        LOGE("VIDEO: can't get Activity");
        return 0;
    }
    
    jclass cls = (*env)->GetObjectClass(env, activity);
    
    /* Create Java string for the path */
    jstring jpath = (*env)->NewStringUTF(env, path);
    if (!jpath) {
        LOGE("VIDEO: can't create path string");
        (*env)->DeleteLocalRef(env, cls);
        (*env)->DeleteLocalRef(env, activity);
        return 0;
    }
    
    /* Call playVideo(String path, int skipAllowed) */
    jmethodID mid = (*env)->GetMethodID(env, cls, "playVideo", "(Ljava/lang/String;I)I");
    if (!mid) {
        LOGE("VIDEO: can't find playVideo method");
        (*env)->DeleteLocalRef(env, jpath);
        (*env)->DeleteLocalRef(env, cls);
        (*env)->DeleteLocalRef(env, activity);
        return 0;
    }
    
    jobject globalActivity = (*env)->NewGlobalRef(env, activity);
    jint result = (*env)->CallIntMethod(env, globalActivity, mid, jpath, skip);
    
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        LOGE("VIDEO: Java exception");
        result = 0;
    }
    
    LOGI("VIDEO: result = %d (1=played, 2=skipped, 0=failed)", result);
    
    (*env)->DeleteGlobalRef(env, globalActivity);
    (*env)->DeleteLocalRef(env, jpath);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
    
    return (result > 0) ? 1 : 0;
}

#endif /* __ANDROID__ */
