#include "../src/gesture_math.h"
#include <assert.h>
#include <math.h>
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

static void test_scroll_gate_ignores_fewer_fingers(void)
{
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 3, 4);
	assert(!scroll_gate_should_drop(&g, true));
}

static void test_scroll_gate_drops_swipe_with_resting_palm(void)
{
	// 4 fingers + palm: the palm is an extra contact, not another gesture.
	scroll_gate g = { 0 };
	scroll_gate_touches(&g, 5, 4);
	assert(scroll_gate_should_drop(&g, true));
}

static bool near(float a, float b)
{
	return fabsf(a - b) < 1e-5f;
}

// Four fingertips in a cluster, as traced on a large trackpad, plus slots
// for a fifth contact. PALM_X/Y is where that trace's palm rested.
static const float HAND_X[] = { 0.21f, 0.26f, 0.57f, 0.39f, 0.35f };
static const float HAND_Y[] = { 0.43f, 0.76f, 0.86f, 0.92f, 0.55f };
#define PALM_X 0.90f
#define PALM_Y 0.08f
#define ISO 0.5f

static void test_swipe_contacts_allows_four_finger_swipe(void)
{
	float dx[] = { 0.10f, 0.11f, 0.09f, 0.10f };
	float out = 0;
	assert(swipe_contacts(HAND_X, HAND_Y, dx, 4, 4, 0.3f, ISO, &out));
	assert(near(out, 0.10f));
}

static void test_swipe_contacts_ignores_sliding_palm_under_four_fingers(void)
{
	// Traced: the palm slid 53% of the fingers' mean. It is far from every
	// finger, so it is ignored however much it moves.
	float x[] = { HAND_X[0], HAND_X[1], PALM_X, HAND_X[2], HAND_X[3] };
	float y[] = { HAND_Y[0], HAND_Y[1], PALM_Y, HAND_Y[2], HAND_Y[3] };
	float dx[] = { 0.179f, 0.122f, 0.090f, 0.204f, 0.176f };
	float out = 0;
	assert(swipe_contacts(x, y, dx, 5, 4, 0.3f, ISO, &out));
	assert(near(out, (0.179f + 0.122f + 0.204f + 0.176f) / 4));
}

static void test_swipe_contacts_ignores_palm_on_leftward_swipe(void)
{
	float x[] = { PALM_X, HAND_X[0], HAND_X[1], HAND_X[2], HAND_X[3] };
	float y[] = { PALM_Y, HAND_Y[0], HAND_Y[1], HAND_Y[2], HAND_Y[3] };
	float dx[] = { -0.001f, -0.10f, -0.11f, -0.09f, -0.10f };
	float out = 0;
	assert(swipe_contacts(x, y, dx, 5, 4, 0.3f, ISO, &out));
	assert(near(out, -0.10f));
}

static void test_swipe_contacts_blocks_three_finger_drag_with_still_palm(void)
{
	float x[] = { HAND_X[0], HAND_X[1], HAND_X[2], PALM_X };
	float y[] = { HAND_Y[0], HAND_Y[1], HAND_Y[2], PALM_Y };
	float dx[] = { 0.10f, 0.11f, 0.09f, 0.002f };
	float out = 1;
	assert(!swipe_contacts(x, y, dx, 4, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_blocks_three_finger_drag_with_sliding_palm(void)
{
	// The palm moving like a finger must not make up the fourth finger.
	float x[] = { HAND_X[0], HAND_X[1], HAND_X[2], PALM_X };
	float y[] = { HAND_Y[0], HAND_Y[1], HAND_Y[2], PALM_Y };
	float dx[] = { 0.10f, 0.11f, 0.09f, 0.05f };
	float out = 1;
	assert(!swipe_contacts(x, y, dx, 4, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_blocks_five_moving_fingers(void)
{
	float dx[] = { 0.10f, 0.11f, 0.09f, 0.10f, 0.10f };
	float out = 0;
	assert(!swipe_contacts(HAND_X, HAND_Y, dx, 5, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_ignores_still_contact_inside_the_hand(void)
{
	// A resting thumb near the fingers is not isolated, but it is still.
	float dx[] = { 0.10f, 0.11f, 0.09f, 0.10f, 0.004f };
	float out = 0;
	assert(swipe_contacts(HAND_X, HAND_Y, dx, 5, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_blocks_moving_contact_inside_the_hand(void)
{
	float dx[] = { 0.10f, 0.10f, 0.10f, 0.10f, 0.05f };
	float out = 0;
	assert(!swipe_contacts(HAND_X, HAND_Y, dx, 5, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_blocks_four_fingers_when_set_to_three(void)
{
	float dx[] = { 0.10f, 0.11f, 0.09f, 0.10f };
	float out = 0;
	assert(!swipe_contacts(HAND_X, HAND_Y, dx, 4, 3, 0.3f, ISO, &out));
}

static void test_swipe_contacts_tolerates_lagging_finger(void)
{
	// Mean 0.0875; the lagging finger moved 0.05, well over 30% of it.
	float dx[] = { 0.10f, 0.11f, 0.10f, 0.05f };
	float out = 0;
	assert(swipe_contacts(HAND_X, HAND_Y, dx, 4, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_blocks_finger_moving_opposite(void)
{
	float dx[] = { -0.10f, -0.11f, -0.09f, 0.04f };
	float out = 0;
	assert(!swipe_contacts(HAND_X, HAND_Y, dx, 4, 4, 0.3f, ISO, &out));
}

static void test_swipe_contacts_blocks_without_motion_or_contacts(void)
{
	float zero[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float out = 0;
	assert(!swipe_contacts(HAND_X, HAND_Y, zero, 4, 4, 0.3f, ISO, &out));
	assert(!swipe_contacts(HAND_X, HAND_Y, zero, 3, 4, 0.3f, ISO, &out));
}

static void test_only_palms_remain_after_fingers_lift(void)
{
	float x[] = { HAND_X[0], HAND_X[1], PALM_X, HAND_X[2], HAND_X[3] };
	float y[] = { HAND_Y[0], HAND_Y[1], PALM_Y, HAND_Y[2], HAND_Y[3] };
	bool live[] = { false, false, true, false, false };
	assert(only_palms_remain(x, y, live, 5, ISO));
}

static void test_only_palms_remain_false_while_a_finger_is_down(void)
{
	float x[] = { HAND_X[0], HAND_X[1], PALM_X, HAND_X[2], HAND_X[3] };
	float y[] = { HAND_Y[0], HAND_Y[1], PALM_Y, HAND_Y[2], HAND_Y[3] };
	bool live[] = { false, false, true, true, false };
	assert(!only_palms_remain(x, y, live, 5, ISO));
}

static void test_only_palms_remain_false_for_contact_inside_the_hand(void)
{
	// A thumb resting among the fingers is not a palm: the hand is still down.
	bool live[] = { false, false, false, false, true };
	assert(!only_palms_remain(HAND_X, HAND_Y, live, 5, ISO));
}

static void test_scroll_gate_disarms_when_only_the_palm_is_left(void)
{
	// Palm lands first, then 4 fingers; the fingers lift, the palm stays.
	scroll_gate g = { 0 };
	for (int count = 1; count <= 5; ++count)
		scroll_gate_touches(&g, count, 4);
	assert(scroll_gate_should_drop(&g, true));
	for (int count = 4; count >= 2; --count)
		scroll_gate_touches(&g, count, 4);
	assert(scroll_gate_should_drop(&g, false)); // staggered lift: still ours
	scroll_gate_touches(&g, 1, 4);
	assert(scroll_gate_should_drop(&g, false)); // momentum tail
	// Next: a 2-finger scroll with the palm still resting.
	scroll_gate_touches(&g, 3, 4);
	assert(!scroll_gate_should_drop(&g, true));
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
	test_scroll_gate_ignores_fewer_fingers();
	test_scroll_gate_drops_swipe_with_resting_palm();
	test_swipe_contacts_allows_four_finger_swipe();
	test_swipe_contacts_ignores_sliding_palm_under_four_fingers();
	test_swipe_contacts_ignores_palm_on_leftward_swipe();
	test_swipe_contacts_blocks_three_finger_drag_with_still_palm();
	test_swipe_contacts_blocks_three_finger_drag_with_sliding_palm();
	test_swipe_contacts_blocks_five_moving_fingers();
	test_swipe_contacts_ignores_still_contact_inside_the_hand();
	test_swipe_contacts_blocks_moving_contact_inside_the_hand();
	test_swipe_contacts_blocks_four_fingers_when_set_to_three();
	test_swipe_contacts_tolerates_lagging_finger();
	test_swipe_contacts_blocks_finger_moving_opposite();
	test_swipe_contacts_blocks_without_motion_or_contacts();
	test_only_palms_remain_after_fingers_lift();
	test_only_palms_remain_false_while_a_finger_is_down();
	test_only_palms_remain_false_for_contact_inside_the_hand();
	test_scroll_gate_disarms_when_only_the_palm_is_left();
	printf("All gesture_math tests passed.\n");
	return 0;
}
