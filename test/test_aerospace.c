// Unit test for the pure output-parsing helpers in aerospace.c. Needs no
// running AeroSpace — no socket, no CLI — so it runs anywhere. Run via
// `make test`.
#include "../src/aerospace.h"
#include <assert.h>
#include <stdio.h>

static void test_parse_id_plain(void)
{
	assert(parse_id("1") == 1);
	assert(parse_id("2") == 2);
	assert(parse_id("13") == 13);
}

// What the socket/CLI paths actually hand back: the CLI path strips one
// trailing newline, the socket path does not strip anything.
static void test_parse_id_tolerates_surrounding_space(void)
{
	assert(parse_id("2\n") == 2);
	assert(parse_id("  2") == 2);
	assert(parse_id("\t2\r\n") == 2);
}

// Anything that isn't a monitor id must report failure rather than a
// plausible-looking 0, which would be passed to `focus-monitor` as a real id.
static void test_parse_id_rejects_non_ids(void)
{
	assert(parse_id(NULL) == -1);
	assert(parse_id("") == -1);
	assert(parse_id("   ") == -1);
	assert(parse_id("-1") == -1);
	assert(parse_id("none") == -1);

	// execute_aerospace_command() returns stderr on a non-zero exit, so a
	// failed query arrives here as an error message.
	assert(parse_id("None of the monitors match the pattern(s)") == -1);
}

int main(void)
{
	test_parse_id_plain();
	test_parse_id_tolerates_surrounding_space();
	test_parse_id_rejects_non_ids();
	printf("All aerospace tests passed.\n");
	return 0;
}
