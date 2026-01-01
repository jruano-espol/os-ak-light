#include "common.h"

#define assert_eq(a, b) do {\
    if ((a) == (b)) {\
        printfln("INFO: Test passed %s == %s", #a, #b); \
    } else {\
        eprintfln("ERROR: Test failed %s != %s", #a, #b);\
        exit(1);\
    }\
} while (0)


#define cstr_topics_match(a, b) topics_match(parse_topic(str8(a)), parse_topic(str8(b)))

int main() {
    assert_eq(cstr_topics_match("a", "b"), false);
    assert_eq(cstr_topics_match("#", "b"), true);
    assert_eq(cstr_topics_match("a", "#"), true);
    assert_eq(cstr_topics_match("#", "#"), true);
    printfln();

    assert_eq(cstr_topics_match("a/b/c", "a/b/c"), true);
    assert_eq(cstr_topics_match("+/b/+", "a/b/c"), true);
    assert_eq(cstr_topics_match("+/b", "a/b/c"), false);
    assert_eq(cstr_topics_match("#", "a/b/c"), true);
    assert_eq(cstr_topics_match("a/b/c/#", "a/#"), true);
    printfln();

    return 0;
}
