#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

## Setup various variables for later us
version := $(subst -, ,$(shell git describe --long --dirty --tags))
COMMIT := $(strip $(word 3, $(version)))
COMMITS_PAST := $(strip $(word 2, $(version)))
DIRTY := $(strip $(word 4, $(version)))

ifneq ($(COMMITS_PAST), 0)
	BUILD_INFO_COMMITS := "."$(COMMITS_PAST)
endif

ifneq ($(DIRTY),)
	BUILD_INFO_DIRTY :="+"
endif

export BUILD_TAG :=$(strip $(word 1, $(version)))
export BUILD_INFO := $(COMMIT)$(BUILD_INFO_COMMITS)$(BUILD_INFO_DIRTY)

## Set the project name and other boilerplate stuff
PROJECT_NAME := esp32-ota-example

include $(IDF_PATH)/make/project.mk