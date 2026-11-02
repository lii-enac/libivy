/* Internal parsing of Ivy discovery messages; no socket side effects. */
#ifndef IVY_DISCOVERY_H
#define IVY_DISCOVERY_H

#include <limits.h>
#include <stddef.h>
#include <string.h>

/* Read a decimal field followed by horizontal whitespace. */
static int IvyDiscoveryNumber(const char **cursor, unsigned int limit,
                              unsigned int *value)
{
    const char *p = *cursor;
    unsigned int number = 0;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p < '0' || *p > '9') return 0;
    do {
        unsigned int digit = (unsigned int)(*p - '0');
        if (number > (limit - digit) / 10) return 0;
        number = number * 10 + digit;
        ++p;
    } while (*p >= '0' && *p <= '9');
    if (*p != ' ' && *p != '\t') return 0;
    while (*p == ' ' || *p == '\t') ++p;
    *value = number;
    *cursor = p;
    return 1;
}

/* Reject oversized fields rather than silently truncating identities. */
static int IvyParseDiscovery(const char *line, int *version,
                             unsigned short *port, char *appid, size_t id_size,
                             char *appname, size_t name_size)
{
    const char *p = line;
    const char *id;
    const char *name;
    size_t id_length, name_length;
    unsigned int parsed_version, parsed_port;

    if (!IvyDiscoveryNumber(&p, INT_MAX, &parsed_version) ||
        !IvyDiscoveryNumber(&p, 65535, &parsed_port) || parsed_port == 0)
        return 0;
    id = p;
    id_length = strcspn(p, " \t\r\n");
    if (id_length == 0 || id_length >= id_size) return 0;
    p += id_length;
    if (*p != ' ' && *p != '\t') return 0;
    while (*p == ' ' || *p == '\t') ++p;
    name = p;
    name_length = strcspn(p, "\r\n");
    if (name_length == 0 || name_length >= name_size) return 0;
    p += name_length;
    if (*p == '\r') ++p;
    if (*p == '\n') ++p;
    if (*p != '\0') return 0;

    memcpy(appid, id, id_length);
    appid[id_length] = '\0';
    memcpy(appname, name, name_length);
    appname[name_length] = '\0';
    *version = (int)parsed_version;
    *port = (unsigned short)parsed_port;
    return 1;
}
#endif
