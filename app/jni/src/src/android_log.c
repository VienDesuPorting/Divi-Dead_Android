/*
 * Android log redirection - sends stdout/stderr to logcat
 */
#ifdef __ANDROID__

#include <stdio.h>
#include <unistd.h>
#include <android/log.h>
#include <pthread.h>

static int pfd[2];
static pthread_t log_thread;
static const char *tag = "DiviDead";

static void *log_thread_func(void *arg) {
    char buf[512];
    ssize_t n;
    while ((n = read(pfd[0], buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        /* Remove trailing newline */
        if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
        __android_log_write(ANDROID_LOG_INFO, tag, buf);
    }
    return NULL;
}

void android_redirect_stdio(void) {
    /* Create a pipe */
    if (pipe(pfd) < 0) return;
    
    /* Redirect stdout and stderr to the pipe */
    dup2(pfd[1], 1);  /* stdout */
    dup2(pfd[1], 2);  /* stderr */
    
    /* Start a thread to read from the pipe and write to logcat */
    pthread_create(&log_thread, NULL, log_thread_func, NULL);
    pthread_detach(log_thread);
}

#endif /* __ANDROID__ */
