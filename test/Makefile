INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TEST_TOP = $(shell pwd)
TOP = $(TEST_TOP)/..

TEST_DIRECTORIES = base real

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/gis.conf cnf/local.conf templates start-redis-db
TEST_FINISH_TARGETS := dummy stop-redis-db
ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += geonames-database
TEST_FINISH_TARGETS += stop-test-db
else
GEONAMES_HOST_EDIT := cat
endif

all:

clean:
	for test_dir in $(TEST_DIRECTORIES) ; do $(MAKE) -C $$test_dir clean; done
	rm -f cnf/geonames.conf
	rm -fr tmp-geonames-db
	rm -f tmp-geonames-db.log
	rm -rf conf/grid

test:	$(TEST_PREPARE_TARGETS)
	@ok=true; \
	for test_dir in $(TEST_DIRECTORIES); do \
		if ! $(MAKE) -C $$test_dir $@ ; then ok=false; fi; \
	done; \
	$(MAKE) test-grid-impl || ok=false; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok;

test-sqlite test-oracle test-postgresql: $(TEST_PREPARE_TARGETS)
	@ok=true; \
	for test_dir in $(TEST_DIRECTORIES); do \
		if ! $(MAKE) -C $$test_dir $@ ; then ok=false; fi; \
	done; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok;

test-grid: $(TEST_PREPARE_TARGETS)
	if $(MAKE) test-grid-impl; then true; else $(MAKE) $(TEST_FINISH_TARGETS); false; fi

test-grid-impl:
	$(MAKE) -C grid test

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

start-redis-db:
	rm -rf cnf/grid
	cp -pr $(datadir)/smartmet/test/grid cnf/
	ls -lR conf/grid
	@echo "*** Creating the Redis configuration file."
	smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf

stop-redis-db:
	@if [ -f cnf/grid/redis/redis-server.pid ] ; then \
		cat cnf/grid/redis/redis-server.pid | xargs kill -9; \
		rm -f cnf/grid/redis/redis-server.pid; \
	fi

cnf/local.conf: dummy
	if [ -f /smartmet/cnf/smartmetd/dev/plugins/wfs.conf ] ; then \
	    awk '/^\s*geoserverConnStr\s*=/{print}' /smartmet/cnf/smartmetd/dev/plugins/wfs.conf >$@; \
	else \
	    echo 'geoserverConnStr = "";' >$@ ; \
	fi

templates:
	$(MAKE) -C ../cnf/templates all

dummy:

.PHONY: cnf/geonames.conf cnf/gis.conf dummy
