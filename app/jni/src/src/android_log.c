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
        /* Process line by line so each gets its own logcat entry */
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
        /* Handle last partial line (no trailing \n) */
        if (*start) {
            __android_log_write(ANDROID_LOG_INFO, tag, start);
        }
    }
    return NULL;
}

void android_redirect_stdio(void) {
    /* Create a pipe */
    if (pipe(pfd) < 0) return;
    
    /* Redirect stdout and stderr to the pipe */
    dup2(pfd[1], 1);  /* stdout */
    dup2(pfd[1], 2);  /* stderr */
    
    /* Set stdout to line-buffered so each \n flushes to the pipe.
     * Without this, stdio buffers output (4KB) and logcat doesn't
     * see messages until the buffer fills up. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    
    /* Start a thread to read from the pipe and write to logcat */
    pthread_create(&log_thread, NULL, log_thread_func, NULL);
    pthread_detach(log_thread);
}

#endif /* __ANDROID__ */
