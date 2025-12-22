/**
 * =============================================================================
 * @file    http_server.c
 * @brief   HTTP сервер для веб-интерфейса майнера
 * =============================================================================
 * 
 * Веб-интерфейс предоставляет:
 * - Просмотр статуса майнера (хешрейт, температура, и т.д.)
 * - Настройку параметров (частота, напряжение, пулы)
 * - Обновление прошивки (OTA)
 * - Системное администрирование
 * 
 * Реализован простой HTTP сервер на базе lwIP.
 * Поддерживает методы GET и POST.
 * 
 * CGI обработчики:
 * - /cgi-bin/get_miner_status.cgi  - Статус майнера (JSON)
 * - /cgi-bin/set_miner_conf.cgi    - Настройка майнера
 * - /cgi-bin/get_network_info.cgi  - Сетевая информация
 * - /cgi-bin/set_network_conf.cgi  - Настройка сети
 * - /cgi-bin/reboot.cgi            - Перезагрузка
 * - /cgi-bin/upgrade.cgi           - Обновление прошивки
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "miner.h"
#include "avalon10.h"
#include "http_server.h"
#include "network.h"

/* lwIP заголовки */
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/sockets.h"

/* FreeRTOS */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

/* =============================================================================
 * КОНСТАНТЫ
 * ============================================================================= */

/** Максимальный размер HTTP запроса */
#define HTTP_MAX_REQUEST_SIZE   4096

/** Максимальный размер HTTP ответа */
#define HTTP_MAX_RESPONSE_SIZE  8192

/** Размер буфера для чтения */
#define HTTP_RECV_BUF_SIZE      1024

/** Максимальное количество одновременных соединений */
#define HTTP_MAX_CONNECTIONS    4

/** Таймаут соединения (мс) */
#define HTTP_CONN_TIMEOUT       30000

/** Максимальный размер URI */
#define HTTP_MAX_URI_SIZE       256

/** Максимальный размер тела запроса */
#define HTTP_MAX_BODY_SIZE      2048

/* =============================================================================
 * HTTP ЗАГОЛОВКИ
 * ============================================================================= */

/** HTTP статус 200 OK */
static const char http_200_ok[] = 
    "HTTP/1.1 200 OK\r\n";

/** HTTP статус 400 Bad Request */
static const char http_400_bad[] = 
    "HTTP/1.1 400 Bad Request\r\n";

/** HTTP статус 404 Not Found */
static const char http_404_not_found[] = 
    "HTTP/1.1 404 Not Found\r\n";

/** HTTP статус 500 Internal Server Error */
static const char http_500_error[] = 
    "HTTP/1.1 500 Internal Server Error\r\n";

/** Content-Type для HTML */
static const char http_content_html[] = 
    "Content-Type: text/html; charset=utf-8\r\n";

/** Content-Type для JSON */
static const char http_content_json[] = 
    "Content-Type: application/json\r\n";

/** Content-Type для CSS */
static const char http_content_css[] = 
    "Content-Type: text/css\r\n";

/** Content-Type для JavaScript */
static const char http_content_js[] = 
    "Content-Type: application/javascript\r\n";

/** Заголовок Connection: close */
static const char http_conn_close[] = 
    "Connection: close\r\n\r\n";

/* =============================================================================
 * СТАТИЧЕСКИЙ КОНТЕНТ (HTML страницы)
 * ============================================================================= */

/**
 * @brief Главная страница index.html
 * 
 * Содержит базовую структуру веб-интерфейса с навигацией.
 */
static const char index_html[] = 
"<!DOCTYPE html>\n"
"<html lang=\"ru\">\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>Avalon A1126pro - Панель управления</title>\n"
"    <link rel=\"stylesheet\" href=\"/styles.css\">\n"
"</head>\n"
"<body>\n"
"    <header>\n"
"        <h1>🔧 Avalon A1126pro</h1>\n"
"        <nav>\n"
"            <a href=\"/\" class=\"active\">Главная</a>\n"
"            <a href=\"/miner.html\">Майнер</a>\n"
"            <a href=\"/pools.html\">Пулы</a>\n"
"            <a href=\"/network.html\">Сеть</a>\n"
"            <a href=\"/system.html\">Система</a>\n"
"        </nav>\n"
"    </header>\n"
"    \n"
"    <main>\n"
"        <section class=\"status-cards\">\n"
"            <div class=\"card\">\n"
"                <h3>⚡ Хешрейт</h3>\n"
"                <p id=\"hashrate\">-- TH/s</p>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>🌡️ Температура</h3>\n"
"                <p id=\"temperature\">-- °C</p>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>💨 Вентиляторы</h3>\n"
"                <p id=\"fanspeed\">-- %</p>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>✓ Принято</h3>\n"
"                <p id=\"accepted\">--</p>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>✗ Отклонено</h3>\n"
"                <p id=\"rejected\">--</p>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>⏱️ Аптайм</h3>\n"
"                <p id=\"uptime\">--</p>\n"
"            </div>\n"
"        </section>\n"
"        \n"
"        <section class=\"pool-status\">\n"
"            <h2>Текущий пул</h2>\n"
"            <p id=\"pool-info\">Не подключено</p>\n"
"        </section>\n"
"        \n"
"        <section class=\"device-info\">\n"
"            <h2>Информация об устройстве</h2>\n"
"            <table>\n"
"                <tr><td>Модель:</td><td id=\"model\">Avalon A1126pro</td></tr>\n"
"                <tr><td>Прошивка:</td><td id=\"firmware\">--</td></tr>\n"
"                <tr><td>CGMiner:</td><td id=\"cgminer\">--</td></tr>\n"
"                <tr><td>MAC адрес:</td><td id=\"mac\">--</td></tr>\n"
"                <tr><td>IP адрес:</td><td id=\"ip\">--</td></tr>\n"
"            </table>\n"
"        </section>\n"
"    </main>\n"
"    \n"
"    <footer>\n"
"        <p>&copy; 2024 Avalon A1126pro Control Panel</p>\n"
"    </footer>\n"
"    \n"
"    <script src=\"/scripts.js\"></script>\n"
"    <script>\n"
"        // Обновление статуса каждые 5 секунд\n"
"        updateStatus();\n"
"        setInterval(updateStatus, 5000);\n"
"    </script>\n"
"</body>\n"
"</html>\n";

/**
 * @brief CSS стили
 */
static const char styles_css[] = 
"* {\n"
"    margin: 0;\n"
"    padding: 0;\n"
"    box-sizing: border-box;\n"
"}\n"
"\n"
"body {\n"
"    font-family: 'Segoe UI', Arial, sans-serif;\n"
"    background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);\n"
"    color: #eee;\n"
"    min-height: 100vh;\n"
"}\n"
"\n"
"header {\n"
"    background: rgba(0,0,0,0.3);\n"
"    padding: 1rem 2rem;\n"
"    display: flex;\n"
"    justify-content: space-between;\n"
"    align-items: center;\n"
"    flex-wrap: wrap;\n"
"}\n"
"\n"
"header h1 {\n"
"    color: #4ecca3;\n"
"    font-size: 1.5rem;\n"
"}\n"
"\n"
"nav a {\n"
"    color: #aaa;\n"
"    text-decoration: none;\n"
"    margin-left: 1.5rem;\n"
"    padding: 0.5rem 1rem;\n"
"    border-radius: 5px;\n"
"    transition: all 0.3s;\n"
"}\n"
"\n"
"nav a:hover, nav a.active {\n"
"    color: #fff;\n"
"    background: #4ecca3;\n"
"}\n"
"\n"
"main {\n"
"    padding: 2rem;\n"
"    max-width: 1400px;\n"
"    margin: 0 auto;\n"
"}\n"
"\n"
".status-cards {\n"
"    display: grid;\n"
"    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));\n"
"    gap: 1.5rem;\n"
"    margin-bottom: 2rem;\n"
"}\n"
"\n"
".card {\n"
"    background: rgba(255,255,255,0.05);\n"
"    border-radius: 10px;\n"
"    padding: 1.5rem;\n"
"    text-align: center;\n"
"    border: 1px solid rgba(78, 204, 163, 0.3);\n"
"    transition: transform 0.3s;\n"
"}\n"
"\n"
".card:hover {\n"
"    transform: translateY(-5px);\n"
"    border-color: #4ecca3;\n"
"}\n"
"\n"
".card h3 {\n"
"    color: #4ecca3;\n"
"    margin-bottom: 0.5rem;\n"
"    font-size: 0.9rem;\n"
"}\n"
"\n"
".card p {\n"
"    font-size: 1.8rem;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"section {\n"
"    background: rgba(255,255,255,0.05);\n"
"    border-radius: 10px;\n"
"    padding: 1.5rem;\n"
"    margin-bottom: 1.5rem;\n"
"}\n"
"\n"
"section h2 {\n"
"    color: #4ecca3;\n"
"    margin-bottom: 1rem;\n"
"    font-size: 1.2rem;\n"
"}\n"
"\n"
"table {\n"
"    width: 100%;\n"
"    border-collapse: collapse;\n"
"}\n"
"\n"
"table td {\n"
"    padding: 0.75rem;\n"
"    border-bottom: 1px solid rgba(255,255,255,0.1);\n"
"}\n"
"\n"
"table td:first-child {\n"
"    color: #888;\n"
"    width: 150px;\n"
"}\n"
"\n"
"input, select, button {\n"
"    padding: 0.75rem 1rem;\n"
"    border-radius: 5px;\n"
"    border: 1px solid rgba(255,255,255,0.2);\n"
"    background: rgba(0,0,0,0.3);\n"
"    color: #fff;\n"
"    font-size: 1rem;\n"
"}\n"
"\n"
"button {\n"
"    background: #4ecca3;\n"
"    border: none;\n"
"    cursor: pointer;\n"
"    font-weight: bold;\n"
"    transition: background 0.3s;\n"
"}\n"
"\n"
"button:hover {\n"
"    background: #3db892;\n"
"}\n"
"\n"
".btn-danger {\n"
"    background: #e74c3c;\n"
"}\n"
"\n"
".btn-danger:hover {\n"
"    background: #c0392b;\n"
"}\n"
"\n"
"footer {\n"
"    text-align: center;\n"
"    padding: 1rem;\n"
"    color: #666;\n"
"}\n"
"\n"
".form-group {\n"
"    margin-bottom: 1rem;\n"
"}\n"
"\n"
".form-group label {\n"
"    display: block;\n"
"    margin-bottom: 0.5rem;\n"
"    color: #aaa;\n"
"}\n"
"\n"
".form-row {\n"
"    display: flex;\n"
"    gap: 1rem;\n"
"    flex-wrap: wrap;\n"
"}\n"
"\n"
".form-row .form-group {\n"
"    flex: 1;\n"
"    min-width: 200px;\n"
"}\n"
"\n"
"input[type=\"text\"],\n"
"input[type=\"number\"],\n"
"input[type=\"password\"],\n"
"select {\n"
"    width: 100%;\n"
"}\n"
"\n"
".alert {\n"
"    padding: 1rem;\n"
"    border-radius: 5px;\n"
"    margin-bottom: 1rem;\n"
"}\n"
"\n"
".alert-success {\n"
"    background: rgba(78, 204, 163, 0.2);\n"
"    border: 1px solid #4ecca3;\n"
"}\n"
"\n"
".alert-error {\n"
"    background: rgba(231, 76, 60, 0.2);\n"
"    border: 1px solid #e74c3c;\n"
"}\n";

/**
 * @brief JavaScript код
 */
static const char scripts_js[] = 
"// Функция обновления статуса\n"
"function updateStatus() {\n"
"    fetch('/cgi-bin/get_miner_status.cgi')\n"
"        .then(response => response.json())\n"
"        .then(data => {\n"
"            // Обновляем значения на странице\n"
"            if (data.hashrate !== undefined) {\n"
"                document.getElementById('hashrate').textContent = \n"
"                    (data.hashrate / 1e12).toFixed(2) + ' TH/s';\n"
"            }\n"
"            if (data.temperature !== undefined) {\n"
"                document.getElementById('temperature').textContent = \n"
"                    data.temperature + ' °C';\n"
"            }\n"
"            if (data.fanspeed !== undefined) {\n"
"                document.getElementById('fanspeed').textContent = \n"
"                    data.fanspeed + ' %';\n"
"            }\n"
"            if (data.accepted !== undefined) {\n"
"                document.getElementById('accepted').textContent = \n"
"                    data.accepted;\n"
"            }\n"
"            if (data.rejected !== undefined) {\n"
"                document.getElementById('rejected').textContent = \n"
"                    data.rejected;\n"
"            }\n"
"            if (data.uptime !== undefined) {\n"
"                document.getElementById('uptime').textContent = \n"
"                    formatUptime(data.uptime);\n"
"            }\n"
"            if (data.pool) {\n"
"                document.getElementById('pool-info').textContent = \n"
"                    data.pool;\n"
"            }\n"
"            if (data.firmware) {\n"
"                document.getElementById('firmware').textContent = \n"
"                    data.firmware;\n"
"            }\n"
"            if (data.cgminer) {\n"
"                document.getElementById('cgminer').textContent = \n"
"                    data.cgminer;\n"
"            }\n"
"            if (data.mac) {\n"
"                document.getElementById('mac').textContent = \n"
"                    data.mac;\n"
"            }\n"
"            if (data.ip) {\n"
"                document.getElementById('ip').textContent = \n"
"                    data.ip;\n"
"            }\n"
"        })\n"
"        .catch(err => console.error('Ошибка получения статуса:', err));\n"
"}\n"
"\n"
"// Форматирование времени работы\n"
"function formatUptime(seconds) {\n"
"    var days = Math.floor(seconds / 86400);\n"
"    var hours = Math.floor((seconds % 86400) / 3600);\n"
"    var mins = Math.floor((seconds % 3600) / 60);\n"
"    \n"
"    if (days > 0) {\n"
"        return days + 'д ' + hours + 'ч ' + mins + 'м';\n"
"    } else if (hours > 0) {\n"
"        return hours + 'ч ' + mins + 'м';\n"
"    } else {\n"
"        return mins + 'м';\n"
"    }\n"
"}\n"
"\n"
"// Отправка формы\n"
"function submitForm(formId, url, callback) {\n"
"    var form = document.getElementById(formId);\n"
"    var formData = new FormData(form);\n"
"    \n"
"    fetch(url, {\n"
"        method: 'POST',\n"
"        body: formData\n"
"    })\n"
"    .then(response => response.json())\n"
"    .then(data => {\n"
"        if (callback) callback(data);\n"
"    })\n"
"    .catch(err => {\n"
"        console.error('Ошибка:', err);\n"
"        alert('Ошибка отправки данных');\n"
"    });\n"
"}\n"
"\n"
"// Показать сообщение\n"
"function showMessage(elementId, message, isError) {\n"
"    var el = document.getElementById(elementId);\n"
"    if (el) {\n"
"        el.textContent = message;\n"
"        el.className = 'alert ' + (isError ? 'alert-error' : 'alert-success');\n"
"        el.style.display = 'block';\n"
"        setTimeout(function() {\n"
"            el.style.display = 'none';\n"
"        }, 5000);\n"
"    }\n"
"}\n";

/* =============================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/** Сокет сервера */
static int server_socket = -1;

/** Время запуска (для аптайма) */
static uint32_t startup_time = 0;

/* =============================================================================
 * CGI ОБРАБОТЧИКИ
 * ============================================================================= */

/**
 * @brief CGI: Получение статуса майнера
 * 
 * Возвращает JSON со статусом:
 * - hashrate: хешрейт в H/s
 * - temperature: температура в °C
 * - fanspeed: скорость вентиляторов в %
 * - accepted: количество принятых шар
 * - rejected: количество отклонённых шар
 * - uptime: время работы в секундах
 * - pool: текущий пул
 * - firmware: версия прошивки
 * - cgminer: версия CGMiner
 * - mac: MAC адрес
 * - ip: IP адрес
 * 
 * @param response [out] Буфер для ответа
 * @param max_len Максимальный размер ответа
 * @return Длина ответа
 */
static int cgi_get_miner_status(char *response, int max_len)
{
    avalon10_device_t status;
    avalon10_get_device_status(&status);
    
    uint32_t uptime = (get_ms_time() - startup_time) / 1000;
    
    char ip_str[16] = "0.0.0.0";
    char mac_str[18] = "00:00:00:00:00:00";
    network_get_ip(ip_str);
    network_get_mac(mac_str);
    
    const char *pool_str = "Не подключено";
    if (current_pool != NULL && current_pool->state == POOL_CONNECTED) {
        pool_str = current_pool->url;
    }
    
    int len = snprintf(response, max_len,
        "{\n"
        "  \"hashrate\": %llu,\n"
        "  \"temperature\": %d,\n"
        "  \"fanspeed\": %d,\n"
        "  \"accepted\": %u,\n"
        "  \"rejected\": %u,\n"
        "  \"uptime\": %u,\n"
        "  \"pool\": \"%s\",\n"
        "  \"firmware\": \"%s\",\n"
        "  \"cgminer\": \"%s\",\n"
        "  \"mac\": \"%s\",\n"
        "  \"ip\": \"%s\",\n"
        "  \"frequency\": %d,\n"
        "  \"voltage\": %d,\n"
        "  \"asic_count\": %d,\n"
        "  \"device_count\": %d\n"
        "}\n",
        (unsigned long long)status.hashrate,
        status.temp_current,
        status.fan_speed,
        total_accepted,
        total_rejected,
        uptime,
        pool_str,
        FIRMWARE_VERSION,
        CGMINER_VERSION,
        mac_str,
        ip_str,
        status.frequency,
        status.voltage,
        status.asic_count,
        avalon10_get_device_count()
    );
    
    return len;
}

/**
 * @brief CGI: Настройка параметров майнера
 * 
 * Обрабатывает POST запрос с параметрами:
 * - frequency: частота в МГц
 * - voltage: напряжение в мВ
 * - fan_mode: режим вентиляторов (auto/manual)
 * - fan_speed: скорость вентиляторов (для manual)
 * 
 * @param body Тело POST запроса
 * @param body_len Длина тела
 * @param response [out] Буфер для ответа
 * @param max_len Максимальный размер ответа
 * @return Длина ответа
 */
static int cgi_set_miner_conf(const char *body, int body_len, 
                               char *response, int max_len)
{
    /* Парсим параметры из тела запроса */
    /* Формат: frequency=500&voltage=380&fan_mode=auto&fan_speed=80 */
    
    bool success = true;
    char msg[64] = "Настройки сохранены";
    
    /* Поиск и применение параметров */
    char *freq_str = strstr(body, "frequency=");
    if (freq_str != NULL) {
        int freq = atoi(freq_str + 10);
        if (freq >= AVA10_FREQ_MIN && freq <= AVA10_FREQ_MAX) {
            for (int i = 0; i < avalon10_get_device_count(); i++) {
                avalon10_set_frequency(i, freq);
            }
            applog(LOG_INFO, "HTTP: установлена частота %d МГц", freq);
        } else {
            success = false;
            snprintf(msg, sizeof(msg), "Неверная частота: %d", freq);
        }
    }
    
    char *volt_str = strstr(body, "voltage=");
    if (volt_str != NULL) {
        int volt = atoi(volt_str + 8);
        if (volt >= AVA10_VOLTAGE_MIN && volt <= AVA10_VOLTAGE_MAX) {
            for (int i = 0; i < avalon10_get_device_count(); i++) {
                avalon10_set_voltage(i, volt);
            }
            applog(LOG_INFO, "HTTP: установлено напряжение %d мВ", volt);
        } else {
            success = false;
            snprintf(msg, sizeof(msg), "Неверное напряжение: %d", volt);
        }
    }
    
    char *fan_speed_str = strstr(body, "fan_speed=");
    if (fan_speed_str != NULL) {
        int fan = atoi(fan_speed_str + 10);
        if (fan >= 0 && fan <= 100) {
            avalon10_set_fan_speed(fan);
            applog(LOG_INFO, "HTTP: установлена скорость вентиляторов %d%%", fan);
        }
    }
    
    /* Сохраняем конфигурацию */
    if (success) {
        avalon10_save_config();
    }
    
    int len = snprintf(response, max_len,
        "{\n"
        "  \"success\": %s,\n"
        "  \"message\": \"%s\"\n"
        "}\n",
        success ? "true" : "false",
        msg
    );
    
    return len;
}

/**
 * @brief CGI: Получение сетевой информации
 * 
 * @param response [out] Буфер для ответа
 * @param max_len Максимальный размер ответа
 * @return Длина ответа
 */
static int cgi_get_network_info(char *response, int max_len)
{
    char ip[16] = "0.0.0.0";
    char netmask[16] = "0.0.0.0";
    char gateway[16] = "0.0.0.0";
    char dns[16] = "0.0.0.0";
    char mac[18] = "00:00:00:00:00:00";
    char hostname[32] = "avalon";
    bool dhcp = true;
    
    network_get_ip(ip);
    network_get_netmask(netmask);
    network_get_gateway(gateway);
    network_get_dns(dns);
    network_get_mac(mac);
    network_get_hostname(hostname);
    dhcp = network_is_dhcp();
    
    int len = snprintf(response, max_len,
        "{\n"
        "  \"ip\": \"%s\",\n"
        "  \"netmask\": \"%s\",\n"
        "  \"gateway\": \"%s\",\n"
        "  \"dns\": \"%s\",\n"
        "  \"mac\": \"%s\",\n"
        "  \"hostname\": \"%s\",\n"
        "  \"dhcp\": %s\n"
        "}\n",
        ip, netmask, gateway, dns, mac, hostname,
        dhcp ? "true" : "false"
    );
    
    return len;
}

/**
 * @brief CGI: Перезагрузка устройства
 * 
 * @param response [out] Буфер для ответа
 * @param max_len Максимальный размер ответа
 * @return Длина ответа
 */
static int cgi_reboot(char *response, int max_len)
{
    applog(LOG_INFO, "HTTP: запрошена перезагрузка");
    
    int len = snprintf(response, max_len,
        "{\n"
        "  \"success\": true,\n"
        "  \"message\": \"Устройство перезагружается...\"\n"
        "}\n"
    );
    
    /* Планируем перезагрузку через 2 секунды */
    schedule_reboot(2000);
    
    return len;
}

/* =============================================================================
 * HTTP СЕРВЕР
 * ============================================================================= */

/**
 * @brief Определение Content-Type по расширению файла
 * 
 * @param uri URI запроса
 * @return Строка Content-Type
 */
static const char* get_content_type(const char *uri)
{
    const char *ext = strrchr(uri, '.');
    if (ext == NULL) {
        return http_content_html;
    }
    
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return http_content_html;
    } else if (strcmp(ext, ".css") == 0) {
        return http_content_css;
    } else if (strcmp(ext, ".js") == 0) {
        return http_content_js;
    } else if (strcmp(ext, ".json") == 0) {
        return http_content_json;
    }
    
    return http_content_html;
}

/**
 * @brief Обработка HTTP запроса
 * 
 * @param client_socket Сокет клиента
 */
static void handle_http_request(int client_socket)
{
    char recv_buf[HTTP_MAX_REQUEST_SIZE];
    char response[HTTP_MAX_RESPONSE_SIZE];
    char uri[HTTP_MAX_URI_SIZE];
    char body[HTTP_MAX_BODY_SIZE];
    
    /* Читаем запрос */
    int recv_len = recv(client_socket, recv_buf, sizeof(recv_buf) - 1, 0);
    if (recv_len <= 0) {
        return;
    }
    recv_buf[recv_len] = '\0';
    
    /* Парсим метод и URI */
    bool is_get = (strncmp(recv_buf, "GET ", 4) == 0);
    bool is_post = (strncmp(recv_buf, "POST ", 5) == 0);
    
    if (!is_get && !is_post) {
        /* Неизвестный метод */
        send(client_socket, http_400_bad, strlen(http_400_bad), 0);
        send(client_socket, http_conn_close, strlen(http_conn_close), 0);
        return;
    }
    
    /* Извлекаем URI */
    const char *uri_start = recv_buf + (is_get ? 4 : 5);
    const char *uri_end = strchr(uri_start, ' ');
    if (uri_end == NULL || uri_end - uri_start >= sizeof(uri)) {
        send(client_socket, http_400_bad, strlen(http_400_bad), 0);
        return;
    }
    
    int uri_len = uri_end - uri_start;
    memcpy(uri, uri_start, uri_len);
    uri[uri_len] = '\0';
    
    /* Извлекаем тело POST запроса */
    int body_len = 0;
    if (is_post) {
        const char *body_start = strstr(recv_buf, "\r\n\r\n");
        if (body_start != NULL) {
            body_start += 4;
            body_len = recv_len - (body_start - recv_buf);
            if (body_len > sizeof(body) - 1) {
                body_len = sizeof(body) - 1;
            }
            memcpy(body, body_start, body_len);
            body[body_len] = '\0';
        }
    }
    
    applog(LOG_DEBUG, "HTTP %s %s", is_get ? "GET" : "POST", uri);
    
    /* Формируем ответ */
    int resp_len = 0;
    const char *status = http_200_ok;
    const char *content_type = http_content_html;
    
    /* Обрабатываем запрос */
    if (strcmp(uri, "/") == 0 || strcmp(uri, "/index.html") == 0) {
        /* Главная страница */
        resp_len = snprintf(response, sizeof(response), "%s", index_html);
        content_type = http_content_html;
    }
    else if (strcmp(uri, "/styles.css") == 0) {
        /* CSS стили */
        resp_len = snprintf(response, sizeof(response), "%s", styles_css);
        content_type = http_content_css;
    }
    else if (strcmp(uri, "/scripts.js") == 0) {
        /* JavaScript */
        resp_len = snprintf(response, sizeof(response), "%s", scripts_js);
        content_type = http_content_js;
    }
    else if (strcmp(uri, "/cgi-bin/get_miner_status.cgi") == 0) {
        /* CGI: статус майнера */
        resp_len = cgi_get_miner_status(response, sizeof(response));
        content_type = http_content_json;
    }
    else if (strcmp(uri, "/cgi-bin/set_miner_conf.cgi") == 0 && is_post) {
        /* CGI: настройка майнера */
        resp_len = cgi_set_miner_conf(body, body_len, response, sizeof(response));
        content_type = http_content_json;
    }
    else if (strcmp(uri, "/cgi-bin/get_network_info.cgi") == 0) {
        /* CGI: сетевая информация */
        resp_len = cgi_get_network_info(response, sizeof(response));
        content_type = http_content_json;
    }
    else if (strcmp(uri, "/cgi-bin/reboot.cgi") == 0 && is_post) {
        /* CGI: перезагрузка */
        resp_len = cgi_reboot(response, sizeof(response));
        content_type = http_content_json;
    }
    else {
        /* 404 Not Found */
        status = http_404_not_found;
        resp_len = snprintf(response, sizeof(response),
            "<html><body><h1>404 Not Found</h1><p>%s</p></body></html>", uri);
    }
    
    /* Отправляем ответ */
    send(client_socket, status, strlen(status), 0);
    send(client_socket, content_type, strlen(content_type), 0);
    
    /* Content-Length */
    char content_len_str[32];
    snprintf(content_len_str, sizeof(content_len_str), 
             "Content-Length: %d\r\n", resp_len);
    send(client_socket, content_len_str, strlen(content_len_str), 0);
    
    send(client_socket, http_conn_close, strlen(http_conn_close), 0);
    
    /* Тело ответа */
    send(client_socket, response, resp_len, 0);
}

/**
 * @brief Инициализация HTTP сервера
 * 
 * @param port Порт для прослушивания (обычно 80)
 * @return 0 при успехе
 */
int http_server_init(int port)
{
    startup_time = get_ms_time();
    
    /* Создаём сокет сервера */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        applog(LOG_ERR, "HTTP: не удалось создать сокет");
        return -1;
    }
    
    /* Разрешаем повторное использование адреса */
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Привязываем к порту */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        applog(LOG_ERR, "HTTP: не удалось привязать сокет к порту %d", port);
        close(server_socket);
        server_socket = -1;
        return -2;
    }
    
    /* Начинаем слушать */
    if (listen(server_socket, HTTP_MAX_CONNECTIONS) < 0) {
        applog(LOG_ERR, "HTTP: ошибка listen()");
        close(server_socket);
        server_socket = -1;
        return -3;
    }
    
    /* Делаем сокет неблокирующим */
    int flags = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);
    
    applog(LOG_INFO, "HTTP сервер запущен на порту %d", port);
    
    return 0;
}

/**
 * @brief Обработка HTTP запросов (неблокирующая)
 * 
 * Вызывается периодически из задачи HTTP сервера.
 */
void http_server_poll(void)
{
    if (server_socket < 0) {
        return;
    }
    
    /* Принимаем входящее соединение */
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_socket = accept(server_socket, 
                                (struct sockaddr *)&client_addr, 
                                &addr_len);
    
    if (client_socket < 0) {
        /* Нет входящих соединений - это нормально */
        return;
    }
    
    /* Устанавливаем таймаут для клиентского сокета */
    struct timeval tv;
    tv.tv_sec = HTTP_CONN_TIMEOUT / 1000;
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    /* Обрабатываем запрос */
    handle_http_request(client_socket);
    
    /* Закрываем соединение */
    close(client_socket);
}
