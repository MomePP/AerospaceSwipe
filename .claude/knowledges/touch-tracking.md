# Per-finger touch tracking

How a physical finger is followed across frames, and the two bugs that
established the current design. Both were user-visible as "swipes just don't
fire", with no error and no log line.

## Why slots exist

`process_touches()` builds each frame's touch array by iterating an
`NSSet<NSTouch*>`, and **`NSSet` enumeration order is not stable across
enumerations** — each gesture callback receives a fresh `NSSet` from a new
`CGEvent`.

The gesture state machine originally indexed its per-finger history by raw
array position, assuming `touches[i]` this frame was the same physical finger
as `touches[i]` last frame. It isn't. When the order shuffled — more likely the
more fingers are down, which is why 4-finger swipes suffered worst — the
per-finger delta check diffed *two different fingers* against each other,
produced a garbage delta, and silently reset the gesture.

`NSTouch.identity` is stable for the life of one contact. `touch_slot_acquire()`
maps that identity to a stable index in `[0, MAX_TOUCHES)`, and `gesture_ctx`
arrays (`prev_x`, `prev_valid`) are indexed by **slot, not array position**.

## The slot leak (fixed in v1.0.2)

`touch_slot_release()` was only reachable from `convert_nstouch:`'s
`nt.phase == 8` branch — but `process_touches()` filtered ended touches out
*before* conversion, so that branch was dead code and no slot was ever returned
to the 16-slot pool.

Symptom: **swiping stopped working entirely once an external trackpad had been
connected, until the service was restarted.**

The mechanism is worth understanding before touching this code:

- The slot pool is shared across every trackpad.
- Each multitouch device carries its **own set of `NSTouch` identity objects**,
  and a Bluetooth trackpad hands out a fresh set every time it re-enumerates —
  plug-in, sleep/wake. Those never compare equal to the retained ones.
- Identity objects **are recycled per device**, which is why a single trackpad
  never hit this: one run logged 284 successful swipes over a week (1100+
  contacts at `fingers: 4`) without failing. Only a second device, or repeated
  re-enumeration of one, grows the set past 16.
- Once exhausted, `touch_slot_acquire()` returns `-1` for every finger,
  `handle_tracking_state()` skips them all, `acc_dx` never accumulates, and no
  swipe fires again until the process restarts. **Nothing is logged.**

Now `touch_end()` retires ended *and* cancelled contacts, returning the slot and
freeing the cached velocity state. Handling `NSTouchPhaseCancelled` also fixed a
smaller bug: cancelled touches were counted as live contacts, so a cancelled
gesture stayed in `GS_TRACKING` until a real release.

**If you change `process_touches()`, make sure every terminal contact still
reaches `touch_end()`.** That is the invariant the whole pool depends on, and
breaking it fails silently days later.

## Stale `prev_x` on a recycled slot

Because slots are recycled on release, `prev_x[slot]` may hold the last position
of whichever finger held the slot *before*. A finger landing mid-gesture was
differenced against that stale value, and a large enough spurious jump could
push `acc_dx` past a step threshold and fire a swipe the user never made.

`prev_valid[slot]` now tracks whether `prev_x[slot]` describes the *same* finger.
A newly seen slot is seeded and starts contributing from the next frame. Slots
absent from the current frame have their validity dropped — every frame carries
the full contact set, so an absent slot means that finger lifted.

## Counting fingers

`process_touches()` must exclude **only** terminal phases, never
`NSTouchPhaseStationary`. An earlier version excluded stationary touches, which
silently dropped a finger that was merely holding still between motion frames —
structural undercounting, frame after frame. That was the real cause of
"4-finger swipes are hard to trigger"; tolerating a few miscounted frames had
not helped, because the count was not briefly wrong, it was consistently wrong.

## Testing

`test/test_touch_slots.m` covers the allocator without live input: identity
stability, slot reuse after release, safe double-release, pool exhaustion, and a
regression driving 8 device generations x 5 fingers (40 acquisitions against a
16-slot pool) that fails against the pre-v1.0.2 code.

The gesture handlers themselves are `static` in `main.m` with no test harness,
so changes there still need manual verification.
