/*
 * Android log redirection - sends stdout/stderr to logcat
 */
#ifdef __ANDROID__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>
#include <pthread.h>

static int pfd[2];
static pthread_t log_thread;
static const char *tag = "DiviDead";

static void *log_thread_func(void *arg) {
    char buf[1024];
    ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf) - 1)) > 0) {
                char *start = buf;
        char *end;
        buf[n] = '\0';
        while ((end = strchr(start, '\n')) != NULL) {
            *end = '\0';
            if (*start) {
                __android_log_write(ANDROID_LOG_INFO, tag, start);
            }
            start = end + 1;
        }
                if (*start) {
            __android_log_write(ANDROID_LOG_INFO, tag, start);
        }
    }
    return NULL;
}

void android_redirect_stdio(void) {
        if (pipe(pfd) < 0) return;
    
        dup2(pfd[1], 1);  /* stdout */
    dup2(pfd[1], 2);  /* stderr */
    
        setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    
        pthread_create(&log_thread, NULL, log_thread_func, NULL);
    pthread_detach(log_thread);
}

#endif /* __ANDROID__ */
