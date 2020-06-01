PREFIX ?= /usr

processor := $(shell uname -p)

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

includedir = $(PREFIX)/include

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always
CXX_STD ?= c++11

TARGETS = decode_feature_id

#
# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  BOOST_INCLUDES := -I/usr/include/boost169
  BOOST_LIBS := -L/usr/lib64/boost169
endif

CFLAGS := -DUNIX -std=$(CXX_STD) -I ../libwfs -I $(includedir)/smartmet \
	-O2 -Wall -Wextra -Wno-unused-parameter $(BOOST_INCLUDES)
DECODE_FEATURE_ID_LIBS := -L $(libdir)/smartmet $(BOOST_LIBS) -lsmartmet-spine -lsmartmet-macgyver \
	-lboost_date_time -lcrypto

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

decode_feature_id: decode_feature_id.cpp ../libsmartmet-plugin-wfs.a ../libwfs/FeatureID.h ../libwfs/BStream.h
	g++ $(CFLAGS) -o $@ decode_feature_id.cpp ../libsmartmet-plugin-wfs.a $(DECODE_FEATURE_ID_LIBS)
