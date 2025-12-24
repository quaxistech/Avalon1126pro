/**
 * =============================================================================
 * @file    network.h
 * @brief   Avalon A1126pro - Сетевой модуль (заголовочный файл)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Управление сетевым интерфейсом DM9051 SPI Ethernet.
 * Предоставляет абстракцию над lwIP сокетами.
 * 
 * =============================================================================
 */

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <stdint.h>
#include <stddef.h>

/* ===========================================================================
 * ИНИЦИАЛИЗАЦИЯ И КОНФИГУРАЦИЯ
 * =========================================================================== */

/**
 * @brief Инициализация сети
 * @return 0 при успехе, -1 при ошибке
 */
int network_init(void);

/**
 * @brief Получение IP адреса
 * @param ip Буфер для IP (4 байта)
 * @return 0 при успехе
 */
int network_get_ip(uint8_t *ip);

/**
 * @brief Получение маски подсети
 * @param mask Буфер для маски (4 байта)
 * @return 0 при успехе
 */
int network_get_netmask(uint8_t *mask);

/**
 * @brief Получение шлюза по умолчанию
 * @param gw Буфер для шлюза (4 байта)
 * @return 0 при успехе
 */
int network_get_gateway(uint8_t *gw);

/**
 * @brief Получение MAC адреса
 * @param mac Буфер для MAC (6 байт)
 * @return 0 при успехе
 */
int network_get_mac(uint8_t *mac);

/**
 * @brief Проверка состояния сети
 * @return 1 если подключено, 0 иначе
 */
int network_is_connected(void);

/**
 * @brief Проверка инициализации
 * @return 1 если инициализировано, 0 иначе
 */
int network_is_initialized(void);

/**
 * @brief Установка статического IP
 * @param ip   IP адрес (4 байта)
 * @param mask Маска подсети (4 байта)
 * @param gw   Шлюз (4 байта)
 * @return 0 при успехе
 */
int network_set_static_ip(const uint8_t *ip, const uint8_t *mask, const uint8_t *gw);

/**
 * @brief Включение DHCP
 * @return 0 при успехе
 */
int network_enable_dhcp(void);

/* ===========================================================================
 * СОКЕТНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Создание TCP сокета
 * @return Дескриптор сокета или -1 при ошибке
 */
int network_socket_create(void);

/**
 * @brief Подключение TCP сокета к удалённому хосту
 * @param sock Дескриптор сокета
 * @param host Имя хоста или IP адрес
 * @param port Порт
 * @return 0 при успехе, -1 при ошибке
 */
int network_socket_connect(int sock, const char *host, int port);

/**
 * @brief Отправка данных через сокет
 * @param sock Дескриптор сокета
 * @param data Данные для отправки
 * @param len  Длина данных
 * @return Количество отправленных байт или -1 при ошибке
 */
int network_socket_send(int sock, const void *data, size_t len);

/**
 * @brief Приём данных из сокета
 * @param sock    Дескриптор сокета
 * @param buf     Буфер для данных
 * @param len     Максимальная длина
 * @param timeout Таймаут в мс (0 = без блокировки)
 * @return Количество прочитанных байт или -1 при ошибке
 */
int network_socket_recv(int sock, void *buf, size_t len, int timeout);

/**
 * @brief Закрытие сокета
 * @param sock Дескриптор сокета
 */
void network_socket_close(int sock);

/**
 * @brief Проверка доступных данных в сокете
 * @param sock Дескриптор сокета
 * @return Количество доступных байт
 */
int network_socket_available(int sock);

/**
 * @brief Создание слушающего TCP сокета (для сервера)
 * @param sock    Дескриптор сокета
 * @param port    Порт для прослушивания
 * @param backlog Максимальное количество ожидающих соединений
 * @return 0 при успехе или -1 при ошибке
 */
int network_socket_listen(int sock, int port, int backlog);

/**
 * @brief Принятие входящего соединения
 * @param listen_sock Слушающий сокет
 * @param timeout     Таймаут в мс
 * @return Дескриптор нового сокета или -1 при ошибке
 */
int network_socket_accept(int listen_sock, int timeout);

#endif /* __NETWORK_H__ */
