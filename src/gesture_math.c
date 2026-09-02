#include "gesture_math.h"
#include <math.h>

int compute_target_step(float acc_dx, float distance_pct, int max_steps)
{
	int target = (int)(acc_dx / distance_pct);
	if (target > max_steps)
		target = max_steps;
	if (target < -max_steps)
		target = -max_steps;
	return target;
}

swipe_axis decide_axis(float dx, float dy, float lock_threshold)
{
	if (fabsf(dx) < lock_threshold && fabsf(dy) < lock_threshold)
		return AXIS_UNDECIDED;
	return fabsf(dy) > fabsf(dx) ? AXIS_VERTICAL : AXIS_HORIZONTAL;
}

void scroll_gate_touches(scroll_gate* gate, int count, int fingers)
{
	if (count == fingers)
		gate->armed = true;
	else if (count == 0)
		gate->armed = false;
}

bool scroll_gate_should_drop(scroll_gate* gate, bool began)
{
	// A new sequence is ours only if the swipe is still in progress. A
	// sequence that is already ours stays ours through its momentum tail,
	// which arrives after every finger has lifted and the gate disarmed.
	if (began)
		gate->dropping = gate->armed;
	else if (gate->armed)
		gate->dropping = true;
	return gate->dropping;
}
