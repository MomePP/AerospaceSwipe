#pragma once
#include <stdbool.h>

typedef enum {
	AXIS_UNDECIDED,
	AXIS_HORIZONTAL,
	AXIS_VERTICAL
} swipe_axis;

// Clamped, truncated-toward-zero target step count for the given
// accumulated horizontal displacement. distance_pct must be > 0.
int compute_target_step(float acc_dx, float distance_pct, int max_steps);

// Decides whether accumulated displacement should lock the swipe axis to
// horizontal or vertical, or remain undecided. lock_threshold is the
// magnitude (on whichever axis) that must be crossed before locking.
swipe_axis decide_axis(float dx, float dy, float lock_threshold);

// Decides whether a scroll-wheel event belongs to a swipe this app is
// tracking and should be dropped before it reaches the app under the
// cursor. macOS emits scroll events for any unclaimed multi-finger drag
// (a 4-finger horizontal drag with the system gesture off scrolls exactly
// like a 2-finger one), including a momentum tail after the fingers lift.
//
// Both entry points run on the event-tap thread, in event delivery order.
typedef struct {
	bool armed;     // a frame with exactly `fingers` contacts has been seen
	                // and the fingers have not all lifted since
	bool dropping;  // the current scroll sequence (and its momentum) is ours
} scroll_gate;

// Feed the live contact count of every touch frame.
void scroll_gate_touches(scroll_gate* gate, int count, int fingers);

// began: the event opens a new scroll sequence (Began / MayBegin phase).
// Returns true if the event must be dropped.
bool scroll_gate_should_drop(scroll_gate* gate, bool began);
