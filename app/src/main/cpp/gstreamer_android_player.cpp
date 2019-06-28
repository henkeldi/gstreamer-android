#include <jni.h>
#include <gst/gst.h>
#include <gio/gio.h>
#include <android/log.h>
#include <string>

static char const* TAG = "GStreamerPlayer";
static JavaVM  *_java_vm = nullptr;

static void gst_native_init(JNIEnv* env, jobject thiz) {

}

static void gst_native_finalize(JNIEnv* env, jobject thiz) {

}

static jboolean gst_native_class_init(JNIEnv* env, jclass klass) {


    return JNI_TRUE;
}

/* List of implemented native methods */
static JNINativeMethod native_methods[] = {
        { "nativeInit", "()V", (void *) gst_native_init},
        { "nativeFinalize", "()V", (void *) gst_native_finalize},
};

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    _java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not retrieve JNIEnv");
        return -1;
    }

    jclass klass = env->FindClass("com/example/gstreamer/GStreamerPlayer");
    if (!klass) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not retrieve class com.example.gstreamer.GStreamerPlayer");
        return -1;
    }

    if (env->RegisterNatives(klass, native_methods, G_N_ELEMENTS(native_methods))) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not register native methods for org.freedesktop.gstreamer.GStreamer");
        return -1;
    }

    return JNI_VERSION_1_6;
}