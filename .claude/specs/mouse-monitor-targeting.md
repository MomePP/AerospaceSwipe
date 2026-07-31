# Mouse-monitor targeting

## Problem

On a multi-monitor setup, a swipe changes the workspace on AeroSpace's
**focused** monitor, which is often not the monitor the user is pointing at.
Focus on a browser on monitor 1, cursor hovering Ghostty on monitor 2, swipe —
monitor 1's space changes. The user expects monitor 2's to.

## Why this can't be solved in AeroSpace's config

Checked against AeroSpace 0.21.3-Beta:

- There is **no focus-follows-mouse setting**. The only mouse-related hook is
  `on-focused-monitor-changed`, which pushes the *mouse toward focus* — the
  opposite direction.
- `aerospace workspace` has **no `--monitor` flag**. It always acts on the
  focused monitor, and no setting re-points it.
- `mouse` is accepted as a `--monitor` selector by query commands
  (`list-monitors`, `list-workspaces`) but **rejected by `focus-monitor`**,
  which needs a numeric id.

So "which monitor does a swipe act on" can only be decided by the client
issuing the commands. It has to live here.

## Decision

Re-anchor AeroSpace's focused monitor to the monitor under the cursor
immediately before the existing switch runs. Everything downstream is
unchanged: `list-workspaces --monitor focused` now resolves to the mouse's
monitor, and `workspace next/prev` follows it.

This was chosen over two alternatives:

- **Retarget the candidate list** (pass `--monitor mouse` to
  `list-workspaces`, keep `workspace next --stdin`). Smallest diff, but rests
  on undefined behavior — `next` would have to locate the focused workspace in
  a list that doesn't contain it — and silently does nothing when both
  `skip_empty` and `wrap_around` are off, since that path sends no list.
- **Compute the target workspace ourselves** (query the list plus the visible
  workspace, compute index ±1, `workspace <name>`). Deterministic, but
  reimplements next/prev, skip-empty and wrap-around — logic AeroSpace already
  owns and that we would have to keep in sync with it.

Re-anchoring adds **no switching logic at all**. That is the point.

## Focus behavior, and the `on-focused-monitor-changed` interaction

Focus follows to the mouse's monitor. It is not restored to the original
window afterward.

This is not merely a preference — restoring focus is actively hostile to a
common AeroSpace config. With
`on-focused-monitor-changed = ['move-mouse monitor-lazy-center']` set:

1. `focus-monitor <mouse>` → focused monitor changed → hook fires → cursor is
   already on that monitor, so `lazy` suppresses the move. No cursor movement.
2. `workspace next` → the space changes.
3. *If we restored focus* to the original monitor → hook fires again → cursor
   is **not** on that monitor → the cursor gets warped to its center.

That would rip the pointer off the window the user was aiming at, and because
the pointer has moved, the next swipe would target the wrong monitor. The
feature would work exactly once.

Letting focus follow means exactly one focus change per gesture, always toward
the monitor the cursor is already on, so `lazy` suppresses the warp every time.
**Zero cursor movement.**

## Configuration

One key: `follow_mouse_monitor`, bool, default `true`.

| Value | Behavior |
| --- | --- |
| `true` (default) | Swipes act on the monitor under the cursor; focus follows there |
| `false` | Exactly the pre-existing behavior; no added AeroSpace calls |

Gets a `config_store_toggle_follow_mouse_monitor` and a "Follow Mouse Monitor"
menu-bar item, matching every other bool.

## Implementation

Three additions to the AeroSpace client:

```c
// Monitor id under the mouse cursor, or -1 if it can't be resolved.
int aerospace_mouse_monitor(aerospace* client);
// Focus a monitor by id. False if the command failed.
bool aerospace_focus_monitor(aerospace* client, int monitor_id);
// Pure: parse `--format %{monitor-id}` output. -1 on empty/garbage.
int parse_monitor_id(const char* out);
```

`parse_monitor_id` is split out because it is the only genuinely testable
piece, mirroring how `gesture_math` is already factored.

Wiring: `switch_workspace()` gains a `bool* retargeted` out-param alongside the
existing `char** cached_workspaces`. On the first call of a gesture it resolves
the mouse monitor and focuses it; later calls skip. `reset_gesture_state()`
clears the flag alongside the cached workspace list.

**Resolved once per gesture, not per step.** Fingers are on the trackpad, so
the cursor cannot move mid-gesture; re-resolving on each of up to `max_steps`
switches would be waste.

The focused monitor is deliberately **not** queried for comparison.
`focus-monitor` on the already-focused monitor is a no-op, and checking first
would cost a query on every gesture to avoid a command that costs less than the
query. This assumes a no-change `focus-monitor` does not fire
`on-focused-monitor-changed` — verify early, since cursor behavior depends on
it.

## Failure handling

Degrade to the previous behavior; never break the swipe.

| Condition | Behavior |
| --- | --- |
| Mouse monitor unresolvable (`-1`) | Skip retarget, switch on the focused monitor. Warn once per process, not per gesture. |
| `focus-monitor` fails | Warn, proceed with the switch anyway. |
| `follow_mouse_monitor = false` | No added calls. |
| Single monitor | `focus-monitor` is a no-op; costs one query + one command per gesture. |

## Testing

Unit (no AeroSpace required):

- `parse_monitor_id`: empty, `NULL`, whitespace, `"2"`, `"2\n"`, garbage,
  negative, leading whitespace.
- `follow_mouse_monitor` default and JSON parsing.
- The new store toggle, folded into the existing TSan concurrency test.

Manual (requires two monitors):

- Focus a window on monitor 1, cursor over monitor 2, swipe → **monitor 2's**
  space changes, **cursor does not move**, focus lands on monitor 2.
- Single monitor → unchanged.
- `follow_mouse_monitor = false` → unchanged.
