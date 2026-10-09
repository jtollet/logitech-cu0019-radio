.. SPDX-License-Identifier: GPL-2.0-only

=========================================================
Radio diagnostics for proprietary wireless HID receivers
=========================================================

Status: a discussion outline, not an agreed kernel interface or stable ABI.

Problem and scope
=================

A user troubleshooting a wireless mouse or keyboard should not need to
reverse engineer each vendor's protocol just to inspect the available
radio observations. The initial question is whether proprietary wireless
HID receivers could expose diagnostics with common meanings.

The C-U0019 module is one experimental data source. It is not evidence
that the same protocol, register map or interpretation applies to other
receivers. A second independently understood receiver family would help
test whether any proposed conventions generalize.

Bluetooth is outside the proposed kernel change. HCI Read RSSI and the
Linux Management Get Connection Information command already provide
connection RSSI access. BlueZ Device1.RSSI covers inquiry/advertising
observations. A user-facing tool could reuse those existing interfaces
without changing the Bluetooth subsystem. Wi-Fi similarly keeps its
existing cfg80211/nl80211 interfaces. USB is a host transport, not a radio
protocol or a reason to merge unrelated kernel subsystems.

Minimum semantics to discuss
===========================

* Source and scope: receiver-wide observation or identified remote peer.
  One receiver can represent several peripherals; do not invent a peer
  association for a global register.
* Kind: last packet, average, discovery observation, ambient/global
  measurement or explicitly unknown meaning.
* Units: documented/calibrated dBm when supported; otherwise a raw value
  with device-specific interpretation. Do not compare raw scales across
  vendors or convert an unknown value into a percentage.
* Freshness: distinguish host read time from radio measurement time.
  Measurement age can be unknown even after a successful register read.
* Availability: unsupported, inactive, failed and stale are not zero signal.
  Unsupported properties should remain absent or explicitly unavailable.

RSSI, noise and link quality are different quantities. A power observation
alone does not establish desired-signal strength, interference, SNR, packet
loss or responsiveness. No generic on/off or liveness claim follows from
the availability of a receiver register.

For C-U0019, this repository deliberately exposes ``rssi_raw`` rather than
claiming a calibrated peer RSSI. Its PLL frequency candidate and RSSI are
read sequentially and can straddle a channel hop. RF measurement age and
mouse-to-receiver attribution remain unknown.

Kernel versus userspace
======================

Reading the vendor register does not inherently require kernel support;
the separate userspace project already uses hidraw. A userspace collector
can implement a bounded allowlist, caching and a common presentation.

The kernel experiment offers a narrow root-readable interface, shared
on-demand cache and device-lifecycle handling while preserving normal
input bindings. These benefits must justify their maintenance cost. Its
private mutex does not arbitrate transactions with unrelated hidraw clients;
the non-exclusive companion architecture is itself a review question.

A common vocabulary does not predetermine a new kernel class, sysfs ABI,
netlink family or daemon. First identify the information missing from
existing interfaces and the use cases that cannot reasonably be served
in userspace. debugfs is only the receiver-specific experimental interface.

Questions for HID maintainers
============================

1. Is an optional common diagnostic model useful for proprietary receivers?
2. Which semantics are sufficiently understood to expose consistently?
3. Should initial support remain in userspace, or integrate with the
   existing device driver to coordinate vendor feature transactions?
4. Is a non-exclusive debugfs companion acceptable for experimentation?
5. What additional hardware and lifecycle tests would make a later patch
   proposal useful to review?

References
==========

* Linux hidraw: https://docs.kernel.org/hid/hidraw.html
* Linux debugfs: https://docs.kernel.org/filesystems/debugfs.html
* BlueZ connection information:
  https://github.com/bluez/bluez/blob/master/doc/mgmt-protocol.rst
* BlueZ Device API:
  https://github.com/bluez/bluez/blob/master/doc/org.bluez.Device.rst
