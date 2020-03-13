TOP = $(shell pwd)/../../

XML_TEST_SRC = $(shell find xml -name '*.xml')
XML_POST_TESTS = $(subst :,\:,$(patsubst xml/%.xml, input/%.xml.post, $(XML_TEST_SRC)))
KVP_TEST_SRC = $(shell find kvp -name '*.kvp')
KVP_GET_TESTS = $(subst :,\:,$(patsubst kvp/%.kvp, input/%.kvp.get, $(KVP_TEST_SRC)))

VALGRIND = valgrind \
	--num-callers=30 \
	--suppressions=../cnf/valgrind/valgrind_libclntsh.supp \
	--suppressions=../cnf/valgrind/valgrind_brainstorm.supp

all:

clean:
	rm -f $(shell find input -type f -a -name '*.xml.post')
	rm -rf failures

test:	../PluginTest s-input-files ../cnf/local.conf
	@mkdir -p failures
	@rm -f $(shell find failures -name '*.xml')
	@../PluginTest

debug: ../PluginTest s-input-files ../cnf/local.conf
	@mkdir -p failures
	@rm -f $(shell find failures -name '*.xml')
	@gdb --args ../PluginTest

valgrind: ../PluginTest s-input-files ../cnf/local.conf
	@mkdir -p failures
	@rm -f $(shell find failures -name '*.xml')
	@$(VALGRIND) ../PluginTest

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
