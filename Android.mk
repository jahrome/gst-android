# This file is the top android makefile for all sub-modules.

LOCAL_PATH := $(call my-dir)

GST_ANDROID_TOP := $(LOCAL_PATH)

include $(CLEAR_VARS)

include $(GST_ANDROID_TOP)/gstplayer/Android.mk
include $(GST_ANDROID_TOP)/sink/audioflingersink/Android.mk
include $(GST_ANDROID_TOP)/sink/surfaceflingersink/Android.mk
