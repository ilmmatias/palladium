/* SPDX-FileCopyrightText: (C) 2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const unsigned char *Current;
} ScanContext;

int PalladiumRand(void);
void PalladiumSrand(unsigned Seed);
int __vscanf(
    const char *Format,
    va_list Arguments,
    void *Context,
    int (*ReadCharacter)(void *Context),
    void (*UnreadCharacter)(void *Context, int Character));

static int TestFailures;

#define TEST_CHECK(Expression)                                                             \
    do {                                                                                   \
        if (!(Expression)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #Expression); \
            TestFailures++;                                                                \
        }                                                                                  \
    } while (false)

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function supplies the scanner's character input callback.
 *
 * PARAMETERS:
 *     ContextPointer - String scan context.
 *
 * RETURN VALUE:
 *     Next unsigned input byte, or EOF at the string terminator.
 *-----------------------------------------------------------------------------------------------*/
static int ReadCharacter(void *ContextPointer) {
    ScanContext *Context = ContextPointer;
    return *Context->Current ? *Context->Current++ : EOF;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function supplies the scanner's one-character input rollback callback.
 *
 * PARAMETERS:
 *     ContextPointer - String scan context.
 *     Character - Character to return, or EOF for no action.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void UnreadCharacter(void *ContextPointer, int Character) {
    ScanContext *Context = ContextPointer;
    if (Character != EOF) {
        Context->Current--;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calls the Palladium scanner using a string-backed input context.
 *
 * PARAMETERS:
 *     Input - Input string.
 *     Format - Scan format.
 *     ... - Output arguments selected by Format.
 *
 * RETURN VALUE:
 *     Number of assignments, or EOF for an input failure before the first assignment.
 *-----------------------------------------------------------------------------------------------*/
static int Scan(const char *Input, const char *Format, ...) {
    ScanContext Context = {.Current = (const unsigned char *)Input};
    va_list Arguments;
    va_start(Arguments, Format);
    int Result = __vscanf(Format, Arguments, &Context, ReadCharacter, UnreadCharacter);
    va_end(Arguments);
    return Result;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements the scanner's decimal width parser without using host libc.
 *
 * PARAMETERS:
 *     String - First decimal digit.
 *     End - Where to return the first unconsumed character.
 *     Base - Requested base, which must be ten for scanner widths.
 *
 * RETURN VALUE:
 *     Parsed positive decimal value.
 *-----------------------------------------------------------------------------------------------*/
long PalladiumStrtol(const char *String, char **End, int Base) {
    long Value = 0;
    TEST_CHECK(Base == 10);
    while (*String >= '0' && *String <= '9') {
        Value = Value * 10 + *String++ - '0';
    }

    *End = (char *)String;
    return Value;
}

int PalladiumIsspace(int Character) {
    return Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r' ||
           Character == '\f' || Character == '\v';
}

int PalladiumIsdigit(int Character) {
    return Character >= '0' && Character <= '9';
}

int PalladiumIslower(int Character) {
    return Character >= 'a' && Character <= 'z';
}

int PalladiumIsupper(int Character) {
    return Character >= 'A' && Character <= 'Z';
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates the declared rand range and deterministic reseeding behavior.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TestRandom(void) {
    PalladiumSrand(0x12345678);
    int First = PalladiumRand();
    PalladiumSrand(0x12345678);
    TEST_CHECK(PalladiumRand() == First);

    for (int Index = 0; Index < 10000; Index++) {
        int Value = PalladiumRand();
        TEST_CHECK(Value >= 0);
        TEST_CHECK(Value <= RAND_MAX);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates suppressed character/string input at EOF and ordinary bounded input.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TestScanner(void) {
    char Characters[2] = {0};
    char String[4] = {0};

    TEST_CHECK(Scan("", "%*c") == EOF);
    TEST_CHECK(Scan("", "%*s") == EOF);
    TEST_CHECK(Scan("x", "%*c%c", Characters) == EOF);
    TEST_CHECK(Scan("ab", "%2c", Characters) == 1);
    TEST_CHECK(Characters[0] == 'a' && Characters[1] == 'b');
    TEST_CHECK(Scan("abc rest", "%3s", String) == 1);
    TEST_CHECK(String[0] == 'a' && String[1] == 'b' && String[2] == 'c' && !String[3]);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs the portable CRT host tests.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Zero when every test passes, nonzero otherwise.
 *-----------------------------------------------------------------------------------------------*/
int main(void) {
    TestRandom();
    TestScanner();

    if (TestFailures) {
        fprintf(stderr, "portable CRT tests failed: %d\n", TestFailures);
        return 1;
    }

    puts("portable CRT tests passed");
    return 0;
}
