/**
 * =============================================================================
 * @file    k210_hal.c
 * @brief   Уровень аппаратной абстракции (HAL) для процессора Kendryte K210
 * =============================================================================
 * 
 * Этот файл содержит функции низкоуровневого доступа к аппаратуре K210:
 * - GPIO
 * - SPI
 * - UART
 * - I2C
 * - Таймеры и PWM
 * - Flash память
 * - Системные функции
 * 
 * K210 - двухъядерный RISC-V процессор с частотой до 800 МГц.
 * Адреса регистров соответствуют документации Kendryte.
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
#include "k210_hal.h"

/* =============================================================================
 * РЕГИСТРЫ K210 (адреса в памяти)
 * ============================================================================= */

/* Базовые адреса периферии */
#define SYSCTL_BASE         0x50440000UL    /**< Системный контроллер */
#define FPIOA_BASE          0x502B0000UL    /**< Мультиплексор функций GPIO */
#define GPIO_BASE           0x50200000UL    /**< GPIO контроллер */
#define SPI0_BASE           0x52000000UL    /**< SPI0 контроллер */
#define SPI1_BASE           0x53000000UL    /**< SPI1 контроллер */
#define UART1_BASE          0x50210000UL    /**< UART1 контроллер */
#define UART2_BASE          0x50220000UL    /**< UART2 контроллер */
#define UART3_BASE          0x50230000UL    /**< UART3 контроллер */
#define I2C0_BASE           0x50280000UL    /**< I2C0 контроллер */
#define I2C1_BASE           0x50290000UL    /**< I2C1 контроллер */
#define I2C2_BASE           0x502A0000UL    /**< I2C2 контроллер */
#define TIMER0_BASE         0x502D0000UL    /**< Таймер 0 */
#define TIMER1_BASE         0x502E0000UL    /**< Таймер 1 */
#define TIMER2_BASE         0x502F0000UL    /**< Таймер 2 */
#define WDT0_BASE           0x50400000UL    /**< Watchdog 0 */
#define WDT1_BASE           0x50410000UL    /**< Watchdog 1 */
#define PLIC_BASE           0x0C000000UL    /**< Контроллер прерываний */
#define SPI_FLASH_BASE      0x90000000UL    /**< Память SPI Flash (XIP) */

/* =============================================================================
 * СТРУКТУРЫ РЕГИСТРОВ
 * ============================================================================= */

/**
 * @brief Регистры системного контроллера
 */
typedef struct {
    volatile uint32_t git_id;           /* 0x00: Git ID */
    volatile uint32_t clk_freq;         /* 0x04: Частота */
    volatile uint32_t pll0;             /* 0x08: PLL0 */
    volatile uint32_t pll1;             /* 0x0C: PLL1 */
    volatile uint32_t pll2;             /* 0x10: PLL2 */
    volatile uint32_t reserved1[3];
    volatile uint32_t pll_lock;         /* 0x20: Статус блокировки PLL */
    volatile uint32_t rom_error;        /* 0x24: ROM error */
    volatile uint32_t reserved2[6];
    volatile uint32_t clk_sel0;         /* 0x40: Выбор тактирования 0 */
    volatile uint32_t clk_sel1;         /* 0x44: Выбор тактирования 1 */
    volatile uint32_t clk_en_cent;      /* 0x48: Включение центрального такта */
    volatile uint32_t clk_en_peri;      /* 0x4C: Включение периферии */
    volatile uint32_t soft_reset;       /* 0x50: Программный сброс */
    volatile uint32_t peri_reset;       /* 0x54: Сброс периферии */
    volatile uint32_t clk_th0;          /* 0x58: Делитель 0 */
    volatile uint32_t clk_th1;          /* 0x5C: Делитель 1 */
    volatile uint32_t clk_th2;          /* 0x60: Делитель 2 */
    volatile uint32_t clk_th3;          /* 0x64: Делитель 3 */
    volatile uint32_t clk_th4;          /* 0x68: Делитель 4 */
    volatile uint32_t clk_th5;          /* 0x6C: Делитель 5 */
    volatile uint32_t clk_th6;          /* 0x70: Делитель 6 */
    volatile uint32_t misc;             /* 0x74: Разное */
    volatile uint32_t peri;             /* 0x78: Периферия */
    volatile uint32_t spi_sleep;        /* 0x7C: SPI sleep */
    volatile uint32_t reset_status;     /* 0x80: Статус сброса */
    volatile uint32_t dma_sel0;         /* 0x84: Выбор DMA 0 */
    volatile uint32_t dma_sel1;         /* 0x88: Выбор DMA 1 */
    volatile uint32_t power_sel;        /* 0x8C: Выбор питания */
} sysctl_regs_t;

/**
 * @brief Регистры GPIO
 */
typedef struct {
    volatile uint32_t data_out;         /* 0x00: Выходные данные */
    volatile uint32_t direction;        /* 0x04: Направление (1=выход) */
    volatile uint32_t source;           /* 0x08: Источник данных */
    volatile uint32_t reserved1[9];
    volatile uint32_t int_enable;       /* 0x30: Разрешение прерываний */
    volatile uint32_t int_mask;         /* 0x34: Маска прерываний */
    volatile uint32_t int_level;        /* 0x38: Тип прерывания (уровень/фронт) */
    volatile uint32_t int_polarity;     /* 0x3C: Полярность прерывания */
    volatile uint32_t int_status;       /* 0x40: Статус прерываний */
    volatile uint32_t int_raw_status;   /* 0x44: Сырой статус */
    volatile uint32_t int_debounce;     /* 0x48: Дебаунс */
    volatile uint32_t int_clear;        /* 0x4C: Очистка прерываний */
    volatile uint32_t data_in;          /* 0x50: Входные данные */
    volatile uint32_t reserved2[3];
    volatile uint32_t sync_level;       /* 0x60: Синхронизация уровня */
    volatile uint32_t id_code;          /* 0x64: ID код */
    volatile uint32_t int_both_edge;    /* 0x68: Прерывание по обоим фронтам */
} gpio_regs_t;

/**
 * @brief Регистры SPI
 */
typedef struct {
    volatile uint32_t ctrl0;            /* 0x00: Контроль 0 */
    volatile uint32_t ctrl1;            /* 0x04: Контроль 1 */
    volatile uint32_t ssienr;           /* 0x08: Включение SSI */
    volatile uint32_t mwcr;             /* 0x0C: Microwire control */
    volatile uint32_t ser;              /* 0x10: Выбор ведомого */
    volatile uint32_t baudr;            /* 0x14: Скорость передачи */
    volatile uint32_t txftlr;           /* 0x18: Порог TX FIFO */
    volatile uint32_t rxftlr;           /* 0x1C: Порог RX FIFO */
    volatile uint32_t txflr;            /* 0x20: Уровень TX FIFO */
    volatile uint32_t rxflr;            /* 0x24: Уровень RX FIFO */
    volatile uint32_t sr;               /* 0x28: Статус */
    volatile uint32_t imr;              /* 0x2C: Маска прерываний */
    volatile uint32_t isr;              /* 0x30: Статус прерываний */
    volatile uint32_t risr;             /* 0x34: Сырой статус прерываний */
    volatile uint32_t txoicr;           /* 0x38: TX overflow clear */
    volatile uint32_t rxoicr;           /* 0x3C: RX overflow clear */
    volatile uint32_t rxuicr;           /* 0x40: RX underflow clear */
    volatile uint32_t msticr;           /* 0x44: Multi-master clear */
    volatile uint32_t icr;              /* 0x48: Общий clear */
    volatile uint32_t dmacr;            /* 0x4C: DMA контроль */
    volatile uint32_t dmatdlr;          /* 0x50: DMA TX уровень */
    volatile uint32_t dmardlr;          /* 0x54: DMA RX уровень */
    volatile uint32_t idr;              /* 0x58: Идентификатор */
    volatile uint32_t version;          /* 0x5C: Версия */
    volatile uint32_t dr[36];           /* 0x60: Data register */
    volatile uint32_t rx_sample_dly;    /* 0xF0: Задержка семплирования */
    volatile uint32_t spi_ctrl0;        /* 0xF4: SPI контроль */
    volatile uint32_t reserved1;
    volatile uint32_t xip_mode_bits;    /* 0xFC: XIP mode bits */
    volatile uint32_t xip_incr_inst;    /* 0x100: XIP instruction */
    volatile uint32_t xip_wrap_inst;    /* 0x104: XIP wrap instruction */
    volatile uint32_t xip_ctrl;         /* 0x108: XIP контроль */
    volatile uint32_t xip_ser;          /* 0x10C: XIP slave enable */
    volatile uint32_t xrxoicr;          /* 0x110: XIP RX overflow clear */
    volatile uint32_t xip_cnt_timeout;  /* 0x114: XIP timeout */
    volatile uint32_t endian;           /* 0x118: Порядок байт */
} spi_regs_t;

/**
 * @brief Регистры UART
 */
typedef struct {
    union {
        volatile uint32_t rbr;          /* 0x00: Receive buffer (LCR[7]=0) */
        volatile uint32_t thr;          /* 0x00: Transmit holding (LCR[7]=0) */
        volatile uint32_t dll;          /* 0x00: Divisor latch low (LCR[7]=1) */
    };
    union {
        volatile uint32_t dlh;          /* 0x04: Divisor latch high (LCR[7]=1) */
        volatile uint32_t ier;          /* 0x04: Interrupt enable (LCR[7]=0) */
    };
    union {
        volatile uint32_t fcr;          /* 0x08: FIFO control (write) */
        volatile uint32_t iir;          /* 0x08: Interrupt identify (read) */
    };
    volatile uint32_t lcr;              /* 0x0C: Line control */
    volatile uint32_t mcr;              /* 0x10: Modem control */
    volatile uint32_t lsr;              /* 0x14: Line status */
    volatile uint32_t msr;              /* 0x18: Modem status */
    volatile uint32_t scr;              /* 0x1C: Scratch */
    volatile uint32_t reserved1[4];
    volatile uint32_t srbr_sthr[16];    /* 0x30: Shadow RBR/THR */
    volatile uint32_t far;              /* 0x70: FIFO access */
    volatile uint32_t tfr;              /* 0x74: TX FIFO read */
    volatile uint32_t rfw;              /* 0x78: RX FIFO write */
    volatile uint32_t usr;              /* 0x7C: UART status */
    volatile uint32_t tfl;              /* 0x80: TX FIFO level */
    volatile uint32_t rfl;              /* 0x84: RX FIFO level */
    volatile uint32_t srr;              /* 0x88: Software reset */
    volatile uint32_t srts;             /* 0x8C: Shadow RTS */
    volatile uint32_t sbcr;             /* 0x90: Shadow break control */
    volatile uint32_t sdmam;            /* 0x94: Shadow DMA mode */
    volatile uint32_t sfe;              /* 0x98: Shadow FIFO enable */
    volatile uint32_t srt;              /* 0x9C: Shadow RX trigger */
    volatile uint32_t stet;             /* 0xA0: Shadow TX trigger */
    volatile uint32_t htx;              /* 0xA4: Halt TX */
    volatile uint32_t dmasa;            /* 0xA8: DMA software ack */
    volatile uint32_t tcr;              /* 0xAC: Transceiver control */
    volatile uint32_t de_en;            /* 0xB0: DE enable */
    volatile uint32_t re_en;            /* 0xB4: RE enable */
    volatile uint32_t det;              /* 0xB8: DE timing */
    volatile uint32_t tat;              /* 0xBC: Turn-around timing */
    volatile uint32_t dlf;              /* 0xC0: Divisor latch fraction */
    volatile uint32_t rar;              /* 0xC4: Receive address */
    volatile uint32_t tar;              /* 0xC8: Transmit address */
    volatile uint32_t lcr_ext;          /* 0xCC: Line control extended */
} uart_regs_t;

/**
 * @brief Регистры таймера
 */
typedef struct {
    volatile uint32_t load;             /* 0x00: Начальное значение */
    volatile uint32_t value;            /* 0x04: Текущее значение */
    volatile uint32_t ctrl;             /* 0x08: Контроль */
    volatile uint32_t eoi;              /* 0x0C: End of interrupt */
    volatile uint32_t intr_stat;        /* 0x10: Статус прерывания */
} timer_channel_regs_t;

/* =============================================================================
 * ГЛОБАЛЬНЫЕ УКАЗАТЕЛИ НА РЕГИСТРЫ
 * ============================================================================= */

static volatile sysctl_regs_t * const sysctl = (sysctl_regs_t *)SYSCTL_BASE;
static volatile gpio_regs_t * const gpio = (gpio_regs_t *)GPIO_BASE;
static volatile spi_regs_t * const spi0 = (spi_regs_t *)SPI0_BASE;
static volatile spi_regs_t * const spi1 = (spi_regs_t *)SPI1_BASE;
static volatile uart_regs_t * const uart1 = (uart_regs_t *)UART1_BASE;
static volatile timer_channel_regs_t * const timer0 = (timer_channel_regs_t *)TIMER0_BASE;

/* =============================================================================
 * СИСТЕМНЫЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация системного контроллера
 */
void sysctl_init(void)
{
    /* Включаем все необходимые тактовые сигналы */
    sysctl->clk_en_cent = 0xFFFFFFFF;
    sysctl->clk_en_peri = 0xFFFFFFFF;
}

/**
 * @brief Установка частоты PLL
 * 
 * @param pll Номер PLL (0, 1 или 2)
 * @param freq Целевая частота в Гц
 * @return 0 при успехе
 */
int sysctl_pll_set_freq(int pll, uint32_t freq)
{
    /* Упрощённая реализация - в реальности нужен расчёт делителей */
    volatile uint32_t *pll_reg;
    
    switch (pll) {
        case 0: pll_reg = &sysctl->pll0; break;
        case 1: pll_reg = &sysctl->pll1; break;
        case 2: pll_reg = &sysctl->pll2; break;
        default: return -1;
    }
    
    /* Конфигурация PLL (упрощённо) */
    /* В реальной реализации нужно рассчитать CLKR, CLKF, CLKOD */
    
    /* Ждём блокировки PLL */
    while ((sysctl->pll_lock & (1 << pll)) == 0) {
        /* Ждём */
    }
    
    return 0;
}

/**
 * @brief Установка частоты CPU
 * 
 * @param freq Частота в Гц
 * @return 0 при успехе
 */
int sysctl_cpu_set_freq(uint32_t freq)
{
    /* Выбираем источник тактирования для CPU */
    /* CPU использует PLL0 */
    sysctl->clk_sel0 = 0x01;  /* Выбор PLL0 для CPU */
    
    return 0;
}

/**
 * @brief Включение тактирования периферии
 * 
 * @param periph Номер периферийного устройства
 */
void sysctl_clock_enable(int periph)
{
    sysctl->clk_en_peri |= (1 << periph);
}

/**
 * @brief Программный сброс системы
 */
void sysctl_reset(void)
{
    sysctl->soft_reset = 0x01;
    
    /* Никогда не должны сюда вернуться */
    while (1) { }
}

/**
 * @brief Планирование перезагрузки
 * 
 * @param delay_ms Задержка перед перезагрузкой в мс
 */
void schedule_reboot(int delay_ms)
{
    /* Запускаем таймер для отложенной перезагрузки */
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    sysctl_reset();
}

/* =============================================================================
 * GPIO ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация GPIO
 */
void gpio_init(void)
{
    /* Сброс состояния GPIO */
    gpio->data_out = 0;
    gpio->direction = 0;
    gpio->int_enable = 0;
}

/**
 * @brief Установка режима GPIO пина
 * 
 * @param pin Номер пина
 * @param mode Режим (GPIO_DM_INPUT или GPIO_DM_OUTPUT)
 */
void gpio_set_drive_mode(int pin, int mode)
{
    if (pin < 0 || pin >= 32) return;
    
    if (mode == GPIO_DM_OUTPUT) {
        gpio->direction |= (1 << pin);
    } else {
        gpio->direction &= ~(1 << pin);
    }
}

/**
 * @brief Установка значения GPIO пина
 * 
 * @param pin Номер пина
 * @param value GPIO_PV_LOW или GPIO_PV_HIGH
 */
void gpio_set_pin(int pin, int value)
{
    if (pin < 0 || pin >= 32) return;
    
    if (value) {
        gpio->data_out |= (1 << pin);
    } else {
        gpio->data_out &= ~(1 << pin);
    }
}

/**
 * @brief Чтение значения GPIO пина
 * 
 * @param pin Номер пина
 * @return GPIO_PV_LOW или GPIO_PV_HIGH
 */
int gpio_get_pin(int pin)
{
    if (pin < 0 || pin >= 32) return 0;
    
    return (gpio->data_in & (1 << pin)) ? GPIO_PV_HIGH : GPIO_PV_LOW;
}

/**
 * @brief Управление светодиодом
 * 
 * @param pin Номер пина светодиода
 * @param on true для включения
 */
void led_set(int pin, bool on)
{
    gpio_set_pin(pin, on ? GPIO_PV_HIGH : GPIO_PV_LOW);
}

/**
 * @brief Установка статуса светодиода
 * 
 * @param status Код статуса
 */
void led_set_status(int status)
{
    switch (status) {
        case LED_STATUS_OFF:
            led_set(PIN_LED_STATUS, false);
            led_set(PIN_LED_ERROR, false);
            break;
        case LED_STATUS_READY:
            led_set(PIN_LED_STATUS, true);
            led_set(PIN_LED_ERROR, false);
            break;
        case LED_STATUS_ERROR:
            led_set(PIN_LED_STATUS, false);
            led_set(PIN_LED_ERROR, true);
            break;
    }
}

/**
 * @brief Мигание красным светодиодом (индикация ошибки)
 */
void led_error_blink(void)
{
    for (int i = 0; i < 5; i++) {
        led_set(PIN_LED_ERROR, true);
        delay_ms(200);
        led_set(PIN_LED_ERROR, false);
        delay_ms(200);
    }
}

/* =============================================================================
 * FPIOA ФУНКЦИИ (мультиплексор GPIO)
 * ============================================================================= */

/**
 * @brief Настройка функции пина FPIOA
 * 
 * @param pin Номер физического пина
 * @param func Функция пина
 */
void fpioa_set_function(int pin, int func)
{
    volatile uint32_t *fpioa = (uint32_t *)FPIOA_BASE;
    
    if (pin < 0 || pin >= 48) return;
    
    fpioa[pin] = func;
}

/* =============================================================================
 * SPI ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация SPI
 * 
 * @param spi_num Номер SPI (0 или 1)
 * @param mode Режим SPI (0-3)
 * @param frame_format Формат кадра
 * @param data_bits Количество бит данных
 * @param endian Порядок байт
 */
void spi_init(int spi_num, int mode, int frame_format, int data_bits, int endian)
{
    volatile spi_regs_t *spi;
    
    if (spi_num == 0) {
        spi = spi0;
    } else if (spi_num == 1) {
        spi = spi1;
    } else {
        return;
    }
    
    /* Отключаем SPI */
    spi->ssienr = 0;
    
    /* Настройка режима */
    uint32_t ctrl0 = 0;
    ctrl0 |= (data_bits - 1) << 0;    /* DFS - Data frame size */
    ctrl0 |= frame_format << 4;       /* FRF - Frame format */
    ctrl0 |= (mode & 0x01) << 6;      /* SCPH - Clock phase */
    ctrl0 |= ((mode >> 1) & 0x01) << 7; /* SCPOL - Clock polarity */
    ctrl0 |= 0 << 8;                  /* TMOD - Transfer mode (TX & RX) */
    
    spi->ctrl0 = ctrl0;
    
    /* Настройка порогов FIFO */
    spi->txftlr = 0;
    spi->rxftlr = 0;
    
    /* Включаем SPI */
    spi->ssienr = 1;
}

/**
 * @brief Установка скорости SPI
 * 
 * @param spi_num Номер SPI
 * @param baudrate Скорость в Гц
 */
void spi_set_clk_rate(int spi_num, uint32_t baudrate)
{
    volatile spi_regs_t *spi;
    
    if (spi_num == 0) {
        spi = spi0;
    } else if (spi_num == 1) {
        spi = spi1;
    } else {
        return;
    }
    
    /* Рассчитываем делитель */
    /* Baudrate = SCLK / BAUDR, где BAUDR - чётное число от 2 до 65534 */
    uint32_t spi_clk = K210_CPU_FREQ / 2;  /* Частота SPI тактирования */
    uint32_t div = spi_clk / baudrate;
    
    if (div < 2) div = 2;
    if (div > 65534) div = 65534;
    if (div & 1) div++;  /* Должен быть чётным */
    
    spi->ssienr = 0;     /* Выключаем */
    spi->baudr = div;
    spi->ssienr = 1;     /* Включаем */
}

/**
 * @brief Отправка и приём данных по SPI
 * 
 * @param spi_num Номер SPI
 * @param tx_data Данные для отправки
 * @param rx_data Буфер для приёма (может быть NULL)
 * @param len Длина данных в байтах
 * @return 0 при успехе
 */
int spi_send_receive(int spi_num, const uint8_t *tx_data, uint8_t *rx_data, int len)
{
    volatile spi_regs_t *spi;
    
    if (spi_num == 0) {
        spi = spi0;
    } else if (spi_num == 1) {
        spi = spi1;
    } else {
        return -1;
    }
    
    int tx_idx = 0;
    int rx_idx = 0;
    
    while (tx_idx < len || rx_idx < len) {
        /* Записываем в TX FIFO */
        while (tx_idx < len && (spi->sr & 0x02)) {  /* TNF - TX FIFO not full */
            spi->dr[0] = tx_data ? tx_data[tx_idx] : 0xFF;
            tx_idx++;
        }
        
        /* Читаем из RX FIFO */
        while (rx_idx < len && (spi->sr & 0x08)) {  /* RNE - RX FIFO not empty */
            uint8_t data = spi->dr[0];
            if (rx_data) {
                rx_data[rx_idx] = data;
            }
            rx_idx++;
        }
    }
    
    /* Ждём завершения передачи */
    while (spi->sr & 0x01) {  /* Busy */
        /* Ждём */
    }
    
    return 0;
}

/* =============================================================================
 * UART ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация UART
 * 
 * @param uart_num Номер UART
 */
void uart_init(int uart_num)
{
    /* Конфигурация по умолчанию в uart_configure */
}

/**
 * @brief Настройка UART
 * 
 * @param uart_num Номер UART
 * @param baudrate Скорость передачи
 * @param data_bits Биты данных
 * @param stop_bits Стоп-биты
 * @param parity Чётность
 */
void uart_configure(int uart_num, uint32_t baudrate, int data_bits, int stop_bits, int parity)
{
    volatile uart_regs_t *uart = uart1;  /* Используем UART1 */
    
    /* Рассчитываем делитель */
    uint32_t uart_clk = K210_CPU_FREQ / 2;
    uint32_t divisor = uart_clk / (16 * baudrate);
    
    /* Включаем доступ к DLL/DLH */
    uart->lcr = 0x80;  /* DLAB = 1 */
    
    /* Устанавливаем делитель */
    uart->dll = divisor & 0xFF;
    uart->dlh = (divisor >> 8) & 0xFF;
    
    /* Настраиваем формат: 8N1 */
    uint32_t lcr = 0;
    lcr |= (data_bits - 5) & 0x03;  /* WLS - Word length */
    if (stop_bits == UART_STOP_2) {
        lcr |= 0x04;  /* STB - Stop bits */
    }
    if (parity != UART_PARITY_NONE) {
        lcr |= 0x08;  /* PEN - Parity enable */
        if (parity == UART_PARITY_EVEN) {
            lcr |= 0x10;  /* EPS - Even parity */
        }
    }
    
    uart->lcr = lcr;  /* DLAB = 0 */
    
    /* Включаем FIFO */
    uart->fcr = 0x07;  /* Enable FIFO, clear TX/RX FIFO */
}

/**
 * @brief Отправка байта по UART
 * 
 * @param uart_num Номер UART
 * @param ch Байт для отправки
 */
void uart_send(int uart_num, uint8_t ch)
{
    volatile uart_regs_t *uart = uart1;
    
    /* Ждём пока TX FIFO не освободится */
    while ((uart->lsr & 0x20) == 0) {
        /* Ждём */
    }
    
    uart->thr = ch;
}

/**
 * @brief Приём байта по UART
 * 
 * @param uart_num Номер UART
 * @return Принятый байт или -1 если данных нет
 */
int uart_recv(int uart_num)
{
    volatile uart_regs_t *uart = uart1;
    
    /* Проверяем есть ли данные */
    if ((uart->lsr & 0x01) == 0) {
        return -1;
    }
    
    return uart->rbr;
}

/* =============================================================================
 * I2C ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация I2C
 * 
 * @param i2c_num Номер I2C
 * @param slave_addr Адрес ведомого
 * @param addr_width Ширина адреса (7 или 10 бит)
 * @param clk_rate Скорость I2C
 */
void i2c_init(int i2c_num, uint8_t slave_addr, int addr_width, uint32_t clk_rate)
{
    /* Упрощённая реализация I2C */
    /* В реальной прошивке нужна полноценная инициализация */
}

/* =============================================================================
 * ТАЙМЕР И PWM ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация таймера
 * 
 * @param timer_num Номер таймера
 */
void timer_init(int timer_num)
{
    timer0->ctrl = 0;  /* Выключен */
}

/**
 * @brief Установка интервала таймера
 * 
 * @param timer_num Номер таймера
 * @param channel Канал таймера
 * @param interval_ns Интервал в наносекундах
 */
void timer_set_interval(int timer_num, int channel, uint32_t interval_ns)
{
    /* Рассчитываем значение счётчика */
    uint32_t timer_clk = K210_CPU_FREQ / 2;  /* Частота таймера */
    uint32_t count = (uint64_t)timer_clk * interval_ns / 1000000000ULL;
    
    timer0->load = count;
}

/**
 * @brief Включение/выключение таймера
 * 
 * @param timer_num Номер таймера
 * @param channel Канал
 * @param enable 1 для включения
 */
void timer_set_enable(int timer_num, int channel, int enable)
{
    if (enable) {
        timer0->ctrl = 0x01;  /* Enable */
    } else {
        timer0->ctrl = 0x00;  /* Disable */
    }
}

/**
 * @brief Установка частоты ШИМ
 * 
 * @param pwm_num Номер ШИМ
 * @param channel Канал
 * @param freq Частота в Гц
 * @param duty Скважность (0.0 - 1.0)
 */
void pwm_set_frequency(int pwm_num, int channel, uint32_t freq, double duty)
{
    /* Используем таймер для генерации ШИМ */
    uint32_t timer_clk = K210_CPU_FREQ / 2;
    uint32_t period = timer_clk / freq;
    uint32_t pulse = (uint32_t)(period * duty);
    
    timer0->load = period;
    timer0->ctrl = 0x03;  /* Enable + Auto-reload */
}

/* =============================================================================
 * FLASH ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Чтение данных из Flash
 * 
 * @param addr Адрес во Flash
 * @param data Буфер для данных
 * @param len Длина данных
 * @return 0 при успехе
 */
int flash_read(uint32_t addr, uint8_t *data, int len)
{
    /* При XIP режиме Flash отображается в адресное пространство */
    const uint8_t *flash = (const uint8_t *)(SPI_FLASH_BASE + addr);
    
    memcpy(data, flash, len);
    
    return 0;
}

/**
 * @brief Запись данных во Flash
 * 
 * @param addr Адрес во Flash
 * @param data Данные для записи
 * @param len Длина данных
 * @return 0 при успехе
 */
int flash_write(uint32_t addr, const uint8_t *data, int len)
{
    /* Упрощённая реализация */
    /* В реальности нужно использовать команды SPI Flash */
    
    /* Flash можно записывать только после стирания */
    /* И только по страницам (256 байт) */
    
    return 0;  /* TODO: Полная реализация */
}

/**
 * @brief Стирание сектора Flash
 * 
 * @param addr Адрес сектора
 * @return 0 при успехе
 */
int flash_erase_sector(uint32_t addr)
{
    /* Упрощённая реализация */
    /* Сектор обычно 4KB */
    
    return 0;  /* TODO: Полная реализация */
}

/* =============================================================================
 * WATCHDOG ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация Watchdog
 * 
 * @param wdt_num Номер WDT
 * @param timeout_ms Таймаут в миллисекундах
 * @param callback Функция обратного вызова (может быть NULL)
 */
void wdt_init(int wdt_num, uint32_t timeout_ms, void (*callback)(void))
{
    volatile uint32_t *wdt = (uint32_t *)WDT0_BASE;
    
    /* Рассчитываем значение таймера */
    uint32_t wdt_clk = K210_CPU_FREQ / 2;
    uint32_t count = (uint64_t)wdt_clk * timeout_ms / 1000;
    
    /* Устанавливаем таймаут */
    wdt[0] = count;  /* TORR - Timeout range */
    wdt[2] = 0x00;   /* CR - Control (пока выключен) */
}

/**
 * @brief Запуск Watchdog
 * 
 * @param wdt_num Номер WDT
 */
void wdt_start(int wdt_num)
{
    volatile uint32_t *wdt = (uint32_t *)WDT0_BASE;
    
    wdt[2] = 0x01;  /* CR - Enable */
}

/**
 * @brief Сброс (кормление) Watchdog
 * 
 * @param wdt_num Номер WDT
 */
void wdt_feed(int wdt_num)
{
    volatile uint32_t *wdt = (uint32_t *)WDT0_BASE;
    
    wdt[3] = 0x76;  /* CRR - Counter restart (magic value) */
}

/* =============================================================================
 * PLIC (контроллер прерываний)
 * ============================================================================= */

/**
 * @brief Инициализация контроллера прерываний
 */
void plic_init(void)
{
    volatile uint32_t *plic = (uint32_t *)PLIC_BASE;
    
    /* Установка порогов приоритета */
    plic[0x200000/4] = 0;  /* Threshold для hart 0 */
}

/* =============================================================================
 * УТИЛИТЫ ВРЕМЕНИ
 * ============================================================================= */

/** Счётчик миллисекунд (обновляется из SysTick) */
static volatile uint32_t tick_count = 0;

/**
 * @brief Получение времени в миллисекундах
 * 
 * @return Время в мс с момента запуска
 */
uint32_t get_ms_time(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief Задержка в миллисекундах
 * 
 * @param ms Задержка в мс
 */
void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief Задержка в микросекундах
 * 
 * @param us Задержка в мкс
 */
void delay_us(uint32_t us)
{
    /* Busy-loop для коротких задержек */
    volatile uint32_t cycles = us * (K210_CPU_FREQ / 1000000) / 5;
    while (cycles--) {
        __asm__ volatile("nop");
    }
}

/* =============================================================================
 * ЛОГИРОВАНИЕ
 * ============================================================================= */

/**
 * @brief Функция логирования (аналог CGMiner applog)
 * 
 * @param level Уровень логирования
 * @param fmt Формат строки
 * @param ... Аргументы
 */
void applog(int level, const char *fmt, ...)
{
    static const char *level_str[] = {
        "[ERR]", "[WARN]", "[INFO]", "[DEBUG]"
    };
    
    if (level < 0 || level > LOG_DEBUG) {
        level = LOG_INFO;
    }
    
    /* Выводим метку уровня */
    printf("%s ", level_str[level]);
    
    /* Выводим сообщение */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\r\n");
}

/* =============================================================================
 * ПРЕРЫВАНИЯ
 * ============================================================================= */

/**
 * @brief Отключение прерываний
 */
void __disable_irq(void)
{
    __asm__ volatile("csrc mstatus, 8");  /* Очистка MIE в mstatus */
}

/**
 * @brief Включение прерываний
 */
void __enable_irq(void)
{
    __asm__ volatile("csrs mstatus, 8");  /* Установка MIE в mstatus */
}
