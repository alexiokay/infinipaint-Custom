# Workspace lock

The toolbar's padlock toggles the lock. "Lock options" opens its configuration.
Default behavior is view-only: pan/zoom are available, canvas/layer editing and
undo/redo are blocked, and the previous tool is restored after unlocking.
The inspector shows "Workspace locked" and offers an explicit unlock action.

Three independently saved choices are available while unlocked:

- Block canvas editing (also guards selection edits, text, paste/import,
  undo/redo and the layer editor).
- Temporarily block touch drawing, even when the user's normal touch setting
  allows it. This is an effective-input override, never a write to that setting.
- Lock the floating tool panel's position.

The active lock is per-document/session and is never serialized. New documents
start unlocked. Existing contacts are completed before policy changes; locking
is not an undo operation. Tool switching cancels unfinished lassos through the
existing tool lifecycle. Options cannot change while locked, avoiding mid-contact
policy changes. Locking is a local accident-prevention feature, not security or
a collaboration permission: remote updates and pending image downloads continue.

Navigation, including touch gestures and camera rotation, remains available.
The ordinary fixed toolbar has no draggable position. The lock does not prevent
changing global application settings or saving/exporting the document.

## Validation

`workspace_lock` tests preference round trips, defaults, edit policy and every
combination of saved touch setting / temporary override / active state.
Source wiring tests check the application guards. No local build is performed.

Manual acceptance after CI compilation:

1. Draw with the brush, lock, pan/zoom with mouse, pen and touch; no ink appears.
2. Try eraser-tip switching, selection/delete, paste/drop, text and undo/redo.
   Canvas and layers must remain unchanged. Remote peer edits may still arrive.
3. Unlock; the previous tool works immediately without a dangling release.
4. With normal touch drawing enabled, choose only temporary touch blocking;
   lock: touch cannot draw, pen still can. Unlock: touch drawing returns.
5. Repeat with normal touch drawing disabled; unlocking must not enable it.
6. Choose only panel-position locking; drawing works, panel drags do not.
7. Save/restart: choices remain, active lock does not. Test switching document
   tabs during contacts and locking during an in-progress stroke/navigation drag.
