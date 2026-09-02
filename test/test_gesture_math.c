#include "../src/gesture_math.h"
#include <assert.h>
#include <stdio.h>

static void test_compute_target_step_zero(void)
{
	assert(compute_target_step(0.0f, 0.08f, 5) == 0);
}

static void test_compute_target_step_below_threshold(void)
{
	assert(compute_target_step(0.05f, 0.08f, 5) == 0);
}

static void test_compute_target_step_one_positive_step(void)
{
	assert(compute_target_step(0.09f, 0.08f, 5) == 1);
}

static void test_compute_target_step_one_negative_step(void)
{
	assert(compute_target_step(-0.09f, 0.08f, 5) == -1);
}

static void test_compute_target_step_multiple_steps(void)
{
	assert(compute_target_step(0.25f, 0.08f, 5) == 3);
}

static void test_compute_target_step_clamped_at_max(void)
{
	assert(compute_target_step(10.0f, 0.08f, 5) == 5);
	assert(compute_target_step(-10.0f, 0.08f, 5) == -5);
}

static void test_decide_axis_undecided_below_threshold(void)
{
	assert(decide_axis(0.01f, 0.01f, 0.05f) == AXIS_UNDECIDED);
}

static void test_decide_axis_horizontal(void)
{
	assert(decide_axis(0.10f, 0.02f, 0.05f) == AXIS_HORIZONTAL);
}

static void test_decide_axis_vertical(void)
{
	assert(decide_axis(0.02f, 0.10f, 0.05f) == AXIS_VERTICAL);
}

static void test_decide_axis_equal_magnitude_prefers_horizontal(void)
{
	assert(decide_axis(0.10f, 0.10f, 0.05f) == AXIS_HORIZONTAL);
}

// Sequence observed in a real capture: 4 fingers land, scroll Began
// arrives ~100ms later, Changed frames interleave with touch frames, the
// fingers lift, and a momentum tail runs on for ~400ms with no contacts.
static void test_scroll_gate_drops_whole_swipe_including_momentum(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 4, 4);
	assert(scroll_gate_should_drop(&g, true));   // Began
	assert(scroll_gate_should_drop(&g, false));  // Changed
	scroll_gate_touches(&g, 3, 4);               // staggered lift
	assert(scroll_gate_should_drop(&g, false));  // Changed
	assert(scroll_gate_should_drop(&g, false));  // Ended
	scroll_gate_touches(&g, 0, 4);               // full release
	assert(scroll_gate_should_drop(&g, false));  // momentum Began
	assert(scroll_gate_should_drop(&g, false));  // momentum Changed
	assert(scroll_gate_should_drop(&g, false));  // momentum Ended
}

static void test_scroll_gate_passes_two_finger_scroll(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 2, 4);
	assert(!scroll_gate_should_drop(&g, true));
	assert(!scroll_gate_should_drop(&g, false));
	scroll_gate_touches(&g, 0, 4);
	assert(!scroll_gate_should_drop(&g, false)); // momentum
}

static void test_scroll_gate_next_two_finger_scroll_passes_after_a_swipe(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 4, 4);
	assert(scroll_gate_should_drop(&g, true));
	scroll_gate_touches(&g, 0, 4);
	assert(scroll_gate_should_drop(&g, false));  // momentum tail
	scroll_gate_touches(&g, 2, 4);
	assert(!scroll_gate_should_drop(&g, true));  // fresh 2-finger sequence
	assert(!scroll_gate_should_drop(&g, false));
}

// Also observed: a staggered lift can open a *new* scroll sequence while
// two of the four fingers are still down. It is still part of our swipe.
static void test_scroll_gate_drops_sequence_restarted_during_staggered_lift(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 4, 4);
	assert(scroll_gate_should_drop(&g, true));
	scroll_gate_touches(&g, 2, 4);
	assert(scroll_gate_should_drop(&g, false)); // Ended
	assert(scroll_gate_should_drop(&g, true));  // Began again, 2 fingers left
	scroll_gate_touches(&g, 0, 4);
	assert(scroll_gate_should_drop(&g, false)); // its momentum
}

static void test_scroll_gate_drops_when_fingers_arrive_mid_sequence(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 2, 4);
	assert(!scroll_gate_should_drop(&g, true));
	scroll_gate_touches(&g, 4, 4);
	assert(scroll_gate_should_drop(&g, false));
}

static void test_scroll_gate_ignores_other_finger_counts(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 3, 4);
	assert(!scroll_gate_should_drop(&g, true));
	scroll_gate_touches(&g, 5, 4);
	assert(!scroll_gate_should_drop(&g, false));
}

int main(void)
{
	test_compute_target_step_zero();
	test_compute_target_step_below_threshold();
	test_compute_target_step_one_positive_step();
	test_compute_target_step_one_negative_step();
	test_compute_target_step_multiple_steps();
	test_compute_target_step_clamped_at_max();
	test_decide_axis_undecided_below_threshold();
	test_decide_axis_horizontal();
	test_decide_axis_vertical();
	test_decide_axis_equal_magnitude_prefers_horizontal();
	test_scroll_gate_drops_whole_swipe_including_momentum();
	test_scroll_gate_passes_two_finger_scroll();
	test_scroll_gate_next_two_finger_scroll_passes_after_a_swipe();
	test_scroll_gate_drops_sequence_restarted_during_staggered_lift();
	test_scroll_gate_drops_when_fingers_arrive_mid_sequence();
	test_scroll_gate_ignores_other_finger_counts();
	printf("All gesture_math tests passed.\n");
	return 0;
}
