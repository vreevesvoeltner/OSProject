PREFIX = ${HOME}

TARGET = libphase5.a
ASSIGNMENT = 452phase5
CC = gcc
AR = ar

COBJS = phase5.o p1.o libuser5.o
CSRCS = ${COBJS:.o=.c}

PHASE1LIB = patrickphase1
PHASE2LIB = patrickphase2
PHASE3LIB = patrickphase3
PHASE4LIB = patrickphase4
#PHASE1LIB = patrickphase1debug
#PHASE2LIB = patrickphase2debug
#PHASE3LIB = patrickphase3debug
#PHASE4LIB = patrickphase4debug

HDRS = vm.h

${COBJS}:	${HDRS}

INCLUDE = ${PREFIX}/include

# -DPHASE_3 is needed here to get the additional definitions from
#     usyscall.h -- these include SYS_VMINIT and SYS_VMDESTROY
CFLAGS = -Wall -g -std=gnu99 -I. -I${INCLUDE} -DPHASE_3

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
        CFLAGS += -D_XOPEN_SOURCE
endif

LDFLAGS += -L. -L${PREFIX}/lib

TESTDIR = testcases
TESTS = test01 test02 test03 test04 test05 test06 test07 test08 \
	test09 test10 test11

LIBS = -lusloss3.6 -l$(PHASE1LIB) -l$(PHASE2LIB) -l$(PHASE3LIB) \
       -l$(PHASE4LIB) -lphase5 -l$(PHASE4LIB)

$(TARGET):	$(COBJS) $(HDRS)
		$(AR) -r $@ $(COBJS) 

$(TESTS):	$(TARGET)
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS)

clean:
	rm -f $(COBJS) $(TARGET) test??.o test?? test??.txt \
		term[0-3].out disk0 disk1

submit: $(CSRCS) $(HDRS)
	tar cvzf phase5.tgz $(CSRCS) $(HDRS) Makefile

