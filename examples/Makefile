INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TOP = $(shell pwd)/..

REQUIRES = gdal

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

ARFLAGS = -r
DEFINES = -DUNIX -D_REENTRANT

INCLUDES += \
	-I$(includedir)/mysql

LIBS += -L$(libdir) \
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
	$(GDAL_LIBS) \
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

../libsmartmet-plugin-wfs:
	$(MAKE) -C .. libsmartmet-plugin-wfs.a

.SUFFIXES: $(SUFFIXES) .cpp

%:  %.cpp; echo Building $@
	$(CXX) -o $@ $(CFLAGS) $(INCLUDES) $< -L .. -Wl,--rpath=$(libdir) -lsmartmet-plugin-wfs $(LDFLAGS) $(LIBS)

$(EXAMPLE_BINS): ../libsmartmet-plugin-wfs.a

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
