package org.freedesktop.gstreamer

import android.content.Context
import android.content.res.AssetManager
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

class GStreamer {

    companion object {
        init {
            System.loadLibrary("gstreamer_android")
        }

        @JvmStatic
        @Throws(Exception::class)
        external fun nativeInit(context: Context)

        @Throws(Exception::class)
        fun init(context: Context) {
            nativeInit(context)
            copyFonts(context)
            copyCaCertificates(context)
        }

        private fun copyFonts(context: Context) {
            val assetManager = context.assets
            val filesDir = context.filesDir
            val fontsFCDir = File(filesDir, "fontconfig")
            val fontsDir = File(fontsFCDir, "fonts")
            val fontsCfg = File(fontsFCDir, "fonts.conf")
            fontsDir.mkdirs()
            try {
                /* Copy the config file */
                copyFile(assetManager, "fontconfig/fonts.conf", fontsCfg)
                /* Copy the fonts */
                for(filename in assetManager.list("fontconfig/fonts/truetype").orEmpty()) {
                    val font = File(fontsDir, filename)
                    copyFile(assetManager, "fontconfig/fonts/truetype/" + filename, font)
                }
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }

        private fun copyCaCertificates(context: Context) {
            val assetManager = context.assets
            val filesDir = context.filesDir
            val sslDir = File(filesDir, "ssl")
            val certsDir = File(sslDir, "certs")
            val certs = File(certsDir, "ca-certificates.crt")
            certsDir.mkdirs()
            try {
                /* Copy the certificates file */
                copyFile(assetManager, "ssl/certs/ca-certificates.crt", certs)
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }

        @Throws(IOException::class)
        private fun copyFile(assetManager: AssetManager, assetPath: String, outFile: File) {
            if(outFile.exists()) {
                outFile.delete()
            }
            val buffer = ByteArray(1024)
            val inputStream = assetManager.open(assetPath)
            val outputStream = FileOutputStream(outFile)
            var numRead = inputStream.read(buffer)
            while (numRead != -1) {
                outputStream.write(buffer, 0, numRead)
                numRead = inputStream.read(buffer)
            }
            inputStream.close()
            outputStream.flush()
            outputStream.close()
        }
    }

}