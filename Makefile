#-------------------------------------------------------------------------------
# Title Switcher dual-shell dispatcher
#
#   make           - build both the WUPS plugin (.wps) and the standalone app (.wuhb)
#   make plugin    - build only the WUPS plugin
#   make app       - build only the standalone homebrew app
#   make clean     - clean both
#-------------------------------------------------------------------------------

.PHONY: all plugin app clean plugin-clean app-clean

all: plugin app

plugin:
	@$(MAKE) -f Makefile.plugin

app:
	@$(MAKE) -f Makefile.app

clean: plugin-clean app-clean

plugin-clean:
	@$(MAKE) -f Makefile.plugin clean

app-clean:
	@$(MAKE) -f Makefile.app clean
