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

`slot_live[slot]` now says whether `prev_x[slot]` describes the *same* contact.
A newly seen slot is seeded (`track_slot()`, displacement 0) and contributes
from the next frame. A slot absent from the current frame goes non-live — every
frame carries the full contact set, so an absent slot means that contact
lifted — and its displacement is frozen.

## Counting fingers

`process_touches()` must exclude **only** terminal phases, never
`NSTouchPhaseStationary`. An earlier version excluded stationary touches, which
silently dropped a finger that was merely holding still between motion frames —
structural undercounting, frame after frame. That was the real cause of
"4-finger swipes are hard to trigger"; tolerating a few miscounted frames had
not helped, because the count was not briefly wrong, it was consistently wrong.

## Palm rejection

A palm resting on the pad is just another contact to `NSTouch` —
`isResting` stayed 0 for it in every traced frame. Failures that drove the
design, all with `fingers: 4` on a large trackpad and macOS three-finger drag
enabled:

- 3-finger drag + palm = 4 contacts → a sideways drag switched workspaces.
- 4-finger swipe + palm = 5 contacts → the gesture never started (it required
  *exactly* `fingers` contacts); the user had to lift the palm.

### Identify the palm by isolation, not motion

A traced palm rested in the bottom-right corner (x ≈ 0.84–0.92, y ≈ 0.02–0.11)
while the fingertips started at y ≥ 0.41. **It slides along with a swipe**:
37–53% of the fingers' mean travel, against ~70% for the slowest finger. A
motion-only rule ("extras must stay under 30%") blocked every palm-down swipe,
and no single threshold separates palm from lagging finger reliably.
Distance does: the palm sat ≥ 0.77 from its nearest finger; fingertips were
≤ 0.33 from theirs. So:

`swipe_contacts()` (`gesture_math.c`): a contact farther than
`PALM_ISOLATION` (0.5, normalized) from **every** other contact's join
position is a palm and is ignored however it moves. Of the rest, contacts are
ranked by `|dx|`; exactly `fingers` must move together (sign of their mean,
each ≥ `PALM_MIN_SHARE` = 30% of it) and any others must stay under 30% (e.g.
a resting thumb inside the hand). The switch distance is the **moving
fingers' mean**, so a palm never dilutes travel. 5 moving fingers, or 4 with
`fingers: 3`, stay rejected — finger counts remain exclusive — and a sliding
palm can't make up the 4th finger of a 3-finger drag.

### The gesture ends when only the palm is left

Single-swipe mode fires at gesture end, which used to mean `count == 0`. With
a resting palm the count never reaches 0: traced swipes were accepted but
fired only when the palm lifted, up to ~1 s later. The same assumption kept
the gesture alive into the next swipe and kept the scroll gate armed, dropping
ordinary 2-finger scrolls. Now `only_palms_remain()` ends the gesture once no
live tracked contact is non-isolated; `end_gesture()` fires (single-swipe)
and resets, and the next swipe starts fresh with the palm still down.

### Wiring in `main.m`

- A gesture starts at `count >= fingers`.
- `slot_dx`, `slot_x0/slot_y0`, `slot_tracked`, `slot_live` in `gesture_ctx`
  hold per-contact displacement and join position; `collect_tracked()` packs
  them for the pure functions. `slot_live` also marks `prev_x[slot]` as
  belonging to the same contact (it replaced `prev_valid`).
- Contacts join while the axis is undecided — fingers never land in one
  frame, so a gesture may start on palm + 3 fingers. Any contact landing *or
  lifting* before the lock re-baselines `start_x/start_y`: the palm is far
  from the fingers, so its arrival or departure alone shifted the average by
  ~0.13 and locked the axis vertical.
- Contacts landing after the lock are not judged. Lifted contacts keep their
  frozen displacement.
- `gesture_swipe_dx()` runs before every dispatch: per frame in multi-swipe
  (its result becomes `acc_dx`), once at gesture end in single-swipe.

Known limits: `peak_velx` (fast-flick) and axis lock still average over all
contacts, so a palm slightly damps both. With `fingers: 2`, two fingers more
than 0.5 apart would both count as palms.

Rejected alternatives: upstream's old filter (removed in `b6d615b`) marked
any contact still for 60 ms as a palm — the stationary-undercount bug above
in another form. `NSTouch.isResting` never fires for a palm. MultitouchSupport
contact size would work but is a private API. A bottom-band rule (y < 0.2)
depends on hand placement.

## Testing

`test/test_touch_slots.m` covers the allocator without live input: identity
stability, slot reuse after release, safe double-release, pool exhaustion, and a
regression driving 8 device generations x 5 fingers (40 acquisitions against a
16-slot pool) that fails against the pre-v1.0.2 code. `test/test_gesture_math.c`
covers `swipe_contacts()` and `only_palms_remain()` with the traced palm and
finger positions (sliding palm ignored, 3 fingers + palm blocked even when the
palm slides, extra movers blocked, lagging finger allowed, resting thumb
inside the hand is not a palm).

The gesture handlers themselves are `static` in `main.m` with no test harness,
so changes there still need manual verification.
