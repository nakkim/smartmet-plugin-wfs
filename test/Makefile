INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TEST_TOP = $(shell pwd)
TOP = $(TEST_TOP)/..

TEST_DIRECTORIES = base real

REQUIRES = gdal jsoncpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# Compiler options

ARFLAGS = -r
DEFINES = -DUNIX -D_REENTRANT

LIBS += \
	-L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-gis \
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
	$(GDAL_LIBS) \
	-lconfig++ \
	-lconfig \
	-lctpp2 \
	$(JSONCPP_LIBS) \
	-lcurl \
	-lcrypto \
	-lbz2 -lz \
	-lstdc++ \
	-lssl \
	-lpthread \
	-lm

INCLUDES := -I$(TOP)/wfs -I$(TOP)/libwfs $(INCLUDES)

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
	@$(CXX) -o $@ $(CFLAGS) $(INCLUDES) $< -Ltestsuite -Wl,-rpath=$(libdir) $(LIBS)


ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
