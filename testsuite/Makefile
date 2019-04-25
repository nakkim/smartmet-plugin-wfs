INCDIR = smartmet/plugins/$(SUBNAME)
TOP = $(shell pwd)/..

# Compiler options

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always

DEFINES = -DUNIX -D_REENTRANT


FLAGS = -std=c++11 -fPIC -Wall -W -Wno-unused-parameter -Wno-unknown-pragmas \
      -Wcast-align \
      -Wcast-qual \
      -Wno-inline \
      -Wno-multichar \
      -Wno-pmf-conversions \
      -Woverloaded-virtual  \
      -Wpointer-arith \
      -Wredundant-decls \
      -fdiagnostics-color=$(GCC_DIAG_COLOR) \
      -Wwrite-strings

# Compile options in detault, debug and profile modes

CFLAGS_RELEASE = $(DEFINES) $(FLAGS) -DNDEBUG -O2 -g
CFLAGS_DEBUG   = $(DEFINES) $(FLAGS) -Werror  -O0 -g

ARFLAGS = -r
DEFINES = -DUNIX -D_REENTRANT

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  override CFLAGS += $(CFLAGS_DEBUG)
else
  override CFLAGS += $(CFLAGS_RELEASE)
endif

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

bindir = $(PREFIX)/bin
includedir = $(PREFIX)/include
datadir = $(PREFIX)/share
enginedir = $(datadir)/smartmet/engines
plugindir = $(datadir)/smartmet/plugins
objdir = obj

#

INCLUDES = -I$(includedir) \
	-I$(includedir)/smartmet \
	-I$(includedir)/mysql \
	-Iinclude

LIBS = -L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lboost_date_time \
        -lboost_serialization \
	-lboost_thread \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_filesystem \
        -lboost_chrono \
	-lboost_system \
        -lxqilla \
	-lxerces-c \
	-lgdal \
	-lpqxx -lpq \
	-lconfig++ \
	-lconfig \
	-lctpp2 \
	-lcurl \
	-lcrypto \
	-lbz2 -lz \
	-lpthread \
	-lm \
	-ldl

INCLUDES := -I$(TOP)/include $(INCLUDES)

obj/%.o : %.cpp ; @echo Compiling $<
	@mkdir -p obj
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d.new, $@) -o $@ $<
	@sed -e "s|^$(notdir $@):|$@:|" $(patsubst obj/%.o, obj/%.d.new, $@) >$(patsubst obj/%.o, obj/%.d, $@)
	@rm -f $(patsubst obj/%.o, obj/%.d.new, $@)



TESTSUITE_SRCS = $(wildcard *.cpp)
TESTSUITE_OBJS = $(patsubst %.cpp,obj/%.o,$(TESTSUITE_SRCS))
TESTSUITE_TARGETS = $(patsubst %.cpp,%.test,$(TESTSUITE_SRCS))

all:

clean:
	rm -rf obj/*.o obj/*.d
	rm -rf $(TESTSUITE_TARGETS)

check:	../libwfs.a $(TESTSUITE_TARGETS)
	@ok=true; \
	for test in $(TESTSUITE_TARGETS); do \
		if ! LD_LIBRARY_PATH=$(libdir) ./$$test --log_level=message ; then ok=false; fi; \
	done; \
	$$ok;

check-valgrind:	../libwfs.a $(TESTSUITE_TARGETS)
	@ok=true; \
	for test in $(TESTSUITE_TARGETS); do \
		if ! LD_LIBRARY_PATH=$(libdir) valgrind ./$$test --log_level=message ; then ok=false; fi; \
	done; \
	$$ok;

../libwfs.a:
	$(MAKE) -C .. libwfs.a

$(TESTSUITE_TARGETS) : ../libwfs.a

%.test : obj/%.o ; @echo "Building $@"
	$(CXX) -o $@ $(TESTSUITE_CFLAGS) $(INCLUDES) $< -Ltestsuite -Wl,-rpath=$(libdir) -L.. -lwfs $(LIBS)

CFLAGS += -I include

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
