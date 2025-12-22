/*
 * K210 Hardware Initialization for Avalon A1126pro
 * Reconstructed from firmware analysis
 *
 * Based on kendryte-freertos-sdk
 */

#include <stdint.h>
#include "sysctl.h"
#include "gpio.h"
#include "spi.h"
#include "uart.h"
#include "plic.h"

/* Clock Configuration */
#define K210_PLL0_FREQ      800000000   /* 800 MHz */
#define K210_PLL1_FREQ      400000000   /* 400 MHz for peripherals */
#define K210_CPU_FREQ       400000000   /* 400 MHz CPU clock */

/* GPIO Pin Assignments (estimated from analysis) */
#define GPIO_HASH_RESET     0
#define GPIO_HASH_CHAIN_EN  1
#define GPIO_LED_STATUS     2
#define GPIO_FAN_PWM        3

/* SPI Configuration for ASIC communication */
#define SPI_HASH_DEVICE     SPI_DEVICE_0
#define SPI_HASH_BAUDRATE   10000000    /* 10 MHz */
#define SPI_HASH_CS         SPI_CHIP_SELECT_0

void k210_hardware_init(void)
{
    /* Initialize system control */
    sysctl_pll_set_freq(SYSCTL_PLL0, K210_PLL0_FREQ);
    sysctl_pll_set_freq(SYSCTL_PLL1, K210_PLL1_FREQ);
    sysctl_cpu_set_freq(K210_CPU_FREQ);
    
    /* Enable peripheral clocks */
    sysctl_clock_enable(SYSCTL_CLOCK_GPIO);
    sysctl_clock_enable(SYSCTL_CLOCK_SPI0);
    sysctl_clock_enable(SYSCTL_CLOCK_UART1);
    
    /* Configure GPIO */
    gpio_init();
    gpio_set_drive_mode(GPIO_HASH_RESET, GPIO_DM_OUTPUT);
    gpio_set_drive_mode(GPIO_LED_STATUS, GPIO_DM_OUTPUT);
    gpio_set_drive_mode(GPIO_FAN_PWM, GPIO_DM_OUTPUT);
    
    /* Configure SPI for ASIC communication */
    spi_init(SPI_HASH_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_set_clk_rate(SPI_HASH_DEVICE, SPI_HASH_BAUDRATE);
    
    /* Initialize interrupt controller */
    plic_init();
    
    /* Reset hash ASICs */
    gpio_set_pin(GPIO_HASH_RESET, GPIO_PV_LOW);
    /* delay_ms(100); */
    gpio_set_pin(GPIO_HASH_RESET, GPIO_PV_HIGH);
}

/* SPI transaction for hash chip communication */
int spi_hash_xfer(uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    return spi_send_data_standard(SPI_HASH_DEVICE, SPI_HASH_CS,
                                   tx_buf, len, rx_buf, len);
}
