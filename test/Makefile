INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TEST_TOP = $(shell pwd)
TOP = $(TEST_TOP)/..

TEST_DIRECTORIES = base real

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

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize-address-use-after-scope
endif

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
	-I$(includedir)/mysql \
	-I$(includedir)/jsoncpp

LIBS = \
	$(BOOST_LIBS) \
	-L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lboost_date_time \
        -lboost_serialization \
	-lboost_thread \
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
	-lbz2 -lz \
	-lstdc++ \
	-lssl \
	-lpthread \
	-lm

INCLUDES := -I$(TOP)/include $(INCLUDES)

obj/%.o : %.cpp ; @echo Compiling $<
	@mkdir -p obj
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d.new, $@) -o $@ $<
	@sed -e "s|^$(notdir $@):|$@:|" $(patsubst obj/%.o, obj/%.d.new, $@) >$(patsubst obj/%.o, obj/%.d, $@)
	@rm -f $(patsubst obj/%.o, obj/%.d.new, $@)



PLUGIN_TEST_SRCS = PluginTest.cpp
TEST_OBJS = $(patsubst %.cpp,obj/%.o,$(TEST_SRCS))
TEST_TARGETS = PluginTest

all:

clean:
	rm -rf obj/*.o obj/*.d
	rm -vf $(TEST_TARGETS)
	for test_dir in $(TEST_DIRECTORIES) ; do $(MAKE) -C $$test_dir clean; done

test: $(TEST_TARGETS)
	@ok=true; \
	for test_dir in $(TEST_DIRECTORIES); do \
		if ! LD_LIBRARY_PATH=../../../spine:$(libdir) $(MAKE) -C $$test_dir $@ ; then ok=false; fi; \
	done; \
	$$ok;

PluginTest : obj/PluginTest.o ; @echo "Building $@"
	@$(CC) -o $@ $(CFLAGS) $(INCLUDES) $< -Ltestsuite -Wl,-rpath=$(libdir) $(LIBS)


ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
