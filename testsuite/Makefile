
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
  INCLUDES += -isystem /usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

INCLUDES += \
	-Iinclude \
	-I$(includedir)/smartmet \
	-isystem $(includedir)/mysql \
	-isystem $(includedir)/jsoncpp

LIBS += -L$(libdir) \
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

ifneq (,$(findstring check-installed,$(MAKECMDGOALS)))

LIBWFS_INCLUDES := -I$(includedir)/smartmet/plugin/wfs
LIBWFS_LDFLAGS := -lsmartmet-plugin-wfs

else

LIBWFS_INCLUDES := -I../libwfs
LIBWFS_LDFLAGS := ../libsmartmet-plugin-wfs.a

endif

INCLUDES := $(LIBWFS_INCLUDES) $(INCLUDES)

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

check check-installed:	$(TESTSUITE_TARGETS)
	@ok=true; \
	for test in $(TESTSUITE_TARGETS); do \
		if ! LD_LIBRARY_PATH=$(libdir) ./$$test --log_level=message ; then ok=false; fi; \
	done; \
	$$ok;

check-valgrind:	$(TESTSUITE_TARGETS)
	@ok=true; \
	for test in $(TESTSUITE_TARGETS); do \
		if ! LD_LIBRARY_PATH=$(libdir) valgrind ./$$test --log_level=message ; then ok=false; fi; \
	done; \
	$$ok;

%.test : obj/%.o ; @echo "Building $@"
	$(CXX) -o $@ $(TESTSUITE_CFLAGS) $(INCLUDES) $< -Ltestsuite $(LIBWFS_LDFLAGS) $(LIBS)

CFLAGS += -I include -I $(TOP)/libwfs

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
