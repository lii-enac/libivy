/* Deterministic send() results exercise congestion without network sockets. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#define WOULD_BLOCK WSAEWOULDBLOCK
#define INTERRUPTED WSAEINTR
#define BROKEN_CONNECTION WSAECONNRESET
static int fake_send(SOCKET fd, const char *buf, int size, int flags);
#else
#include <sys/types.h>
#include <sys/socket.h>
#define WOULD_BLOCK EWOULDBLOCK
#define INTERRUPTED EINTR
#define BROKEN_CONNECTION EPIPE
static ssize_t fake_send(int fd, const void *buf, size_t size, int flags);
#endif
#define send fake_send
#include "../src/ivyfifo.c"
#undef send

/* Also exercise the real writable-event handler and deferred removal. */
#include "../src/ivysocket.c"
#include "../src/ivyloop.c"

struct send_step { int result; int error; };
static const struct send_step *steps;
static size_t step_count, step_index;
static char sent[64];
static size_t sent_size;

#ifdef WIN32
static int fake_send(SOCKET fd, const char *buf, int size, int flags)
#else
static ssize_t fake_send(int fd, const void *buf, size_t size, int flags)
#endif
{
    struct send_step step;
    (void)fd; (void)flags;
    assert(step_index < step_count);
    step = steps[step_index++];
    if (step.result < 0) {
#ifdef WIN32
        WSASetLastError(step.error);
#else
        errno = step.error;
#endif
        return -1;
    }
    assert((size_t)step.result <= (size_t)size);
    assert(sent_size + (size_t)step.result <= sizeof(sent));
    memcpy(sent + sent_size, buf, (size_t)step.result);
    sent_size += (size_t)step.result;
    return step.result;
}

static unsigned int flush(IvyFifoBuffer *fifo, const struct send_step *script, size_t count)
{
    unsigned int remaining;
    steps = script;
    step_count = count;
    step_index = 0;
    remaining = IvyFifoSendSocket(fifo, -1);
    assert(step_index == step_count);
    return remaining;
}

static void begin(IvyFifoBuffer *fifo)
{
    assert(IvyFifoInit(fifo) == 0);
    sent_size = 0;
    IvyFifoWrite(fifo, "ABCDE", 5);
}

static int decongested;
static void on_decongestion(Client client, const void *data)
{
    (void)client; (void)data;
    ++decongested;
}

static void check_writable_handler(void)
{
    struct _client client = {0};
    const struct send_step blocked[] = {{-1, WOULD_BLOCK}};
    const struct send_step all[] = {{5, 0}};
    const struct send_step fatal[] = {{-1, BROKEN_CONNECTION}};
    client.channel = IvyChannelAdd(0, NULL, NULL, NULL, NULL);
    client.ifb = IvyFifoNew();
    client.handle_decongestion = on_decongestion;
    IvyFifoWrite(client.ifb, "ABCDE", 5);
    IvyChannelAddWritableEvent(client.channel);
    sent_size = 0;
    decongested = 0;
    steps = blocked; step_count = 1; step_index = 0;
    HandleCongestionWrite(client.channel, 0, &client);
    assert(step_index == 1 && !client.channel->tobedeleted);
    assert(client.ifb != NULL && IvyFifoLength(client.ifb) == 5);
    assert(decongested == 0);
    steps = all; step_count = 1; step_index = 0;
    HandleCongestionWrite(client.channel, 0, &client);
    assert(step_index == 1 && !client.channel->tobedeleted);
    assert(client.ifb == NULL && decongested == 1);
    assert(!FD_ISSET(0, &wrdy_fds));
    assert(sent_size == 5 && memcmp(sent, "ABCDE", 5) == 0);
    IvyChannelRemove(client.channel);
    ChannelDefferedDelete();

    client.channel = IvyChannelAdd(0, NULL, NULL, NULL, NULL);
    client.ifb = IvyFifoNew();
    IvyFifoWrite(client.ifb, "ABCDE", 5);
    IvyChannelAddWritableEvent(client.channel);
    decongested = 0;
    steps = fatal; step_count = 1; step_index = 0;
    HandleCongestionWrite(client.channel, 0, &client);
    assert(step_index == 1 && client.channel->tobedeleted);
    assert(decongested == 0 && IvyFifoLength(client.ifb) == 5);
    IvyFifoDelete(client.ifb);
    ChannelDefferedDelete();
}

int main(void)
{
    IvyFifoBuffer fifo;
    const struct send_step blocked[] = {{-1, WOULD_BLOCK}};
    const struct send_step all[] = {{5, 0}};
    const struct send_step partial[] = {{2, 0}};
    const struct send_step rest[] = {{3, 0}};
    const struct send_step interrupted[] = {{-1, INTERRUPTED}, {5, 0}};
    const struct send_step fatal[] = {{-1, BROKEN_CONNECTION}};
    const struct send_step zero[] = {{0, 0}};
    char *original;

    begin(&fifo);
    original = fifo.rptr;
    assert(flush(&fifo, blocked, 1) == 5);
    assert(fifo.rptr == original && IvyFifoLength(&fifo) == 5);
    assert(flush(&fifo, all, 1) == 0);
    assert(sent_size == 5 && memcmp(sent, "ABCDE", 5) == 0);
    assert(flush(&fifo, NULL, 0) == 0); /* Empty FIFO: do not call send. */
    IvyFifoFree(&fifo);
#ifndef WIN32
    {
        const struct send_step again[] = {{-1, EAGAIN}};
        begin(&fifo);
        original = fifo.rptr;
        assert(flush(&fifo, again, 1) == 5 && fifo.rptr == original);
        IvyFifoFree(&fifo);
    }
#endif
    begin(&fifo);
    assert(flush(&fifo, interrupted, 2) == 0);
    assert(sent_size == 5 && memcmp(sent, "ABCDE", 5) == 0);
    IvyFifoFree(&fifo);

    begin(&fifo);
    assert(flush(&fifo, partial, 1) == 3);
    original = fifo.rptr;
    assert(flush(&fifo, blocked, 1) == 3 && fifo.rptr == original);
    assert(flush(&fifo, rest, 1) == 0);
    assert(sent_size == 5 && memcmp(sent, "ABCDE", 5) == 0);
    IvyFifoFree(&fifo);

    begin(&fifo);
    original = fifo.rptr;
    assert(flush(&fifo, fatal, 1) == IVY_FIFO_SEND_ERROR);
    assert(fifo.rptr == original && IvyFifoLength(&fifo) == 5);
    assert(flush(&fifo, zero, 1) == IVY_FIFO_SEND_ERROR);
    assert(fifo.rptr == original && IvyFifoLength(&fifo) == 5);
    IvyFifoFree(&fifo);

    begin(&fifo);
    assert(flush(&fifo, partial, 1) == 3);
    original = fifo.rptr;
    assert(flush(&fifo, fatal, 1) == IVY_FIFO_SEND_ERROR);
    assert(fifo.rptr == original && IvyFifoLength(&fifo) == 3);
    IvyFifoFree(&fifo);

    {
        /* Force the pending data to cross the ring buffer's physical end. */
        const struct send_step wrap_blocked[] = {{2, 0}, {-1, WOULD_BLOCK}};
        unsigned int padding_size;
        char *padding;
        assert(IvyFifoInit(&fifo) == 0);
        padding_size = IvyFifoSize(&fifo) - 2;
        padding = (char *)calloc(padding_size, 1);
        assert(padding != NULL);
        IvyFifoWrite(&fifo, padding, padding_size);
        assert(IvyFifoRead(&fifo, padding, padding_size) == padding_size);
        free(padding);
        IvyFifoWrite(&fifo, "ABCDE", 5);
        sent_size = 0;
        assert(flush(&fifo, wrap_blocked, 2) == 3);
        assert(fifo.rptr == fifo.buffer);
        assert(flush(&fifo, rest, 1) == 0);
        assert(sent_size == 5 && memcmp(sent, "ABCDE", 5) == 0);
        IvyFifoFree(&fifo);
    }
    check_writable_handler();
    puts("Ivy FIFO: congestion, interruptions, partial writes, errors and ring wrap passed");
    return 0;
}
