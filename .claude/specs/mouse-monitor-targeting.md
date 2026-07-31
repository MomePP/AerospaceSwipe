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
immediately before the existing switch runs: look up the workspace already
visible on the mouse's monitor (`list-workspaces --monitor mouse --visible`)
and focus it (`workspace <name>`). Everything downstream is unchanged:
`list-workspaces --monitor focused` now resolves to the mouse's monitor, and
`workspace next/prev` follows it.

This is exactly what SwipeAeroSpace does, down to the once-per-gesture flag.
An earlier revision used `focus-monitor <resolved-id>` instead; both work, but
matching the reference implementation is worth more than the marginal
difference.

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

## Focus behavior

Focus follows to the mouse's monitor. It is not restored to the original
window afterward.

**Restoring it was implemented, tested, and reverted.** AeroSpace has no way to
change a monitor's workspace without focusing it, so restoring focus means
focusing twice per gesture — out to the mouse's monitor and back. That made the
workspace visibly bounce and the space indicator blink on every swipe. The
flicker is inherent to AeroSpace's model, not to the implementation, so no
amount of ordering or batching removes it. SwipeAeroSpace reaches the same
conclusion by construction: it also leaves focus where the swipe put it.

The reverted attempt also had to fight a second problem, kept here because it
constrains any future retry. With
`on-focused-monitor-changed = ['move-mouse monitor-lazy-center']` set:

1. Focusing the mouse monitor's visible workspace → focused monitor changed →
   hook fires → cursor is already on that monitor, so `lazy` suppresses the
   move. No cursor movement.
2. `workspace next` → the space changes.
3. *If we restored focus* to the original monitor → hook fires again → cursor
   is **not** on that monitor → the cursor gets warped to its center.

That rips the pointer off the window the user was aiming at, and because the
pointer has moved, the next swipe targets the wrong monitor — so the feature
would work exactly once. Measured against AeroSpace 0.21.3, that hook runs
*before* the reply to the focus command lands, so warping the cursor back
afterward is deterministic rather than a race. Any future attempt at focus
restoration needs that cursor restore, and still has to solve the flicker.

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
// Workspace visible on the monitor under the cursor. Caller frees, NULL if
// it can't be resolved. Focus it via aerospace_workspace(c, 0, name, "").
char* aerospace_mouse_visible_workspace(aerospace* client);
// Pure: trim surrounding whitespace, keep the first line. NULL if empty.
char* parse_workspace_name(const char* out);
```

`parse_workspace_name` is split out because it is the only genuinely testable
piece, mirroring how `gesture_math` is already factored.

Wiring: `switch_workspace()` gains a `bool* retargeted` out-param alongside the
existing `char** cached_workspaces`. On the first call of a gesture it resolves
the mouse monitor's visible workspace and focuses it; later calls skip.
`reset_gesture_state()` clears the flag alongside the cached workspace list.

**Resolved once per gesture, not per step.** Fingers are on the trackpad, so
the cursor cannot move mid-gesture; re-resolving on each of up to `max_steps`
switches would be waste.

The focused monitor is deliberately **not** queried for comparison. Re-focusing
the workspace already visible on the mouse's monitor is a no-op when the cursor
is on the focused monitor, so checking first would cost a query on every gesture
to save a command that costs no more than the query. SwipeAeroSpace makes the
same call.

## Failure handling

Degrade to the previous behavior; never break the swipe.

| Condition | Behavior |
| --- | --- |
| Mouse workspace unresolvable (`NULL`) | Skip retarget, switch on the focused monitor. Warn once per process, not per gesture. |
| Focusing that workspace fails | Warn, proceed with the switch anyway. |
| `follow_mouse_monitor = false` | No added calls. |
| Single monitor | Re-focusing the already-focused workspace is a no-op; costs one query + one command per gesture. |

## Testing

Unit (no AeroSpace required):

- `parse_workspace_name`: plain names, surrounding whitespace, multi-line
  output (first line only), and empty/`NULL` output.
- `follow_mouse_monitor` default and JSON parsing.
- The new store toggle, folded into the existing TSan concurrency test.

Manual (requires two monitors):

- Focus a window on monitor 1, cursor over monitor 2, swipe → **monitor 2's**
  space changes, **cursor does not move**, focus lands on monitor 2.
- Single monitor → unchanged.
- `follow_mouse_monitor = false` → unchanged.
