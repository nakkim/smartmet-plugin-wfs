PROG = PluginTest TestConfigCreator

REQUIRES = gdal configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas

# Unfortunately otlv4.h forces us to define the full oracle include path
INCLUDES += \
	-I ../wfs \
	-I ../libwfs \
	-isystem /usr/include/mysql \
	-I /usr/include/smartmet \
	-I /usr/include/smartmet/grid-files \
	-I /usr/include/smartmet/grid-content \
	-isystem /usr/include/jsoncpp \
	-isystem /usr/include/soci \
	-isystem /usr/include/oracle/11.2/client64

LIBS +=  -lsmartmet-spine  \
	-lsmartmet-macgyver \
	-lsmartmet-newbase \
	-lsmartmet-grid-files \
	-lsmartmet-grid-content \
	$(CONFIGPP_LIBS) \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_iostreams \
	-lboost_system \
	-lpthread -ldl \
  -lboost_serialization \
  -lboost_chrono \
  -lxqilla \
	-lxerces-c \
	$(GDAL_LIBS) \
	-lpqxx \
	-lconfig++ \
	-lconfig \
	-lctpp2 \
	-lcurl \
	$(JSONCPP_LIBS) \
	-lcrypto \
	-lbz2 -lz \
	-lm

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/gis.conf cnf/local.conf templates
TEST_FINISH_TARGETS := dummy
ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += geonames-database
TEST_FINISH_TARGETS += stop-test-db
else
GEONAMES_HOST_EDIT := cat
endif

all: $(PROG)

clean:
	rm -f $(PROG) *~
	rm -rf failures

test: $(PROG) $(CONFIG_FILES) $(TEST_PREPARE_TARGETS)
	@mkdir -p failures
	@rm -f failures/*
	@echo 
	@echo "*************************************************************"
	@echo "****************** Running wfs grid tests *******************"
	@echo "*************************************************************"
	@echo 
	@echo "*** Creating the Redis configuration file."
	@./TestConfigCreator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf
	@echo "*** Executing plugin tests."
	@echo 
	@./PluginTest -c cnf/reactor.conf
	@rm -f cnf/grid/redis/redis.conf
#	@echo 
#	@echo "*** Shutting down the Redis database and removing useless files." 
#	@echo 
#	@cat cnf/grid/redis/redis-server.pid | xargs kill -9
#	@rm -f cnf/grid/redis/redis-server.pid
#	@rm -f cnf/grid/redis/redis-server.log

$(PROG) : % : %.cpp Makefile ../wfs.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)

cnf/geonames.conf: cnf/geonames.conf.in
	echo $(GEONAMES_HOST_EDIT)
	$(GEONAMES_HOST_EDIT) $< >$@

cnf/gis.conf: cnf/gis.conf.in
	echo $(GEONAMES_HOST_EDIT)
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

templates:
	$(MAKE) -C ../cnf/templates all

cnf/local.conf: dummy
	if [ -f /smartmet/cnf/smartmetd/dev/plugins/wfs.conf ] ; then \
	    awk '/^\s*geoserverConnStr\s*=/{print}' /smartmet/cnf/smartmetd/dev/plugins/wfs.conf >$@; \
	else \
	    echo 'geoserverConnStr = "";' >$@ ; \
	fi

.PHONY: cnf/geonames.conf cnf/gis.conf dummy
