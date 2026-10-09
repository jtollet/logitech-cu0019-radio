.. SPDX-License-Identifier: GPL-2.0-only

=======================================
Logitech C-U0019 kernel RFC preparation
=======================================

Author: Jerome Tollet <jerome.tollet@gmail.com>

This directory contains an **unpublished review draft**, not an accepted
upstream driver or a production-safe release. No email or GitHub publication
has been performed. The existing standalone module on the test server has
not been replaced, autoloaded or reconfigured by this preparation.

The receiver is Logitech C-U0019, USB ID 046d:c542. Active reads are
restricted to bcdDevice 0302. The receiver model, rather than an inferred
chip identity, names the module. Normal input remains with hid-generic and
usbhid. The RFC adds no binding, service, timer or background poller.

Contents
========

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

In a separate checkout at the stated base, apply patches 1 and 2 in order.
Keep patch 0 as a cover letter; it is not a code patch. For example, from
that checkout, substituting the actual package path::

    git apply /path/to/cu0019-kernel-rfc/patches/0001-*.patch
    git apply /path/to/cu0019-kernel-rfc/patches/0002-*.patch

From this package directory, run the tests against the resulting helpers::

    make check KERNEL_TREE=/path/to/linux

Host shims are used only by the host harness; they are not in the kernel
series. These tests issue no USB requests. A host harness pass is not a KUnit
runtime pass, and neither validates USB lifetime or power management.

For a temporary external build, copy the driver, protocol header and KUnit
test source from ``drivers/hid/`` into a dedicated build directory. Copy
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

All code and drafts must be reviewed before publication. The commits have
``Assisted-by: LLM`` attribution and intentionally **no Signed-off-by**.
The human submitter must understand the complete contribution, check its
licensing and personally certify the Developer Certificate of Origin by
adding their sign-off to each commit. An assistant cannot certify it.

The prepared email identity is ``jerome.tollet@gmail.com``. Recipient
candidates were derived from the base tree's ``MAINTAINERS`` using
``scripts/get_maintainer.pl``; refresh them against the eventual submission
base. Neither recipient discovery nor this draft authorizes sending.

Suggested sequence: review the design and code; run the outstanding tests;
record actual results without changing unknown fields into claims; decide
whether kernel integration is justified; add human DCO certification;
regenerate the series; publish a reviewed repository; send a separate RFC
only after explicit approval.

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
