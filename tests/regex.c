/* Regression cases for Ivy's public binding API. No Ivy bus is required. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ivybind.h"

struct test_case {
    const char *pattern;
    const char *message;
    int count; /* Includes the complete match, as IvyBindingExec does. */
    const char *captures[4]; /* NULL means an unset group. */
};

static const struct test_case cases[] = {
    {"^hello$", "hello", 1, {0}},
    {"^hello$", "HELLO", 0, {0}},
    {"^hello$", "hello world", 0, {0}},
    {"", "", 1, {0}},
    {"^(.+) ([0-9]+)$", "plane 123", 3, {"plane", "123"}},
    {"value=(\\d+)", "prefix value=42 suffix", 2, {"42"}},
    {"^(a)?(b)$", "b", 3, {NULL, "b"}},
    {"^(a)(b)?$", "a", 2, {"a"}},
    {"^(a*)(b)$", "b", 3, {"", "b"}},
    {"^(a(b))$", "ab", 3, {"ab", "b"}},
    {"^(?:foo|bar) (.*)$", "bar baz", 2, {"baz"}},
    {"^(?<word>[a-z]+)-\\k<word>$", "abc-abc", 2, {"abc"}},
    {"^(\\w+) \\1$", "same same", 2, {"same"}},
    {"(?<=ID:)(\\d+)(?=;)", "ID:123;", 2, {"123"}},
    {"(?i)^hello (.*)$", "HELLO World", 2, {"World"}},
    {"^(.+)$", "caf\xc3\xa9", 2, {"caf\xc3\xa9"}},
    {"^(.)$", "\xff", 2, {"\xff"}}, /* Keep byte semantics, no implicit UTF. */
    {"^(.*)$", "", 2, {""}},
    {"^(.+)$", "line\n", 2, {"line"}},
    /* Patterns used by ivythroughput and Ivy's keyword filter. */
    {"\"(.*)\"$", "12:00 \"hello world\"\n", 2, {"hello world"}},
    {"add regexp \\d+ : (.*)$", "add regexp 12 : ^hello (.*)$", 2, {"^hello (.*)$"}},
    {"(\\^.*)$", "prefix ^hello (.*)$", 2, {"^hello (.*)$"}},
    {"^\\^([a-zA-Z_0-9-]+).*", "^PLANE_ID (.*)$", 2, {"PLANE_ID"}}
};

static IvyBinding compile(const char *pattern)
{
    int offset = -1;
    const char *error = NULL;
    IvyBinding binding = IvyBindingCompile(pattern, &offset, &error);
    assert(binding != NULL);
    return binding;
}

static void check_capture(IvyBinding binding, const char *message,
                          int group, const char *expected)
{
    int length = -1;
    const char *capture = NULL;
    IvyBindingMatch(binding, message, group, &length, &capture);
    if (!expected) {
        assert(length == 0);
#ifndef TEST_PCRE1
        assert(capture == NULL);
#endif
    } else {
        assert(length == (int)strlen(expected));
        assert(capture != NULL);
        assert(memcmp(capture, expected, length) == 0);
    }
}

int main(void)
{
    size_t i;
    int iteration;
    for (iteration = 0; iteration < 100; ++iteration) {
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            const struct test_case *test = &cases[i];
            IvyBinding binding = compile(test->pattern);
            int count = IvyBindingExec(binding, test->message);
            int group;
#ifdef TEST_CASELESS
            if (i == 1) {
                assert(count == 1);
                IvyBindingFree(binding);
                continue;
            }
#endif
            assert(count == test->count);
            for (group = 1; group < count; ++group)
                check_capture(binding, test->message, group, test->captures[group - 1]);
#ifndef TEST_PCRE1
            if (count) {
                check_capture(binding, test->message, -1, NULL);
                check_capture(binding, test->message, 1000, NULL);
            }
#endif
            IvyBindingFree(binding);
        }
    }
    {
        /* More than the 200 groups supported by the old POSIX fallback. */
        char pattern[1000] = "^";
        char message[301];
        IvyBinding binding;
        int group;
        for (group = 0; group < 300; ++group) strcat(pattern, "(a)");
        strcat(pattern, "$");
        memset(message, 'a', 300);
        message[300] = '\0';
        binding = compile(pattern);
        assert(IvyBindingExec(binding, message) == 301);
        for (group = 1; group <= 300; ++group)
            check_capture(binding, message, group, "a");
        IvyBindingFree(binding);
    }
    {
        int offset = -1;
        const char *error = NULL;
        puts("[TEST] Invalid regex: the following compilation error is expected.");
        assert(IvyBindingCompile("abc(", &offset, &error) == NULL);
        assert(offset == 4);
        assert(error != NULL && error[0] != '\0');
        puts("[OK] Invalid regex rejected, with the expected error offset.");
    }
    {
        IvyBinding binding = compile("^(a)?(b)$");
        assert(IvyBindingExec(binding, "ab") == 3);
        check_capture(binding, "ab", 1, "a");
        assert(IvyBindingExec(binding, "b") == 3);
        check_capture(binding, "b", 1, NULL);
        check_capture(binding, "b", 2, "b");
        assert(IvyBindingExec(binding, "no match") == 0);
#ifndef TEST_PCRE1
        check_capture(binding, "no match", 1, NULL);
#endif
        IvyBindingFree(binding);
    }
    {
        IvyBinding binding = compile("^(a)(b)?$");
        assert(IvyBindingExec(binding, "a") == 2);
        check_capture(binding, "a", 2, NULL);
        IvyBindingFree(binding);
    }
    {
        /* A fresh binding has no captures until a successful match. */
        IvyBinding binding = compile("^(.*)$");
#ifndef TEST_PCRE1
        check_capture(binding, "hello", 0, NULL);
#endif
        assert(IvyBindingExec(binding, "hello") == 2);
        check_capture(binding, "hello", 0, "hello");
        check_capture(binding, "hello", 1, "hello");
        assert(IvyBindingExec(binding, "") == 2);
        check_capture(binding, "", 0, "");
        check_capture(binding, "", 1, "");
        IvyBindingFree(binding);
    }
    {
        /* Runtime errors must leave no captures available. */
        IvyBinding binding = compile("(*LIMIT_MATCH=1)^(a+)+$");
        assert(IvyBindingExec(binding, "aaaaaaaaaaaaaaaaaaaaaaaa!") == 0);
#ifndef TEST_PCRE1
        check_capture(binding, "aaaaaaaaaaaaaaaaaaaaaaaa!", 1, NULL);
#endif
        IvyBindingFree(binding);
    }
    for (iteration = 0; iteration < 100; ++iteration) {
        const char *filters[] = {"PLANE", "GROUND"};
        assert(IvyBindingFilter("^ANY (.*)$") == 1);
        IvyBindingSetFilter(2, filters);
        assert(IvyBindingFilter("^PLANE (.*)$") == 1);
        assert(IvyBindingFilter("^GROUND (.*)$") == 1);
        assert(IvyBindingFilter("^OTHER (.*)$") == 0);
        assert(IvyBindingFilter("unanchored (.*)") == 1);
        IvyBindingRemoveFilter("PLANE");
        assert(IvyBindingFilter("^PLANE (.*)$") == 0);
        assert(IvyBindingFilter("^GROUND (.*)$") == 1);
        IvyBindingTerminate();
        IvyBindingTerminate(); /* Repeated termination is safe. */
    }
    puts("Ivy regex: group zero, fresh bindings, runtime error and 100 filter lifecycles passed");
    IvyBindingFree(NULL);
    assert(IvyBindingExec(NULL, "hello") == 0);
    printf("Ivy regex: %lu cases x 100, 300 captures, invalid pattern and repeated matches passed\n",
           (unsigned long)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
