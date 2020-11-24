ifeq ($(TOP),)
$(error "TOP no defined")
endif

ifeq ($(wildcard $(TOP)/cnf/observation_common.conf),)
$(error "Cannot find configuration files")
endif

XML_TEST_SRC = $(shell find xml -name '*.xml')
XML_POST_TESTS = $(subst :,\:,$(patsubst xml/%.xml, input/%.xml.post, $(XML_TEST_SRC)))
KVP_TEST_SRC = $(shell find kvp -name '*.kvp')
KVP_GET_TESTS = $(subst :,\:,$(patsubst kvp/%.kvp, input/%.kvp.get, $(KVP_TEST_SRC)))

VALGRIND = valgrind \
	--num-callers=30 \
	--gen-suppressions=all \
	--suppressions=../cnf/valgrind/valgrind_libclntsh.supp \
	--suppressions=../cnf/valgrind/valgrind_brainstorm.supp

foo:
	echo $(XML_TEST_SRC)
	echo $(XML_POST_TESTS)

all:

ifdef CI
TEST_TARGETS := fminames test_sqlite
EXTRA_IGNORE := .testignore-ci
else
TEST_TARGETS := test_sqlite test_oracle test_postgresql
EXTRA_IGNORE :=
endif

test:	all
	@ok=true; failed=; \
	for test in $(TEST_TARGETS); do \
	  if ! $(MAKE) $$test; then ok=false; failed="$$failed $$test"; fi; \
        done; \
	if ! $$ok ; then \
	  echo "======================================================================="; \
	  echo "Failed: $$failed"; \
	  echo "======================================================================="; \
        fi; \
        $$ok

clean:
	rm -fv $(shell find input -type f -a -name '*.xml.post' -o -name '*.xml.get')
	rm -rf failures-oracle failures-postgresql failures-sqlite
	rm -vf cnd/wfs_plugin_test_*.conf

test-oracle:		DB_TYPE=oracle
test-postgresql:	DB_TYPE=postgresql
test-sqlite:		DB_TYPE=sqlite

fminames:
	sed -i -e 's/"smartmet-test"/"localhost"/g' cnf/*.conf
	@/usr/share/smartmet/test/db/init-and-start.sh && /usr/share/smartmet/test/db/install-test-db.sh > /dev/null

test-oracle test-postgresql test-sqlite: ../PluginTest s-input-files ../cnf/local.conf
	rm -rf failures-$(DB_TYPE)
	mkdir -p failures-$(DB_TYPE)
	cat $(TOP)/cnf/wfs_plugin_test.conf.in | sed -e 's:@TARGET@:$(DB_TYPE):g' \
		>cnf/wfs_plugin_test_$(DB_TYPE).conf
	../PluginTest \
		--reactor-config cnf/wfs_plugin_test_$(DB_TYPE).conf \
		--failures-dir failures-$(DB_TYPE) \
		$(foreach fn, $(EXTRA_IGNORE), --ignore $(fn)) \
		--ignore input/.ignore-$(DB_TYPE)

s-input-files:
	rm -f $(shell find input -name '*.xml.post' -o -name '*.kvp.get')
	$(MAKE) $(XML_POST_TESTS)
	$(MAKE) $(KVP_GET_TESTS)

input/%.xml.post : xml/%.xml ; @mkdir -p $(shell dirname $@)
	@perl ../MakeXMLPOST.pl $< $@ /wfs

input/%.kvp.get : kvp/%.kvp ; @mkdir -p $(shell dirname $@)
	@perl ../MakeKVPGET.pl $< $@ /wfs

../cnf/local.conf:
	@echo "***************************************************************************************************"
	@echo "* Please copy file $(shell cd ..; pwd)/cnf/local.conf.in to"
	@echo "* $(shell cd ..; pwd)/cnf/local.conf and edit required names in it"
	@echo "***************************************************************************************************"
	@false

../PluginTest:
	$(MAKE) -C .. PluginTest
