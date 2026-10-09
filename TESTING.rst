.. SPDX-License-Identifier: GPL-2.0-only

=========================================
C-U0019 controlled measurements and tests
=========================================

Status: a proposed protocol, not measurements already taken. Do not start
disk workloads, unplug devices, unload modules or suspend a remote server
without agreeing on the operation and maintaining a recovery path.

USB3 near/far comparison
=======================

Intel documents that some USB3 connectors, cables and devices radiate
broadband noise into the 2.4 GHz band and that receiver separation can
improve wireless-mouse performance. This makes USB3 a plausible hypothesis,
not a demonstrated cause on this receiver or server. A connected idle USB3
device can also be a source; lack of additional degradation during traffic
does not rule out interference.

Use the same USB2 extension and the same host port in every condition.
Keep mouse-to-dongle distance, orientation, surface, line of sight, hand
position, battery and mouse motion pattern as constant as practical. Record
USB3 device/cable identity, negotiated USB speed, distances, orientation,
Wi-Fi conditions and other nearby radios. Do not publish a private inventory.

Prefer moving the USB3 device/cable relative to a fixed mouse and dongle.
For interference from a host port, move the dongle with its mouse position
adjusted to preserve the relative geometry. Simply moving the dongle nearer
to the mouse would confound interference with stronger desired signal.
Avoid placing it beside another USB3 cable in the far condition.

Use these four conditions; distances are example test points, not guaranteed
safe distances:

=================== ========================= ============================
Condition           Dongle to USB3 source      USB3 activity
=================== ========================= ============================
near_usb3_idle      approximately 2--5 cm      connected, no deliberate load
far_usb3_idle       approximately 0.5--1 m     connected, no deliberate load
near_usb3_active    approximately 2--5 cm      agreed read-only test traffic
far_usb3_active     approximately 0.5--1 m     the same read-only traffic
=================== ========================= ============================

First take roughly two minutes per condition (120 samples at 1.1 seconds).
If differences appear, perform at least three repetitions, alternating or
randomizing order. Return to the original condition to check reversibility.
Record failed/inactive reads as well as successful ones. Keep initial
settling time and the USB keep-awake policy identical in all phases.

Do not use a raw-device write, destructive benchmark or global cache drop.
Any load should read an agreed existing test file, with its effects on
normal server services considered. A cached file read may not generate USB3
traffic: verify actual device traffic rather than assuming a command does.
No disk-load command is provided or executed by these tools.

With the opt-in module already enabled and reviewed, save one phase from a
regular shell (the output directory must already exist)::

    sudo python3 tools/capture_radio.py \
        --condition near_usb3_idle --count 120 --interval 1.1 \
        --keep-awake > captures/near_usb3_idle_run1.jsonl

Repeat with the other labels only after physically setting each condition.
``--keep-awake`` opens the matching hidraw node temporarily, without input
reads or vendor ioctls. It does not wake or establish liveness of the mouse
itself. Omit it if an existing normal HID client already keeps the receiver
active; use the same choice for every phase. Do not run a second vendor
feature client. Physical receiver removal ends capture with an error; check
the new HID identity and restart rather than silently continuing.

Summarize files offline::

    python3 tools/summarize_radio.py captures/*.jsonl

The summary reports count, minimum, median and maximum raw RSSI by condition
and PLL frequency candidate, plus all status counts. Grouping by frequency
is useful for exploration but does not make the sequential PLL/RSSI pair
an atomic same-channel sample. No dBm conversion is performed.

Interpretation limits
=====================

A reproducible raw-value change correlated with geometry/traffic would
support further investigation; it would not by itself establish noise
power, calibration, RF packet loss or causation. No change does not rule
out interference: the register may be stale, quantized, sampled at a
different channel/time or measuring a quantity not yet understood.

Keep mouse-stationary and standardized mouse-motion experiments separate.
An optional mouse-off baseline can help explore ambient response, but can
also change receiver state/channel behavior; it is not a calibration.
Subjective lag can be recorded. Counts of evdev events are not RF packet-loss
counters, and latency claims need a controlled timing reference.

Kernel correctness before submission
====================================

These are separate from the RF usefulness experiment and remain pending
for the final RFC unless a result is explicitly recorded in VALIDATION.rst:

1. Load with defaults; open all files and verify zero vendor commands.
2. Opt in on the supported unit, while leaving hid-generic/usbhid bound.
   Verify bounded polling, guard failure handling and normal mouse input.
3. Read concurrently, seek an open snapshot and inspect identity/diagnostics;
   verify one shared rate-limited cache and no polling from non-radio files.
4. Close all files and unload. Also verify unload refusal with a live file
   descriptor, without forced removal. Do not transplant prototype results.
5. Repeat physical receiver unplug/replug, including during an active open;
   verify removal, clean recreation, new HID identity and continued input.
6. Test runtime autosuspend/resume: inactive reads must not wake the device;
   normal HID access must allow later opt-in reads without changing policy.
7. Test system suspend/resume, driver unbind and USB removal with an agreed
   local recovery method. Look for hangs and stale transport references.
8. Run removal/concurrent-open stress with lockdep and KASAN on a disposable
   test system. Exercise allocation failures and partial debugfs creation.
9. Verify rejection of unsupported firmware and report layouts with safe
   mocks or genuinely different units; do not spoof identity on production.
10. Resolve or explicitly review feature-channel arbitration with hidraw.
    An advisory lock and a private module mutex are insufficient guarantees.
11. Run the KUnit suite, full target-mainline build and documentation build;
    include additional architectures and relevant PM/build configurations.

Source
======

Intel, document 327216-001, April 2012:
https://www.usb.org/sites/default/files/327216.pdf
