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

    CPPFLAGS = -D _SOLARIS6_ -g

    ifeq ($(UNAME_R), 4.1.3)
        CPPFLAGS = -D _SUNOS_ -g
    endif

	ifeq ($(UNAME_R), 4.1.4)
		CPPFLAGS = -D _SUNOS_ -g
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

CX_LIBS = \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_KEYBOARD_NAME)   \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_SCREEN_NAME)     \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_EDITBUFFER_NAME) \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_JSON_NAME)       \
	$(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_BASE_NAME)

ALL_LIBS = $(CX_LIBS) $(PLATFORM_LIBS)

OBJECTS = \
	$(APP_OBJECT_DIR)/Cm.o                   \
	$(APP_OBJECT_DIR)/CommandLineView.o      \
	$(APP_OBJECT_DIR)/EditView.o             \
	$(APP_OBJECT_DIR)/ScreenEditor.o         \
	$(APP_OBJECT_DIR)/ProgramDefaults.o		 \
	$(APP_OBJECT_DIR)/Project.o				 \
	$(APP_OBJECT_DIR)/FileListView.o		 \
	$(APP_OBJECT_DIR)/HelpTextView.o		 \
	$(APP_OBJECT_DIR)/MarkUp.o

## Targets ####################################################


ALL: MAKE_OBJ_DIR $(APP_OBJECT_DIR)/cm

MAKE_OBJ_DIR:
	test -d $(APP_OBJECT_DIR) || mkdir $(APP_OBJECT_DIR)


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
	sudo cp $(APP_OBJECT_DIR)/cm /usr/local/bin/cm
	sudo chmod 755 /usr/local/bin/cm


$(APP_OBJECT_DIR)/cm: $(OBJECTS)
	$(CPP) -v $(CPPFLAGS) $(INC) $(OBJECTS) -o $(APP_OBJECT_DIR)/cm $(ALL_LIBS)


## Conversions ################################################

$(APP_OBJECT_DIR)/Cm.o 				: Cm.cpp
$(APP_OBJECT_DIR)/ProgramDefaults.o : ProgramDefaults.cpp
$(APP_OBJECT_DIR)/CommandLineView.o : CommandLineView.cpp
$(APP_OBJECT_DIR)/EditView.o		: EditView.cpp
$(APP_OBJECT_DIR)/ScreenEditor.o	: ScreenEditor.cpp
$(APP_OBJECT_DIR)/Project.o			: Project.cpp
$(APP_OBJECT_DIR)/FileListView.o	: FileListView.cpp
$(APP_OBJECT_DIR)/HelpTextView.o	: HelpTextView.cpp
$(APP_OBJECT_DIR)/MarkUp.o			: MarkUp.cpp


.PRECIOUS: $(CX_LIBS)
.SUFFIXES: .cpp .C .cc .cxx .o


$(OBJECTS):
	$(CPP) $(CPPFLAGS) $(INC) -c $? -o $@



