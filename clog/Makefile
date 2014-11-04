
CC			= gcc 

#CFLAGS 	= -O3 -std=c99 -Wall -Wextra -Ideps
#CFLAGS  += -fPIC -Wall -ansi -std=c99 -pedantic

TOPDIR 	:= $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

SRCDIR  = src
INCDIR	=./include
CFLAGS  += -g -I$(INCDIR) 

LIBSRC 	= iniparser astr
LIBPRE	:= $(TOPDIR)
LIBDIR 	:= $(LIBPRE)/lib
CMLIBS 		= $(patsubst %, $(LIBDIR)/lib%.a, $(LIBSRC))
CLN_LIBS= $(addprefix _clean_, $(LIBSRC))

LDFLAGS += -L$(LIBDIR)
LDFLAGS += $(patsubst %, -l%, $(LIBSRC))
LDFLAGS	+= -lpthread -lrt

VERSION=`svn info 2>&1 | awk '{if($$1=="Revision:")print $$2}'`

SOURCE  := $(wildcard $(TOPDIR)/$(SRCDIR)/*.c)
OBJDIR 	:= obj
OBJS    := $(patsubst $(TOPDIR)/%.c,$(TOPDIR)/$(OBJDIR)/%.o,$(SOURCE))
TARGET	=	ipv4_log

PKG_DIR		= ipv4_log_install
PKG_BASE	= ipv4_log
PKG_TMP		= install
INSCRIPT  = script/install.sh
SRVSCRIPT = script/ipv4_log.sh
CFG_FILE 	= config.txt

VER = $(if $(VERSION) $(VERSION),0)
PKG = ipv4_log_$(VER).tar.gz

.PHONY : all clean gen_ver $(LIBS) $(CLN_LIBS)
all: $(TARGET) 
	@rm -rf $(PKG_DIR)
	@mkdir -p $(PKG_DIR)/$(PKG_BASE)/bin/
	@mkdir -p $(PKG_DIR)/$(PKG_BASE)/etc/
	@cp -f $(TARGET) $(PKG_DIR)/$(PKG_BASE)/bin/
	@cp -f $(CFG_FILE) $(PKG_DIR)/$(PKG_BASE)/etc/
	@cp -f $(SRVSCRIPT) $(PKG_DIR)/$(PKG_BASE)/etc/ipv4_log
	@cp -f $(INSCRIPT) $(PKG_DIR)
	@cd $(PKG_DIR) && tar zcvf $(PKG_BASE).tar.gz $(PKG_BASE)
	@rm -rf $(PKG_DIR)/$(PKG_BASE)
	@tar zcvf $(PKG) $(PKG_DIR)

$(INCDIR)/version.h:
	@echo "#ifndef _VERSION_H" > $(INCDIR)/version.h
	@echo "#define _VERSION_H" >> $(INCDIR)/version.h
	@echo "#define VERSION ($(VER))" >>$(INCDIR)/version.h
	@echo "#endif " >>$(INCDIR)/version.h
	@echo "=====version:$(VERSION)====="

$(TARGET): $(INCDIR)/version.h $(CMLIBS) $(OBJS) 
	@echo $(TOPDIR) $(LIBS)
	@mkdir -p $(patsubst %, $(TOPDIR)/$(OBJDIR)/%, $(SRCDIR))
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TOPDIR)/$(OBJDIR)/%.o: $(TOPDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS)  -o $@ $< 

$(CMLIBS):
	@mkdir -p $(LIBDIR)
	$(MAKE) -C $(patsubst $(LIBDIR)/lib%.a, %, $@) PREFIX=$(LIBPRE) install

$(CLN_LIBS):
	$(MAKE) -C $(patsubst _clean_%, %, $@) PREFIX=$(LIBPRE) uninstall
	$(MAKE) -C $(patsubst _clean_%, %, $@) PREFIX=$(LIBPRE) clean

clean: $(CLN_LIBS)
	rm -rf $(INCDIR)/version.h
	rm -rf $(PKG_DIR)
	rm -rf ipv4_log_*.tar.gz
	rm -f $(TARGET) $(OBJS)


