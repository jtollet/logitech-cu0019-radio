# SPDX-License-Identifier: GPL-2.0-only
KERNEL_TREE ?= .
KDIR ?= /lib/modules/$(shell uname -r)/build
HOSTCC ?= cc

.PHONY: check check-python module
check: check-python
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Itests/shims -I$(KERNEL_TREE)/drivers/hid \
		tests/protocol.c -o tests/protocol-test
	./tests/protocol-test

check-python:
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v

# Linux only. This compiles the module; it never installs or loads it.
module:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)/drivers/hid" W=1 modules
