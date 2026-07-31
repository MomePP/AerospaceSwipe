// Unit test for the pure output-parsing helpers in aerospace.c. Needs no
// running AeroSpace — no socket, no CLI — so it runs anywhere. Run via
// `make test`.
#include "../src/aerospace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(const char* input, const char* expected)
{
	char* got = parse_workspace_name(input);

	if (expected == NULL) {
		assert(got == NULL);
		return;
	}

	assert(got != NULL);
	assert(strcmp(got, expected) == 0);
	free(got);
}

static void test_plain_names(void)
{
	check("1", "1");
	check("main", "main");
	check("7", "7");
}

// What the socket and CLI paths actually hand back: the CLI path strips one
// trailing newline, the socket path strips nothing.
static void test_tolerates_surrounding_space(void)
{
	check("1\n", "1");
	check("  main  ", "main");
	check("\tdev\r\n", "dev");
}

// `--visible` returns one workspace per monitor, so a query that matched more
// than intended must not yield a name with an embedded newline — that would go
// straight to `workspace` as a single argument.
static void test_keeps_only_the_first_line(void)
{
	check("1\n2\n3", "1");
	check("\n\nsecond", "second");
}

static void test_rejects_empty_output(void)
{
	check(NULL, NULL);
	check("", NULL);
	check("   ", NULL);
	check("\n\n", NULL);
}

int main(void)
{
	test_plain_names();
	test_tolerates_surrounding_space();
	test_keeps_only_the_first_line();
	test_rejects_empty_output();
	printf("All aerospace tests passed.\n");
	return 0;
}
