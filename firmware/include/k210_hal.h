/**
 * =============================================================================
 * @file    k210_hal.h
 * @brief   Заголовочный файл HAL для Kendryte K210
 * =============================================================================
 * 
 * Определения для доступа к аппаратуре процессора K210.
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef _K210_HAL_H_
#define _K210_HAL_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * КОНСТАНТЫ GPIO
 * ============================================================================= */

/** Режим GPIO - вход */
#define GPIO_DM_INPUT       0

/** Режим GPIO - выход */
#define GPIO_DM_OUTPUT      1

/** Значение GPIO - низкий уровень */
#define GPIO_PV_LOW         0

/** Значение GPIO - высокий уровень */
#define GPIO_PV_HIGH        1

/* =============================================================================
 * КОНСТАНТЫ UART
 * ============================================================================= */

#define UART_BITWIDTH_8BIT  8
#define UART_STOP_1         1
#define UART_STOP_2         2
#define UART_PARITY_NONE    0
#define UART_PARITY_ODD     1
#define UART_PARITY_EVEN    2

/* =============================================================================
 * КОНСТАНТЫ SPI
 * ============================================================================= */

/** SPI режим 0: CPOL=0, CPHA=0 */
#define SPI_WORK_MODE_0     0
/** SPI режим 1: CPOL=0, CPHA=1 */
#define SPI_WORK_MODE_1     1
/** SPI режим 2: CPOL=1, CPHA=0 */
#define SPI_WORK_MODE_2     2
/** SPI режим 3: CPOL=1, CPHA=1 */
#define SPI_WORK_MODE_3     3

/** Стандартный формат кадра SPI */
#define SPI_FF_STANDARD     0

/* =============================================================================
 * КОНСТАНТЫ СВЕТОДИОДОВ
 * ============================================================================= */

#define LED_STATUS_OFF      0
#define LED_STATUS_READY    1
#define LED_STATUS_ERROR    2

/* =============================================================================
 * КОНСТАНТЫ ТАКТИРОВАНИЯ
 * ============================================================================= */

/** Номера периферийных устройств для sysctl_clock_enable */
#define SYSCTL_CLOCK_GPIO   0
#define SYSCTL_CLOCK_SPI0   1
#define SYSCTL_CLOCK_SPI1   2
#define SYSCTL_CLOCK_I2C0   3
#define SYSCTL_CLOCK_I2C1   4
#define SYSCTL_CLOCK_I2C2   5
#define SYSCTL_CLOCK_UART1  6
#define SYSCTL_CLOCK_UART2  7
#define SYSCTL_CLOCK_UART3  8
#define SYSCTL_CLOCK_TIMER0 9
#define SYSCTL_CLOCK_TIMER1 10
#define SYSCTL_CLOCK_TIMER2 11

/** Номера PLL */
#define SYSCTL_PLL0         0
#define SYSCTL_PLL1         1
#define SYSCTL_PLL2         2

/* =============================================================================
 * КОНСТАНТЫ FPIOA (функции пинов)
 * ============================================================================= */

#define FUNC_GPIO0          0
#define FUNC_GPIO1          1
#define FUNC_GPIO2          2
#define FUNC_GPIO3          3
#define FUNC_GPIO4          4
#define FUNC_GPIO5          5
#define FUNC_GPIO6          6
#define FUNC_GPIO7          7

#define FUNC_SPI0_D0        8   /**< SPI0 MOSI */
#define FUNC_SPI0_D1        9   /**< SPI0 MISO */
#define FUNC_SPI0_SCLK      10  /**< SPI0 Clock */
#define FUNC_SPI0_SS0       11  /**< SPI0 Chip Select 0 */
#define FUNC_SPI0_SS1       12  /**< SPI0 Chip Select 1 */
#define FUNC_SPI0_SS2       13  /**< SPI0 Chip Select 2 */
#define FUNC_SPI0_SS3       14  /**< SPI0 Chip Select 3 */

#define FUNC_I2C0_SDA       24  /**< I2C0 Data */
#define FUNC_I2C0_SCLK      25  /**< I2C0 Clock */

#define FUNC_UART1_TX       32  /**< UART1 TX */
#define FUNC_UART1_RX       33  /**< UART1 RX */

#define FUNC_TIMER0_TOGGLE1 40  /**< Timer0 Toggle 1 (PWM) */
#define FUNC_TIMER0_TOGGLE2 41  /**< Timer0 Toggle 2 (PWM) */

/* =============================================================================
 * КОНСТАНТЫ ТАЙМЕРОВ
 * ============================================================================= */

#define TIMER_DEVICE_0      0
#define TIMER_DEVICE_1      1
#define TIMER_DEVICE_2      2

#define TIMER_CHANNEL_0     0
#define TIMER_CHANNEL_1     1
#define TIMER_CHANNEL_2     2
#define TIMER_CHANNEL_3     3

/* =============================================================================
 * КОНСТАНТЫ PWM
 * ============================================================================= */

#define PWM_DEVICE_0        0
#define PWM_DEVICE_1        1

#define PWM_CHANNEL_0       0
#define PWM_CHANNEL_1       1
#define PWM_CHANNEL_2       2
#define PWM_CHANNEL_3       3

/* =============================================================================
 * КОНСТАНТЫ WATCHDOG
 * ============================================================================= */

#define WDT_DEVICE_0        0
#define WDT_DEVICE_1        1

/* =============================================================================
 * УРОВНИ ЛОГИРОВАНИЯ
 * ============================================================================= */

#define LOG_ERR             0   /**< Ошибка */
#define LOG_WARNING         1   /**< Предупреждение */
#define LOG_INFO            2   /**< Информация */
#define LOG_DEBUG           3   /**< Отладка */

/* =============================================================================
 * ФУНКЦИИ СИСТЕМНОГО КОНТРОЛЛЕРА
 * ============================================================================= */

/**
 * @brief Инициализация системного контроллера
 */
void sysctl_init(void);

/**
 * @brief Установка частоты PLL
 * @param pll Номер PLL (0, 1 или 2)
 * @param freq Целевая частота в Гц
 * @return 0 при успехе
 */
int sysctl_pll_set_freq(int pll, uint32_t freq);

/**
 * @brief Установка частоты CPU
 * @param freq Частота в Гц
 * @return 0 при успехе
 */
int sysctl_cpu_set_freq(uint32_t freq);

/**
 * @brief Включение тактирования периферии
 * @param periph Номер периферийного устройства
 */
void sysctl_clock_enable(int periph);

/**
 * @brief Программный сброс системы
 */
void sysctl_reset(void);

/**
 * @brief Планирование перезагрузки
 * @param delay_ms Задержка перед перезагрузкой в мс
 */
void schedule_reboot(int delay_ms);

/* =============================================================================
 * ФУНКЦИИ GPIO
 * ============================================================================= */

/**
 * @brief Инициализация GPIO
 */
void gpio_init(void);

/**
 * @brief Установка режима GPIO пина
 * @param pin Номер пина
 * @param mode Режим (GPIO_DM_INPUT или GPIO_DM_OUTPUT)
 */
void gpio_set_drive_mode(int pin, int mode);

/**
 * @brief Установка значения GPIO пина
 * @param pin Номер пина
 * @param value GPIO_PV_LOW или GPIO_PV_HIGH
 */
void gpio_set_pin(int pin, int value);

/**
 * @brief Чтение значения GPIO пина
 * @param pin Номер пина
 * @return GPIO_PV_LOW или GPIO_PV_HIGH
 */
int gpio_get_pin(int pin);

/**
 * @brief Управление светодиодом
 * @param pin Номер пина светодиода
 * @param on true для включения
 */
void led_set(int pin, bool on);

/**
 * @brief Установка статуса светодиода
 * @param status Код статуса
 */
void led_set_status(int status);

/**
 * @brief Мигание красным светодиодом
 */
void led_error_blink(void);

/* =============================================================================
 * ФУНКЦИИ FPIOA
 * ============================================================================= */

/**
 * @brief Настройка функции пина FPIOA
 * @param pin Номер физического пина
 * @param func Функция пина
 */
void fpioa_set_function(int pin, int func);

/* =============================================================================
 * ФУНКЦИИ SPI
 * ============================================================================= */

/**
 * @brief Инициализация SPI
 * @param spi_num Номер SPI (0 или 1)
 * @param mode Режим SPI (0-3)
 * @param frame_format Формат кадра
 * @param data_bits Количество бит данных
 * @param endian Порядок байт
 */
void spi_init(int spi_num, int mode, int frame_format, int data_bits, int endian);

/**
 * @brief Установка скорости SPI
 * @param spi_num Номер SPI
 * @param baudrate Скорость в Гц
 */
void spi_set_clk_rate(int spi_num, uint32_t baudrate);

/**
 * @brief Отправка и приём данных по SPI
 * @param spi_num Номер SPI
 * @param tx_data Данные для отправки
 * @param rx_data Буфер для приёма
 * @param len Длина данных
 * @return 0 при успехе
 */
int spi_send_receive(int spi_num, const uint8_t *tx_data, uint8_t *rx_data, int len);

/* =============================================================================
 * ФУНКЦИИ UART
 * ============================================================================= */

/**
 * @brief Инициализация UART
 * @param uart_num Номер UART
 */
void uart_init(int uart_num);

/**
 * @brief Настройка UART
 * @param uart_num Номер UART
 * @param baudrate Скорость передачи
 * @param data_bits Биты данных
 * @param stop_bits Стоп-биты
 * @param parity Чётность
 */
void uart_configure(int uart_num, uint32_t baudrate, int data_bits, int stop_bits, int parity);

/**
 * @brief Отправка байта по UART
 * @param uart_num Номер UART
 * @param ch Байт для отправки
 */
void uart_send(int uart_num, uint8_t ch);

/**
 * @brief Приём байта по UART
 * @param uart_num Номер UART
 * @return Принятый байт или -1 если данных нет
 */
int uart_recv(int uart_num);

/* =============================================================================
 * ФУНКЦИИ I2C
 * ============================================================================= */

/**
 * @brief Инициализация I2C
 * @param i2c_num Номер I2C
 * @param slave_addr Адрес ведомого
 * @param addr_width Ширина адреса (7 или 10 бит)
 * @param clk_rate Скорость I2C
 */
void i2c_init(int i2c_num, uint8_t slave_addr, int addr_width, uint32_t clk_rate);

/* =============================================================================
 * ФУНКЦИИ ТАЙМЕРА И PWM
 * ============================================================================= */

/**
 * @brief Инициализация таймера
 * @param timer_num Номер таймера
 */
void timer_init(int timer_num);

/**
 * @brief Установка интервала таймера
 * @param timer_num Номер таймера
 * @param channel Канал таймера
 * @param interval_ns Интервал в наносекундах
 */
void timer_set_interval(int timer_num, int channel, uint32_t interval_ns);

/**
 * @brief Включение/выключение таймера
 * @param timer_num Номер таймера
 * @param channel Канал
 * @param enable 1 для включения
 */
void timer_set_enable(int timer_num, int channel, int enable);

/**
 * @brief Установка частоты ШИМ
 * @param pwm_num Номер ШИМ
 * @param channel Канал
 * @param freq Частота в Гц
 * @param duty Скважность (0.0 - 1.0)
 */
void pwm_set_frequency(int pwm_num, int channel, uint32_t freq, double duty);

/* =============================================================================
 * ФУНКЦИИ FLASH
 * ============================================================================= */

/**
 * @brief Чтение данных из Flash
 * @param addr Адрес во Flash
 * @param data Буфер для данных
 * @param len Длина данных
 * @return 0 при успехе
 */
int flash_read(uint32_t addr, uint8_t *data, int len);

/**
 * @brief Запись данных во Flash
 * @param addr Адрес во Flash
 * @param data Данные для записи
 * @param len Длина данных
 * @return 0 при успехе
 */
int flash_write(uint32_t addr, const uint8_t *data, int len);

/**
 * @brief Стирание сектора Flash
 * @param addr Адрес сектора
 * @return 0 при успехе
 */
int flash_erase_sector(uint32_t addr);

/* =============================================================================
 * ФУНКЦИИ WATCHDOG
 * ============================================================================= */

/**
 * @brief Инициализация Watchdog
 * @param wdt_num Номер WDT
 * @param timeout_ms Таймаут в миллисекундах
 * @param callback Функция обратного вызова
 */
void wdt_init(int wdt_num, uint32_t timeout_ms, void (*callback)(void));

/**
 * @brief Запуск Watchdog
 * @param wdt_num Номер WDT
 */
void wdt_start(int wdt_num);

/**
 * @brief Сброс Watchdog
 * @param wdt_num Номер WDT
 */
void wdt_feed(int wdt_num);

/* =============================================================================
 * ФУНКЦИИ PLIC
 * ============================================================================= */

/**
 * @brief Инициализация контроллера прерываний
 */
void plic_init(void);

/* =============================================================================
 * УТИЛИТЫ ВРЕМЕНИ
 * ============================================================================= */

/**
 * @brief Получение времени в миллисекундах
 * @return Время в мс с момента запуска
 */
uint32_t get_ms_time(void);

/**
 * @brief Задержка в миллисекундах
 * @param ms Задержка в мс
 */
void delay_ms(uint32_t ms);

/**
 * @brief Задержка в микросекундах
 * @param us Задержка в мкс
 */
void delay_us(uint32_t us);

/* =============================================================================
 * ЛОГИРОВАНИЕ
 * ============================================================================= */

/**
 * @brief Функция логирования
 * @param level Уровень логирования
 * @param fmt Формат строки
 * @param ... Аргументы
 */
void applog(int level, const char *fmt, ...);

/* =============================================================================
 * ПРЕРЫВАНИЯ
 * ============================================================================= */

/**
 * @brief Отключение прерываний
 */
void __disable_irq(void);

/**
 * @brief Включение прерываний
 */
void __enable_irq(void);

#ifdef __cplusplus
}
#endif

#endif /* _K210_HAL_H_ */
