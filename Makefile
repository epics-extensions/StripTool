#
# Makefile,v 1.1 1996/07/18 18:38:05 epics Exp
#

ifndef EPICS
TOP = ../..
ifneq ($(wildcard $(TOP)/config)x,x)
  # New Makefile.Host config file location
  include $(TOP)/config/CONFIG_EXTENSIONS
  include $(TOP)/config/RULES_ARCHS
else
  # Old Makefile.Unix config file location
  EPICS=../../..
  include $(EPICS)/config/CONFIG_EXTENSIONS
  include $(EPICS)/config/RULES_ARCHS
endif
else
TOP	= $(EPICS)
include $(EPICS)/config/CONFIG_EXTENSIONS
include $(EPICS)/config/CONFIG_BASE
include $(EPICS)/config/RULES_ARCHS
endif

