package com.example.gstreamer

class GStreamerPlayer {

    private val native_custom_data: Long = 0

    external fun nativeInit()

    fun onGStreamerInitialized() {

    }

    companion object {
        private external fun nativeClassInit(): Boolean

        init {
            System.loadLibrary("gstreamer_android_player")
        }
    }

}