.. SPDX-License-Identifier: GPL-2.0-only

================================
Validation record: 9 October 2026
================================

This record concerns the new RFC, not the earlier standalone module.
No controlled RF measurements have been taken for this series.

Source and patch checks
=======================

Base: ``af32da41b0327b9c6a37856ba82b6760d6c8d10e``.
Local kernel commits:

* ``049a88c36005066c741e1c25f6a3749608d8fbe6``: companion and documentation.
* ``dca71b782577a9bde5f75359355bc81245426f0f``: KUnit protocol tests.

``git diff --check`` on the base-to-series diff: successful.

Strict source-file checkpatch on the module, header and KUnit source:
zero errors and zero warnings. Rechecking the standalone snapshots with
``--no-tree --strict --file`` reports one CAMELCASE CHECK for the existing
USB descriptor member ``bcdDevice``; that standard kernel field is unchanged.
Strict patch-mode checkpatch:
zero errors and zero checks, with one generic MAINTAINERS path-change
advisory per patch. Existing ``Documentation/hid/``, ``drivers/hid/`` and
``drivers/hid/hid-logitech-*`` patterns already cover the added paths.
``MISSING_SIGN_OFF`` was intentionally excluded from style checking; the
draft is not DCO-certified and is not ready to send without human sign-off.

The scripts used ``--no-tree`` because the validation checkout is sparse;
this does not skip parsing the patches or the local MAINTAINERS file.

Completed compilation and offline tests
======================================

* External-module build against running 7.2.6-070206-generic headers with
  GCC 15.2 and ``W=1``: successful, no driver C diagnostics.
* KUnit source compiled as an object against those headers: successful.
  It has NOT been linked into or executed in a KUnit-enabled kernel.
* Exact protocol-helper host harness on macOS (Clang) and Linux (GCC):
  successful. It exhaustively validates the 16-bit address allowlist,
  request construction/rejection, reply size/signature, DMA/RF guards and
  PLL range. These tests do not communicate with the USB receiver.
* Nine Python capture/summary unit tests on macOS and Linux: successful.
  Hardware access is mocked; no hardware capture result is implied.

Publication packaging added a tenth Python test that reconstructs the four
new source/documentation files from patches 1 and 2 and compares them to
the browsable snapshots. ``make check`` passes all ten Python tests and
the exhaustive C host harness on macOS, without an external kernel tree.
GitHub Actions is configured to run the same offline checks on Linux and
macOS. This workflow does not build or load a kernel module or run KUnit.

The external build reports compiler package revisions different from the
kernel build (GCC 15.2-16 versus 15.2-12), pahole 1.30 versus 1.31, and skipped
BTF generation because vmlinux is unavailable. These are recorded build
environment limitations, not hidden driver warnings. A full build against
the stated mainline base remains pending.

Runtime validation and gaps
==========================

The final RFC module was compiled but **not loaded**. Runtime observations
below concern the older ``logitech_cu0019_debug`` prototype, not the sources
in this repository. The prototype was unloaded after the latest capture.
Publication does not change host driver bindings, runtime-PM policy,
services, timers or boot configuration. No current remote-host state is
inferred from this historical record.

Exploratory prototype observations
=================================

On 9 October 2026, direct / USB2-extension / direct exploratory samples
had raw-value medians of 40 / 16 / 40. The first two phases contained
30 successful samples each. The return-to-direct phase was interrupted
after 23 successful samples by USB hub disconnections; connected storage
also went offline. The cause was not established. The capture did not
request a USB reset or a power-policy change. This incident is a reason
to use a disposable test host for further lifetime and physical-USB tests.

A later direct-connected capture after reboot collected 30 successful
samples with raw-value median 40, minimum 16 and maximum 48. Its diagnostic
counters showed 240 firmware read commands and zero read errors. The
prototype was unloaded afterward.

The user reported smoother mouse behavior with the extension. Geometry,
movement, channel selection and possible nearby interference were not
controlled enough to infer calibrated signal strength, noise power,
packet loss or USB3 causation. These are exploratory prototype results,
not a controlled RF comparison or runtime validation of the RFC module.
Raw captures and host logs remain private and are excluded from publication.

Outstanding tests
=================

Earlier prototype load/unload, cache and PM observations are not runtime
validation of the rewritten RFC. Physical unplug/replug, system suspend,
lockdep/KASAN, allocation failure, feature-channel arbitration, KUnit
execution, full mainline/documentation builds and controlled USB3 comparisons
are still outstanding. See TESTING.rst before any hardware operations.

Patch application
=================

Both patches were applied in order to a separate temporary Git index loaded
from the stated base, without changing the working checkout. The resulting
tree and the series HEAD tree both equal::

    4d03a9bdbadb8647af4132d004b80f86cb40ec13

This validates patch application and reproduction of the prepared tree;
it is not a full build or a runtime test of that mainline tree.
