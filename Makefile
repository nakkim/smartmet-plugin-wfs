SUBNAME = wfs
SPEC = smartmet-plugin-$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)
TOP = $(shell pwd)

REQUIRES_GDAL = yes

include common.mk


DEFINES = -DUNIX -D_REENTRANT


INCLUDES += \
	-I$(includedir)/smartmet \
	-isystem $(includedir)/jsoncpp

LIBS += -L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
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
	-lgdal \
	-lpqxx \
	-lconfig++ \
	-lconfig \
	-lctpp2 \
	-lcurl \
	-ljsoncpp \
	-lcrypto \
	-lbz2 -lz \
	-lpthread \
	-lm

INCLUDES := -I$(TOP)/libwfs -I$(TOP)/wfs $(INCLUDES)

obj/%.o : %.cpp ; @echo Compiling $<
	@mkdir -p obj
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d.new, $@) -o $@ $<
	@sed -e "s|^$(notdir $@):|$@:|" $(patsubst obj/%.o, obj/%.d.new, $@) >$(patsubst obj/%.o, obj/%.d, $@)
	@rm -f $(patsubst obj/%.o, obj/%.d.new, $@)

# What to install

LIBFILE = $(SUBNAME).so

# Compilation directories

vpath %.cpp wfs wfs/stored_queries libwfs libwfs/request
vpath %.h include libwfs

# The files to be compiled

SRCS = $(wildcard wfs/*.cpp) $(wildcard wfs/stored_queries/*.cpp)
HDRS = $(wildcard wfs/*.h) $(wildcard wfs/stored_queries/*.h)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

LIBWFS_SRCS := $(wildcard libwfs/*.cpp) $(wildcard libwfs/request/*.cpp)
LIBWFS_HDRS := $(wildcard libwfs/*.h) $(wildcard libwfs/request/*.h)
LIBWFS_OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(LIBWFS_SRCS)))

TEMPLATES = $(wildcard cnf/templates/*.template)
COMPILED_TEMPLATES = $(patsubst %.template, %.c2t, $(TEMPLATES))

CONFIG_FILES = $(wildcard cnf/crs/*.conf) \
	$(wildcard cnf/features/*.conf)

.PHONY: test rpm

LIBWFS = $(TOP)/libsmartmet-plugin-wfs.a

# The rules

all: configtest objdir $(LIBWFS) $(LIBFILE) all-templates

debug: all

release: all

profile: all

configtest:
	@ok=true; \
	if [  -x "$$(command -v cfgvalidate)" ]; then \
	  for file in $(CONFIG_FILES); do \
	     echo Validating $$file; cfgvalidate $$file || ok=false; \
	  done; \
	fi; \
	$$ok

$(LIBFILE): $(OBJS) $(LIBWFS)
	$(CXX) -shared -rdynamic $(LDFLAGS) -o $@ $(OBJS) $(LIBWFS) $(LIBS)

$(LIBWFS): $(LIBWFS_OBJS)
	ar rcs $@ $(LIBWFS_OBJS)
	ranlib $@

clean:	clean-templates
	rm -f $(LIBFILE) obj/*.o obj/*.d *~ source/*~ include/*~ cnf/templates/*.c2t
	rm -f files.list files.tmp
	$(MAKE) -C testsuite $@
	$(MAKE) -C examples $@
	$(MAKE) -C test $@

format:
	clang-format -i -style=file include/*.h include/*/*.h source/*.cpp source/*/*.cpp test/*.cpp
	$(MAKE) -C libwfs $@

install:
	mkdir -p $(plugindir)
	mkdir -p $(libdir)
	mkdir -p $(includedir)/smartmet/plugin/wfs/request
	mkdir -p $(sysconfdir)/smartmet/plugins/wfs/templates
	$(INSTALL_PROG) $(LIBFILE) $(plugindir)/$(LIBFILE)
	@for file in cnf/templates/*.c2t; do \
	 echo $(INSTALL_DATA) $$file $(sysconfdir)/smartmet/plugins/wfs/templates/; \
	 $(INSTALL_DATA) $$file $(sysconfdir)/smartmet/plugins/wfs/templates/; \
	done
	$(INSTALL_DATA) cnf/XMLGrammarPool.dump $(sysconfdir)/smartmet/plugins/wfs/
	$(INSTALL_DATA) cnf/XMLSchemas.cache $(sysconfdir)/smartmet/plugins/wfs/
	$(INSTALL_DATA) cnf/XMLSchemas.cache $(sysconfdir)/smartmet/plugins/wfs/
	$(INSTALL_DATA) $(wildcard libwfs/*.h) $(includedir)/smartmet/plugin/wfs/
	$(INSTALL_DATA) $(wildcard libwfs/request/*.h) $(includedir)/smartmet/plugin/wfs/request/
	$(INSTALL_DATA) $(LIBWFS) $(libdir)

# Separate depend target is no more needed as dependencies are updated automatically
# and are always up to time
depend:

test:
	$(MAKE) -C test $@

all-templates:
	$(MAKE) -C cnf/templates all

clean-templates:
	$(MAKE) -C cnf/templates clean

html::
	mkdir -p /data/local/html/lib/$(SPEC)
	doxygen $(SPEC).dox

objdir:
	@mkdir -p $(objdir)

# Forcibly lower RPM_BUILD_NCPUs in CircleCI cloud(but not on local builds)
RPMBUILD=$(shell test "$$CIRCLE_BUILD_NUM" && echo RPM_BUILD_NCPUS=2 rpmbuild || echo rpmbuild)

rpm: $(SPEC).spec
	$(MAKE) clean
	$(MAKE) file-list
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs \
		--transform "s,^,plugins/$(SPEC)/," --files-from files.list
	$(RPMBUILD) -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

file-list:
	find . -name '.gitignore' >files.list.new
	find . -name 'Makefile' -o -name '*.spec' >>files.list.new
	find libwfs -name '*.h' -o -name '*.cpp' >>files.list.new
	find wfs -name '*.h' -o -name '*.cpp' >>files.list.new
	find tools -name '*.h' -o -name '*.cpp' >>files.list.new
	find testsuite -name '*.h' -o -name '*.cpp' >>files.list.new
	find examples -name '*.h' -o -name '*.cpp' >>files.list.new
	find cnf -name '*.conf' -o -name '*.template' >>files.list.new
	echo cnf/templates/template_depend.pl >>files.list.new
	echo cnf/XMLGrammarPool.dump >>files.list.new
	echo cnf/XMLSchemas.cache >>files.list.new
	echo common.mk >>files.list.new
	find test/base -name '*.conf' >>files.list.new
	find test/base/output -name '*.get' -o -name '*.kvp.post' -o -name '*.xml.post' >>files.list.new
	find test/base/kvp -name '*.kvp' >>files.list.new
	find test/base/xml -name '*.xml' >>files.list.new
	find test -name '*.pl' >>files.list.new
	find test -name '*.cpp' >>files.list.new
	echo ./test/PluginTest.cpp >>files.list.new
	find server_tests -name '*.xml' -o -name '*.exp' -o -name '*.pl' >>files.list.new
	find server_tests -name '*.kvp' -o -name '*.xslt' >>files.list.new
	find tools/xml/xml_samples -name '*.xml' >>files.list.new
	cat files.list.new | sed -e 's:^\./::' | sort | uniq >files.list
	rm -f files.list.new

chaile-list-test: file-list
	git ls-files . | sort >files.tmp
	diff -u files.list files.tmp
	rm -f files.tmp

headertest:
	@echo "Checking self-sufficiency of each header:"
	@echo
	$(MAKE) -C libwfs $@
	@for hdr in $(HDRS); do \
	echo $$hdr; \
	echo "#include \"$$hdr\"" > /tmp/$(SPEC).cpp; \
	echo "int main() { return 0; }" >> /tmp/$(SPEC).cpp; \
	$(CXX) $(CFLAGS) $(INCLUDES) -o /dev/null /tmp/$(SPEC).cpp $(LIBS); \
	done

cnf/templates/%.c2t: cnf/templates/%.template ; ( cd cnf/templates && $(PREFIX)/bin/ctpp2c $(notdir $<) $(notdir $@) )

.SUFFIXES: $(SUFFIXES) .cpp

check check-valgrind: $(LIBWFS)
	$(MAKE) -C testsuite $@

check-installed:
	$(MAKE) -C testsuite $@

examples:	examples-build

examples-build:	libwfs-build $(LIBFILE)
	$(MAKE) -C examples examples PREFIX=$(PREFIX)

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
