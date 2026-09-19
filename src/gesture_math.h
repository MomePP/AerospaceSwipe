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

// Palm rejection. For each contact, (x0, y0) is where it joined the gesture
// and dx its horizontal displacement since. A contact farther than
// `isolation` from every other contact is a palm and is ignored however it
// moves — a palm resting under a swiping hand slides along with it, so
// motion alone can't tell it from a lagging finger. Of the rest, exactly
// `fingers` must move together (each sharing the sign of their mean and
// reaching min_share of it) and any others must stay still (under min_share
// of that mean). *swipe_dx receives the moving fingers' mean. So 3 fingers +
// palm is not a 4-finger swipe, but 4 fingers + palm is. False with too few
// contacts or no motion. At most 32 contacts are considered.
bool swipe_contacts(const float* x0, const float* y0, const float* dx, int n,
	int fingers, float min_share, float isolation, float* swipe_dx);

// True once no live contact is a finger: every live one is isolated (a palm,
// judged as in swipe_contacts over all n contacts). The gesture is over then,
// even though the palm keeps the trackpad's contact count above zero.
bool only_palms_remain(const float* x0, const float* y0, const bool* live, int n,
	float isolation);

// Decides whether a scroll-wheel event belongs to a swipe this app is
// tracking and should be dropped before it reaches the app under the
// cursor. macOS emits scroll events for any unclaimed multi-finger drag
// (a 4-finger horizontal drag with the system gesture off scrolls exactly
// like a 2-finger one), including a momentum tail after the fingers lift.
//
// Both entry points run on the event-tap thread, in event delivery order.
typedef struct {
	bool armed;     // a frame with `fingers` or more contacts has been seen
	                // and the fingers have not all lifted since
	bool dropping;  // the current scroll sequence (and its momentum) is ours
	int extras;     // most contacts beyond `fingers` seen while armed (a palm)
} scroll_gate;

// Feed the live contact count of every touch frame. `fingers` or more contacts
// arm the gate: extra contacts are a resting palm (see swipe_contacts). It
// disarms once the count falls back to those extras — the fingers are gone
// and only the palm is left — which is 0 without a palm.
void scroll_gate_touches(scroll_gate* gate, int count, int fingers);

// began: the event opens a new scroll sequence (Began / MayBegin phase).
// Returns true if the event must be dropped.
bool scroll_gate_should_drop(scroll_gate* gate, bool began);
