
INCDIR = smartmet/plugins/$(SUBNAME)
TOP = $(shell pwd)/..

REQUIRES_GDAL = yes

include ../common.mk

# Compiler options

DEFINES = -DUNIX -D_REENTRANT

#BOOST169_WARNING_DISABLE = -Wno-redundant-decls -Wno-overloaded-virtual
#
#FLAGS = -std=c++11 -fPIC -Wall -W -Wno-unused-parameter -Wno-unknown-pragmas \
#      -Wcast-align \
#      -Wcast-qual \
#      -Wno-inline \
#      -Wno-multichar \
#      -Wno-pmf-conversions \
#      -Woverloaded-virtual  \
#      -Wpointer-arith \
#      -Wredundant-decls \
#      -fdiagnostics-color=$(GCC_DIAG_COLOR) \
#      $(BOOST169_WARNING_DISABLE) \
#      -Wwrite-strings

# Compile options in detault, debug and profile modes

INCLUDES += \
	-Iinclude \
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

LIBWFS_INCLUDES := -I$(includedir)/smartmet/plugin/gribwfs
LIBWFS_LDFLAGS := -lsmartmet-plugin-gribwfs

else

LIBWFS_INCLUDES := -I../libwfs
LIBWFS_LDFLAGS := ../libsmartmet-plugin-gribwfs.a

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
