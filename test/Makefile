INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TEST_TOP = $(shell pwd)
TOP = $(TEST_TOP)/..

TEST_DIRECTORIES = base real

REQUIRES = gdal jsoncpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# Compiler options

ARFLAGS = -r
DEFINES = -DUNIX -D_REENTRANT

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/local.conf
TEST_FINISH_TARGETS := dummy
ifdef CI
GEONAMES_HOST_EDIT := sed -e 's/"smartmet-test"/"localhost"/g'
TEST_PREPARE_TARGETS += geonames-database
TEST_FINISH_TARGETS += stop-test-db
else
GEONAMES_HOST_EDIT := cat
endif

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

LIBS += \
	-L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-gis \
	-lsmartmet-macgyver \
	-lboost_date_time \
	-lboost_program_options \
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
	rm -f cnf/geonames.conf
	rm -fr tmp-geonames-db
	rm -f tmp-geonames-db.log

test test-sqlite test-oracle test-postgresql: $(TEST_TARGETS) $(TEST_PREPARE_TARGETS)
	@ok=true; \
	for test_dir in $(TEST_DIRECTORIES); do \
		if ! LD_LIBRARY_PATH=../../../spine:$(libdir) $(MAKE) -C $$test_dir $@ ; then ok=false; fi; \
	done; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok;

PluginTest : obj/PluginTest.o ; @echo "Building $@"
	@$(CXX) -o $@ $(CFLAGS) $(INCLUDES) $< -Ltestsuite -Wl,-rpath=$(libdir) $(LIBS)


cnf/geonames.conf: cnf/geonames.conf.in dummy
	$(GEONAMES_HOST_EDIT) $< >$@

geonames-database:
	rm -rf tmp-geonames-db
	if ! /usr/share/smartmet/test/db/create-local-db.sh $(TEST_DB_DIR) >tmp-geonames-db.log 2>&1 ; then \
	    cat tmp-geonames-db.log; \
	    false; \
	fi
	/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) start -w

stop-test-db:
	/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) stop

cnf/local.conf: dummy
	if [ -f /smartmet/cnf/smartmetd/dev/plugins/wfs.conf ] ; then \
	    grep ^geoserverConnStr /smartmet/cnf/smartmetd/dev/plugins/wfs.conf >$@; \
	else \
	    echo 'geoserverConnStr = "";' >$@ ; \
	fi

dummy:
	true

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
