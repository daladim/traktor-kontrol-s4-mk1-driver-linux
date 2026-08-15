# Mixxx Jog-Wheel Mapping Review

Review date: 2026-08-14  
Scope: `mappings/mixxx/Traktor-Kontrol-S4-mk1.js` and
`mappings/mixxx/TraktorKontrol_S4_MK1_JFM.midi.xml`, assessed against
[`jog-wheel-mapping-research.md`](jog-wheel-mapping-research.md).

## Summary

The core jog mapping is sound:

- The XML routes wheel turn (CC `0x02`) and touch (CC `0x03`) for all four deck
  MIDI channels to the appropriate JavaScript handlers.
- `wheelTurn` decodes 7-bit two's-complement relative values, calls
  `engine.scratchTick()` while scratching, and otherwise sends a deck `jog`
  nudge.
- Per-deck nudge remainders prevent slow non-touch motion from being lost.
- The touch watchdog cancels/restarts its one-shot timer correctly.
- The driver's deck-layer channel switching aligns with the XML bindings for
  Channels 1–4.

## Proposed changes

### 1. Fix shifted fine-rate direction — high priority

`fineRate` receives the same signed relative wheel values as `wheelTurn`, but
currently treats every value greater than `1` as positive:

```javascript
if (value > 1) rate += 0.001;
else rate -= 0.001;
```

Two's-complement negative values are `64`–`127`, so backward wheel turns are
interpreted as positive. Extract a shared `decodeRelative(value)` helper and
use the resulting sign in both `wheelTurn` and `fineRate`.

Apply a sign-only fine-rate step unless hardware testing establishes that
magnitude scaling is wanted. The driver batches wheel deltas, so multiplying
rate changes by received magnitude may cause unexpectedly large adjustments.

### 2. Reset nudge remainder on scratch entry — medium priority

A nudge remainder accumulated before a touch can survive a scratch interaction
and become an unexpected nudge after release. Clear
`jogNudgeRemainders[deckNumber]` when scratch begins, or when scratch-mode
wheel movement is processed.

### 3. Calibrate scratch resolution and RPM — high priority, test-gated

The mapping sets:

```javascript
jogScratchTicksPerRevolution = 512;
// scratch RPM argument: 8
```

The driver starts with a 0–1023 absolute wheel position and coalesces deltas
according to `jog_wheel_sensitivity` (currently `10` ms). The current values
may be intentionally paired tuning, but the repository contains no evidence
that they were calibrated against final emitted MIDI deltas.

Before changing either value:

1. capture several full forward and backward rotations at the final MIDI port;
2. decode and sum their signed deltas;
3. compare the stable result with `512`;
4. test normal and fast scratch speed for dropped or clamped movement;
5. change only resolution or RPM in a given listening test.

Do **not** change directly to raw 1024 ticks/revolution or 33⅓ RPM without this
measurement.

### 4. Make the touch watchdog testable/configurable — medium priority

The 100 ms one-shot watchdog is a sensible workaround for the documented S4
behaviour of repeated touch reports without a release. However, if a held touch
stops sending reports for more than 100 ms, the mapping exits scratch mode and
the next movement becomes a nudge.

At minimum, name the timeout as a top-level mapping setting. Test held touch,
slow scratch, explicit release, and missing-release scenarios before changing
its duration or refresh logic.

### 5. Add defensive mapping shutdown — low priority

Add `shutdown()` to stop outstanding touch timers, disable scratching for
virtual decks 1–4, and clear nudge remainders. This prevents a stuck scratch
state during a mapping reload or controller shutdown.

### 6. Make jog parameters self-documenting — low priority

Promote magic values to named constants:

- scratch ticks per revolution;
- scratch RPM;
- alpha/beta filter settings;
- touch-release timeout;
- nudge ticks per step.

Document that these parameters are calibration-dependent and link to
[`jog-wheel-mapping-research.md`](jog-wheel-mapping-research.md).

## Driver changes: `classes/Jog.cpp`

### Required: preserve oversized relative deltas

`Jog::get_value_jog()` batches movement and then clamps a single emitted MIDI
relative value to `+63` or `-64`. It resets the accumulated counter before
clamping, so excess movement is permanently discarded. For example, a batched
`+90` delta emits `+63` and loses `27` ticks; a `-90` delta emits `-64` and
loses `26` ticks. This makes fast scratching inaccurate and invalidates
calibration based on final MIDI output.

Change the driver to split an oversized accumulated delta into multiple valid
7-bit two's-complement relative MIDI messages in the same handling pass (for
example, `+63`, then the remainder). Do not defer the excess to a subsequent
physical event because another event may not arrive.

### Required: use a monotonic, non-`int` batching clock

`Jog::updated` is an `int`, while `MidiEventOut::get_time()` returns a
`system_clock` millisecond count as an `int`. This risks integer overflow and
is affected by wall-clock changes. A backward system-clock adjustment can
extend the batching period, creating a large delta which is subsequently
clamped or split.

Use `std::chrono::steady_clock` with a 64-bit duration or time point. This
will require coordinated changes in `includes/Jog.h` and
`MidiEventOut::get_time()` (and any other callers), not only `Jog.cpp`.

### Recommended after hardware capture: add touch hysteresis

The driver emits touch-on when a sample is at or above one threshold and
touch-off otherwise. If touch capacitance hovers around `jog_touch_threshold`,
Mixxx can repeatedly leave and re-enter scratch mode.

After examining raw touch traces, use distinct press and release thresholds,
with the release threshold lower than the press threshold. Continue emitting
repeated touch-on messages while the touch remains active: the current
JavaScript mapping relies on those reports to refresh its 100 ms
missing-release watchdog.

### No driver change indicated

- The 0–1023 wheel-position wrap handling is correct.
- The driver's negative relative-MIDI encoding matches the JavaScript
  mapping's two's-complement decoding.
- Ignoring the first wheel value to establish an initial position is
  appropriate.

## Validation performed

- `TraktorKontrol_S4_MK1_JFM.midi.xml` parsed successfully with Python's XML
  parser.
- `node --check mappings/mixxx/Traktor-Kontrol-S4-mk1.js` completed without a
  syntax error.
- No automated jog replay trace source was present in this checkout; no runtime
  Mixxx or hardware validation was performed.
