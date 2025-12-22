/**
 * =============================================================================
 * @file    fan_control.c
 * @brief   Управление вентиляторами охлаждения
 * =============================================================================
 * 
 * Реализует:
 * - ПИД-регулирование скорости вентиляторов
 * - Защиту от перегрева
 * - Мониторинг оборотов
 * - Автоматическое и ручное управление
 * 
 * Система охлаждения критична для работы ASIC майнера.
 * Перегрев приводит к снижению производительности и отказам.
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include "fan_control.h"
#include "k210_hal.h"

/* =============================================================================
 * КОНСТАНТЫ И ПАРАМЕТРЫ
 * ============================================================================= */

/**
 * @brief   GPIO пины для вентиляторов
 */
#define FAN_PWM_PIN_0           28      /**< ШИМ вентилятора 0 */
#define FAN_PWM_PIN_1           29      /**< ШИМ вентилятора 1 */
#define FAN_TACH_PIN_0          30      /**< Тахометр вентилятора 0 */
#define FAN_TACH_PIN_1          31      /**< Тахометр вентилятора 1 */

/**
 * @brief   Параметры ШИМ
 */
#define FAN_PWM_FREQUENCY       25000   /**< Частота ШИМ 25кГц (стандарт Intel) */
#define FAN_PWM_CHANNEL_0       0       /**< Канал ШИМ для вентилятора 0 */
#define FAN_PWM_CHANNEL_1       1       /**< Канал ШИМ для вентилятора 1 */

/**
 * @brief   Ограничения
 */
#define FAN_MIN_DUTY            20      /**< Минимальный рабочий цикл (%) */
#define FAN_MAX_DUTY            100     /**< Максимальный рабочий цикл (%) */
#define FAN_START_DUTY          30      /**< Начальный рабочий цикл (%) */

/**
 * @brief   Температурные пороги (°C)
 */
#define TEMP_MIN                30      /**< Минимальная рабочая температура */
#define TEMP_TARGET_DEFAULT     75      /**< Целевая температура по умолчанию */
#define TEMP_WARNING            85      /**< Предупреждение о перегреве */
#define TEMP_CRITICAL           95      /**< Критическая температура */
#define TEMP_SHUTDOWN           100     /**< Температура аварийного отключения */

/**
 * @brief   Коэффициенты ПИД-регулятора
 * 
 * Настроены для плавного реагирования без осцилляций:
 * - Kp: быстрая реакция на изменение температуры
 * - Ki: устранение статической ошибки
 * - Kd: демпфирование колебаний
 */
#define PID_KP                  2.0f    /**< Пропорциональный коэффициент */
#define PID_KI                  0.1f    /**< Интегральный коэффициент */
#define PID_KD                  0.5f    /**< Дифференциальный коэффициент */
#define PID_INTEGRAL_MAX        100.0f  /**< Ограничение интегральной суммы */

/**
 * @brief   Параметры тахометра
 * 
 * Большинство вентиляторов выдают 2 импульса на оборот.
 * RPM = (частота_импульсов * 60) / 2
 */
#define TACH_PULSES_PER_REV     2       /**< Импульсов на оборот */
#define TACH_SAMPLE_TIME_MS     1000    /**< Период измерения (мс) */

/* =============================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/**
 * @brief   Состояние системы охлаждения
 */
static fan_state_t g_fan_state = {
    .mode = FAN_MODE_AUTO,
    .target_temp = TEMP_TARGET_DEFAULT,
    .duty_cycle = {FAN_START_DUTY, FAN_START_DUTY},
    .rpm = {0, 0},
    .enabled = false
};

/**
 * @brief   Данные ПИД-регулятора
 */
static struct {
    float integral;         /**< Интегральная сумма */
    float prev_error;       /**< Предыдущая ошибка */
    uint32_t last_update;   /**< Время последнего обновления */
} g_pid = {0};

/**
 * @brief   Счётчики импульсов тахометра
 */
static volatile uint32_t g_tach_count[2] = {0, 0};
static uint32_t g_tach_last_time = 0;

/* =============================================================================
 * ОБРАБОТЧИКИ ПРЕРЫВАНИЙ
 * ============================================================================= */

/**
 * @brief   Обработчик прерывания тахометра вентилятора 0
 * 
 * Вызывается по переднему фронту сигнала тахометра.
 * Просто инкрементирует счётчик импульсов.
 */
void fan0_tach_isr(void) {
    g_tach_count[0]++;
}

/**
 * @brief   Обработчик прерывания тахометра вентилятора 1
 */
void fan1_tach_isr(void) {
    g_tach_count[1]++;
}

/* =============================================================================
 * ВНУТРЕННИЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief   Установка скважности ШИМ вентилятора
 * @param   fan_id: номер вентилятора (0 или 1)
 * @param   duty: скважность в процентах (0-100)
 */
static void set_fan_pwm(int fan_id, uint8_t duty) {
    if (duty > FAN_MAX_DUTY) {
        duty = FAN_MAX_DUTY;
    }
    
    /* Минимальный порог для запуска вентилятора */
    if (duty > 0 && duty < FAN_MIN_DUTY) {
        duty = FAN_MIN_DUTY;
    }
    
    int channel = (fan_id == 0) ? FAN_PWM_CHANNEL_0 : FAN_PWM_CHANNEL_1;
    
    hal_pwm_set_duty(channel, duty);
    
    g_fan_state.duty_cycle[fan_id] = duty;
}

/**
 * @brief   ПИД-регулятор температуры
 * @param   current_temp: текущая температура
 * @return  рассчитанная скважность ШИМ (0-100)
 * 
 * Реализует классический ПИД-алгоритм:
 * output = Kp * error + Ki * integral(error) + Kd * d(error)/dt
 */
static uint8_t pid_calculate(float current_temp) {
    float error;
    float derivative;
    float output;
    uint32_t now = hal_get_tick();
    float dt;
    
    /* Вычисляем временной шаг */
    dt = (now - g_pid.last_update) / 1000.0f;
    if (dt < 0.001f) {
        dt = 0.001f;    /* Защита от деления на ноль */
    }
    g_pid.last_update = now;
    
    /* Вычисляем ошибку (положительная = нужно охлаждать) */
    error = current_temp - g_fan_state.target_temp;
    
    /* Интегральная составляющая с ограничением (anti-windup) */
    g_pid.integral += error * dt;
    if (g_pid.integral > PID_INTEGRAL_MAX) {
        g_pid.integral = PID_INTEGRAL_MAX;
    } else if (g_pid.integral < -PID_INTEGRAL_MAX) {
        g_pid.integral = -PID_INTEGRAL_MAX;
    }
    
    /* Дифференциальная составляющая */
    derivative = (error - g_pid.prev_error) / dt;
    g_pid.prev_error = error;
    
    /* Выход ПИД-регулятора */
    output = PID_KP * error + PID_KI * g_pid.integral + PID_KD * derivative;
    
    /*
     * Преобразуем в скважность:
     * - При целевой температуре (error=0) - базовая скважность 50%
     * - При превышении - увеличиваем
     * - При недостижении - уменьшаем
     */
    output = 50.0f + output;
    
    /* Ограничиваем диапазон */
    if (output < FAN_MIN_DUTY) {
        output = FAN_MIN_DUTY;
    } else if (output > FAN_MAX_DUTY) {
        output = FAN_MAX_DUTY;
    }
    
    return (uint8_t)output;
}

/**
 * @brief   Вычисление RPM по счётчику тахометра
 * @param   fan_id: номер вентилятора
 * @return  скорость вращения в RPM
 */
static uint32_t calculate_rpm(int fan_id) {
    uint32_t count = g_tach_count[fan_id];
    uint32_t now = hal_get_tick();
    uint32_t elapsed = now - g_tach_last_time;
    uint32_t rpm;
    
    if (elapsed < TACH_SAMPLE_TIME_MS) {
        /* Ещё не прошло время измерения */
        return g_fan_state.rpm[fan_id];
    }
    
    /* Сбрасываем счётчик */
    g_tach_count[fan_id] = 0;
    
    /*
     * Вычисляем RPM:
     * RPM = (count / PULSES_PER_REV) * (60000 / elapsed_ms)
     */
    rpm = (count * 60000) / (TACH_PULSES_PER_REV * elapsed);
    
    return rpm;
}

/* =============================================================================
 * ПУБЛИЧНЫЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief   Инициализация системы охлаждения
 * @return  0 при успехе
 * 
 * Настраивает:
 * - ШИМ каналы для управления вентиляторами
 * - GPIO для тахометров с прерываниями
 * - Начальную скорость вращения
 */
int fan_init(void) {
    /*
     * Настройка ШИМ для вентиляторов
     * Частота 25кГц - стандарт для 4-pin вентиляторов
     */
    hal_pwm_init(FAN_PWM_CHANNEL_0, FAN_PWM_FREQUENCY);
    hal_pwm_init(FAN_PWM_CHANNEL_1, FAN_PWM_FREQUENCY);
    
    /*
     * Настройка GPIO для ШИМ выходов
     */
    hal_gpio_set_function(FAN_PWM_PIN_0, GPIO_FUNC_PWM0);
    hal_gpio_set_function(FAN_PWM_PIN_1, GPIO_FUNC_PWM1);
    
    /*
     * Настройка тахометров с прерываниями по переднему фронту
     */
    hal_gpio_set_mode(FAN_TACH_PIN_0, GPIO_MODE_INPUT_PULLUP);
    hal_gpio_set_mode(FAN_TACH_PIN_1, GPIO_MODE_INPUT_PULLUP);
    
    hal_gpio_set_irq(FAN_TACH_PIN_0, GPIO_IRQ_RISING, fan0_tach_isr);
    hal_gpio_set_irq(FAN_TACH_PIN_1, GPIO_IRQ_RISING, fan1_tach_isr);
    
    /*
     * Устанавливаем начальную скорость
     */
    set_fan_pwm(0, FAN_START_DUTY);
    set_fan_pwm(1, FAN_START_DUTY);
    
    /*
     * Инициализация ПИД-регулятора
     */
    g_pid.integral = 0;
    g_pid.prev_error = 0;
    g_pid.last_update = hal_get_tick();
    
    g_tach_last_time = hal_get_tick();
    
    g_fan_state.enabled = true;
    
    return 0;
}

/**
 * @brief   Обновление системы охлаждения
 * @param   temp_board: температура платы (°C)
 * @param   temp_chips: средняя температура чипов (°C)
 * @return  Статус системы охлаждения
 * 
 * Должна вызываться периодически (рекомендуется 1 раз в секунду).
 * Выполняет:
 * - Обновление измерений RPM
 * - Расчёт ПИД или применение ручных настроек
 * - Проверку критических температур
 */
fan_status_t fan_update(float temp_board, float temp_chips) {
    uint32_t now = hal_get_tick();
    float max_temp;
    uint8_t duty;
    
    if (!g_fan_state.enabled) {
        return FAN_STATUS_DISABLED;
    }
    
    /* Берём максимальную температуру для регулирования */
    max_temp = (temp_board > temp_chips) ? temp_board : temp_chips;
    
    /*
     * Обновляем измерения RPM
     */
    if ((now - g_tach_last_time) >= TACH_SAMPLE_TIME_MS) {
        g_fan_state.rpm[0] = calculate_rpm(0);
        g_fan_state.rpm[1] = calculate_rpm(1);
        g_tach_last_time = now;
    }
    
    /*
     * Проверка критических температур
     */
    if (max_temp >= TEMP_SHUTDOWN) {
        /* Аварийная ситуация - максимальное охлаждение */
        set_fan_pwm(0, FAN_MAX_DUTY);
        set_fan_pwm(1, FAN_MAX_DUTY);
        return FAN_STATUS_EMERGENCY;
    }
    
    if (max_temp >= TEMP_CRITICAL) {
        /* Критическая температура - максимальное охлаждение */
        set_fan_pwm(0, FAN_MAX_DUTY);
        set_fan_pwm(1, FAN_MAX_DUTY);
        return FAN_STATUS_CRITICAL;
    }
    
    if (max_temp >= TEMP_WARNING) {
        /* Предупреждение - повышенное охлаждение */
        duty = 90;
        set_fan_pwm(0, duty);
        set_fan_pwm(1, duty);
        return FAN_STATUS_WARNING;
    }
    
    /*
     * Нормальный режим - применяем регулирование
     */
    if (g_fan_state.mode == FAN_MODE_AUTO) {
        /* Автоматический режим - ПИД-регулятор */
        duty = pid_calculate(max_temp);
        set_fan_pwm(0, duty);
        set_fan_pwm(1, duty);
    }
    /* В ручном режиме скважность уже установлена */
    
    /*
     * Проверка работоспособности вентиляторов
     */
    if (g_fan_state.duty_cycle[0] > FAN_MIN_DUTY && g_fan_state.rpm[0] < 100) {
        return FAN_STATUS_FAN0_FAIL;
    }
    if (g_fan_state.duty_cycle[1] > FAN_MIN_DUTY && g_fan_state.rpm[1] < 100) {
        return FAN_STATUS_FAN1_FAIL;
    }
    
    return FAN_STATUS_OK;
}

/**
 * @brief   Установка режима работы вентиляторов
 * @param   mode: режим (FAN_MODE_AUTO или FAN_MODE_MANUAL)
 */
void fan_set_mode(fan_mode_t mode) {
    g_fan_state.mode = mode;
    
    if (mode == FAN_MODE_AUTO) {
        /* Сброс ПИД при переключении в авто */
        g_pid.integral = 0;
        g_pid.prev_error = 0;
    }
}

/**
 * @brief   Установка целевой температуры для автоматического режима
 * @param   temp: целевая температура (°C)
 */
void fan_set_target_temp(uint8_t temp) {
    if (temp < TEMP_MIN) {
        temp = TEMP_MIN;
    } else if (temp > TEMP_WARNING) {
        temp = TEMP_WARNING;
    }
    
    g_fan_state.target_temp = temp;
}

/**
 * @brief   Ручная установка скорости вентиляторов
 * @param   fan_id: номер вентилятора (0, 1 или -1 для всех)
 * @param   percent: скорость в процентах (0-100)
 */
void fan_set_speed(int fan_id, uint8_t percent) {
    if (g_fan_state.mode != FAN_MODE_MANUAL) {
        return; /* Игнорируем в автоматическом режиме */
    }
    
    if (fan_id < 0) {
        /* Установить для всех */
        set_fan_pwm(0, percent);
        set_fan_pwm(1, percent);
    } else if (fan_id < 2) {
        set_fan_pwm(fan_id, percent);
    }
}

/**
 * @brief   Получение текущего состояния системы охлаждения
 * @return  Указатель на структуру состояния
 */
const fan_state_t *fan_get_state(void) {
    return &g_fan_state;
}

/**
 * @brief   Экстренная остановка вентиляторов
 */
void fan_emergency_stop(void) {
    set_fan_pwm(0, 0);
    set_fan_pwm(1, 0);
    g_fan_state.enabled = false;
}

/**
 * @brief   Включение вентиляторов на максимум (для тестирования)
 */
void fan_test_max(void) {
    set_fan_pwm(0, FAN_MAX_DUTY);
    set_fan_pwm(1, FAN_MAX_DUTY);
}

/**
 * @brief   Получение текущих RPM вентиляторов
 * @param   rpm0: указатель для RPM вентилятора 0
 * @param   rpm1: указатель для RPM вентилятора 1
 */
void fan_get_rpm(uint32_t *rpm0, uint32_t *rpm1) {
    if (rpm0) {
        *rpm0 = g_fan_state.rpm[0];
    }
    if (rpm1) {
        *rpm1 = g_fan_state.rpm[1];
    }
}
