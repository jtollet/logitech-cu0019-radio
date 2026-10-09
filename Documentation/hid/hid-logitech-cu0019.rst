.. SPDX-License-Identifier: GPL-2.0-only

=======================================
Logitech C-U0019 radio debugfs companion
=======================================

This experimental companion observes USB receiver 046d:c542 through a
non-exclusive HID bus interface. It neither registers a HID/USB driver nor
changes the driver binding, input reports, pairing or power policy. Normal
mouse operation remains with hid-generic and usbhid.

Usage
=====

Enable CONFIG_HID_LOGITECH_CU0019_DEBUG and CONFIG_DEBUG_FS, mount debugfs,
and explicitly load the companion::

    modprobe hid-logitech-cu0019-debug
    cat /sys/kernel/debug/cu0019/*/identity

Loading alone performs no vendor transfers. To opt into active reads,
unload the companion and reload with enable_reads=1. The parameter is
read-only and defaults to false. There is no device modalias, automatic
loading requirement, background polling, timer or worker.

Only USB bcdDevice 0302 and feature report 5 with 56 data bits are accepted
for active reads. Suspended devices are not resumed. A normal HID client
can wake the receiver; this companion does not change autosuspend settings.

Do not use another vendor-feature client while these reads are enabled.
The companion's mutex serializes its own operations, not hidraw ioctls
from other clients. No exclusive ownership of the feature channel is
provided by this RFC design. Undocumented access is not provably risk-free.

Debugfs files
=============

The directory name is the existing HID device name. All four files are
root-readable (0400), with no write operation. This is debugfs, not a stable
userspace ABI.

``identity``
  HID name, USB ID, bcdDevice, read opt-in and supported-firmware flag.
  Reading this file never polls the receiver.

``radio``
  Status and, only for a successful sample, raw RSSI and PLL-derived
  frequency_mhz_candidate. No calibration or diagnostic counters are mixed
  into the radio measurements.

``registers``
  Status and, only for a successful sample, the fixed raw register words.
  Decoded radio values are not repeated here.

``diagnostics``
  Last attempt status/error, quarantine, attempts, firmware read commands,
  read errors, skipped reads, cache hits and attempt timing. Suspended/busy
  devices count as skipped reads, not protocol failures. Reading this file
  never polls the receiver.

``radio`` and ``registers`` share a cache. Opening either may trigger one
new collection, with at least one second between attempts, measured from
completion. Each open receives an independent copy: read and seek do not
initiate transfers. Failure invalidates the previous sample, so stale radio
values are not presented as valid. A transport/protocol failure quarantines
further probing until the companion is reloaded or the device is recreated.

Protocol and interpretation
===========================

Each firmware read is an eight-byte feature SET_REPORT followed by a
GET_REPORT. The request is ``05 c0 LL HH 00 00 00 00``; LL/HH are the fixed
little-endian register address. SET_REPORT submits a read opcode, not a
register write. The reply is four raw data bytes followed by the observed
signature ``03 02 21 84``. Short, long and unexpected replies are rejected.

Only these four-byte words can be requested:

* 0x0508: observed DMA layout guard;
* 0x0438: observed RF configuration guard;
* 0x0740: system tick before and after the radio reads;
* 0x04d4: PLL word, whose upper half is a frequency candidate;
* 0x0458: raw RSSI word.

A successful collection uses eight firmware read commands: initial DMA/RF
guards, tick/PLL/RSSI/tick, then final DMA/RF guards. It does not access RX
packet payload, arbitrary addresses, firmware, OTP or pairing data. There is
no register-write, LED, reset or radio-reconfiguration command.

RSSI calibration and measurement age are unknown. In particular, raw RSSI
must not be labelled as dBm, mouse signal strength, noise floor or link
quality without additional validation. A stopped mouse does not establish
absence of RF activity. The system tick is not a packet counter or last-seen
timestamp. The PLL candidate and RSSI are sequential reads, not an atomic
same-channel measurement, and hopping can occur between them.

Review scope
============

The companion uses the full debugfs proxy and device references for removal
lifetime. It takes transport trylocks rather than waiting for unbind, and
holds an active-only runtime PM reference for a collection. The receiver
mutex is released before removing debugfs entries and draining active opens.

This RFC does not establish production safety. Physical hot-unplug, system
suspend/resume, allocation-failure and lockdep/KASAN stress tests, as well as
feature-channel arbitration with hidraw, require review and validation.
