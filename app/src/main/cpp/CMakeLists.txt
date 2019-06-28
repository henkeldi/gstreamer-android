cmake_minimum_required(VERSION 3.4.1)

set(ARCH arm)
set(GSTREAMER_ROOT /home/dimitri/Downloads/gstreamer-1.0-android-universal-1.14.5/${ARCH})

include_directories(
        ${GSTREAMER_ROOT}/include/gstreamer-1.0
        ${GSTREAMER_ROOT}/include/glib-2.0
        ${GSTREAMER_ROOT}/lib/glib-2.0/include)

link_directories(
        ${GSTREAMER_ROOT}/lib
        ${GSTREAMER_ROOT}/lib/gstreamer-1.0)

find_library(log-lib log)

SET(GST_LIBS gstreamer-1.0 gmodule-2.0 gobject-2.0 glib-2.0 iconv intl ffi)
SET(GST_PLUGINS_CORE_LIBS gstaudioresample gstaudiotestsrc gstaudioconvert)
SET(GST_PLUGINS_BASE gstautodetect gstallocators-1.0 orc-test-0.4 gstapp-1.0 gstaudio-1.0 gstfft-1.0 gstpbutils-1.0 gstriff-1.0 gstrtp-1.0 gstrtsp-1.0 gstsdp-1.0 gsttag-1.0 gstvideo-1.0 gstgl-1.0 orc-0.4 gstopensles OpenSLES gstaudio-1.0 gstbase-1.0)

add_library(gstreamer_android SHARED gstreamer_android.cpp)
target_link_libraries(gstreamer_android ${log-lib} ${GST_LIBS} ${GST_PLUGINS_CORE_LIBS} ${GST_PLUGINS_BASE})

add_library(gstreamer_android_player SHARED gstreamer_android_player.cpp)
target_link_libraries(gstreamer_android_player ${log-lib} ${GST_LIBS} ${GST_PLUGINS_CORE_LIBS} ${GST_PLUGINS_BASE})
