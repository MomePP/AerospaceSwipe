# Scroll suppression during a swipe

Why the event tap is active rather than listen-only, and why it watches
scroll-wheel events it never uses for gesture detection.

## The leak

macOS emits **scroll-wheel events for any multi-finger drag no system gesture
claims**. With "Swipe between full-screen applications" off (a prerequisite for
using this app with 4 fingers), a 4-finger horizontal drag scrolls the app under
the cursor exactly like a 2-finger one. Apps whose 2-finger swipes are
scroll-driven — Arc's sidebar space switcher was the report — fired on every
workspace swipe.

The tap was originally `kCGEventTapOptionListenOnly` on gesture events only:
it could observe the swipe but had no way to stop macOS delivering it.

Captured with a listen-only logger (signed with the local identity and the
app's bundle id, so TCC treated it as the trusted app — a process spawned from
an agent shell has no Input Monitoring and a tap there silently receives
nothing):

- The 4-contact gesture frame arrives ~100 ms **before** the first scroll
  `Began`, so gating on the tap thread's own contact count is race-free.
- Scroll `Changed` frames interleave with touch frames until the fingers lift.
- A **momentum tail** (`kCGScrollWheelEventMomentumPhase` 1→2→3) runs ~400 ms
  after every contact has ended. Dropping only while fingers are down still
  scrolls the sidebar.
- A staggered lift can open a **new** scroll sequence (`phase=1`) while two of
  the four fingers are still down. It is still part of the swipe.

## The gate

`scroll_gate` in `gesture_math.[ch]` is a pure two-flag state machine, fed
only from the event-tap thread in delivery order:

- `armed` — set by a frame with exactly `fingers` live contacts, cleared by a
  full release (`count == 0`). Mirrors when the gesture state machine would
  be tracking, without the async hop to `g_gesture_queue`.
- `dropping` — a sequence that begins while armed is ours, and stays ours
  through its momentum tail after disarm. Contacts reaching `fingers` in the
  middle of an unowned sequence take it over. The next `Began` while
  disarmed (a fresh 2-finger scroll) releases it.

`key_handler()` returns `NULL` for a dropped scroll event and passes
everything else. **Gesture events are never dropped**: Mission Control's
4-finger up/down and any app reading `NSTouch` directly depend on them.

With the app disabled from the menu, every event passes.

## Constraints

- `kCGEventTapOptionDefault` means the callback now runs for every scroll
  event system-wide. Scroll events are decided with
  `CGEventGetIntegerValueField` only — no `NSEvent` bridging. The existing
  `kCGEventTapDisabledByTimeout` re-enable is the safety net.
- The gate is not gated on axis. A 4-finger vertical drag that no gesture
  claims would leak the same way, and axis is decided on the other thread.
- Testing: `test/test_gesture_math.c` replays the captured sequences (full
  swipe with momentum, 2-finger pass-through, sequence restart on staggered
  lift, fingers arriving mid-scroll, other counts ignored). The tap wiring in
  `main.m` remains manual-verification only.
