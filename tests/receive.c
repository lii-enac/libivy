/* Exercise the actual protocol receiver without opening network sockets.
 * Including ivy.c gives this test access to its private receiver and registry.
 */
#include <assert.h>
/* Capture error replies without opening a socket. */
#define SocketSend test_socket_send
#include "../src/ivy.c"
#undef SocketSend

static char last_reply[2048];
static int reply_count;

SendState test_socket_send(Client client, const char *fmt, ...)
{
    va_list args;
    int length;
    (void)client;
    va_start(args, fmt);
    length = vsnprintf(last_reply, sizeof(last_reply), fmt, args);
    va_end(args);
    assert(length >= 0 && (size_t)length < sizeof(last_reply));
    ++reply_count;
    return SendOk;
}

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

static void check_invalid_regexp(size_t length)
{
    struct _clnt_lst_dict client = {0};
    MsgSndDictPtr entry = NULL;
    char *pattern = (char *)malloc(length + 1);
    const char *error_text;
    int type, offset;
    assert(pattern != NULL && length > 0);
    memset(pattern, 'a', length);
    pattern[length - 1] = '(';
    pattern[length] = '\0';
    client.app_name = "regex-error-test";
    client.id = 77;
    reply_count = 0;
    addRegexpToDictionary(pattern, &client);
    assert(reply_count == 1);
    assert(sscanf(last_reply, "%d %d", &type, &offset) == 2);
    assert(type == Error && offset == (int)length);
    error_text = strchr(last_reply, *ARG_START);
    assert(error_text != NULL);
    ++error_text;
    assert(strncmp(error_text, "Error compiling '", 17) == 0);
    /* Includes the final wire newline, but not the terminating NUL. */
    assert(strlen(error_text) <= 1024);
    assert(last_reply[strlen(last_reply) - 1] == '\n');
    if (length < 100) {
        assert(strstr(error_text, pattern) != NULL);
        assert(strstr(error_text, "missing closing parenthesis") != NULL);
    }
    HASH_FIND_STR(messSndByRegexp, pattern, entry);
    assert(entry != NULL);
    delOneClientFromDictionaryEntry(entry, client.client);
    assert(messSndByRegexp == NULL);
    free(pattern);
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
    puts("[TEST] Invalid regex error replies: compilation errors below are expected.");
    fflush(stdout);
    check_invalid_regexp(4);
    check_invalid_regexp(2001);
    puts("Ivy receive: short and oversized regex error replies passed");
    puts("Ivy receive: argument limits, empty captures and recovery passed");
    return 0;
}
