NXDK_DIR = $(CURDIR)/../nxdk
NXDK_NET = y

XBE_TITLE = triangle
GEN_XISO = $(XBE_TITLE).iso
SRCS = $(wildcard $(CURDIR)/*.c)
SHADER_OBJS = ps.inl vs.inl

SRCS += $(CURDIR)/lib/protobuf-c/protobuf-c.c

CFLAGS_EXTRA += -I$(CURDIR)
CFLAGS_EXTRA += -I$(CURDIR)/lib

include $(NXDK_DIR)/Makefile

.PHONY:clean_local
clean_local:
	rm -f *.inl* log.txt dump.cap main.lib

clean: clean_local

%_pb2.py: %.proto
	protoc --python_out=$(CURDIR)/ $(notdir $<)

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=$(CURDIR)/ $(notdir $<)
