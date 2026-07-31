# Gesture model

How a swipe becomes one or more workspace switches, and why the earlier
state machine was replaced rather than patched.

## Continuous accumulated displacement

`gesture_ctx` has two states — `GS_IDLE` and `GS_TRACKING` — and accumulates
signed horizontal displacement in `acc_dx` for the whole gesture. The target
step count is derived from it (`compute_target_step()`: `acc_dx / distance_pct`,
clamped to `max_steps`), and the delta between target and `executed_step` is
what fires. Swiping further in the same direction simply raises the target.

**This replaced a fire-once-then-latch design that could wedge.** The old
`fire_gesture()` refused to fire when `direction == last_fire_dir`, and
`GS_COMMITTED` only exited on a clean full lift or a direction reversal.
Repeated same-direction swiping satisfied neither — real staggered finger lifts
rarely produce a frame where every touch is simultaneously `Ended` before the
next gesture starts — so after ~10 swipes in one direction it stopped firing
entirely until you paused or swiped the other way. There is no latch now, by
construction.

Axis is locked **once** per gesture (`decide_axis()`), not re-checked per frame.
A per-frame check let a diagonal correction mid-swipe retroactively cancel an
already-progressing horizontal gesture.

Once `GS_TRACKING` starts, a frame whose finger count doesn't match `fingers`
is skipped, not reset. Only a true full release (`count == 0`) ends the gesture.

## Bounded, lossless dispatch

`maybe_dispatch_switch()` keeps **at most one** dispatch outstanding
(`dispatch_in_flight`). The dispatched block loops, re-reading the true current
target each iteration, until the delta is zero.

This matters because `workspace next/prev` are *relative* commands: a dropped
intermediate step is a permanently wrong landing spot, not a skipped animation
frame. So excess fires are **coalesced, never dropped**. The backlog is bounded
by `max_steps` naturally, since the worker always converges on the latest target
rather than replaying a queue.

An earlier version dispatched every fire onto the global concurrent queue with
no cap, which produced visible lag under rapid swiping as blocking socket
round-trips piled up.

## Queues

Two dedicated serial queues, and the split is deliberate:

- `g_gesture_queue` — frame processing. **Must** be serial: gesture math sums
  displacement across frames, and a concurrent queue doesn't guarantee FIFO
  delivery under contention.
- `g_workspace_queue` — blocking workspace-switch socket I/O, kept off the
  gesture queue so it never delays frame processing.

`g_aerospace_mutex` serializes the actual AeroSpace calls, because back-to-back
swipes can reach `switch_workspace()` from different work items at once and the
list-then-switch pair must not interleave with another call's pair.

## Per-gesture workspace list cache

With multi-step firing one gesture can call `switch_workspace()` up to
`max_steps` times, each otherwise re-fetching the workspace list. The list is
fetched once per gesture into `ctx->cached_workspace_list` and freed on reset.

This is correct, not just an optimisation: window-to-workspace assignment
doesn't change from focus-switching alone, so a stale-by-milliseconds snapshot
still describes the right cycling order.

## Single-swipe mode

With `multi_swipe: false`, nothing fires mid-gesture; exactly one step fires at
release, if `|acc_dx|` clears the threshold. The fast-flick early-trigger fields
(`fast_distance_factor`, `fast_velocity_threshold`) apply **only** to this path —
the live multi-step path is already proportional to distance, so a fast flick
naturally crosses more steps without a separate rule.

Firing live in this mode was tried for haptic perceptibility and felt worse in
testing; it was reverted to release-based firing deliberately.

## Tunables that were never derived from measurement

`max_steps: 5` and the axis-lock threshold were carried over from
SwipeAeroSpace's tuning rather than independently derived. Treat them as
adjustable if the feel is off.

## Testing

`test/test_gesture_math.c` covers the pure helpers (`compute_target_step`,
`decide_axis`). The handlers themselves are `static` in `main.m` with no
harness, so behavioural changes need manual verification.
