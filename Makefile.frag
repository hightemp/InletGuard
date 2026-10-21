# SPDX-License-Identifier: MIT
# Extra targets added to the phpize-generated Makefile.

.PHONY: unit unit-asan

unit:
	$(MAKE) -C $(srcdir)/tests/unit run

unit-asan:
	$(MAKE) -C $(srcdir)/tests/unit clean
	$(MAKE) -C $(srcdir)/tests/unit run SANITIZE=1
