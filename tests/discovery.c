#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ivydiscovery.h"

struct parsed {
    int version;
    unsigned short port;
    char id[128];
    char name[2048];
};

static int parse(const char *line, struct parsed *result)
{
    return IvyParseDiscovery(line, &result->version, &result->port,
        result->id, sizeof(result->id), result->name, sizeof(result->name));
}

int main(void)
{
    struct parsed result;
    size_t i;
    const char *invalid[] = {
        "", " ", "3", "3 2010", "3 2010 id", "3 2010 id ",
        "3 2010 id\nname", "3 2010 id name\nextra",
        "3 2010 id name\rjunk", "-3 2010 id name", "+3 2010 id name",
        "3 -1 id name", "3 +1 id name", "3 0 id name", "3 65536 id name",
        "3 999999999999999999999999 id name",
        "999999999999999999999999 2010 id name",
        "2147483648 2010 id name", "3x 2010 id name", "3 20x id name"
    };
    assert(parse("3 2010 uuid my application", &result));
    assert(result.version == 3 && result.port == 2010);
    assert(strcmp(result.id, "uuid") == 0);
    assert(strcmp(result.name, "my application") == 0);
    assert(parse(" 3\t65535\tuuid\tapplication\r\n", &result));
    assert(result.port == 65535);
    assert(strcmp(result.name, "application") == 0);
    assert(parse("3 1 uuid application\n", &result));
    assert(result.port == 1);
    assert(parse("3 2010 uuid caf\xc3\xa9", &result));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        assert(!parse(invalid[i], &result));
    {
        char line[5000];
        /* Both fields at their maximum accepted sizes, including terminators. */
        strcpy(line, "3 2010 ");
        memset(line + 7, 'i', 127);
        line[134] = ' ';
        memset(line + 135, 'n', 2047);
        line[2182] = '\0';
        assert(parse(line, &result));
        assert(strlen(result.id) == 127 && strlen(result.name) == 2047);
        /* One extra byte in the name must be rejected, not truncated. */
        line[2182] = 'n';
        line[2183] = '\0';
        assert(!parse(line, &result));
        /* An overlong identity must not spill into the name field. */
        memset(line + 7, 'i', 128);
        strcpy(line + 135, " app");
        assert(!parse(line, &result));
        /* Original overflow reproducer: a 256-byte identity. */
        memset(line + 7, 'i', 256);
        strcpy(line + 263, " app");
        assert(!parse(line, &result));
        /* Oversized app name, with an ordinary identity. */
        strcpy(line, "3 2010 id ");
        memset(line + 10, 'n', 4000);
        line[4010] = '\0';
        assert(!parse(line, &result));
    }
    puts("Ivy discovery: valid messages, field limits and malformed messages passed");
    return 0;
}
