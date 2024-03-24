#!/bin/make
export	PREFIX	:=
export	CC	:=	$(PREFIX)emcc
export	LD	:=	$(CC)
export	OBJCOPY	:=	$(PREFIX)objcopy
export	NM	:=	$(PREFIX)nm
export	SIZE	:=	$(PREFIX)size
export	BIN2O	:=	bin2o
export	GLSLANG	:=	glslang

#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------
TARGET		:=	weblsi-11
INCLUDES	:=	include vt240/web/include
SOURCES		:=	src vt240/web/src
GLSLSOURCES	:=	vt240/web/glsl
BINSOURCES	:=	rx02 rl02
BUILD		:=	build

ASAN		:=	#-fsanitize=address
OPT		:=	-O3 -g0 -mnontrapping-fptoint

CFLAGS		:=	$(OPT) -Wall -std=gnu99 \
			-ffunction-sections -fdata-sections \
			$(INCLUDE) -DUNIX -DNDEBUG \
			-DBOOT_$(BOOT) $(NODLG) \
			$(RX2U0) $(RX2U1) $(RL2U0) \
			-DGL_GLEXT_PROTOTYPES -DVT240_NO_BUFFER \
			$(ASAN)

LIBS		:=	-lGL -lglut
LDFLAGS		:=	-Wl,--gc-sections $(LIBS) $(OPT) $(ASAN) \
			-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
GLSLFILES	:=	$(foreach dir,$(GLSLSOURCES),$(notdir $(wildcard $(dir)/*.glsl)))
BINFILES	:=	$(foreach dir,$(BINSOURCES),$(notdir $(wildcard $(dir)/*.bin)))

ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------
export	DEPSDIR	:=	$(CURDIR)/$(BUILD)
export	OFILES	:=	$(CFILES:.c=.o) $(GLSLFILES:.glsl=.o) $(BINFILES:.bin=.o)
export	VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GLSLSOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(BINSOURCES),$(CURDIR)/$(dir)) $(CURDIR)
export	INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
				-I$(CURDIR)/$(BUILD)
export	OUTPUT	:=	$(CURDIR)/$(TARGET)

ifndef BOOTDEV
export	BOOT	:=	RX02
else
export	BOOT	:=	$(BOOTDEV)
endif

ifdef RX2IMG0
export	RX2U0	:=	-DRX2U0IMG=$(RX2IMG0)
endif

ifdef RX2IMG1
export	RX2U1	:=	-DRX2U1IMG=$(RX2IMG1)
endif

ifdef RL2IMG
export	RL2U0	:=	-DRL2IMG=$(RL2IMG)
endif

ifndef NODIALOG
export	NODLG	:=
else
export	NODLG	:=	-DBOOT_NODLG
endif

.PHONY: $(BUILD) clean all

$(BUILD):
	@echo compiling...
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo "[CLEAN]"
	@rm -rf $(BUILD) $(TFILES) $(OFILES) demo

$(TARGET): $(TFILES)

else

#-------------------------------------------------------------------------------
# main target
#-------------------------------------------------------------------------------
.PHONY: all

all: $(TARGET).js

%.o: %.c
	@echo "[CC]    $(notdir $@)"
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

%.o: %.glsl
	@echo "[GLSL]  $(notdir $@)"
	@$(GLSLANG) $<
	@echo -e "const char $(subst .,_,$(basename $@))[] = { $(shell cat $< | sed -e 's/#version 330/#version 300 es\nprecision highp float;precision highp int;precision highp usampler2D;/' | xxd -i), 0 };" | $(CC) $(OPT) -std=c99 -c -x c -o $@ -

%.o: %.bin
	@echo "[BIN]   $(notdir $@)"
	@cat <(echo -e "char $(subst .,_,$(basename $@))[] = { ") <(cat $< | xxd -i) <(echo -n " };") | $(CC) $(OPT) -std=c99 -c -x c -o $@ -

$(TARGET).js: $(OFILES)
	@echo "[LD]    $(notdir $@)"
	@$(LD) $(LDFLAGS) $(OFILES) -o $@ -Wl,-Map=$(@:.elf=.map)

-include $(DEPSDIR)/*.d

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
