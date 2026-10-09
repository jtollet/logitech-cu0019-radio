# SPDX-License-Identifier: GPL-2.0-only
KERNEL_TREE ?= ../linux
HOSTCC ?= cc

.PHONY: check check-python
check: check-python
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -pedantic \
		-Itests/shims -I$(KERNEL_TREE)/drivers/hid \
		tests/protocol.c -o tests/protocol-test
	./tests/protocol-test

check-python:
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
