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

CFLAGS := -DUNIX -std=$(CXX_STD) -I ../include -I $(includedir)/smartmet \
	-O2 -Wall -Wextra -Wno-unused-parameter
DECODE_FEATURE_ID_LIBS := -L $(libdir)/smartmet -lsmartmet-spine -lsmartmet-macgyver \
	-lboost_date_time -lcrypto

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

decode_feature_id: decode_feature_id.cpp \
		../include/FeatureID.h ../source/FeatureID.cpp \
		../include/BStream.h ../source/BStream.cpp
	g++ $(CFLAGS) -o $@ \
		decode_feature_id.cpp ../source/FeatureID.cpp ../source/BStream.cpp \
		$(DECODE_FEATURE_ID_LIBS)
