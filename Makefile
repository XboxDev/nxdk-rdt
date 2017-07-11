NXDK_DIR = $(CURDIR)/../nxdk
NXDK_NET = y

XBE_TITLE = nxdk-rdt
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(wildcard $(CURDIR)/*.c)
SHADER_OBJS = ps.inl vs.inl

SRCS += $(CURDIR)/lib/protobuf-c/protobuf-c.c
SRCS += $(CURDIR)/dbg.pb-c.c # Might not exist when the wildcard above runs

CFLAGS += -I$(CURDIR)
CFLAGS += -I$(CURDIR)/lib

all_local: all dbg_pb2.py

include $(NXDK_DIR)/Makefile

.PHONY:clean_local
clean_local:
	rm -f *.inl* log.txt dump.cap main.lib *.pb-c.c *.pb-c.h *_pb2.py

clean: clean_local

%_pb2.py: %.proto
	protoc --python_out=$(CURDIR)/ $(notdir $<)

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=$(CURDIR)/ $(notdir $<)
