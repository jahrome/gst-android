# This file is the top android makefile for all sub-modules.

LOCAL_PATH := $(call my-dir)

GSTREAMER_TOP := $(LOCAL_PATH)

include $(CLEAR_VARS)

include $(GSTREAMER_TOP)/gstplayer/Android.mk

