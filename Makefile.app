#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

WUT_ROOT := $(DEVKITPRO)/wut

#-------------------------------------------------------------------------------
# Standalone Title Switcher app (.wuhb)
#-------------------------------------------------------------------------------
TARGET		:=	TitleSwitcher
BUILD		:=	build_app
SOURCES		:=	src/shell_app \
				src/core/common src/core/input src/core/render \
				src/core/titles src/core/storage src/core/menu src/core/menu/panels \
				src/core/editor src/core/presets src/core/ui
DATA		:=	data
INCLUDES	:=	src src/core src/shell_app \
				src/core/common src/core/input src/core/render \
				src/core/titles src/core/storage src/core/menu src/core/menu/panels \
				src/core/editor src/core/presets src/core/ui src/core/utils

# Metadata for wuhbtool
APP_NAME	:=	Title Switcher
APP_SHORTNAME	:=	TitleSwitcher
APP_AUTHOR	:=	R11
APP_ICON	:=	$(TOPDIR)/assets/icon.png
APP_TV_SPLASH	:=	$(TOPDIR)/assets/tv-splash.png
APP_DRC_SPLASH	:=	$(TOPDIR)/assets/drc-splash.png

#-------------------------------------------------------------------------------
CFLAGS	:=	-Wall -O3 -ffunction-sections \
			$(MACHDEP)

CFLAGS	+=	$(INCLUDE) -D__WIIU__ -D__WUT__ -DSHELL_APP

CXXFLAGS	:= $(CFLAGS) -std=c++20

ASFLAGS	:=	$(ARCH)
LDFLAGS	=	$(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

ifeq ($(DEBUG),1)
CXXFLAGS += -DDEBUG -g
CFLAGS += -DDEBUG -g
endif

LIBS	:= -lwhb -lgd -lpng -ljpeg -lz -lwut

LIBDIRS	:= $(PORTLIBS) $(WUT_ROOT)

#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# Pass wuhb metadata into the wuhbtool invocation below.
export APP_NAME APP_SHORTNAME APP_AUTHOR APP_ICON APP_TV_SPLASH APP_DRC_SPLASH

.PHONY: $(BUILD) clean all

#-------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@$(shell [ ! -d $(BUILD) ] && mkdir -p $(BUILD))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile.app

#-------------------------------------------------------------------------------
clean:
	@echo clean app ...
	@rm -fr $(BUILD) $(TARGET).wuhb $(TARGET).rpx $(TARGET).elf

#-------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
all	:	$(OUTPUT).wuhb

# wuhbtool wraps an .rpx with metadata + optional icon/splash images into a .wuhb
$(OUTPUT).wuhb : $(OUTPUT).rpx
	@echo wuhb $(notdir $@)
	@WUHB_OPTS="--name=\"$(APP_NAME)\" --short-name=\"$(APP_SHORTNAME)\" --author=\"$(APP_AUTHOR)\""; \
	[ -f "$(APP_ICON)" ] && WUHB_OPTS="$$WUHB_OPTS --icon=$(APP_ICON)"; \
	[ -f "$(APP_TV_SPLASH)" ] && WUHB_OPTS="$$WUHB_OPTS --tv-image=$(APP_TV_SPLASH)"; \
	[ -f "$(APP_DRC_SPLASH)" ] && WUHB_OPTS="$$WUHB_OPTS --drc-image=$(APP_DRC_SPLASH)"; \
	eval wuhbtool "$<" "$@" $$WUHB_OPTS

$(OUTPUT).rpx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

%.bin.o	%_bin.h :	%.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
