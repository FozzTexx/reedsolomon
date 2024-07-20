ifdef APPLE2
CC=cl65
PLATFORM=apple2enh
ARCH := $(PLATFORM)
RC_HOSTTYPE := $(PLATFORM)
BINDIR := .
else
CC=gcc
CFLAGS=-g -Wall -Wextra -std=c99 -O2
ARCH := $(shell arch)
RC_HOSTTYPE := $(OSTYPE)-$(ARCH)
BINDIR := $(RC_HOSTTYPE)_bin
endif

OBJDIR := $(RC_HOSTTYPE)_obj

SRC= op.c decode.c encode.c test.c array.c
OBJECTS=$(SRC:.c=.o)
DEP=$(SRC:.c=.d)
OBJ := $(addprefix $(OBJDIR)/, $(OBJECTS))

define single-to-double
  unsingle $< && mv $<.ad $@ && mv .AppleDouble/$<.ad .AppleDouble/$@
endef

define link-bin
  $(CC) -t $(PLATFORM) $^ -o $@
endef

all: $(BINDIR)/test

ifdef APPLE2
$(BINDIR)/test: $(BINDIR)/test.bin
	$(single-to-double)

$(BINDIR)/test.bin: $(OBJ) | $(BINDIR)
	$(link-bin)
else
$(BINDIR)/test: $(OBJ) | $(BINDIR)
	$(CC) $^ -o $@
endif

.PHONY: clean

clean:
	$(RM) -rf $(OBJDIR) $(BINDIR)
	$(RM) $(DEP)

-include $(DEP)

ifdef APPLE2
$(OBJDIR)/%.o : %.c
	$(CC) -l $(basename $@).lst -c -O -t $(PLATFORM) $< -o $(notdir $@) \
	  && mv $(notdir $@) $@
else
$(OBJDIR)/%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
endif

$(OBJDIR):
	mkdir $(OBJDIR)
$(BINDIR):
	mkdir $(BINDIR)
$(OBJ): | $(OBJDIR)

depend: $(DEP)

%.d : %.c
	$(CC) -MM $< | sed -re 's,([^:]+:),$$(OBJDIR)/\1,' > $@
