## Makefile: cmacs ############################################

CPP=        g++ 
SHELL=		/bin/sh
MOVE=		mv
COPY=		ln -s
RM=			rm -rf
TOUCH=		touch
LIBRARIAN=	ar rvu
INC=        -I../..

## Platform ###################################################

# get the OS
UNAME_S := $(shell uname -s | tr '[A-Z]' '[a-z]' )

# if this is linux
ifeq ($(UNAME_S),linux)
	ARCH := $(shell uname -m | tr '[A-Z]' '[a-z]' )
   	CPPFLAGS = -D _LINUX_  -g -Wno-deprecated
endif

#if this is OSX
ifeq ($(UNAME_S), darwin)
	ARCH := $(shell uname -m | tr '[A-Z]' '[a-z]' )
	CPPFLAGS = -D _OSX_ -g -Wno-deprecated
endif

#if this is SUNOS or SOLARIS
ifeq ($(UNAME_S),sunos)
    UNAME_R := $(shell uname -r)

    ifeq ($(UNAME_R), 4.1.3)
        CPPFLAGS = -D _SUNOS_ -g
    endif

    ifeq ($(UNAME_R), 4.1.4)
        CPPFLAGS = -D _SUNOS_ -g
    endif

    ifeq ($(UNAME_R), 5.6)
        CPPFLAGS = -D _SOLARIS6_ -g
    endif

    ifeq ($(UNAME_R), 5.7)
        CPPFLAGS = -D _SOLARIS6_ -g
    endif

    ifeq ($(UNAME_R), 5.10)
        CPPFLAGS = -D _SOLARIS10_ -g
    endif
endif

#if this is IRIX
ifeq ($(UNAME_S),irix)
    UNAME_R := $(shell uname -r)
    ifeq ($(UNAME_R), 6.5)
        CPPFLAGS = -D _IRIX6_ -g
    endif
endif

#if this is NETBSD
ifeq ($(UNAME_S),netbsd)
	CPPFLAGS = -D _NETBSD_ -g
endif

#if this is NeXT
ifeq ($(UNAME_S),nextstep)
    ARCH := $(shell uname -m | tr '[A-Z]' '[a-z]' )
    CPPFLAGS = -D _NEXT_ -D_POSIX_SOURCE=1 -g
endif


## Object & Libraries #########################################


LIB_CX_PLATFORM_LIB_DIR=../../lib/$(UNAME_S)_$(ARCH)

APP_OBJECT_DIR=$(UNAME_S)_$(ARCH)

LIB_CX_BASE_NAME=libcx_base.a
LIB_CX_NET_NAME=libcx_net.a
LIB_CX_THREAD_NAME=libcx_thread.a
LIB_CX_DB_NAME=libcx_db.a
LIB_CX_NETAPP_NAME=libcx_netapp.a
LIB_CX_SCREEN_NAME=libcx_screen.a
LIB_CX_COMMANDLINE_NAME=libcx_commandline.a
LIB_CX_KEYBOARD_NAME=libcx_keyboard.a
LIB_CX_EDITBUFFER_NAME=libcx_editbuffer.a
LIB_CX_JSON_NAME=libcx_json.a
LIB_CX_COMPLETER_NAME=libcx_commandcompleter.a
LIB_CX_REGEX_NAME=libcx_regex.a
LIB_CX_PROCESS_NAME=libcx_process.a
LIB_CX_BUILDOUTPUT_NAME=libcx_buildoutput.a

CX_LIBS = \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_KEYBOARD_NAME)   \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_SCREEN_NAME)     \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_EDITBUFFER_NAME) \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_COMPLETER_NAME)  \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_PROCESS_NAME)    \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_BUILDOUTPUT_NAME) \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_JSON_NAME)

# Base library must come last - other libs depend on it (e.g., net uses CxError)
CX_LIBS_BASE = $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_BASE_NAME)

ALL_LIBS = $(CX_LIBS) $(CX_LIBS_BASE) $(PLATFORM_LIBS)

OBJECTS = \
	$(APP_OBJECT_DIR)/Cm.o                   \
	$(APP_OBJECT_DIR)/CommandLineView.o      \
	$(APP_OBJECT_DIR)/EditView.o             \
	$(APP_OBJECT_DIR)/EditViewDisplay.o      \
	$(APP_OBJECT_DIR)/EditViewInput.o        \
	$(APP_OBJECT_DIR)/ScreenEditor.o         \
	$(APP_OBJECT_DIR)/ScreenEditorCommands.o \
	$(APP_OBJECT_DIR)/ScreenEditorCore.o \
	$(APP_OBJECT_DIR)/ProgramDefaults.o		 \
	$(APP_OBJECT_DIR)/Project.o				 \
	$(APP_OBJECT_DIR)/ProjectView.o		 \
	$(APP_OBJECT_DIR)/BuildView.o            \
	$(APP_OBJECT_DIR)/MarkUp.o               \
	$(APP_OBJECT_DIR)/MarkUpColorizers.o     \
	$(APP_OBJECT_DIR)/MarkUpParsing.o        \
	$(APP_OBJECT_DIR)/CommandTable.o         \
	$(APP_OBJECT_DIR)/UTFSymbols.o

# HelpView depends on CmVersion.h and has its own build rule
ALL_OBJECTS = $(OBJECTS) $(APP_OBJECT_DIR)/HelpView.o

## MCP Support (Linux/macOS only, developer builds) ###########
#
# MCP (Model Context Protocol) support for Claude Desktop integration.
# This is experimental and disabled by default.
#
# To build with MCP support:
#   make DEVELOPER=1
#   make DEVELOPER=1 mcp_bridge
#
######################################################

ifdef DEVELOPER
  ifeq ($(UNAME_S),darwin)
      BUILD_MCP=1
  endif
  ifeq ($(UNAME_S),linux)
      BUILD_MCP=1
  endif
endif

ifdef BUILD_MCP
    # Enable MCP code paths in source
    CPPFLAGS += -DCM_MCP_ENABLED
    # Add MCP handler object
    OBJECTS += $(APP_OBJECT_DIR)/MCPHandler.o

    # Add thread, net, and regex libraries for MCP
    CX_LIBS += \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_THREAD_NAME) \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_NET_NAME)    \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_REGEX_NAME)

    # pthread for threading
    PLATFORM_LIBS += -lpthread
endif

MCP_BRIDGE_LIBS = \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_JSON_NAME) \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_NET_NAME) \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_BASE_NAME)

## Targets ####################################################


ALL: MAKE_OBJ_DIR $(APP_OBJECT_DIR)/cm

MAKE_OBJ_DIR:
	test -d $(APP_OBJECT_DIR) || mkdir $(APP_OBJECT_DIR)

ifdef BUILD_MCP
.PHONY: mcp_bridge
mcp_bridge: MAKE_OBJ_DIR
	$(CPP) $(CPPFLAGS) $(INC) mcp_bridge.cpp -o $(APP_OBJECT_DIR)/mcp_bridge $(MCP_BRIDGE_LIBS)
ifeq ($(UNAME_S), darwin)
	xattr -cr $(APP_OBJECT_DIR)/mcp_bridge
endif
	@echo "Built mcp_bridge"
else
.PHONY: mcp_bridge
mcp_bridge:
	@echo "mcp_bridge: skipping (not Linux/macOS)"
endif


cleanupmac:
	$(RM) ._*

clean:
	$(RM) \
	$(APP_OBJECT_DIR)/cm    \
	$(APP_OBJECT_DIR)/*.o   \
	$(APP_OBJECT_DIR)/*.dbx \
	$(APP_OBJECT_DIR)/*.i   \
	$(APP_OBJECT_DIR)/*.ixx \
	$(APP_OBJECT_DIR)/core  \
	$(APP_OBJECT_DIR)/a.out \
	$(APP_OBJECT_DIR)/*.a

install:
ifeq ($(UNAME_S), linux)
	sudo cp $(APP_OBJECT_DIR)/cm /usr/local/bin/cm
	sudo chmod 755 /usr/local/bin/cm
else ifeq ($(UNAME_S), darwin)
	sudo cp $(APP_OBJECT_DIR)/cm /usr/local/bin/cm
	sudo chmod 755 /usr/local/bin/cm
	sudo xattr -cr /usr/local/bin/cm
else
	cp $(APP_OBJECT_DIR)/cm /usr/local/bin/cm
	chmod 755 /usr/local/bin/cm
endif

install-help:
ifeq ($(UNAME_S), linux)
	sudo mkdir -p /usr/local/share/cm
	sudo cp cm_help.md /usr/local/share/cm/cm_help.md
	sudo cp cm_help.txt /usr/local/share/cm/cm_help.txt
	sudo chmod 644 /usr/local/share/cm/cm_help.md
	sudo chmod 644 /usr/local/share/cm/cm_help.txt
else ifeq ($(UNAME_S), darwin)
	sudo mkdir -p /usr/local/share/cm
	sudo cp cm_help.md /usr/local/share/cm/cm_help.md
	sudo cp cm_help.txt /usr/local/share/cm/cm_help.txt
	sudo chmod 644 /usr/local/share/cm/cm_help.md
	sudo chmod 644 /usr/local/share/cm/cm_help.txt
else
	mkdir -p /usr/local/share/cm
	cp cm_help.md /usr/local/share/cm/cm_help.md
	cp cm_help.txt /usr/local/share/cm/cm_help.txt
	chmod 644 /usr/local/share/cm/cm_help.md
	chmod 644 /usr/local/share/cm/cm_help.txt
endif

install-all: install install-help


$(APP_OBJECT_DIR)/cm: $(ALL_OBJECTS)
	$(CPP) -v $(CPPFLAGS) $(INC) $(ALL_OBJECTS) -o $(APP_OBJECT_DIR)/cm $(ALL_LIBS)
ifeq ($(UNAME_S), darwin)
	xattr -cr $(APP_OBJECT_DIR)/cm
endif


## Conversions ################################################

$(APP_OBJECT_DIR)/Cm.o 				: Cm.cpp
$(APP_OBJECT_DIR)/ProgramDefaults.o : ProgramDefaults.cpp
$(APP_OBJECT_DIR)/CommandLineView.o : CommandLineView.cpp
$(APP_OBJECT_DIR)/EditView.o		: EditView.cpp
$(APP_OBJECT_DIR)/EditViewDisplay.o	: EditViewDisplay.cpp
$(APP_OBJECT_DIR)/EditViewInput.o	: EditViewInput.cpp
$(APP_OBJECT_DIR)/ScreenEditor.o	: ScreenEditor.cpp
$(APP_OBJECT_DIR)/ScreenEditorCommands.o : ScreenEditorCommands.cpp
$(APP_OBJECT_DIR)/ScreenEditorCore.o : ScreenEditorCore.cpp
$(APP_OBJECT_DIR)/Project.o			: Project.cpp
$(APP_OBJECT_DIR)/ProjectView.o	: ProjectView.cpp
$(APP_OBJECT_DIR)/HelpView.o	: HelpView.cpp CmVersion.h
	$(CPP) $(CPPFLAGS) $(INC) -c HelpView.cpp -o $@
$(APP_OBJECT_DIR)/BuildView.o		: BuildView.cpp
$(APP_OBJECT_DIR)/MarkUp.o			: MarkUp.cpp
$(APP_OBJECT_DIR)/MarkUpColorizers.o: MarkUpColorizers.cpp
$(APP_OBJECT_DIR)/MarkUpParsing.o	: MarkUpParsing.cpp
$(APP_OBJECT_DIR)/CommandTable.o	: CommandTable.cpp
$(APP_OBJECT_DIR)/UTFSymbols.o		: UTFSymbols.cpp

ifdef BUILD_MCP
$(APP_OBJECT_DIR)/MCPHandler.o		: MCPHandler.cpp
endif

.PRECIOUS: $(CX_LIBS)
.SUFFIXES: .cpp .C .cc .cxx .o


$(OBJECTS):
	$(CPP) $(CPPFLAGS) $(INC) -c $? -o $@


########################################################################################
# Create tar archive for distribution
#
########################################################################################

archive:
	@echo "Creating cxapps_unix.tar..."
	@echo "  (extracts to cx_apps/cm/ when untarred from parent directory)"
	@test -d ../../ARCHIVE || mkdir ../../ARCHIVE
	@tar cvf ../../ARCHIVE/cxapps_unix.tar \
		--transform 's,^\./,cx_apps/cm/,' \
		--exclude='*.o' \
		--exclude='*.a' \
		--exclude='.git' \
		--exclude='.claude' \
		--exclude='.DS_Store' \
		--exclude='darwin_*' \
		--exclude='linux_*' \
		--exclude='sunos_*' \
		--exclude='irix_*' \
		--exclude='netbsd_*' \
		--exclude='nextstep_*' \
		--exclude='*.xcodeproj' \
		--exclude='*.xcworkspace' \
		--exclude='xcuserdata' \
		--exclude='DerivedData' \
		--exclude='*.pbxuser' \
		--exclude='*.mode1v3' \
		--exclude='*.mode2v3' \
		--exclude='*.perspectivev3' \
		--exclude='*.xcuserstate' \
		.
	@echo "Archive created: ../../ARCHIVE/cxapps_unix.tar"



