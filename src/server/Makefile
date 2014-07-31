CCSOURCES = readstat.c main.c array.c

CC = gcc
CCFLAGS_RELEASE = -std=c99 -O3 -DNDEBUG
CCFLAGS_DEBUG = -std=c99 -O0 -ggdb -g3 -fno-inline -D_DEBUG
CCFLAGS = -Wall $(CCFLAGS_DEBUG)
OBJDIR = .build

LN = gcc
LNFLAGS = -std=c99

OUTNAME = remote-pimon-server
OUTDIR = .out

server:  link

OBJS = $(addprefix $(OBJDIR)/, $(CCSOURCES:.c=.o))
DEPS = $(OBJS:.o=.d)

#$(OBJDIR):
#	@mkdir -p $(foreach SOURCE,$(CXXSOURCES),$(OBJDIR)/$(dir $(SOURCE)))
	
#$(OUTDIR):
#	@mkdir -p $(OUTDIR)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) -MMD -c -o $@ $<

$(OUTDIR)/$(OUTNAME): $(OBJS)
	@mkdir -p $(OUTDIR)
	$(LN) $(LNFLAGS) -o $(OUTDIR)/$(OUTNAME) $(OBJS)

-include $(DEPS)

link: $(OUTDIR)/$(OUTNAME)

.PHONY: clean
clean:
	rm -rf $(OBJS) $(DEPS) $(OUTDIR)/$(OUTNAME)
