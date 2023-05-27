#@BEGIN-C_SRC-DEFAULT-RULES@
ERL=$(shell readlink -f `which erl`)
ERL_TOP=$(ERL:%/bin/erl=%)
OS=$(shell uname -s)
DEPFLAGS=-MMD -MP -MF .$<.d
CFLAGS=$(DEPFLAGS) -I$(ERL_TOP)/usr/include

#@END-C_SRC-DEFAULT-RULES@

CXX=g++

APP=matrix
CFLAGS+= -Wall -Wextra -Wswitch-enum -Wswitch-default -fno-common -O2 # --std=gnu11
CFLAGS+= -msse3 # -mavx2  # -msse3
# raspberry pi
# CFLAGS+=-mfloat-abi=softfp -mfpu=neon -flax-vector-conversions
# CFLAGS+=-mfpu=neon -flax-vector-conversions
CXXFLAGS+= -Wall -Wextra -Wswitch-enum -Wswitch-default -fno-common -g #-O2
CXXFLAGS+=$(DEPFLAGS)
CXXFLAGS+= -msse3

LDFLAGS+=-shared

OBJS = jitter_x86.o jitter_emu.o jitter_util.o \
	jitter_test.o
LIBS = -lasmjit

all: jitter_test

jitter_test:	$(OBJS)
	$(CXX) $(OBJS) $(LIBS) -g -o $ $@

#jitter_test.o:	jitter.h jitter_types.h jitter_asm.h
#jitter_util.o:   jitter.h jitter_types.h
#jitter_emu.o:   jitter.h jitter_types.h
#jitter_x86.o:	jitter.h jitter_types.h jitter_asm.h

%.o:	%.c
	$(CC) $(CFLAGS) -c -fPIC $<

%.o:	%.cpp
	$(CXX) $(CXXFLAGS) -c -fPIC $<

.%.d:	;

-include .*.d
