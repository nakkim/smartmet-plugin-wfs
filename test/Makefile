INCDIR = smartmet/smartmet/plugins/$(SUBNAME)
TEST_TOP = $(shell pwd)
TOP = $(TEST_TOP)/..

TEST_DIRECTORIES = base real

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

TEST_PREPARE_TARGETS := cnf/geonames.conf cnf/local.conf templates
TEST_FINISH_TARGETS := dummy
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

test test-sqlite test-oracle test-postgresql: $(TEST_PREPARE_TARGETS)
	@ok=true; \
	for test_dir in $(TEST_DIRECTORIES); do \
		if ! $(MAKE) -C $$test_dir $@ ; then ok=false; fi; \
	done; \
	$(MAKE) $(TEST_FINISH_TARGETS); \
	$$ok;

cnf/geonames.conf: cnf/geonames.conf.in
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

cnf/local.conf: dummy
	if [ -f /smartmet/cnf/smartmetd/dev/plugins/wfs.conf ] ; then \
	    awk '/^\s*geoserverConnStr\s*=/{print}' /smartmet/cnf/smartmetd/dev/plugins/wfs.conf >$@; \
	else \
	    echo 'geoserverConnStr = "";' >$@ ; \
	fi

templates:
	$(MAKE) -C ../cnf/templates all

dummy:

.PHONY: cnf/geonames.conf dummy
