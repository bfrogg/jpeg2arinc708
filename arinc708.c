#include "arinc708.h"

#define DEBUG

/**
 * @brief Функция раскодировки угла ARINC708 в double
 * @param[in]  scan_angle    Закодированное значение угла
 * @return     Возвращает угол
 */
static double angle_conversion(uint16_t scan_angle)
{
    uint16_t i;
    double angle_d = 0;

    /* Проверяем установлены ли биты в каждом разряде scan_angle */
    for (i = 0; i < 12; i++)
        angle_d += (0x01 & (scan_angle >> i)) * scan_angle_bits[i];

    angle_d = angle_d - 90;
    angle_d = angle_d - ((uint32_t)(angle_d / 360)) * 360;

    return angle_d;
}

/**
 * @brief Функция заполнения одного луча развертки цветовой информацией
 * @param[in]  packet_p     Указатель на сгенерированный пакет
 * @param[in]  input_buf_p  Входной буфер пикселей
 * @param[in]  fi           Угол
 * @details В этой функции мы заполняем луч на заданном угле. Получаем
 * пиксели из буфера по координатам в декартовой СК, преобразуем в цветовую
 * схему RGB 3bit, переводим в цветовую схему ARINC708 и заполняем точку на луче.
 */
static void fill_ray(a708_packet *packet_p, GdkPixbuf *input_buf_p, double fi)
{
    uint8_t  *pixels, *pixels_new, *p;
    uint32_t width, height, rowstride, n_channels, pixel_offset;
    uint32_t radius; /* Радиус ARINC708 */
    uint32_t x, y;   /* Координаты в декартовой системе входного изображения */
    double   x_d = 0, y_d = 0;
    uint32_t rgb_colour = 0, arinc_colour;
    uint32_t bitnum, bytenum;
    uint8_t  bitmask = 0;    /* номер бита в байте */
    int32_t  j;

    /* Проверка параметров изображения */
    n_channels = gdk_pixbuf_get_n_channels (input_buf_p);
    g_assert (gdk_pixbuf_get_colorspace (input_buf_p) == GDK_COLORSPACE_RGB);
    g_assert (gdk_pixbuf_get_bits_per_sample (input_buf_p) == 8);
    g_assert (!gdk_pixbuf_get_has_alpha (input_buf_p));
    g_assert (n_channels == 3);

    /* Получение параметров из pixbuf */
    width = gdk_pixbuf_get_width (input_buf_p);
    height = gdk_pixbuf_get_height (input_buf_p);
    rowstride = gdk_pixbuf_get_rowstride (input_buf_p);
    pixels = gdk_pixbuf_get_pixels (input_buf_p);
    pixels_new = gdk_pixbuf_get_pixels (input_buf_p);

    /* Проходим по лучу и обрабатываем пиксели входного изображения */
    for (radius = 1; radius <= 512; radius++)
    {
        /* Переход от полярной СК в Декартову */
        x_d = radius * RELATIVE_RADIUS * cos(fi * PIdiv180);
        x = round(x_d) + CENTER_X;

        /* В ARINC708 нулевой угол направлен вниз */
        y_d = radius * RELATIVE_RADIUS * sin(fi * PIdiv180);
        y = CENTER_Y - round(y_d);

        /* Ограничиваем значения координат размерами изображения */
        if (x > width) {
            x = width;
        }

        if (y > height) {
            y = height;
        }

        g_assert (x >= 0 && x <= width);
        g_assert (y >= 0 && y <= height);

        /* Понижаем глубину цвета до 3 бит */
        pixel_offset = y * rowstride + x * n_channels;
        p = pixels + pixel_offset;
        rgb_colour = p[2] / 128 | (p[1] / 128 << 1) | (p[0] / 128 << 2);
        p = pixels_new + pixel_offset;
        p[0] = p[0] / 128 * 255;
        p[1] = p[1] / 128 * 255;
        p[2] = p[2] / 128 * 255;

        /* Переводим полученное значение в цветовую схему ARINC708 */
        arinc_colour = rgb_arinc_conversion[rgb_colour];

        /* Заполняем поле данных в пакете по одному биту. Записываем биты цвета arinc_colour
        в пакет по одному. Сначала определяем номер бита от самого начала data_payload.

        bitnum = (radius-1)*3;

        Потом определяем номер байта целочисленным делением. Затем, определяем какой бит в байте
        нужно записать. Остаток от деления bitnum % 8 является позицией бита в байте. В левой части
        выражения,

        ((arinc_colour >> j) & 1) << (bitnum % 8)

        определяем что мы запишем в бит, согласно коду цвета */

        bitnum = (radius - 1) * 3;  /* номер бита в пакете */

        for (j = 2; j >= 0; --j)
        {
            bytenum = bitnum / 8;
            bitmask = ((arinc_colour >> j) & 1) << (bitnum % 8);   /* Номер бита в байте и его значение. В пакете младший вперед */
            packet_p->A708_STR.data_payload[bytenum] |= bitmask;   /* Побитовое ИЛИ с байтом пакета */
            bitnum++;
        }
    }
}

/**
 * @brief Функция инициализации пакета ARINC708
 * @return  Возвращает пакет ARINC708
 * @details Label 0x2D
 */
static a708_packet arinc708_init()
{
    a708_packet testpacket;
    uint32_t i;

    testpacket.A708_STR.label = 0b10110100;

    testpacket.A708_STR.STR_A708_B1.control_accept = 0b11;
    testpacket.A708_STR.STR_A708_B1.slave = 0;
    testpacket.A708_STR.STR_A708_B1.spare = 0;
    testpacket.A708_STR.STR_A708_B1.mode_annunciation = 0;
    testpacket.A708_STR.STR_A708_B1.faults = 0;
    testpacket.A708_STR.STR_A708_B1.stabilization = 0;
    testpacket.A708_STR.STR_A708_B1.operating_mode = 0b100;
    testpacket.A708_STR.STR_A708_B1.tilt = 0b1;
    testpacket.A708_STR.STR_A708_B1.gain_l = 0;

    testpacket.A708_STR.STR_A708_B2.gain_h = 0;
    testpacket.A708_STR.STR_A708_B2.range = 0b100;

    testpacket.A708_STR.STR_A708_B3.spare_1 = 0;
    testpacket.A708_STR.STR_A708_B3.data_accept = 0;
    testpacket.A708_STR.STR_A708_B3.scan_angle_l = 0;

    testpacket.A708_STR.STR_A708_B4.scan_angle_h = 0;
    testpacket.A708_STR.STR_A708_B4.spare_2 = 0;

    for (i = 0; i < PACKET_LEN_A708 - 8; i++)
        testpacket.A708_STR.data_payload[i] = 0;

    return testpacket;

}

/**
 * @brief В функции осуществляется проход по окружности изображения с радара с шагом scan_angle_step,
 * заполнение лучей развертки и отправки их по UDP.
 * @param[in]  input_p      Входной буфер пикселей
 */
extern void arinc708_send(GdkPixbuf *input_p)
{
    uint16_t scan_angle;  /* Угол развертки */
    GError *error = NULL;
    double fi = 0;
    struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
    a708_packet output_packet;
    const uint16_t scan_angle_step = 0b0000000000000001; /* Шаг развертки самый малый */
    GdkPixbuf *output;

    output = gdk_pixbuf_copy(input_p); /* Для создания проверочного изображения */

    /* Создание сокета */
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        fprintf(stderr, "socket creation failed\n");
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);

    if (inet_aton(USS_IP , &si_other.sin_addr) == 0)
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Проход по окружности с минимальным шагом */
    for (scan_angle = 0; scan_angle < 0b0001000000000000; scan_angle += scan_angle_step)
    {
        /* Инициализировать пакет */
        output_packet = arinc708_init();

        /* Получить угол */
        output_packet.A708_STR.STR_A708_B3.scan_angle_l = (uint8_t)((scan_angle & 0b0000000000011111));
        output_packet.A708_STR.STR_A708_B4.scan_angle_h = (uint8_t)(scan_angle >> 5);

        fi = angle_conversion(scan_angle);

        /* Получить цветовую информацию из буфера пикселей */
        fill_ray(&output_packet, input_p, fi);

        /* Отправка пакетов по UDP */
        sendto(s, &output_packet, PACKET_LEN_A708, 0 , (struct sockaddr *) &si_other, slen);

        /* Обеспечить паузу, согласно ТЗ */
        usleep(4400);

#ifdef DEBUG

        /* Отображение пакетов в консоли */
        uint32_t i, j, bytenum, bitinbyte;
        uint32_t bitnum = 0;

        for (i = 1; i <= 512; i++)
        {
            g_print("scan angle %f: radius: %d colour:", fi, i);

            bitnum = (i-1)*3;

            for(j = 0; j < 3; j++)
            {
                bytenum = bitnum / 8;
                bitinbyte = bitnum % 8;
                g_print("%d", (output_packet.A708_STR.data_payload[bytenum] >> bitinbyte) & 1);
                bitnum++;
            }
            g_print("\n");
        }

#endif // DEBUG

    }

    /* Сохранение буфера пикселей для просмотра обработанного изображения */
    gdk_pixbuf_save (output, "GENERATED_IMAGE", "png", &error, NULL);

}
