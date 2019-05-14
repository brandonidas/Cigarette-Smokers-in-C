UTHREAD = .
TARGETS = smoke  

JUNKF = *.o *~ *.dSYM
WARNINGS = -Wall -Wno-unused-variable -Wno-unused-const-variable

override CFLAGS  += $(WARNINGS) -g -std=gnu11 -I$(UTHREAD)
override LDFLAGS += -pthread

all: $(TARGETS)

smoke:          smoke.o          uthread.o uthread_mutex_cond.o
smoke.o:          smoke.c          uthread.h uthread_mutex_cond.h
uthread.o:        uthread.c        uthread.h uthread_util.h
uthread_mutex_cond.o: uthread_mutex_cond.c uthread_mutex_cond.h uthread.h uthread_util.h

tidy:
	-rm -rf $(JUNKF)
clean: tidy
	-rm -rf $(TARGETS)
