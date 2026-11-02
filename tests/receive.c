/* Exercise the actual protocol receiver without opening network sockets.
 * Including ivy.c gives this test access to its private receiver and registry.
 */
#include <assert.h>
#include "../src/ivy.c"

static int callback_count;
static int expected_argc;
static int expect_empty;

static void received(IvyClientPtr client, void *data, int argc, char **argv)
{
    int i;
    (void)client;
    (void)data;
    ++callback_count;
    assert(argc == expected_argc);
    for (i = 0; i < argc; ++i) {
        char expected[16];
        if (expect_empty && (i == 0 || i == argc / 2 || i == argc - 1))
            expected[0] = '\0';
        else
            snprintf(expected, sizeof(expected), "%d", i);
        assert(strcmp(argv[i], expected) == 0);
    }
}

static void receive_arguments(int count, int empty)
{
    struct _clnt_lst_dict client = {0};
    size_t capacity = (size_t)count * 16 + 32;
    char *line = (char *)malloc(capacity);
    char *p;
    int i;
    assert(line != NULL);
    strcpy(line, "2 42\002");
    p = line + strlen(line);
    for (i = 0; i < count; ++i) {
        if (!(empty && (i == 0 || i == count / 2 || i == count - 1)))
            p += snprintf(p, capacity - (size_t)(p - line), "%d", i);
        *p++ = '\003';
    }
    *p = '\0';
    expected_argc = count;
    expect_empty = empty;
    callback_count = 0;
    Receive(NULL, &client, line);
    assert(callback_count == (count <= MAX_MATCHING_ARGS ? 1 : 0));
    free(line);
}

int main(void)
{
    struct _msg_rcv receiver = {0};
    receiver.id = 42;
    receiver.callback = received;
    msg_recv = &receiver;
    receive_arguments(0, 0);
    receive_arguments(1, 0);
    receive_arguments(MAX_MATCHING_ARGS - 1, 0);
    receive_arguments(MAX_MATCHING_ARGS, 0);
    receive_arguments(MAX_MATCHING_ARGS, 1);
    puts("[TEST] Oversized messages: rejection diagnostics below are expected.");
    fflush(stdout);
    receive_arguments(MAX_MATCHING_ARGS + 1, 0);
    receive_arguments(1, 0); /* A rejected message must not break the next one. */
    receive_arguments(MAX_MATCHING_ARGS + 1, 1);
    receive_arguments(1, 0);
    receive_arguments(500, 0);
    receive_arguments(MAX_MATCHING_ARGS, 1);
    msg_recv = NULL;
    puts("Ivy receive: argument limits, empty captures and recovery passed");
    return 0;
}
