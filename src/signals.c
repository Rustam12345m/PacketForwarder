#include "app.h"

#include <signal.h>

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

int init_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        fprintf(stderr, "sigaction(SIGINT)\n");
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        fprintf(stderr, "sigaction(SIGTERM)\n");
        return -2;
    }
    return 0;
}

