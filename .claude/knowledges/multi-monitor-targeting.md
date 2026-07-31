# Multi-monitor targeting

How a swipe decides *which* monitor's workspace to change, and why the
surrounding constraints are what they are. Verified against AeroSpace
0.21.3-Beta.

## How it works

With `follow_mouse_monitor` (default `true`), `switch_workspace()` re-anchors
AeroSpace's focused monitor onto the monitor under the cursor before the
existing switch runs:

```
list-workspaces --monitor mouse --visible   →  <workspace name>
workspace <workspace name>                  →  focused monitor is now the mouse's
```

Everything downstream is untouched — `list-workspaces --monitor focused` then
resolves to the mouse's monitor and `workspace next/prev` follows it. The
feature adds **no switching logic**; `skip_empty`, `wrap_around` and next/prev
semantics remain AeroSpace's, just re-anchored.

Focusing a workspace that is already visible on the focused monitor is a no-op,
so the same two commands are correct on single-monitor setups and cost one
query plus one command per gesture. The focused monitor is deliberately not
queried for comparison: it would cost a query on every gesture to save a
command that costs no more than the query.

**Resolved once per gesture**, guarded by `gesture_ctx.monitor_retargeted` and
cleared in `reset_gesture_state()`. Fingers are on the trackpad while swiping,
so the cursor cannot move mid-gesture; re-resolving on each of up to
`max_steps` switches is pure waste.

`parse_workspace_name()` trims surrounding whitespace and keeps only the first
line — a multi-line result would otherwise reach `workspace` as a single
argument. It is the only unit-testable piece, covered by `test_aerospace`.

## AeroSpace constraints this is built around

None of this can be expressed in `aerospace.toml`:

- **There is no focus-follows-mouse setting.** The only mouse-related hook is
  `on-focused-monitor-changed`, which pushes the *mouse toward focus* — the
  opposite direction.
- **`aerospace workspace` has no `--monitor` flag.** It always acts on the
  focused monitor and nothing re-points it.
- **`mouse` is a `--monitor` selector, not a focus pattern.** Query commands
  (`list-monitors`, `list-workspaces`) accept it; `focus-monitor` rejects it
  with "None of the monitors match the pattern(s)" and requires a numeric id.
- **There is no way to change a monitor's workspace without focusing it.**
  Switching a workspace *is* focusing it. This is the root constraint behind
  everything below.

## Focus is left on the monitor the swipe acted on

It is not handed back to the window that had it. **This was implemented, tested
on hardware, and reverted** — do not retry it without reading this section.

Because a workspace cannot be shown without being focused, keeping focus put
means focusing twice per gesture: out to the mouse's monitor, then back. The
result is a visible workspace bounce and a blinking space indicator on every
swipe. The flicker is inherent to AeroSpace's model, not to the implementation
— no ordering or batching removes it.

A second problem constrains any future attempt. With the common config

```toml
on-focused-monitor-changed = ['move-mouse monitor-lazy-center']
```

1. Focusing the mouse monitor's workspace → focused monitor changed → hook
   fires → cursor is already on that monitor, so `lazy` suppresses the move.
   **No cursor movement.**
2. `workspace next` → the space changes. Same monitor, so no hook.
3. Restoring focus across monitors → hook fires again → cursor is *not* on the
   target monitor → **the cursor is warped to its center.**

That rips the pointer off the monitor the user was aiming at, and since the
pointer has moved, the *next* swipe targets the wrong monitor — the feature
works exactly once. Any retry must restore the cursor as well as the focus.

**Measured, and useful if anyone does retry:** that hook runs *before* AeroSpace
replies to the focus command. Warping the cursor back after the command returns
is therefore deterministic, not a race. `CGWarpMouseCursorPosition` +
`CGAssociateMouseAndMouseCursorPosition(true)` was the working mechanism.

As it stands, exactly one focus change happens per gesture, always *toward* the
monitor the cursor already occupies, so `lazy` suppresses the warp every time.
Zero cursor movement, and `aerospace.toml` needs no changes — mouse-follows-
focus keeps working for keyboard navigation (`focus-monitor next`, `focus <dir>
--boundaries all-monitors-outer-frame`).

## Reference implementation

[SwipeAeroSpace](https://github.com/MediosZ/SwipeAeroSpace) does exactly the
same two commands, behind the same once-per-gesture flag (`gestureFocusDone`),
and likewise never restores focus. When behavior questions come up, its
`SwipeManager.swift` is the thing to compare against — this project's behavior
was deliberately aligned to it.

## Failure handling

Degrade to the previous behavior; never break the swipe.

| Condition | Behavior |
| --- | --- |
| Mouse workspace unresolvable (`NULL`) | Skip the re-anchor, switch on the focused monitor. Warned once per process, not per gesture. |
| Focusing that workspace fails | Warn, proceed with the switch anyway. |
| `follow_mouse_monitor = false` | No added AeroSpace calls at all. |
