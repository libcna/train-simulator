# Continuous visual repair campaign

User instruction: resolve **all 120** findings in `VISUAL_AUDIT.md`, then include
and resolve additional findings identified from new actual engine screenshots.
Prioritize visible passenger-facing problems before less-visible details. This
covers glazing, seating, the complete interior and exterior, platforms, stations,
landscape and railway infrastructure. Do not treat an individual repair as the
campaign's terminal condition or wait for another instruction between repairs.

For every repair preserve before/after engine captures, inspect both, run relevant
tests, record performance and update the audit. Continue to the next ranked open
item immediately. Completion requires verified closure of every finding, not just
an implementation or successful compilation. Stop only for an unavoidable genuine
blocker, recording what was attempted and what input is needed.

Constraints: English repository content; immutable CNA and sharp-runtime
dependencies; CNA `next`, OpenGL 3.3 desktop only; existing `build/`; shared system
ccache (`CCACHE_DIR=/rv/cnaccache`, `CCACHE_BASEDIR=/rv`); maximum four compilation
workers. Preserve user changes and keep asset/storage costs bounded.

At campaign adoption, ENV-001 through ENV-010 have repair evidence; 110 of the
original findings remain open. The authoritative evolving status and evidence
remain in `VISUAL_AUDIT.md`, not this initial snapshot. Intermittent physical-input
camera freezing remains a separate open functional concern.

Goal-service limitation: the existing thread goal was returned as `blocked`;
creating the expanded goal failed because that goal is unfinished. Available
goal tools cannot edit its objective or resume it. Do not mark it complete just
to bypass this restriction. Repository work can continue independently, but
automatic cross-turn continuation needs the goal resumed through the user UI.

User clarification: the user will manage the application goal. Do not stop
repository repairs because of goal-service status; continue the ranked audit.

The first continuous run proceeded through reflection, mullion, armrest, table,
cup-insert and ceiling-fixture repairs without waiting between them, then checked
furniture contact shading and the complete 61-test suite. The ceiling close-ups
added ENV-121 (HVAC grille construction). See the audit for exact acceptance,
performance, test outcomes and remaining work; this is not campaign completion.

## Moving-world performance gate after ENV-036

The private OpenGL High-quality run `vegetation-moving-soak` used 1,800 frames,
8x route time, route km 52, car 2, active CNA avatars, no audio, and the existing
comparison configuration. It finished at km **63.846** with **38 passengers**,
**54 generated / 7 loaded chunks**, **48 world rebuilds** and **268 buffer reuses**.
Final rendering had **126 draws**, **1,243,560 triangles** and **12 visible NPCs**.
The 1,619 steady samples measured mean **26.8 ms**, p95 **27.0 ms**, p99 **106.2 ms**,
maximum **124.7 ms**. The largest world rebuild was **114.5 ms**; generation/upload
totals were **2,683.1/1,609.4 ms**. Synchronous streaming hitches therefore remain
OPEN and must not be hidden by frozen-route benchmarks or average FPS.

`/usr/bin/time -v` measured **719,100 KiB peak RSS** over **49.16 s**, exit 0.
This is a short stress run with accelerated travel, not hours of play or a proof
of no memory leaks. Evidence: `build/visual-audit/vegetation-moving-soak.log`,
`vegetation-moving-soak-memory.log` and its actual engine PNG. The final image was
inspected with avatars present. The issue is project-side synchronous rebuild
work; no dependency patch or unproven CNA bug is asserted. Continue the ranked
visual repairs while monitoring geometry growth; staged/cached streaming remains
necessary follow-up before claiming polished sustained performance.
