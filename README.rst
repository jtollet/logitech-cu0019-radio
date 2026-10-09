.. SPDX-License-Identifier: GPL-2.0-only

================================
Logitech C-U0019 radio diagnostics
================================

Author: Jerome Tollet <jerome.tollet@gmail.com>

This repository contains an **experimental kernel module and review draft**,
not an accepted upstream driver or a production-safe release. Publishing
this code does not submit it to a kernel mailing list. No RFC email has been
sent, and publication does not install, load or replace anything on a host.

Repository: https://github.com/jtollet/logitech-cu0019-radio

The existing userspace implementation is a separate project:
https://github.com/jtollet/cu0019-monitor

The receiver is Logitech C-U0019, USB ID 046d:c542. Active reads are
restricted to bcdDevice 0302. The receiver model, rather than an inferred
chip identity, names the module. Normal input remains with hid-generic and
usbhid. The RFC adds no binding, service, timer or background poller.

The wider design question is whether proprietary wireless HID receivers
could share diagnostic conventions. ``DESIGN.rst`` describes that question;
this module implements only C-U0019 diagnostics, not a generic radio class.
Bluetooth and Wi-Fi are outside the proposed kernel scope and should keep
their existing interfaces.

Contents
========

* ``drivers/hid/``: directly browsable module, protocol helpers and KUnit tests.
* ``Documentation/hid/hid-logitech-cu0019.rst``: exact kernel-series documentation.
* ``DESIGN.rst``: discussion scope and kernel-versus-userspace trade-offs.
* ``patches/0000-cover-letter.patch``: English RFC introduction for review.
* ``patches/0001-*``: opt-in diagnostic companion and kernel documentation.
* ``patches/0002-*``: KUnit coverage of the pure protocol helpers.
* ``TESTING.rst``: controlled USB3 measurements and runtime validation plan.
* ``VALIDATION.rst``: completed checks and outstanding work, kept separate.
* ``RECIPIENTS.rst``: current MAINTAINERS-derived recipient candidates.
* ``tools/``: bounded debugfs capture and offline raw-value summaries.
* ``tests/``: host protocol harness, host-only header shims and tool tests.
* ``kunitconfig``: minimal KUnit configuration fragment.
* ``external-module.mk``: temporary external-build Makefile.
* ``LICENSE``: GNU General Public License, version 2; code is GPL-2.0-only.
* ``.github/workflows/check.yml``: offline tests on Linux and macOS.

The four standalone source/documentation snapshots are identical to the
files added by the kernel patches. ``make check`` verifies that relationship.

The series targets this exact upstream Linux commit::

    af32da41b0327b9c6a37856ba82b6760d6c8d10e

No stable ABI is proposed. RSSI is a raw register value, not calibrated dBm,
noise floor, mouse signal strength or link quality. The PLL frequency is a
candidate, read separately from RSSI; a channel hop can occur between reads.
Measurement age, mouse presence, battery level and packet loss are unknown.

Outstanding design issue: the companion's mutex does not serialize vendor
feature transactions from unrelated hidraw clients. Do not run those clients
concurrently. An advisory userspace lock does not fix kernel arbitration.
Read the cover letter before evaluating the design.

Build and offline tests
======================

Run the offline tests directly from a clone, with a C compiler, Make and
Python 3.10 or newer::

    make check

These tests require no receiver, root privileges or kernel checkout. They
validate protocol helpers, mock capture/summary behavior and source/patch
consistency; they do not prove runtime safety or RF interpretation.

On Linux, with matching kernel development headers installed, build the
standalone module without installing or loading it::

    make module

For another kernel header tree, pass ``KDIR=/path/to/kernel/build``.
The result is ``drivers/hid/hid-logitech-cu0019-debug.ko``. No automatic
installation, loading or boot-time configuration is supplied. See
``TESTING.rst`` and the kernel documentation before any manual hardware test.
This source uses recent kernel APIs; compatibility with older kernels is
not promised. The validation record identifies the headers actually tested.

To review the in-tree changes instead, use the patch series below.
In a separate checkout at the stated base, apply patches 1 and 2 in order.
Keep patch 0 as a cover letter; it is not a code patch. For example, from
that checkout, substituting the actual package path::

    git apply /path/to/cu0019-kernel-rfc/patches/0001-*.patch
    git apply /path/to/cu0019-kernel-rfc/patches/0002-*.patch

To run the host harness against the resulting tree's helpers instead of
the repository snapshot::

    make check KERNEL_TREE=/path/to/linux

Host shims are used only by the host harness; they are not in the kernel
series. These tests issue no USB requests. A host harness pass is not a KUnit
runtime pass, and neither validates USB lifetime or power management.

Alternatively, for a temporary external build, copy the driver and protocol
header from ``drivers/hid/`` into a dedicated build directory. Copy
``external-module.mk`` there as ``Makefile``, then run from that directory::

    make -C /lib/modules/$(uname -r)/build M="$PWD" W=1 modules

This builds the module; it does not install or load it. The mainline Kconfig
options default to no companion and no active reads. Consult the patched
kernel documentation for manual opt-in usage. The old prototype and this
RFC use the same debugfs root and must not be loaded together.

The KUnit suite contains seven cases, including exhaustive 16-bit address,
DMA and PLL guards. On a suitable full kernel tree with a working KUnit/UML
toolchain, ``kunitconfig`` can be supplied to ``kunit.py``. No successful
KUnit execution is claimed in this package.

Measurements
============

No raw capture, server log, firmware/OTP dump or private host inventory is
included in this repository. ``VALIDATION.rst`` separates the older
prototype's exploratory observations from the untested rewritten module.

Start with ``TESTING.rst``. Collection only reads debugfs; it does not
generate disk traffic. The optional ``--keep-awake`` temporarily opens the
matching hidraw node without reading input or issuing vendor ioctls. It
does not write the runtime-PM policy and closes the descriptor on exit.
This is an explicit opt-in which keeps the USB receiver active for capture;
it does not prove that the mouse radio is awake.

Store captures privately. They contain timestamps and HID device names.
Only publish carefully reviewed, minimized measurements, not server logs,
credentials or an unfiltered sysfs inventory.

Human review and publication
============================

All code and drafts still need human review before kernel submission.
The commits have
``Assisted-by: LLM`` attribution and intentionally **no Signed-off-by**.
The human submitter must understand the complete contribution, check its
licensing and personally certify the Developer Certificate of Origin by
adding their sign-off to each commit. An assistant cannot certify it.

AI assistance was used for the module, protocol helpers, tests, build
packaging, documentation and commit/cover-letter drafts. The work aimed to
keep normal HID handling unchanged, restrict undocumented reads and report
remaining interpretation and validation gaps. See the cover letter for
details. No human DCO certification is implied by GitHub publication.

The prepared email identity is ``jerome.tollet@gmail.com``. Recipient
candidates were derived from the base tree's ``MAINTAINERS`` using
``scripts/get_maintainer.pl``; refresh them against the eventual submission
base. Neither recipient discovery nor this draft authorizes sending.

Suggested sequence: discuss the design using this experimental repository;
review the code and run the outstanding tests; record actual results
without changing unknown fields into claims; decide whether kernel
integration is justified; add human DCO certification and regenerate the
series before a formal patch submission. Sending any email is a separate
operation and requires explicit approval.

References
==========

* `Kernel coding-assistant rules
  <https://docs.kernel.org/process/coding-assistants.html>`_
* `Kernel tool-generated-content guidance
  <https://docs.kernel.org/process/generated-content.html>`_
* `Kernel patch submission process
  <https://docs.kernel.org/process/submitting-patches.html>`_
* `C-U0019 protocol reverse engineering
  <https://dev.to/endes/usb-hid-down-the-rabbit-hole-reverse-engineering-the-logitech-cu0019-usb-receiver-4d6a>`_
* `Intel USB3/2.4 GHz interference white paper
  <https://www.usb.org/sites/default/files/327216.pdf>`_
