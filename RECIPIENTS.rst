.. SPDX-License-Identifier: GPL-2.0-only

============================
RFC recipient review draft
============================

Sender identity: Jerome Tollet <jerome.tollet@gmail.com>.
No email has been sent and this file is not an authorization to send.

Run on the series against its stated base::

    scripts/get_maintainer.pl --no-tree --no-git --no-git-fallback \
        --no-rolestats /path/to/patches/0001-*.patch /path/to/patches/0002-*.patch

The sparse-checkout run on 9 October 2026 returned:

* Jiri Kosina <jikos@kernel.org>
* Benjamin Tissoires <bentiss@kernel.org>
* Jonathan Corbet <corbet@lwn.net>
* Shuah Khan <skhan@linuxfoundation.org>
* Randy Dunlap <rdunlap@infradead.org>
* Filipe Laíns <lains@riseup.net>
* linux-input@vger.kernel.org
* linux-doc@vger.kernel.org
* linux-kernel@vger.kernel.org

The HID maintainers and linux-input are the primary audience; documentation
and other recipients appear because of files/patterns matched by the script.
Review the final per-patch recipient set and refresh against the submission
base. Do not mistake automatic discovery for an endorsement of this design.
