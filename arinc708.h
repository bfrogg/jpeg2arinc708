#ifndef ARINC708_H_INCLUDED
#define ARINC708_H_INCLUDED

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>

/* Адрес и порт устройства */
#define USS_IP "192.168.1.18"
#define PORT 8888

#define PACKET_LEN_A708 200      /* Длина пакета ARINC708 в байтах */
#define CENTER_X 392             /* Координаты центра развертки на входном изображении */
#define CENTER_Y 385
#define RELATIVE_RADIUS 0.75     /* Отношение радиуса в пикселях исходного изображения к радиусу выходного */
#define PIdiv180 0.0174532925    /* pi/180, для перевода градусы в радианы */

/* Кодировки цветов из ARINC708 */
#define BLACK   0b00000000
#define BLUE    0b00000000 /* Синий цвет не используется */
#define GREEN   0b00000100
#define CYAN    0b00000101
#define RED     0b00000110
#define MAGENTA 0b00000001
#define YELLOW  0b00000010
#define WHITE   0b00000111


/* Массив для конвертации RGB в ARINC708. Индексом массива является цвет в RGB. */
static const uint8_t rgb_arinc_conversion[8] = {BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE};

/* Значения из стандарта ARINC708 */
static const double scan_angle_bits[12] =
{
    0.0879, 0.1758, 0.3516, 0.7031, 1.4062, 2.8125, 5.625, 11.25, 22.5, 45.0, 90.0, 180.0
};

/* Формат пакета ARINC708 */
typedef union a708_union
{
#pragma pack(push,1)
    struct a708_str
    {
        uint8_t label;
        struct
        {
            uint32_t control_accept : 2;
            uint32_t slave : 1;
            uint32_t spare : 2;
            uint32_t mode_annunciation : 5;
            uint32_t faults : 7;
            uint32_t stabilization : 1;
            uint32_t operating_mode : 3;
            uint32_t tilt : 7;
            uint32_t gain_l : 4;
        } STR_A708_B1;
        struct
        {
            uint8_t gain_h : 2;
            uint8_t range : 6;
        } STR_A708_B2;
        struct
        {
            uint8_t spare_1 : 1;
            uint8_t data_accept : 2;
            uint8_t scan_angle_l : 5;
        } STR_A708_B3;
        struct
        {
            uint8_t scan_angle_h : 7;
            uint8_t spare_2 : 1;
        } STR_A708_B4;
        uint8_t data_payload[PACKET_LEN_A708 - 8];
    } A708_STR;
    uint8_t default_708[PACKET_LEN_A708];
#pragma pack(pop)
} a708_packet;

extern void arinc708_send(GdkPixbuf *input);

#endif // ARINC708_H_INCLUDED
