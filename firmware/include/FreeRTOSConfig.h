/**
 * =============================================================================
 * @file    FreeRTOSConfig.h
 * @brief   Конфигурация FreeRTOS для Kendryte K210
 * =============================================================================
 * 
 * Этот файл определяет параметры работы FreeRTOS:
 * - Частота системного таймера
 * - Размеры стеков
 * - Доступные функции ядра
 * - Настройки планировщика
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * АППАРАТНЫЕ ХАРАКТЕРИСТИКИ K210
 * ============================================================================= */

/* Частота процессора (400 МГц по умолчанию) */
#define configCPU_CLOCK_HZ              400000000UL

/* Частота системного таймера (для SysTick) */
#define configMTIME_CLOCK_HZ            7800000UL

/* =============================================================================
 * ОСНОВНЫЕ ПАРАМЕТРЫ ПЛАНИРОВЩИКА
 * ============================================================================= */

/* Частота прерываний таймера (тики в секунду) */
#define configTICK_RATE_HZ              ((TickType_t)1000)

/* Максимальное количество приоритетов */
#define configMAX_PRIORITIES            7

/* Минимальный размер стека (в словах, не байтах) */
#define configMINIMAL_STACK_SIZE        ((unsigned short)512)

/* Общий размер кучи для динамического выделения памяти */
#define configTOTAL_HEAP_SIZE           ((size_t)(512 * 1024))

/* Максимальная длина имени задачи */
#define configMAX_TASK_NAME_LEN         16

/* =============================================================================
 * ТИПЫ ДАННЫХ
 * ============================================================================= */

/* Использовать 64-битный тип для тиков (RISC-V 64-bit) */
#define configUSE_16_BIT_TICKS          0

/* Порядок приоритетов (0 = низший приоритет) */
#define configIDLE_SHOULD_YIELD         1

/* =============================================================================
 * ФУНКЦИИ ЯДРА
 * ============================================================================= */

/* Поддержка вытесняющей многозадачности */
#define configUSE_PREEMPTION            1

/* Разделение времени между задачами одного приоритета */
#define configUSE_TIME_SLICING          1

/* Оптимизированный выбор задачи (требует CLZ инструкцию) */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION     0

/* Использование тиклесс-режима (для энергосбережения) */
#define configUSE_TICKLESS_IDLE         0

/* =============================================================================
 * МЬЮТЕКСЫ И СЕМАФОРЫ
 * ============================================================================= */

/* Поддержка мьютексов */
#define configUSE_MUTEXES               1

/* Поддержка рекурсивных мьютексов */
#define configUSE_RECURSIVE_MUTEXES     1

/* Поддержка счётных семафоров */
#define configUSE_COUNTING_SEMAPHORES   1

/* =============================================================================
 * ОЧЕРЕДИ
 * ============================================================================= */

/* Поддержка множеств очередей */
#define configUSE_QUEUE_SETS            1

/* =============================================================================
 * ПРОГРАММНЫЕ ТАЙМЕРЫ
 * ============================================================================= */

/* Включить программные таймеры */
#define configUSE_TIMERS                1

/* Приоритет задачи таймеров */
#define configTIMER_TASK_PRIORITY       (configMAX_PRIORITIES - 1)

/* Длина очереди команд таймера */
#define configTIMER_QUEUE_LENGTH        10

/* Размер стека задачи таймеров */
#define configTIMER_TASK_STACK_DEPTH    (configMINIMAL_STACK_SIZE * 2)

/* =============================================================================
 * ФУНКЦИИ ЗАДАЧ
 * ============================================================================= */

/* Уведомления задач */
#define configUSE_TASK_NOTIFICATIONS    1

/* Количество индексов уведомлений на задачу */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3

/* Поддержка генератора задач (thread local storage) */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* =============================================================================
 * СОПРОГРАММЫ
 * ============================================================================= */

/* Сопрограммы отключены (используем задачи) */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES 2

/* =============================================================================
 * ИСПОЛЬЗОВАНИЕ ПАМЯТИ
 * ============================================================================= */

/* Поддержка статического выделения памяти */
#define configSUPPORT_STATIC_ALLOCATION     1

/* Поддержка динамического выделения памяти */
#define configSUPPORT_DYNAMIC_ALLOCATION    1

/* Размер стека задачи Idle */
#define configIDLE_TASK_STACK_DEPTH     configMINIMAL_STACK_SIZE

/* =============================================================================
 * ХУКИ (CALLBACK-ФУНКЦИИ)
 * ============================================================================= */

/* Хук idle-задачи */
#define configUSE_IDLE_HOOK             0

/* Хук тика таймера */
#define configUSE_TICK_HOOK             0

/* Хук сбоя выделения памяти */
#define configUSE_MALLOC_FAILED_HOOK    1

/* Хук переполнения стека */
#define configCHECK_FOR_STACK_OVERFLOW  2

/* Хук демона таймеров */
#define configUSE_DAEMON_TASK_STARTUP_HOOK  0

/* =============================================================================
 * СТАТИСТИКА И ОТЛАДКА
 * ============================================================================= */

/* Сбор статистики выполнения */
#define configGENERATE_RUN_TIME_STATS   1

/* Макросы для измерения времени выполнения */
extern void vConfigureTimerForRunTimeStats(void);
extern unsigned long ulGetRunTimeCounterValue(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()    vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()            ulGetRunTimeCounterValue()

/* Использование трассировки */
#define configUSE_TRACE_FACILITY        1

/* Форматирование статистики */
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* =============================================================================
 * ASSERT И ПРОВЕРКИ
 * ============================================================================= */

/* Определение assert для отладки */
#define configASSERT(x)     if((x) == 0) { \
    taskDISABLE_INTERRUPTS(); \
    for(;;); \
}

/* Запись идентификатора стека в начало стека */
#define configUSE_NEWLIB_REENTRANT      0

/* =============================================================================
 * ВКЛЮЧАЕМЫЕ API-ФУНКЦИИ
 * ============================================================================= */

/* Какие функции включить в сборку */
#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle  1
#define INCLUDE_eTaskGetState           1
#define INCLUDE_xEventGroupSetBitFromISR    1
#define INCLUDE_xTimerPendFunctionCall  1
#define INCLUDE_xTaskAbortDelay         1
#define INCLUDE_xTaskGetHandle          1
#define INCLUDE_xTaskResumeFromISR      1
#define INCLUDE_xQueueGetMutexHolder    1

/* =============================================================================
 * СПЕЦИФИЧНЫЕ ДЛЯ RISC-V НАСТРОЙКИ
 * ============================================================================= */

/* Приоритет прерывания ядра */
#define configKERNEL_INTERRUPT_PRIORITY         1

/* Максимальный приоритет для системных вызовов */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    7

/* Использование CLINT для таймера */
#define configUSE_CLINT_FOR_TIMER       1

/* Базовый адрес CLINT */
#define configCLINT_BASE_ADDRESS        0x02000000UL

/* =============================================================================
 * РАЗМЕРЫ СТЕКОВ ДЛЯ ЗАДАЧ МАЙНЕРА
 * ============================================================================= */

/*
 * Размеры стеков для различных задач системы
 * Все значения в словах (word), не байтах
 */

/* Задача майнера (работа с ASIC) */
#define MINER_TASK_STACK_SIZE           (4 * 1024)

/* Задача сети (lwIP) */
#define NETWORK_TASK_STACK_SIZE         (4 * 1024)

/* Задача HTTP сервера */
#define HTTP_TASK_STACK_SIZE            (4 * 1024)

/* Задача Stratum клиента */
#define STRATUM_TASK_STACK_SIZE         (4 * 1024)

/* Задача мониторинга */
#define MONITOR_TASK_STACK_SIZE         (2 * 1024)

/* =============================================================================
 * ПРИОРИТЕТЫ ЗАДАЧ
 * ============================================================================= */

/*
 * Приоритеты задач системы
 * Чем больше число, тем выше приоритет
 */

/* Наивысший приоритет - критические операции */
#define MINER_TASK_PRIORITY             (configMAX_PRIORITIES - 1)

/* Высокий приоритет - сеть */
#define NETWORK_TASK_PRIORITY           (configMAX_PRIORITIES - 2)

/* Средний приоритет - Stratum */
#define STRATUM_TASK_PRIORITY           (configMAX_PRIORITIES - 3)

/* Низкий приоритет - HTTP и мониторинг */
#define HTTP_TASK_PRIORITY              (configMAX_PRIORITIES - 4)
#define MONITOR_TASK_PRIORITY           (configMAX_PRIORITIES - 5)

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
