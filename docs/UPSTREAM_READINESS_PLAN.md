# Upstream readiness plan

## Custom UI/input audit — 2026-10-03

Code-level scope: desktop toolbar, floating inspector, lock transitions,
color picker/history, lasso fill, rotation wheel, GUI event dispatch, shared
contact lifecycle, slider/scrollbar layout and pressure/correction separation.
This is not a rendered-layout or physical-device certification.

Confirmed defects repaired on `presentation-lock`:

- Lock text controls widened the entire left rail and rotation wheel, and
  changed width when toggled. Use fixed-size top-bar icons instead.
- Fill rendered duplicate recent-color controls. Remove both inspector copies;
  use the shared foreground color and explicitly label the tool Lasso Fill.
- Picker drag updates polluted recent colors. Record completed gestures or
  palette choices, not intermediate samples; remove all near-duplicate entries.
- Selected button borders/highlights were overwritten. Preserve active-state
  feedback, including transparent toolbar buttons.
- Lasso sampling and minimum bounds depended on canvas zoom. Sample in screen
  coordinates, include the finite release endpoint, retain non-collinearity
  validation. This does not implement bucket/flood fill.
- Rotation allowed foreign-device motion/releases and center normalization.
  Bind the drag to its initiating contact and reject a zero-length direction.
- Floating-panel touch dragging bypassed position locking. Apply the same
  policy to mouse, pen and touch; ignore pen hover motion during panel drags.
- Shared buttons could have a held pen action reset by another mouse/device.
  Bind mouse/pen press and release to one contact, ignore non-left buttons and
  foreign motion, and suppress touch activation during that owned contact.

Reviewed contracts retained: GUI popovers obstruct canvas input; owned canvas
releases are forwarded even outside the canvas; lasso tool switching and
multitouch cancellation clear unfinished points; lock transitions finish the
owning contact, clear navigation, and do not overwrite saved touch preferences.
Inspector scrollbar gutters and slider thumb insets remain in place. Pressure
mode and path correction remain independently configured.

Acceptance still requires a Surface run of the exact built commit: immediate
tool selection followed by a loop without opening a picker, pen release outside
controls, picker drag/release, multitouch interruption, lock/unlock during
navigation, all three panel drag devices, small windows and DPI scaling.
The reported fill activation sequence is not reproduced or proven resolved.
Source wiring tests cannot establish these runtime outcomes, performance,
accessibility, or rendered clipping. Native geometry/contact CTest execution
and full application compilation remain CI/native-host checks; no local build
or prerequisite installation is performed.

Scope: review of `graphite-ui` at 550dab4 against upstream main. This is a
repair and extraction checklist, not a claim that all features are merge-ready.

## Implemented locally after review

- Remove unfinished paper grain completely: shader, renderer coupling, brush
  configuration and preset fields. Old JSON grain keys are ignored; document
  serialization remains unchanged. Brush appearance is no longer affected by a
  global grain setting. This also removes the invalid shader alpha output.
- Generate polygon-only circles for line dots and short round strokes, matching
  mesh persistence and accurate triangulation. Circle approximation targets a
  0.05 object-unit sagitta with a bounded tessellation budget.
- Derive arrow normals from their own direction; start and end arrows now share
  winding even when the boolean union fallback is used.
- Reject nonfinite/zero line inputs and bound each pattern by its actual spacing
  and a 65,536-vertex pattern-body budget before allocation/integer conversion. Over
  budget input produces no preview, not a truncated or silently restyled line.
- Bind lasso motion/release to the initiating device and pen ID; reject
  nonfinite and wholly collinear polygons. The existing size guard is not a
  general proof of CDT safety.
- Add C++ behavioral checks using the same polygon helpers and lasso contact
  lifecycle as production. Extract the production line generator and mesh
  serializer for Skia-backed regression tests.

## Required validation before publication

Local builds/installations are intentionally not performed. Run the existing
CI CMake/CTest suite (including `polygon_geometry` and `contact_lifecycle`). The
ARM64 app build also compiles `skia_geometry.exe`, uploaded as a separate test
artifact; the x64 build runner cannot execute that ARM64 binary. Run it on the
ARM64 test PC. It covers every line pattern, arrow/cap combinations, final path
simplification, shared file/network path serialization, explicit union fallback,
curve rejection and flat-union snapshot isolation. It does not exercise a live
network connection or the application's complete undo manager.

Configure a full Conan app build with `-DINFINIPAINT_GEOMETRY_TESTS=ON` and run
`ctest -C Release --output-on-failure` on a native host for Skia tests. This option
is off for ordinary upstream builds and is enabled in the fork's CI script only.

Manually verify pressure and correction independently, lasso pointer ownership,
flat-overlap undo/redo, selection/deletion and export. Measure long-stroke frame
times and dense-pattern previews at 120/144 Hz; no latency benchmark has yet
established a budget. Circle tolerance is in object coordinates, so evaluate
extreme zoom before claiming screen-space accuracy.

## PR extraction sequence

1. Existing-code hardening, with isolated reproductions and regression tests.
   Fixes to newly added tools stay in their own feature PRs.
2. Preserve the narrow pen stack: input history, pressure, optional correction.
   Validate the exact PR heads; do not infer their status from the UI branch.
3. Line/shape enhancements using upstream-style popups, without Graphite UI.
   Introduce typed pattern/arrow modes and production Skia geometry tests.
4. Lasso and flat overlap as separate feature PRs with ownership and undo tests.
5. Graphite UI independently: inspect layout, interaction, accessibility and
   saved panel preferences without mixing geometry or pen-engine changes.

Do not cherry-pick the entire UI branch into a pen PR. Record dependency/base
branches explicitly, rebase onto upstream and check each extracted diff for
duplicate changes. Existing public PRs are not modified by this local repair.

## Follow-up performance work

- Profile before caching: distinguish input processing, whole-stroke mesh
  generation, PathOps, triangulation and rendering.
- Consider incremental committed-prefix geometry plus a revisable live tail;
  preserve the current pressure/correction contracts and serialization.
- Flat-overlap union currently runs at stroke completion, not every sample;
  measure completion hitches separately from live stroke latency.
- Replace source-text assertions with behavioral tests incrementally. Source
  wiring checks remain useful but cannot prove geometry or lifecycle safety.

No Rust rewrite or file-format expansion is needed to address these findings.

## Application-level acceptance scenarios

These are explicit integration tests to run, not outcomes inferred from source
checks or helper tests. Do not mark them passed until exercised in the app.

1. Draw every line pattern with no/start/end/both arrows, both cap choices,
   short lines and long diagonals. Save, close, reopen, compare fill and endpoints.
   Repeat with a connected peer and verify preview and completed geometry agree.
2. Enable flat overlap, draw two intersecting same-color strokes. Undo once:
   recover the first stroke with its previous transform. Redo: recover the union.
   Repeat after zoom/rotation, then save/reopen. Different colors must not merge.
3. Start a pen lasso, move another pen/mouse/touch pointer, release the foreign
   pointer: none may alter/end that lasso. Finish with its owning pen. Cancel via
   tool switch/window lifecycle; a subsequent new contact must draw normally.
4. Try a diagonal collinear lasso, repeated points, self-crossing polygon and
   empty click. No corrupt component/undo entry should be created. A meaningful
   self-crossing polygon should remain drawable; collinearity is not the same as
   zero signed shoelace area.
5. Import an old config/preset containing grain keys. Pressure, engine and
   correction settings must retain their values; exported config must omit grain.
6. Dense dotted geometry must be rejected by the dot/vertex budget, regardless
   of the otherwise-unused dash-length setting. Check that rejection leaves no
   persistent empty component and that normal drawing resumes afterward.
