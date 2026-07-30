#ifndef MINI_TEST_H
#define MINI_TEST_H
/*
 * mini_test.h - 초경량 테스트 프레임워크
 * 각 테스트 파일(main 포함)에서 1번만 include 하세요.
 * 실무에서는 Unity / CppUTest / GoogleTest를 사용합니다 (SWE.5).
 */
#include <stdio.h>

static int g_mt_pass = 0;
static int g_mt_fail = 0;

#define MT_SECTION(name)  printf("\n== %s ==\n", (name))

#define MT_CHECK(cond, msg) do {                                  \
    if (cond) { g_mt_pass++; printf("  PASS  %s\n", (msg)); }     \
    else      { g_mt_fail++;                                      \
                printf("  FAIL  %s  [%s:%d] %s\n", (msg),         \
                       __FILE__, __LINE__, #cond); }              \
} while (0)

#define MT_SUMMARY() do {                                         \
    printf("\n-----------------------------------------------\n");\
    printf("결과: %d PASS / %d FAIL\n", g_mt_pass, g_mt_fail);    \
    return (g_mt_fail > 0) ? 1 : 0;                               \
} while (0)

#endif /* MINI_TEST_H */
