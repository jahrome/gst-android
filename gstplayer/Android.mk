# external/gstreamer/gstplayer/Android.mk
#
#  Copyright 2009 STN wireless
#
ifeq ($(ANDROID_USE_GSTREAMER),true)

LOCAL_PATH:= $(call my-dir)

# -------------------------------------
# gstplayer library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

gstplayer_FILES := \
	GstPlayer.cpp \
	GstDriver.cpp	

gstplayer_C_INCLUDES := \
	$(LOCAL_PATH) \
    $(TARGET_OUT_HEADERS)/gstreamer-0.10       \
	$(TARGET_OUT_HEADERS)/glib-2.0 			   \
    $(TARGET_OUT_HEADERS)/glib-2.0/glib        \
	external/libxml2/include \
	$(call include-path-for,libgstreamer-0.10 libgstmetadataretriever  libgstbase-0.10 libglib-2.0 libgthread-2.0 libgmodule-2.0 libgobject-2.0 libgstvideo-0.10 libxml2 libmedia)

LOCAL_SRC_FILES := $(gstplayer_FILES)

LOCAL_C_INCLUDES += $(gstplayer_C_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
	libgstmetadataretriever \
	libgstreamer-0.10     \
	libgstbase-0.10       \
	libglib-2.0           \
	libgthread-2.0        \
	libgmodule-2.0        \
	libgobject-2.0 		  \
	libgstvideo-0.10      \
	libgstapp-0.10

ifeq ($(STECONF_ANDROID_VERSION),"FROYO")
LOCAL_SHARED_LIBRARIES += libicuuc 
LOCAL_C_INCLUDES += external/icu4c/common
LOCAL_CFLAGS += -DSTECONF_ANDROID_VERSION_FROYO
endif

LOCAL_SHARED_LIBRARIES += \
	libutils 	\
	libcutils 	\
	libui 		\
	libhardware \
	libandroid_runtime \
	libmedia  	\
	libbinder

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_CFLAGS += -Wall -g -O2
LOCAL_CFLAGS += -DANDROID_USE_GSTREAMER

LOCAL_LDLIBS += -lpthread 

LOCAL_MODULE:= libgstplayer

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false 

include $(BUILD_SHARED_LIBRARY)

# -------------------------------------
# gstmetadataretriever library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

gstmetadataretriever_FILES := \
	GstMetadataRetriever.cpp \
	GstMetadataRetrieverDriver.cpp

gstmetadataretriever_C_INCLUDES := \
	$(LOCAL_PATH) \
    $(TARGET_OUT_HEADERS)/gstreamer-0.10       \
	$(TARGET_OUT_HEADERS)/glib-2.0 			   \
    $(TARGET_OUT_HEADERS)/glib-2.0/glib        \
	external/libxml2/include \
	$(call include-path-for, libgstreamer-0.10 libgstbase-0.10 libglib-2.0 libgthread-2.0 libgmodule-2.0 libgobject-2.0 libgstvideo-0.10 libxml2)

LOCAL_SRC_FILES := $(gstmetadataretriever_FILES)

LOCAL_C_INCLUDES += $(gstmetadataretriever_C_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
	libgstreamer-0.10     \
	libgstbase-0.10       \
	libglib-2.0           \
	libgthread-2.0        \
	libgmodule-2.0        \
	libgobject-2.0 		  \
	libgstvideo-0.10

LOCAL_SHARED_LIBRARIES += \
	libutils 	\
	libcutils 	\
	libui 		\
	libhardware \
	libandroid_runtime \
	libmedia

ifeq ($(STECONF_ANDROID_VERSION),"FROYO")
LOCAL_SHARED_LIBRARIES += libicuuc 
LOCAL_C_INCLUDES += external/icu4c/common
LOCAL_CFLAGS += -DSTECONF_ANDROID_VERSION_FROYO
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_CFLAGS += -Wall -g -O2
LOCAL_CFLAGS += -DANDROID_USE_GSTREAMER

LOCAL_LDLIBS += -lpthread 

LOCAL_MODULE:= libgstmetadataretriever

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false 

include $(BUILD_SHARED_LIBRARY)



# -------------------------------------
# gstmediarecorder library
#
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

gstmediarecorder_FILES := \
	GstMediaRecorder.cpp 		

gstmediarecorder_C_INCLUDES := \
	$(LOCAL_PATH) \
    $(TARGET_OUT_HEADERS)/gstreamer-0.10       \
	$(TARGET_OUT_HEADERS)/glib-2.0 			   \
    $(TARGET_OUT_HEADERS)/glib-2.0/glib        \
	external/libxml2/include \
	external/gst/gstplayer \
	external/icebird/gstreamer-icb-video \
	external/icebird/include \
    external/alsa-lib/include \
	$(call include-path-for, libgstreamer-0.10 libgstbase-0.10 libglib-2.0 libgthread-2.0 libgmodule-2.0 libgobject-2.0 libgstvideo-0.10 libxml2)

LOCAL_SRC_FILES := $(gstmediarecorder_FILES)

LOCAL_C_INCLUDES += $(gstmediarecorder_C_INCLUDES)

LOCAL_WHOLE_STATIC_LIBRARIES := libasound

LOCAL_SHARED_LIBRARIES := \
	libgstreamer-0.10     \
	libgstbase-0.10       \
	libglib-2.0           \
	libgthread-2.0        \
	libgmodule-2.0        \
	libgobject-2.0 		  \
	libgstvideo-0.10 	  \
	libgstapp-0.10 		  \
	libgsticbvideo

LOCAL_SHARED_LIBRARIES += \
	libutils 	\
	libcutils 	\
	libui 		\
	libhardware \
	libandroid_runtime \
	libmedia 	\
	libbinder


ifeq ($(STECONF_ANDROID_VERSION),"FROYO")
LOCAL_SHARED_LIBRARIES += libicuuc 
LOCAL_SHARED_LIBRARIES += libcamera_client
LOCAL_C_INCLUDES += external/icu4c/common
LOCAL_CFLAGS += -DSTECONF_ANDROID_VERSION_FROYO
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_CFLAGS += -Wall -g -O2
LOCAL_CFLAGS += -DANDROID_USE_GSTREAMER

LOCAL_LDLIBS += -lpthread 

LOCAL_MODULE:= libgstmediarecorder

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false 

include $(BUILD_SHARED_LIBRARY)




# -------------------------------------
# gsticbandroidsink library
#
include $(CLEAR_VARS)
 
LOCAL_ARM_MODE := arm

gsticbandroidsink_FILES := \
	 GsticbAndroidVideoSink.cpp \
	 GsticbAndroid.cpp 
	 
gsticbandroidsink_C_INCLUDES := \
	$(LOCAL_PATH)							\
    $(TARGET_OUT_HEADERS)/gstreamer-0.10    	\
	$(TARGET_OUT_HEADERS)/glib-2.0 			  	\
    $(TARGET_OUT_HEADERS)/glib-2.0/glib       	\
	external/gst/gstreamer/android 		  	\
	external/libxml2/include      				\
	external/icebird/gstreamer-icb-video \
	external/icebird/include \
	frameworks/base/libs/audioflinger \
	frameworks/base/media/libmediaplayerservice \
	frameworks/base/media/libmedia	\
	frameworks/base/include/media

LOCAL_SRC_FILES := $(gsticbandroidsink_FILES)

LOCAL_C_INCLUDES += $(gsticbandroidsink_C_INCLUDES)

LOCAL_CFLAGS += -DHAVE_CONFIG_H
LOCAL_CFLAGS += -Wall -Wdeclaration-after-statement -g -O2
LOCAL_CFLAGS += -DANDROID_USE_GSTREAMER

LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += \
	libgstreamer-0.10     \
	libgstbase-0.10       \
	libglib-2.0           \
	libgthread-2.0        \
	libgmodule-2.0        \
	libgobject-2.0        \
	libgstvideo-0.10      

LOCAL_SHARED_LIBRARIES += \
	libutils \
	libcutils \
	libui \
	libhardware \
	libandroid_runtime \
	libmedia \
	libgsticbvideo  \
	libbinder

ifeq ($(STECONF_ANDROID_VERSION),"FROYO")
LOCAL_SHARED_LIBRARIES += libicuuc 
LOCAL_SHARED_LIBRARIES += libsurfaceflinger_client
LOCAL_C_INCLUDES += external/icu4c/common
LOCAL_CFLAGS += -DSTECONF_ANDROID_VERSION_FROYO
endif

LOCAL_MODULE:= libgsticbandroidsink
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/gstreamer-0.10

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false 

include $(BUILD_SHARED_LIBRARY)

# -------------------------------------
# gstmediascanner library
#
include $(CLEAR_VARS)
 
LOCAL_ARM_MODE := arm

gstmediascanner_FILES := \
	 GstMediaScanner.cpp 

gstmediascanner_C_INCLUDES := \
	$(LOCAL_PATH) \
    $(TARGET_OUT_HEADERS)/gstreamer-0.10       \
	$(TARGET_OUT_HEADERS)/glib-2.0 			   \
    $(TARGET_OUT_HEADERS)/glib-2.0/glib        \
	external/libxml2/include      \
	external/icebird/gstreamer-icb-video \
	external/icebird/include 

LOCAL_SRC_FILES := $(gstmediascanner_FILES)

LOCAL_C_INCLUDES += $(gstmediascanner_C_INCLUDES)

LOCAL_CFLAGS += -DHAVE_CONFIG_H
LOCAL_CFLAGS += -Wall -Wdeclaration-after-statement -g -O2
LOCAL_CFLAGS += -DANDROID_USE_GSTREAMER

LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += \
	libgstreamer-0.10     \
	libgstbase-0.10       \
	libglib-2.0           \
	libgthread-2.0        \
	libgmodule-2.0        \
	libgobject-2.0 		  \
	libgstvideo-0.10

LOCAL_SHARED_LIBRARIES += \
	libutils \
	libcutils \
	libui \
	libhardware \
	libandroid_runtime \
	libmedia \
	libgsticbvideo \
	libbinder

ifeq ($(STECONF_ANDROID_VERSION),"FROYO")
LOCAL_SHARED_LIBRARIES += libicuuc 
LOCAL_C_INCLUDES += external/icu4c/common
LOCAL_CFLAGS += -DSTECONF_ANDROID_VERSION_FROYO
endif

LOCAL_MODULE:= libgstmediascanner
#MULTICORE BUILD MAKE -jX
#LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false 

include $(BUILD_SHARED_LIBRARY)


endif  # ANDROID_USE_GSTREAMER == true
