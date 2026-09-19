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

static bool is_isolated(const float* x0, const float* y0, int n, int i, float isolation)
{
	for (int j = 0; j < n; ++j) {
		if (j != i && hypotf(x0[j] - x0[i], y0[j] - y0[i]) <= isolation)
			return false;
	}
	return true;
}

bool swipe_contacts(const float* x0, const float* y0, const float* dx, int n,
	int fingers, float min_share, float isolation, float* swipe_dx)
{
	float sorted[32];
	if (n > 32)
		n = 32;

	// Largest movement first: the moving fingers lead, still contacts trail.
	int m = 0;
	for (int i = 0; i < n; ++i) {
		if (is_isolated(x0, y0, n, i, isolation))
			continue;
		int j = m++;
		while (j > 0 && fabsf(sorted[j - 1]) < fabsf(dx[i])) {
			sorted[j] = sorted[j - 1];
			--j;
		}
		sorted[j] = dx[i];
	}
	n = m;

	if (fingers <= 0 || n < fingers)
		return false;

	float mean = 0;
	for (int i = 0; i < fingers; ++i)
		mean += sorted[i];
	mean /= fingers;

	if (mean == 0)
		return false;

	for (int i = 0; i < fingers; ++i) {
		if (sorted[i] / mean < min_share)
			return false;
	}
	for (int i = fingers; i < n; ++i) {
		if (fabsf(sorted[i] / mean) >= min_share)
			return false;
	}

	*swipe_dx = mean;
	return true;
}

bool only_palms_remain(const float* x0, const float* y0, const bool* live, int n,
	float isolation)
{
	for (int i = 0; i < n; ++i) {
		if (live[i] && !is_isolated(x0, y0, n, i, isolation))
			return false;
	}
	return true;
}

void scroll_gate_touches(scroll_gate* gate, int count, int fingers)
{
	if (count >= fingers) {
		gate->armed = true;
		if (count - fingers > gate->extras)
			gate->extras = count - fingers;
	} else if (count <= gate->extras) {
		gate->armed = false;
		gate->extras = 0;
	}
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
