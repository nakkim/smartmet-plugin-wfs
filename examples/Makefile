INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TOP = $(shell pwd)/..

# Compiler options

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
  BOOST_INCLUDES = -I/usr/include/boost169
  BOOST_LIBS = -L/usr/lib64/boost169
  FLAGS += -Wno-overloaded-virtual -Wno-redundant-decls
endif

INCLUDES = \
	$(BOOST_INCLUDES) \
	-I$(includedir) \
	-I$(includedir)/smartmet \
	-I$(includedir)/mysql

LIBS = -L$(libdir) \
	$(BOOST_LIBS) \
	-lsmartmet-spine \
	-lsmartmet-gis \
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
	-lpqxx \
	-lconfig++ \
	-lconfig \
	-lctpp2 \
	-lcurl \
	-lcrypto \
	-lbz2 -lz \
	-lpthread \
	-lm

INCLUDES := -I$(TOP)/libwfs $(INCLUDES)

obj/%.o : %.cpp ; @echo Compiling $<
	@mkdir -p obj
	$(CXX) -v $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d.new, $@) -o $@ $<
	@sed -e "s|^$(notdir $@):|$@:|" $(patsubst obj/%.o, obj/%.d.new, $@) >$(patsubst obj/%.o, obj/%.d, $@)
	@rm -f $(patsubst obj/%.o, obj/%.d.new, $@)



EXAMPLE_SRCS = $(wildcard source/*.cpp)
EXAMPLE_OBJS = $(patsubst source/%.cpp,obj/%.o,$(EXAMPLE_SRCS))
EXAMPLE_BINS = $(patsubst source/%.cpp,%,$(EXAMPLE_SRCS))

vpath %.cpp source

all examples:	$(EXAMPLE_BINS)

clean:
	rm -rf obj
	rm -f $(EXAMPLE_BINS)

../libwfs.a:
	$(MAKE) -C .. libwfs.a

.SUFFIXES: $(SUFFIXES) .cpp

%:  %.cpp; echo Building $@
	$(CXX) -o $@ $(CFLAGS) $(INCLUDES) $< -L .. -Wl,--rpath=$(libdir) -lwfs $(LIBS)

$(EXAMPLE_BINS): ../libwfs.a

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
