INCDIR = smartmet/plugins/$(SUBNAME)
TOP = $(shell pwd)/..

# Compiler options

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always

DEFINES = -DUNIX -D_REENTRANT

BOOST169_WARNING_DISABLE = -Wno-redundant-decls -Wno-overloaded-virtual



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
      $(BOOST169_WARNING_DISABLE) \
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

# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -I/usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

INCLUDES += -I$(includedir) \
	-I$(includedir)/smartmet \
	-I$(includedir)/mysql \
	-I$(includedir)/jsoncpp \
	-Iinclude

LIBS += -L$(libdir) \
	-lsmartmet-engine-gis \
	-lsmartmet-gis \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
        -lboost_unit_test_framework \
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
	-ljsoncpp \
	-lcurl \
	-lcrypto \
	-lfmt \
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

$(TESTSUITE_TARGETS) : ../libwfs.a gis_engine_link

%.test : obj/%.o ; @echo "Building $@"
	$(CXX) -o $@ $(TESTSUITE_CFLAGS) $(INCLUDES) $< -Ltestsuite -Wl,-rpath=$(shell pwd) -L . ../libwfs.a $(LIBS)

gis_engine_link:
	rm -f libsmartmet-engine-gis.so
	ln -sf /usr/share/smartmet/engines/gis.so libsmartmet-engine-gis.so

CFLAGS += -I include

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
