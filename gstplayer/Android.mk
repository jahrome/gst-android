# external/gstreamer/gstplayer/Android.mk
#
#  Copyright 2009 STN wireless
#

LOCAL_PATH:= $(call my-dir)

# -------------------------------------
# gstplayer library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	GstPlayer.cpp \
	GstDriver.cpp	

LOCAL_CFLAGS := \
	$(shell $(PKG_CONFIG) gstreamer-0.10 --cflags) \
	$(shell $(PKG_CONFIG) gstreamer-app-0.10 --cflags) \
	-DANDROID_USE_GSTREAMER


LOCAL_SHARED_LIBRARIES := \
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libgstpbutils-0.10 \
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0 		\
	libgstvideo-0.10      	\
	libgstapp-0.10		\
	libutils		\
	libcutils		\
	libui			\
	libhardware		\
	libandroid_runtime	\
	libmedia 		\
	libbinder


LOCAL_LDLIBS += -ldl -lpthread 

LOCAL_MODULE:= libgstplayer
LOCAL_MODULE_TAGS := eng debug

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false 

include $(BUILD_SHARED_LIBRARY)

# -------------------------------------
# gstmetadataretriever library
#
#include $(CLEAR_VARS)
#
#LOCAL_ARM_MODE := arm
#
#LOCAL_SRC_FILES := \
#	GstMetadataRetriever.cpp \
#	GstMetadataRetrieverDriver.cpp
#
#LOCAL_SHARED_LIBRARIES := \
#	libgstreamer-0.10	\
#	libgstbase-0.10		\
#	libglib-2.0		\
#	libgthread-2.0		\
#	libgmodule-2.0		\
#	libgobject-2.0		\
#	libgstvideo-0.10	\
#	libutils		\
#	libcutils		\
#	libui			\
#	libhardware		\
#	libandroid_runtime	\
#	libmedia
#
#LOCAL_CFLAGS := \
#	$(shell $(PKG_CONFIG) gstreamer-0.10 --cflags) \
#	$(shell $(PKG_CONFIG) gstreamer-tag-0.10 --cflags) \
#	-DANDROID_USE_GSTREAMER
#
#LOCAL_LDLIBS := -ldl -lpthread 
#
#LOCAL_MODULE:= libgstmetadataretriever
#LOCAL_MODULE_TAGS := eng debug
#
#LOCAL_PRELINK_MODULE := false 
#
#include $(BUILD_SHARED_LIBRARY)
#

#
## -------------------------------------
## gstmediarecorder library
##
#include $(CLEAR_VARS)
#
#LOCAL_ARM_MODE := arm
#
#LOCAL_SRC_FILES := \
#	GstMediaRecorder.cpp 		
#
#LOCAL_WHOLE_STATIC_LIBRARIES := libasound
#
#LOCAL_SHARED_LIBRARIES :=	\
#	libgstreamer-0.10	\
#	libgstbase-0.10		\
#	libglib-2.0		\
#	libgthread-2.0		\
#	libgmodule-2.0		\
#	libgobject-2.0		\
#	libgstvideo-0.10	\
#	libgstapp-0.10		\
#	libgsticbvideo
#
#LOCAL_SHARED_LIBRARIES += 	\
#	libutils 		\
#	libcutils 		\
#	libui 			\
#	libhardware 		\
#	libandroid_runtime 	\
#	libmedia 		\
#	libbinder
#
#
#LOCAL_CFLAGS := -DANDROID_USE_GSTREAMER
#
#LOCAL_LDLIBS := -ldl -lpthread 
#
#LOCAL_MODULE:= libgstmediarecorder
#LOCAL_MODULE_TAGS := optional
#
##
## define LOCAL_PRELINK_MODULE to false to not use pre-link map
##
#LOCAL_PRELINK_MODULE := false 
#
#include $(BUILD_SHARED_LIBRARY)
#
#
## -------------------------------------
## gstmediascanner library
##
#include $(CLEAR_VARS)
# 
#LOCAL_ARM_MODE := arm
#
#LOCAL_SRC_FILES := \
#	 GstMediaScanner.cpp 
#
#LOCAL_CFLAGS += 					\
#	$(shell $(PKG_CONFIG) gstreamer-0.10 --cflags) 	\
#	-DHAVE_CONFIG_H 				\
#	-Wdeclaration-after-statement 			\
#	-DANDROID_USE_GSTREAMER
#
#LOCAL_SHARED_LIBRARIES:=	\
#	libgstreamer-0.10	\
#	libgstbase-0.10		\
#	libglib-2.0		\
#	libgthread-2.0		\
#	libgmodule-2.0		\
#	libgobject-2.0		\
#	libgstvideo-0.10	\
#	libutils		\
#	libcutils		\
#	libui			\
#	libhardware		\
#	libandroid_runtime	\
#	libmedia		\
#	libgsticbvideo		\
#	libbinder
#
#LOCAL_MODULE:= libgstmediascanner
#LOCAL_MODULE_TAGS := optional
#LOCAL_PRELINK_MODULE := false 
#
#include $(BUILD_SHARED_LIBRARY)
