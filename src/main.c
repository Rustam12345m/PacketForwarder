#include "app.h"

volatile uint8_t g_quit = 0;

int main(int argc, char **argv)
{
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        fprintf(stderr, "EAL init failed\n");
        return 1;
    }
    argc -= ret;
    argv += ret;

    struct application app;
    if (application_parse_args(argc, argv, &app) != 0) {
        fprintf(stderr, "bad app args\n");
        return 1;
    }
    if (application_init(&app) != 0) {
        fprintf(stderr, "app init failed\n");
        return 2;
    }
    if (init_signals() != 0) {
        fprintf(stderr, "signals init failed\n");
        return 3;
    }

    switch (app.cfg.mode) {
    case MODE_SIMPLE: {
        if (simple_pipeline_init(&app) != 0) {
            fprintf(stderr, "simple init failed\n");
            return 4;
        }

        printf("Run simple pipeline mode\n");
        run_simple_pipeline(&app);
        break;
    }
    case MODE_MULTICORE: {
        if (multicore_mode_init(&app) != 0) {
            fprintf(stderr, "multicore init failed\n");
            return 5;
        }

        printf("Run multicore pipeline mode\n");
        run_multicore_mode(&app);
        break;
    }
    default: {
        fprintf(stderr, "unknown mode\n");
        return 6;
    }
    }

    application_cleanup(&app);
    return 0;
}

