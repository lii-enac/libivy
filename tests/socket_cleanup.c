/* Check the real deferred socket teardown without opening network sockets. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void *tracked_buffer;
static void *tracked_client;
static int buffer_frees, client_frees, delete_callbacks;
static void counted_free(void *pointer);

#define free counted_free
#include "../src/ivysocket.c"
#undef free
#include "../src/ivyloop.c"

static void counted_free(void *pointer)
{
    if (pointer != NULL && pointer == tracked_buffer) ++buffer_frees;
    if (pointer != NULL && pointer == tracked_client) ++client_frees;
    free(pointer);
}

static void deleting(Client client, const void *data)
{
    assert(data == &delete_callbacks);
    assert(buffer_frees == 0 && client_frees == 0);
    if (client->buffer != NULL) assert(client->buffer[0] == 'R');
    ++delete_callbacks;
}

static void check_cleanup(int with_fifo, int grow_buffer, int with_buffer)
{
    Client client = (Client)calloc(1, sizeof(*client));
    assert(client != NULL);
    assert(clients_list == NULL && channels_list == NULL);
    if (with_buffer) {
        client->buffer = (char *)malloc(IVY_BUFFER_SIZE);
        assert(client->buffer != NULL);
        if (grow_buffer) {
            char *grown = (char *)realloc(client->buffer, IVY_BUFFER_SIZE * 4);
            assert(grown != NULL);
            client->buffer = grown;
        }
        client->buffer[0] = 'R';
        client->ptr = client->buffer + 1;
    }
    if (with_fifo) {
        client->ifb = IvyFifoNew();
        assert(client->ifb != NULL);
        IvyFifoWrite(client->ifb, "pending", 7);
    }
    /* Never close a real descriptor; channel fd 0 is only used in fd_set. */
    client->fd = -1;
    client->handle_delete = deleting;
    client->data = &delete_callbacks;
    clients_list = client;
    client->channel = IvyChannelAdd(0, client, DeleteSocket, NULL, NULL);
    tracked_buffer = client->buffer;
    tracked_client = client;
    buffer_frees = client_frees = delete_callbacks = 0;
    SocketClose(client);
    assert(delete_callbacks == 0 && client_frees == 0);
    ChannelDefferedDelete();
    assert(delete_callbacks == 1);
    assert(buffer_frees == (with_buffer ? 1 : 0));
    assert(client_frees == 1);
    assert(clients_list == NULL && channels_list == NULL);
    tracked_buffer = tracked_client = NULL;
}

int main(void)
{
    int iteration;
    for (iteration = 0; iteration < 100; ++iteration) {
        check_cleanup(0, 0, 1);
        check_cleanup(1, 0, 1);
        check_cleanup(1, 1, 1);
        check_cleanup(0, 0, 0);
    }
    puts("Ivy socket cleanup: 400 deferred closes, receive buffers and pending FIFOs passed");
    return 0;
}
