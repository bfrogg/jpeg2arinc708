
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "arinc708.h"

#define CAPS "video/x-raw,format=RGB,width=1024,pixel-aspect-ratio=1/1" /* Параметры входного видеопотока */



/* На вход подается изображение EXAMLE.JPG, на выходе получаем UDP датаграммы, содержащиее пакеты ARINC708 */
int main(int argc, char *argv[])
{

    GstElement *pipeline, *filesrc, *sink;
    GstSample *sample;
    GstMapInfo map;
    GstBus *bus;
    GError *error = NULL;
    GdkPixbuf *pixbuf;
    GstCaps *caps;
    GstStructure *s;
    gboolean res;
    gint width, height;

    /* Инициализация GStreamer */
    gst_init (&argc, &argv);

    /* Создание конвейера:
         - файл
         - jpeg декодер
         - изображение в видеопоток
         - видеоковертер
         - соотношение сторон
         - API для получения буфера */

    pipeline = gst_parse_launch ("filesrc name=my_filesrc ! jpegdec ! imagefreeze !"
                                 "videoconvert ! videoscale ! appsink name=sink caps=\"" CAPS "\"", &error);

    if (error != NULL)
    {
        g_print ("Could not construct pipeline: %s\n", error->message);
        g_clear_error (&error);
        exit (-1);
    }

    /* Получение файла по имени */
    filesrc = gst_bin_get_by_name (GST_BIN (pipeline), "my_filesrc");
    g_object_set (filesrc, "location", "EXAMPLE.JPG", NULL);
    g_object_unref (filesrc);

    /* Получение сэмпла изображения из appsink */
    sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    bus = gst_element_get_bus (pipeline);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);

    //GstAppSink *appsink = GST_APP_SINK_CAST(sink);

    sample = gst_app_sink_pull_preroll(GST_APP_SINK_CAST(sink));

    /* Получение описания формата буфера снимка. Мы устанавливаем возможности элемента
     * apppsink  таким образом, что это может быть только формат rgb. Единственным
     * параметром, который мы не указали с помощью возможностей является высота,
     * зависящая от соотношения ширины и высоты пикселей исходного материала */

    caps = gst_sample_get_caps (sample);

    if (!caps)
    {
        g_print ("Could not get sample caps\n");
        exit (-1);
    }

    s = gst_caps_get_structure (caps, 0);

    /* Нам нужно получить структуру окончательных возможностей буфера для вычисления размера снимка */
    res = gst_structure_get_int (s, "width", &width);
    res |= gst_structure_get_int (s, "height", &height);

    if (!res)
    {
        g_print ("Could not get dimensions\n");
        exit (-1);
    }

    /* Получение буфера снимка */

    GstBuffer *buffer = gst_sample_get_buffer (sample);

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    /* Получение pixbuf */
    pixbuf = gdk_pixbuf_new_from_data (map.data,
                                       GDK_COLORSPACE_RGB, FALSE, 8, width, height,
                                       GST_ROUND_UP_4 (width * 3), NULL, NULL);
    /* Передача пакетов ARINC708 по сети */
    arinc708_send(pixbuf);

    /* Деинициализация */
    gst_buffer_unmap (buffer, &map);
    gst_sample_unref (sample);
    gst_caps_ref(caps);

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    gst_object_unref (bus);

    return 0;
}

