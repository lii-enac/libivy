/* Two-process smoke test of captures and the Ivy wire protocol. */
#include <stdio.h>
#include <string.h>
#include "ivy.h"
#include "ivyloop.h"
#include "timer.h"

static int result = 1;

static void timeout_cb(TimerId timer, void *data, unsigned long delta)
{
    (void)timer; (void)data; (void)delta;
    fprintf(stderr, "Ivy bus test timed out\n");
    IvyStop();
}

static void ready_cb(IvyClientPtr app, void *data, int argc, char **argv)
{
    (void)app; (void)data; (void)argc; (void)argv;
    IvySendMsg("PCRE2 uav 42");
}

static void ack_cb(IvyClientPtr app, void *data, int argc, char **argv)
{
    (void)app; (void)data; (void)argc; (void)argv;
    result = 0;
    IvyStop();
}

static void message_cb(IvyClientPtr app, void *data, int argc, char **argv)
{
    (void)app; (void)data;
    if (argc != 3 || strcmp(argv[0], "uav") || strcmp(argv[1], "") ||
        strcmp(argv[2], "42")) {
        fprintf(stderr, "Unexpected Ivy captures (argc=%d)\n", argc);
    } else {
        result = 0;
        IvySendMsg("PCRE2 ack");
    }
    IvyStop();
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    if (!strcmp(argv[1], "receiver")) {
        IvyInit("pcre2-receiver", "PCRE2 receiver ready", NULL, NULL, NULL, NULL);
        if (!IvyBindMsg(message_cb, NULL, "^PCRE2 (uav)(?: (optional))? ([0-9]+)$"))
            return 2;
    } else {
        IvyInit("pcre2-sender", "PCRE2 sender ready", NULL, NULL, NULL, NULL);
        IvyBindMsg(ready_cb, NULL, "^PCRE2 receiver ready$");
        IvyBindMsg(ack_cb, NULL, "^PCRE2 ack$");
    }
    IvyStart(argv[2]);
    TimerRepeatAfter(1, 5000, timeout_cb, NULL);
    IvyMainLoop();
    return result;
}
