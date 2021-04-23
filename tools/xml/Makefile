
include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

LIBS += -lboost_serialization -lboost_filesystem -lboost_system -lxerces-c -lcurl -lcrypto

TARGETS = XMLGrammarPoolDump XMLGrammarPool.dump XSDCacheAdd

XML_REQUEST_SAMPLES = $(wildcard xml_samples/request/*.xml)
XML_RESPONSE_SAMPLES = $(patsubst xml_samples/request/%.xml,xml_samples/response/%.xml,$(XML_REQUEST_SAMPLES))
XML_SAMPLES = $(XML_REQUEST_SAMPLES) $(XML_RESPONSE_SAMPLES)

all:	$(TARGETS)

clean:
	rm -f $(TARGETS)
	rm -rf xml_schemas
	rm -vf XMLGrammarPoolDump.*.log
	rm -rf xml_samples/response

s-update-responses:
	( cd xml_samples && ./PerformQueries.sh )

XMLGrammarPoolDump: 	XMLGrammarPoolDump.cpp
	$(CXX) -o $@ $(CFLAGS) $(INCLUDES) $< $(LIBS)

XSDCacheAdd: XSDCacheAdd.cpp
	$(CXX) -o $@ $(CFLAGS) $(INCLUDES) $< $(LIBS)

XMLGrammarPool.dump:	s-update-responses XMLGrammarPoolDump
	$(MAKE) create-dump

create-dump:
	rm -rf xml_schemas
	rm -f XMLGrammarPool.dump
	done=false; \
	for x in $(shell seq 1 5) ; do \
		if ! $$done ; then \
			./XMLGrammarPoolDump -o XMLGrammarPool.dump \
				$$(case $$x in 1) echo -s XMLSchemas.cache ;; *) echo -i XMLGrammarPool.dump ;; esac) \
				-w xml_schemas $(XML_SAMPLES) | tee XMLGrammarPoolDump.$$x.log; \
			grep -q -e '##\ \ *Resolving' XMLGrammarPoolDump.$$x.log || done=true; \
		fi; \
	done; \
	$$done;
