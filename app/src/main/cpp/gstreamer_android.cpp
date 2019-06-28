#include <jni.h>
#include <gst/gst.h>
#include <gio/gio.h>
#include <android/log.h>
#include <string>

/* Declaration of static plugins */
// @PLUGINS_DECLARATION@
#define GST_PLUGIN_STATIC_DECLARE_C(name) \
  extern "C" void G_PASTE(gst_plugin_, G_PASTE(name, _register)) (void)
/* Based on GLib's default handler */
#define CHAR_IS_SAFE(wc) (!((wc < 0x20 && wc != '\t' && wc != '\n' && wc != '\r') || \
			    (wc == 0x7f) || \
			    (wc >= 0x80 && wc < 0xa0)))
#define DEFAULT_LEVELS (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE)
#define INFO_LEVELS (G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)

static char const* TAG = "GStreamer";
static jobject _context = nullptr;
static jobject _class_loader = nullptr;
static JavaVM  *_java_vm = nullptr;
static GstClockTime _priv_gst_info_start_time;

// GST_PLUGIN_STATIC_DECLARE_C(audiotestsrc);

void gst_android_register_static_plugins() {
    // GST_PLUGIN_STATIC_REGISTER(audiotestsrc);
    // GST_PLUGIN_STATIC_REGISTER(audioconvert);
    // GST_PLUGIN_STATIC_REGISTER(audioresample);
}

void gst_android_load_gio_modules() {

}

static void glib_print_handler(const gchar* string) {
    __android_log_print(ANDROID_LOG_INFO, "GLib", "%s", string);
}

static void glib_printerr_handler(const gchar* string) {
    __android_log_print(ANDROID_LOG_ERROR, "GLib", "%s", string);
}

static void gst_debug_logcat(GstDebugCategory * category, GstDebugLevel level,
        const gchar * file, const gchar * function, gint line,
        GObject * object, GstDebugMessage * message, gpointer unused) {

    if (level > gst_debug_category_get_threshold(category)) {
        return;
    }

    GstClockTime elapsed = GST_CLOCK_DIFF(_priv_gst_info_start_time, gst_util_get_timestamp());
    gint android_log_level;
    switch(level) {
        case GST_LEVEL_ERROR:
            android_log_level = ANDROID_LOG_ERROR;
            break;
        case GST_LEVEL_WARNING:
            android_log_level = ANDROID_LOG_WARN;
            break;
        case GST_LEVEL_INFO:
            android_log_level = ANDROID_LOG_INFO;
            break;
        case GST_LEVEL_DEBUG:
            android_log_level = ANDROID_LOG_DEBUG;
            break;
        default:
            android_log_level = ANDROID_LOG_VERBOSE;
            break;
    }

    gchar* tag = g_strdup_printf("GStreamer+%s", gst_debug_category_get_name(category));
    if (object) {
        gchar* obj;
        if (GST_IS_PAD (object) && GST_OBJECT_NAME(object)) {
            obj = g_strdup_printf("<%s:%s>", GST_DEBUG_PAD_NAME (object));
        } else if (GST_IS_OBJECT(object) && GST_OBJECT_NAME (object)) {
            obj = g_strdup_printf("<%s>", GST_OBJECT_NAME (object));
        } else if (G_IS_OBJECT(object)) {
            obj = g_strdup_printf("<%s@%p>", G_OBJECT_TYPE_NAME (object), object);
        } else {
            obj = g_strdup_printf("<%p>", object);
        }

        __android_log_print(android_log_level, tag,
                            "%" GST_TIME_FORMAT " %p %s:%d:%s:%s %s\n",
                            GST_TIME_ARGS (elapsed), g_thread_self (),
                            file, line, function, obj, gst_debug_message_get (message));

        g_free(obj);
    } else {
        __android_log_print(android_log_level, tag,
                            "%" GST_TIME_FORMAT " %p %s:%d:%s %s\n",
                            GST_TIME_ARGS (elapsed), g_thread_self (),
                            file, line, function, gst_debug_message_get (message));
    }
    g_free(tag);
}

static void escape_string(GString* string) {
    const char *p = string->str;
    gunichar wc;

    while (p < string->str + string->len) {
        gboolean safe;

        wc = g_utf8_get_char_validated (p, -1);
        if (wc == (gunichar) - 1 || wc == (gunichar) - 2) {
            gchar *tmp;
            guint pos;

            pos = p - string->str;

            /* Emit invalid UTF-8 as hex escapes
             */
            tmp = g_strdup_printf ("\\x%02x", (guint) (guchar) * p);
            g_string_erase (string, pos, 1);
            g_string_insert (string, pos, tmp);

            p = string->str + (pos + 4);      /* Skip over escape sequence */

            g_free (tmp);
            continue;
        }
        if (wc == '\r') {
            safe = *(p + 1) == '\n';
        } else {
            safe = CHAR_IS_SAFE (wc);
        }

        if (!safe) {
            gchar *tmp;
            guint pos;

            pos = p - string->str;

            /* Largest char we escape is 0x0a, so we don't have to worry
             * about 8-digit \Uxxxxyyyy
             */
            tmp = g_strdup_printf ("\\u%04x", wc);
            g_string_erase (string, pos, g_utf8_next_char (p) - p);
            g_string_insert (string, pos, tmp);
            g_free (tmp);

            p = string->str + (pos + 6);      /* Skip over escape sequence */
        } else
            p = g_utf8_next_char (p);
    }
}

static void glib_log_handler(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data) {
    if (!((log_level & DEFAULT_LEVELS) || (log_level >> G_LOG_LEVEL_USER_SHIFT))) {
        const gchar* domains = g_getenv ("G_MESSAGES_DEBUG");
        if ((log_level & INFO_LEVELS) == 0 || domains == nullptr || (strcmp(domains, "all") != 0 && (!log_domain || !strstr(domains, log_domain)))) {
            return;
        }
    }

    gchar* tag;
    if (log_domain) {
        tag = g_strdup_printf ("GLib+%s", log_domain);
    } else {
        tag = g_strdup ("GLib");
    }

    gint android_log_level;
    switch (log_level & G_LOG_LEVEL_MASK) {
        case G_LOG_LEVEL_ERROR:
            android_log_level = ANDROID_LOG_ERROR;
            break;
        case G_LOG_LEVEL_CRITICAL:
            android_log_level = ANDROID_LOG_FATAL;
            break;
        case G_LOG_LEVEL_WARNING:
            android_log_level = ANDROID_LOG_WARN;
            break;
        case G_LOG_LEVEL_MESSAGE:
            android_log_level = ANDROID_LOG_INFO;
            break;
        case G_LOG_LEVEL_INFO:
            android_log_level = ANDROID_LOG_INFO;
            break;
        case G_LOG_LEVEL_DEBUG:
            android_log_level = ANDROID_LOG_DEBUG;
            break;
        default:
            android_log_level = ANDROID_LOG_INFO;
            break;
    }

    GString* gstring = g_string_new(nullptr);
    if (!message) {
        g_string_append(gstring, "(nullptr) message");
    } else {
        GString* msg = g_string_new(message);
        escape_string(msg);
        g_string_append(gstring, msg->str);
        g_string_free(msg, TRUE);
    }
    gchar* string = g_string_free(gstring, FALSE);

    __android_log_print(android_log_level, tag, "%s", string);

    g_free(string);
    g_free(tag);
}

static gboolean get_application_dirs(JNIEnv* env, jobject context, gchar** cache_dir, gchar** files_dir) {
    jclass context_class = env->GetObjectClass(context);
    if (!context_class) {
        return FALSE;
    }
    jmethodID get_cache_dir_id = env->GetMethodID(context_class, "getCacheDir", "()Ljava/io/File;");
    jmethodID get_files_dir_id = env->GetMethodID(context_class, "getFilesDir", "()Ljava/io/File;");

    if (!get_cache_dir_id || !get_files_dir_id) {
        env->DeleteLocalRef(context_class);
        return FALSE;
    }

    jclass file_class = env->FindClass("java/io/File");
    if (!file_class) {
        env->DeleteLocalRef(context_class);
        return FALSE;
    }

    jmethodID get_absolute_path_id = env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
    if (!get_absolute_path_id) {
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(file_class);
        return FALSE;
    }

    jobject dir = env->CallObjectMethod(context, get_cache_dir_id);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(file_class);
        return FALSE;
    }

    if (dir) {
        jstring abs_path = (jstring) env->CallObjectMethod(dir, get_absolute_path_id);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(dir);
            env->DeleteLocalRef(context_class);
            env->DeleteLocalRef(file_class);
            return FALSE;
        }
        const gchar* abs_path_str = env->GetStringUTFChars(abs_path, nullptr);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(abs_path);
            env->DeleteLocalRef(dir);
            env->DeleteLocalRef(context_class);
            env->DeleteLocalRef(file_class);
            return FALSE;
        }
        *cache_dir = files_dir? g_utf8_strup(abs_path_str, strlen(abs_path_str)): nullptr;

        env->ReleaseStringUTFChars(abs_path, abs_path_str);
        env->DeleteLocalRef(abs_path);
        env->DeleteLocalRef(dir);
    }

    dir = env->CallObjectMethod(context, get_files_dir_id);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->DeleteLocalRef(context_class);
        env->DeleteLocalRef(file_class);
        return FALSE;
    }

    if (dir) {
        jstring abs_path = (jstring) env->CallObjectMethod(dir, get_absolute_path_id);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(dir);
            env->DeleteLocalRef(context_class);
            env->DeleteLocalRef(file_class);
            return FALSE;
        }
        const gchar* abs_path_str = env->GetStringUTFChars(abs_path, nullptr);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            env->DeleteLocalRef(abs_path);
            env->DeleteLocalRef(dir);
            env->DeleteLocalRef(context_class);
            env->DeleteLocalRef(file_class);
            return FALSE;
        }
        *files_dir = files_dir? g_utf8_strup(abs_path_str, strlen(abs_path_str)): nullptr;

        env->ReleaseStringUTFChars(abs_path, abs_path_str);
        env->DeleteLocalRef(abs_path);
        env->DeleteLocalRef(dir);
    }

    env->DeleteLocalRef(file_class);
    env->DeleteLocalRef(context_class);

    return TRUE;
}

static gboolean init(JNIEnv* env, jobject context) {
    jclass context_cls = env->GetObjectClass(context);
    if (!context_cls) {
        return FALSE;
    }

    jmethodID get_class_loader_id = env->GetMethodID(context_cls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return FALSE;
    }

    jobject class_loader = env->CallObjectMethod(context, get_class_loader_id);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return FALSE;
    }

    if(_context) {
        env->DeleteGlobalRef(_context);
    }
    _context = env->NewGlobalRef(context);

    if(_class_loader) {
        env->DeleteGlobalRef(_class_loader);
    }
    _class_loader = env->NewGlobalRef(class_loader);

    return TRUE;
}

static void gst_android_init(JNIEnv* env, jobject context) {
    if (!init(env, context)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "GStreamer failed to initialize");
    }

    if (gst_is_initialized()) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "GStreamer already initialized");
        return;
    }

    gchar *cache_dir;
    gchar *files_dir;

    if (!get_application_dirs(env, context, &cache_dir, &files_dir)) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get application dirs");
    }

    if (cache_dir) {
        g_setenv ("TMP", cache_dir, TRUE);
        g_setenv ("TEMP", cache_dir, TRUE);
        g_setenv ("TMPDIR", cache_dir, TRUE);
        g_setenv ("XDG_RUNTIME_DIR", cache_dir, TRUE);
        g_setenv ("XDG_CACHE_HOME", cache_dir, TRUE);
        gchar* registry = g_build_filename (cache_dir, "registry.bin", nullptr);
        g_setenv ("GST_REGISTRY", registry, TRUE);
        g_free (registry);
        g_setenv ("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "no", TRUE);
    }

    if (files_dir) {
        g_setenv ("HOME", files_dir, TRUE);
        g_setenv ("XDG_DATA_DIRS", files_dir, TRUE);
        g_setenv ("XDG_CONFIG_DIRS", files_dir, TRUE);
        g_setenv ("XDG_CONFIG_HOME", files_dir, TRUE);
        g_setenv ("XDG_DATA_HOME", files_dir, TRUE);

        gchar* fontconfig = g_build_filename (files_dir, "fontconfig", nullptr);
        g_setenv ("FONTCONFIG_PATH", fontconfig, TRUE);
        g_free (fontconfig);

        gchar* certs = g_build_filename (files_dir, "ssl", "certs", "ca-certificates.crt", nullptr);
        g_setenv ("CA_CERTIFICATES", certs, TRUE);
        g_free (certs);
    }
    g_free (cache_dir);
    g_free (files_dir);

    /* Set GLib print handlers */
    g_set_print_handler (glib_print_handler);
    g_set_printerr_handler (glib_printerr_handler);
    g_log_set_default_handler (glib_log_handler, nullptr);

    /* Disable this for releases if performance is important
    * or increase the threshold to get more information */
    gst_debug_set_active (TRUE);
    gst_debug_set_default_threshold (GST_LEVEL_WARNING);
    gst_debug_remove_log_function (gst_debug_log_default);
    gst_debug_add_log_function ((GstLogFunction) gst_debug_logcat, nullptr, nullptr);

    /* get time we started for debugging messages */
    _priv_gst_info_start_time = gst_util_get_timestamp();

    GError *error;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        gchar* message = g_strdup_printf("GStreamer initialization failed: %s",
                error && error->message ? error->message : "(no message)");
        jclass exception_class = env->FindClass("java/lang/Exception");
        __android_log_print (ANDROID_LOG_ERROR, TAG, "%s", message);
        env->ThrowNew(exception_class, message);
        g_free(message);
    }

    gst_android_register_static_plugins();
    gst_android_load_gio_modules();

    __android_log_print (ANDROID_LOG_INFO, TAG, "GStreamer initialization complete");
}

static void gst_android_init_jni(JNIEnv* env, jobject gstreamer, jobject context) {
    gst_android_init(env, context);
}

static JNINativeMethod native_methods[] = {
        {"nativeInit", "(Landroid/content/Context;)V", (void *) gst_android_init_jni}
};

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not retrieve JNIEnv");
        return -1;
    }

    jclass klass = env->FindClass("org/freedesktop/gstreamer/GStreamer");
    if (!klass) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not retrieve class org.freedesktop.gstreamer.GStreamer");
        return -1;
    }

    if (env->RegisterNatives(klass, native_methods, G_N_ELEMENTS(native_methods))) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not register native methods for org.freedesktop.gstreamer.GStreamer");
        return -1;
    }

    _java_vm = vm;

    /* Tell the androidmedia plugin about the Java VM if we can */
    GModule* module = g_module_open (nullptr, G_MODULE_BIND_LOCAL);
    if (module) {
        void (*set_java_vm) (JavaVM*) = nullptr;
        if (g_module_symbol(module, "gst_amc_jni_set_java_vm", (gpointer *) & set_java_vm) && set_java_vm) {
            set_java_vm(vm);
        }
        g_module_close(module);
    }

    return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM* vm, void* reversed) {
    JNIEnv* env;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not retrieve JNIEnv");
        return;
    }

    if (_context) {
        env->DeleteGlobalRef(_context);
        _context = nullptr;
    }

    if (_class_loader) {
        env->DeleteGlobalRef(_class_loader);
        _class_loader = nullptr;
    }

    _java_vm = nullptr;
}