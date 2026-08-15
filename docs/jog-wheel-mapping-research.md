# Mixxx Jog-Wheel Mapping Research

Research date: 2026-08-14  
Scope: practices for mapping a DJ-controller jog wheel to Mixxx, with notes for
this Traktor Kontrol S4 Mk1 driver and its bundled Mixxx mapping.

## Recommended interaction model

1. **Separate platter touch from rotation.** Map a top/platter touch to scratch
   enable/disable, and map relative rotation to `engine.scratchTick()` while
   scratching. When the platter is not touched, send rotation to the deck's
   `jog` control for temporary pitch bend (nudging). This matches Mixxx's
   documented scripting model and the behaviour described by several mature
   controller mappings.
2. **Treat platter and edge/crown differently when hardware allows it.** A
   useful vinyl mode is: platter touch + turn scratches; edge/crown turn
   nudges. A non-vinyl mode can make both surfaces nudge. Make the active mode
   visible with a dedicated control/LED where possible.
3. **Keep library/navigation routing explicit.** If a jog is repurposed to
   scroll the library or adjust loops, it must not also move the playing deck.
   Return immediately to the documented deck behaviour on leaving that mode.

## Mixxx scratch lifecycle

Mixxx's controller-scripting API provides the intended lifecycle:

```javascript
const deck = script.deckFromGroup(group);
engine.scratchEnable(deck, intervalsPerRev, rpm, alpha, beta, ramp);
// For every signed, relative wheel movement:
engine.scratchTick(deck, delta);
// On touch release:
engine.scratchDisable(deck, ramp);
```

- Start from Mixxx's documented baseline: `intervalsPerRev` equal to the
  wheel's *actual emitted* signed-tick count per revolution, `rpm` around
  `33 + 1 / 3`, `alpha = 1 / 8`, and `beta = alpha / 32`. Tune from there with
  real hardware; these are responsiveness/slip-mat parameters, not universal
  constants.
- Use `engine.isScratching(deck)` to choose scratch versus nudge per incoming
  turn event. Obtain the deck from the group (`script.deckFromGroup(group)`) so
  deck layers do not accidentally scratch the wrong deck.
- Enable acceleration only as an optional scrubbing preference. Mixxx's Denon
  MC7000 mapping warns that it causes the track position to drift relative to
  the physical platter, making it unsuitable for precise scratching.
- Do not use busy waits for a release delay or other jog behaviour. Use
  `engine.beginTimer()` and cancel/restart the timer on new input.

## MIDI and input-pipeline practices

### Preserve signed relative motion

Decode the wheel protocol once and keep its direction consistent end-to-end.
For a 7-bit two's-complement relative value, the usual decode is:

```javascript
const delta = value < 64 ? value : value - 128;
```

Some controllers use a 64-centred protocol instead (`value - 64`). Capture a
short forward and backward trace before choosing one; a wrong decode produces
reversed motion or large jumps near zero.

Avoid dropping slow movement. If nudging is deliberately downscaled, retain a
per-deck remainder until it reaches one output step, rather than rounding each
small delta to zero. Do not reuse scratch scaling for nudge scaling: scratching
needs faithful relative position, while nudging is normally gentler.

### Calibrate resolution using emitted data

`intervalsPerRev` must represent the deltas Mixxx receives, not simply the
encoder's raw resolution. To calibrate:

1. capture several complete, steady forward rotations through the final MIDI
   output path;
2. decode and sum their signed deltas;
3. use the stable per-revolution total as the initial `intervalsPerRev`;
4. repeat backward and at normal scratching speed to detect dropped, batched,
   or clamped deltas.

This is especially important if the driver rate-limits, coalesces, or clamps
encoder changes. Such processing can lower the effective emitted resolution or
lose fast turns; changing only the JavaScript `intervalsPerRev` cannot restore
lost motion.

### Make touch reliable

A touch press should be idempotent: do not call `scratchEnable` again while the
deck is already scratching. On a genuine release, disable promptly. For noisy
or incomplete touch hardware, use a short, one-shot inactivity timeout that is
reset by every fresh touch event; cancel it when an explicit release arrives.
Choose the timeout by testing short holds, slow scratches, and a deliberately
lost release—too short interrupts a held scratch, too long leaves the deck in
scratch mode.

## Implications for this repository

The current mapping already follows much of this model:

- `mappings/mixxx/Traktor-Kontrol-S4-mk1.js` maps touch to
  `wheelTouch()` and rotation to `wheelTurn()`.
- It decodes the rotation as 7-bit two's-complement, calls `scratchTick()`
  during scratch mode, and uses a per-deck remainder so eight physical ticks
  become one non-touch `jog` nudge step.
- It uses a 100 ms one-shot touch-release watchdog because the S4 touch sensor
  is documented in the script as repeatedly sending touch values without a
  release.
- The driver (`classes/Jog.cpp`) receives a 0–1023 absolute wheel position,
  converts adjacent positions to a signed delta with wrap handling, and uses
  `jog_wheel_sensitivity` to coalesce events before emitting a 7-bit relative
  MIDI value. It thresholds touch input with `jog_touch_threshold`.

The mapping currently sets `jogScratchTicksPerRevolution` to `512` and passes
`8` as the scratch RPM. These are existing tuning choices, not values verified
by this research. Before changing either, run the calibration procedure above
against the **final MIDI messages**. In particular, compare a full-revolution
sum with 512 and confirm that the driver's coalescing/clamping does not discard
fast rotational movement. Then evaluate 33⅓ RPM and the current lower value as
separate feel settings; resolution and RPM should not be changed together in a
single test.

## Practical acceptance checklist

- Forward/backward slow turns are signed correctly and do not jump at the
  encoder wrap point.
- A complete rotation produces consistent scratch travel in both directions.
- Top touch scratches; an untouched/edge turn nudges without putting the deck
  into scratch mode.
- Repeated touch reports do not restart scratch state or leak timers.
- An explicit release and a missing-release timeout both leave scratch mode.
- Slow nudge movement eventually produces a nudge; fast turns neither stall nor
  saturate unexpectedly.
- Deck-layer switches and library/loop modes route jog movement to exactly one
  target.
- Behaviour remains acceptable at the intended audio buffer/latency. The
  MC7000 mapping explicitly ties recommended sensitivity to audio-buffer size,
  so test at the latency users will actually run.

## Sources

- Mixxx Wiki, [MIDI Scripting — Scratching and jog wheels](https://github.com/mixxxdj/mixxx/wiki/MIDI-Scripting#scratching-and-jog-wheels).
  Documents `scratchEnable`, `scratchTick`, `scratchDisable`, relative-value
  decoding, and initial scratch filter values.
- Mixxx Wiki, [MIDI Scripting — Timed reactions](https://github.com/mixxxdj/mixxx/wiki/MIDI-Scripting#timed-reactions).
  Documents `engine.beginTimer`, `engine.stopTimer`, and the no-busy-wait
  guidance.
- Mixxx User Manual, [Denon MC7000 — Jogwheel Settings](https://manual.mixxx.org/2.7/en/hardware/controllers/denon_mc7000).
  Documents sensitivity/latency interaction and the position-drift trade-off of
  acceleration.
- Mixxx User Manual, [Numark NS6II](https://manual.mixxx.org/2.5/en/hardware/controllers/numark_ns6ii).
  Example of platter scratching versus outer-edge temporary speed adjustment.
- Mixxx User Manual, [Traktor Kontrol S4 MK3 — Jogwheel modes](https://manual.mixxx.org/2.5/en/hardware/controllers/native_instruments_traktor_kontrol_s4_mk3).
  Example of explicit vinyl, jog, and loop-edit wheel modes with user feedback.
- Local implementation reviewed: `classes/Jog.cpp`, `config.json`, and
  `mappings/mixxx/Traktor-Kontrol-S4-mk1.js`.
