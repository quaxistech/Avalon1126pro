/**
 * =============================================================================
 * @file    lwipopts.h
 * @brief   Конфигурация стека lwIP для Avalon A1126pro
 * =============================================================================
 * 
 * Этот файл содержит настройки сетевого стека lwIP:
 * - Параметры буферов и памяти
 * - Включённые протоколы
 * - Тайм-ауты и размеры
 * - Интеграция с FreeRTOS
 * 
 * lwIP (Lightweight IP) - это легковесная реализация TCP/IP стека
 * для встраиваемых систем с ограниченными ресурсами.
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ИНТЕГРАЦИЯ С ОПЕРАЦИОННОЙ СИСТЕМОЙ
 * ============================================================================= */

/*
 * Режим работы с ОС (FreeRTOS)
 * 0 = NO_SYS - работа без ОС
 * 1 = SYS - работа с ОС
 */
#define NO_SYS                          0

/* Поддержка потокобезопасных API */
#define LWIP_TCPIP_CORE_LOCKING         1

/* Использование почтовых ящиков FreeRTOS */
#define SYS_LIGHTWEIGHT_PROT            1

/* Включить ядро TCP/IP */
#define LWIP_TCPIP_CORE_LOCKING_INPUT   1

/* =============================================================================
 * УПРАВЛЕНИЕ ПАМЯТЬЮ
 * ============================================================================= */

/*
 * Размер кучи lwIP
 * Используется для выделения буферов, PCB и т.д.
 */
#define MEM_SIZE                        (64 * 1024)

/* Выравнивание памяти */
#define MEM_ALIGNMENT                   4

/* Использовать собственную реализацию malloc */
#define MEM_LIBC_MALLOC                 0

/* Размещение кучи в определённой секции памяти */
#define LWIP_RAM_HEAP_POINTER           NULL

/* =============================================================================
 * БУФЕРЫ ПАКЕТОВ (PBUF)
 * ============================================================================= */

/*
 * Пулы буферов различных размеров
 */

/* Размер данных в PBUF_POOL буфере */
#define PBUF_POOL_SIZE                  32

/* Размер каждого буфера в пуле */
#define PBUF_POOL_BUFSIZE               1600

/* Количество ссылочных буферов */
#define MEMP_NUM_PBUF                   32

/* Размер заголовка для выравнивания */
#define PBUF_LINK_HLEN                  16

/* =============================================================================
 * ARP - ПРОТОКОЛ РАЗРЕШЕНИЯ АДРЕСОВ
 * ============================================================================= */

/* Включить ARP */
#define LWIP_ARP                        1

/* Размер ARP таблицы */
#define ARP_TABLE_SIZE                  10

/* Время жизни записи ARP (секунды) */
#define ARP_MAXAGE                      300

/* Включить ARP-запросы */
#define ARP_QUEUEING                    1

/* Максимум пакетов в очереди ARP */
#define ARP_QUEUE_LEN                   3

/* =============================================================================
 * IP - ИНТЕРНЕТ ПРОТОКОЛ
 * ============================================================================= */

/* Включить IPv4 */
#define LWIP_IPV4                       1

/* Отключить IPv6 (не нужен для майнера) */
#define LWIP_IPV6                       0

/* Пересылка пакетов (роутер) */
#define IP_FORWARD                      0

/* Опции IP заголовка */
#define IP_OPTIONS_ALLOWED              1

/* Сборка фрагментированных пакетов */
#define IP_REASSEMBLY                   1

/* Фрагментация больших пакетов */
#define IP_FRAG                         1

/* Время ожидания фрагментов (мс) */
#define IP_REASS_MAXAGE                 3

/* Максимальный размер для сборки */
#define IP_REASS_MAX_PBUFS              10

/* TTL по умолчанию */
#define IP_DEFAULT_TTL                  255

/* =============================================================================
 * ICMP - ПРОТОКОЛ КОНТРОЛЬНЫХ СООБЩЕНИЙ
 * ============================================================================= */

/* Включить ICMP (для ping) */
#define LWIP_ICMP                       1

/* TTL для ICMP */
#define ICMP_TTL                        (IP_DEFAULT_TTL)

/* Отвечать на broadcast ping */
#define LWIP_BROADCAST_PING             1

/* Отвечать на multicast ping */
#define LWIP_MULTICAST_PING             1

/* =============================================================================
 * RAW API - НИЗКОУРОВНЕВЫЙ ДОСТУП
 * ============================================================================= */

/* Включить RAW API */
#define LWIP_RAW                        1

/* Максимум RAW PCB */
#define MEMP_NUM_RAW_PCB                4

/* TTL для RAW */
#define RAW_TTL                         (IP_DEFAULT_TTL)

/* =============================================================================
 * DHCP - АВТОМАТИЧЕСКАЯ НАСТРОЙКА СЕТИ
 * ============================================================================= */

/* Включить DHCP клиент */
#define LWIP_DHCP                       1

/* Поддержка автоматического назначения IP (AUTOIP) */
#define LWIP_AUTOIP                     0

/* Время ожидания DHCP (секунды) */
#define DHCP_DOES_ARP_CHECK             1

/* Максимальное количество попыток */
#define DHCP_COARSE_TIMER_MSECS         60000

/* =============================================================================
 * UDP - ПРОТОКОЛ ДЕЙТАГРАММ
 * ============================================================================= */

/* Включить UDP */
#define LWIP_UDP                        1

/* TTL для UDP */
#define UDP_TTL                         (IP_DEFAULT_TTL)

/* Максимум UDP PCB */
#define MEMP_NUM_UDP_PCB                8

/* =============================================================================
 * TCP - ПРОТОКОЛ УПРАВЛЕНИЯ ПЕРЕДАЧЕЙ
 * ============================================================================= */

/* Включить TCP */
#define LWIP_TCP                        1

/* TTL для TCP */
#define TCP_TTL                         (IP_DEFAULT_TTL)

/* Максимальный размер сегмента */
#define TCP_MSS                         1460

/* Размер окна отправки */
#define TCP_SND_BUF                     (8 * TCP_MSS)

/* Размер очереди отправки */
#define TCP_SND_QUEUELEN                ((4 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))

/* Размер окна приёма */
#define TCP_WND                         (8 * TCP_MSS)

/* Максимальное количество TCP соединений */
#define MEMP_NUM_TCP_PCB                16

/* Максимум слушающих сокетов */
#define MEMP_NUM_TCP_PCB_LISTEN         8

/* Размер очереди подключений */
#define TCP_LISTEN_BACKLOG              1

/* Максимальный размер очереди сегментов */
#define MEMP_NUM_TCP_SEG                32

/* Нормализация состояния */
#define TCP_OVERSIZE                    TCP_MSS

/* =============================================================================
 * АЛГОРИТМЫ TCP
 * ============================================================================= */

/* Алгоритм Нейгла */
#define TCP_NODELAY                     0

/* Расчёт RTT */
#define TCP_CALCULATE_EFF_SEND_MSS      1

/* =============================================================================
 * DNS - РАЗРЕШЕНИЕ ИМЁН
 * ============================================================================= */

/* Включить DNS клиент */
#define LWIP_DNS                        1

/* Размер таблицы DNS */
#define DNS_TABLE_SIZE                  4

/* Максимальная длина имени */
#define DNS_MAX_NAME_LENGTH             256

/* Максимум DNS серверов */
#define DNS_MAX_SERVERS                 2

/* Время жизни DNS записей */
#define DNS_MAX_TTL                     604800

/* =============================================================================
 * NETCONN API - ВЫСОКОУРОВНЕВОЕ API
 * ============================================================================= */

/* Включить NETCONN API */
#define LWIP_NETCONN                    1

/* Функция записи вектора */
#define LWIP_NETCONN_SEM_PER_THREAD     0

/* =============================================================================
 * SOCKET API - BSD СОВМЕСТИМЫЙ ИНТЕРФЕЙС
 * ============================================================================= */

/* Включить Socket API */
#define LWIP_SOCKET                     1

/* Совместимость с POSIX */
#define LWIP_POSIX_SOCKETS_IO_NAMES     1

/* Количество сокетов */
#define MEMP_NUM_NETCONN                16

/* Размер буфера приёма */
#define RECV_BUFSIZE_DEFAULT            (16 * 1024)

/* Включить SO_RCVTIMEO */
#define LWIP_SO_RCVTIMEO                1

/* Включить SO_SNDTIMEO */
#define LWIP_SO_SNDTIMEO                1

/* Включить SO_RCVBUF */
#define LWIP_SO_RCVBUF                  1

/* Включить SO_LINGER */
#define LWIP_SO_LINGER                  0

/* =============================================================================
 * СТАТИСТИКА
 * ============================================================================= */

/* Включить статистику */
#define LWIP_STATS                      1

/* Статистика по категориям */
#define LWIP_STATS_DISPLAY              1
#define LINK_STATS                      1
#define ETHARP_STATS                    1
#define IP_STATS                        1
#define ICMP_STATS                      1
#define UDP_STATS                       1
#define TCP_STATS                       1
#define MEM_STATS                       1
#define MEMP_STATS                      1
#define SYS_STATS                       1

/* =============================================================================
 * ОТЛАДКА
 * ============================================================================= */

/* Уровень отладки */
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL

/* Типы отладки */
#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON

/* Отключаем отладочные сообщения для продакшена */
#define ETHARP_DEBUG                    LWIP_DBG_OFF
#define NETIF_DEBUG                     LWIP_DBG_OFF
#define PBUF_DEBUG                      LWIP_DBG_OFF
#define API_LIB_DEBUG                   LWIP_DBG_OFF
#define API_MSG_DEBUG                   LWIP_DBG_OFF
#define SOCKETS_DEBUG                   LWIP_DBG_OFF
#define ICMP_DEBUG                      LWIP_DBG_OFF
#define INET_DEBUG                      LWIP_DBG_OFF
#define IP_DEBUG                        LWIP_DBG_OFF
#define IP_REASS_DEBUG                  LWIP_DBG_OFF
#define RAW_DEBUG                       LWIP_DBG_OFF
#define MEM_DEBUG                       LWIP_DBG_OFF
#define MEMP_DEBUG                      LWIP_DBG_OFF
#define SYS_DEBUG                       LWIP_DBG_OFF
#define TCP_DEBUG                       LWIP_DBG_OFF
#define TCP_INPUT_DEBUG                 LWIP_DBG_OFF
#define TCP_FR_DEBUG                    LWIP_DBG_OFF
#define TCP_RTO_DEBUG                   LWIP_DBG_OFF
#define TCP_CWND_DEBUG                  LWIP_DBG_OFF
#define TCP_WND_DEBUG                   LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG                LWIP_DBG_OFF
#define TCP_RST_DEBUG                   LWIP_DBG_OFF
#define TCP_QLEN_DEBUG                  LWIP_DBG_OFF
#define UDP_DEBUG                       LWIP_DBG_OFF
#define TCPIP_DEBUG                     LWIP_DBG_OFF
#define DHCP_DEBUG                      LWIP_DBG_OFF
#define DNS_DEBUG                       LWIP_DBG_OFF

/* =============================================================================
 * КОНТРОЛЬНЫЕ СУММЫ
 * ============================================================================= */

/* Аппаратные контрольные суммы (если поддерживаются) */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1

/* =============================================================================
 * NETIF - СЕТЕВОЙ ИНТЕРФЕЙС
 * ============================================================================= */

/* Callback при изменении состояния */
#define LWIP_NETIF_STATUS_CALLBACK      1

/* Callback при изменении линка */
#define LWIP_NETIF_LINK_CALLBACK        1

/* Поддержка loopback */
#define LWIP_NETIF_LOOPBACK             0

/* =============================================================================
 * ПРИЛОЖЕНИЯ
 * ============================================================================= */

/* Не включаем SNMP */
#define LWIP_SNMP                       0

/* Не включаем PPP */
#define PPP_SUPPORT                     0

/* Не включаем SLIP */
#define LWIP_HAVE_SLIPIF                0

/* =============================================================================
 * РАСШИРЕНИЯ
 * ============================================================================= */

/* Поддержка получения errno */
#define LWIP_PROVIDE_ERRNO              1

/* Потокобезопасный errno */
#define LWIP_ERRNO_STDINCLUDE           0

/* =============================================================================
 * НАСТРОЙКИ ДЛЯ МАЙНЕРА
 * ============================================================================= */

/*
 * Специфичные настройки для протокола Stratum
 */

/* Максимальный размер JSON сообщения */
#define STRATUM_JSON_MAX_SIZE           4096

/* Тайм-аут соединения с пулом (мс) */
#define POOL_CONNECTION_TIMEOUT         30000

/* Интервал keepalive (мс) */
#define POOL_KEEPALIVE_INTERVAL         60000

/* Количество повторных попыток */
#define POOL_RETRY_COUNT                5

#ifdef __cplusplus
}
#endif

#endif /* LWIPOPTS_H */
