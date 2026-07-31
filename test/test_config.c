#include "../src/config.h"
#include <assert.h>
#include <stdio.h>

static void test_default_config_multi_swipe(void)
{
	Config config = default_config();
	assert(config.multi_swipe == true);
	assert(config.max_steps == 5);
}

// On by default: a swipe acts on the monitor under the cursor. Setting it
// false restores the pre-existing focused-monitor behavior.
static void test_default_config_follow_mouse_monitor(void)
{
	Config config = default_config();
	assert(config.follow_mouse_monitor == true);
	assert(config.restore_focus == true);
}

int main(void)
{
	test_default_config_multi_swipe();
	test_default_config_follow_mouse_monitor();
	printf("All config tests passed.\n");
	return 0;
}
