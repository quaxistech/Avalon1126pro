/**
 * =============================================================================
 * @file    startup.S
 * @brief   Код запуска для Kendryte K210 (RISC-V)
 * =============================================================================
 * 
 * Этот файл содержит начальный код, выполняемый при включении/сбросе:
 * - Инициализация регистров
 * - Настройка стека
 * - Инициализация секций .data и .bss
 * - Вызов main()
 * 
 * K210 имеет два ядра RISC-V RV64IMAFDC.
 * При запуске работает только ядро 0, ядро 1 находится в состоянии ожидания.
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

/* Размер стека по умолчанию */
#ifndef STACK_SIZE
#define STACK_SIZE  0x10000  /* 64 КБ */
#endif

.section .vectors, "ax"
.global _start
.type _start, @function

/* =============================================================================
 * ТАБЛИЦА ВЕКТОРОВ ПРЕРЫВАНИЙ
 * ============================================================================= */

/*
 * В RISC-V вектора прерываний расположены по адресу mtvec.
 * Используем векторизованный режим (mtvec.MODE = 1).
 * Каждый вектор занимает 4 байта (одна инструкция j).
 */

_start:
.org 0x0
    j   reset_handler           /* Вектор 0: Сброс / исключения */
    
.org 0x4
    j   default_handler         /* Вектор 1: Зарезервировано */
    
.org 0x8
    j   default_handler         /* Вектор 2: Зарезервировано */
    
.org 0xC
    j   machine_soft_handler    /* Вектор 3: Machine software interrupt */
    
.org 0x10
    j   default_handler         /* Вектор 4-6: Зарезервировано */
.org 0x14
    j   default_handler
.org 0x18
    j   default_handler

.org 0x1C
    j   machine_timer_handler   /* Вектор 7: Machine timer interrupt */
    
.org 0x20
    j   default_handler         /* Вектор 8-10: Зарезервировано */
.org 0x24
    j   default_handler
.org 0x28
    j   default_handler

.org 0x2C
    j   machine_ext_handler     /* Вектор 11: Machine external interrupt */

/* =============================================================================
 * ОБРАБОТЧИК СБРОСА
 * ============================================================================= */

.section .text.reset_handler, "ax"
.global reset_handler
.type reset_handler, @function

reset_handler:
    /*
     * Отключаем прерывания
     */
    csrci   mstatus, 0x08       /* Очищаем MIE (Machine Interrupt Enable) */
    
    /*
     * Проверяем номер ядра (hart ID)
     * Ядро 0 продолжает выполнение, остальные ждут
     */
    csrr    t0, mhartid
    bnez    t0, secondary_hart  /* Если не ядро 0, переходим в ожидание */
    
    /*
     * ИНИЦИАЛИЗАЦИЯ ЯДРА 0 (ГЛАВНОЕ ЯДРО)
     */
    
    /*
     * Инициализация глобального указателя (gp)
     * __global_pointer$ - середина секции .sdata для оптимизации доступа
     */
.option push
.option norelax
    la      gp, __global_pointer$
.option pop
    
    /*
     * Инициализация указателя стека (sp)
     */
    la      sp, __stack_top
    
    /*
     * Инициализация указателя кадра (fp)
     */
    mv      fp, sp
    
    /*
     * Очистка секции .bss (неинициализированные данные)
     */
    la      t0, __bss_start
    la      t1, __bss_end
    bgeu    t0, t1, bss_done
bss_loop:
    sd      zero, 0(t0)         /* Записываем 0 (8 байт для 64-бит) */
    addi    t0, t0, 8
    bltu    t0, t1, bss_loop
bss_done:

    /*
     * Копирование секции .data из Flash в RAM
     */
    la      t0, __data_load_start   /* Источник (Flash) */
    la      t1, __data_start        /* Назначение (RAM) */
    la      t2, __data_end
    bgeu    t1, t2, data_done
data_loop:
    ld      t3, 0(t0)
    sd      t3, 0(t1)
    addi    t0, t0, 8
    addi    t1, t1, 8
    bltu    t1, t2, data_loop
data_done:

    /*
     * Настройка вектора прерываний (mtvec)
     * Используем векторизованный режим
     */
    la      t0, _start
    ori     t0, t0, 0x01        /* MODE = 1 (Vectored) */
    csrw    mtvec, t0
    
    /*
     * Включаем FPU (Floating Point Unit)
     * Устанавливаем mstatus.FS = 01 (Initial)
     */
    li      t0, 0x00002000      /* FS = Initial */
    csrs    mstatus, t0
    
    /*
     * Вызов функций инициализации C++ (конструкторы глобальных объектов)
     */
    call    __libc_init_array
    
    /*
     * Вызов main()
     * В argc передаём 0, в argv - NULL
     */
    li      a0, 0               /* argc = 0 */
    li      a1, 0               /* argv = NULL */
    li      a2, 0               /* envp = NULL */
    
    call    main
    
    /*
     * Если main() вернулся, вызываем exit()
     */
    mv      a0, zero
    call    exit
    
    /*
     * Бесконечный цикл (не должны сюда попасть)
     */
hang:
    j       hang

/* =============================================================================
 * ВТОРИЧНОЕ ЯДРО (HART 1)
 * ============================================================================= */

.section .text.secondary_hart, "ax"
.type secondary_hart, @function

secondary_hart:
    /*
     * Вторичное ядро ожидает сигнала от основного
     * В этой реализации оно просто зависает
     * TODO: Реализовать межъядерную синхронизацию
     */
    wfi                         /* Wait For Interrupt */
    j       secondary_hart

/* =============================================================================
 * ОБРАБОТЧИКИ ПРЕРЫВАНИЙ ПО УМОЛЧАНИЮ
 * ============================================================================= */

.section .text.handlers, "ax"

/*
 * Обработчик по умолчанию - бесконечный цикл
 * Все неопределённые прерывания попадают сюда
 */
.global default_handler
.weak default_handler
.type default_handler, @function
default_handler:
    j       default_handler

/*
 * Обработчик программного прерывания
 */
.global machine_soft_handler
.weak machine_soft_handler
.type machine_soft_handler, @function
machine_soft_handler:
    /* Сохраняем контекст */
    addi    sp, sp, -128
    sd      ra, 0(sp)
    sd      t0, 8(sp)
    sd      t1, 16(sp)
    sd      t2, 24(sp)
    sd      a0, 32(sp)
    sd      a1, 40(sp)
    sd      a2, 48(sp)
    sd      a3, 56(sp)
    sd      a4, 64(sp)
    sd      a5, 72(sp)
    sd      a6, 80(sp)
    sd      a7, 88(sp)
    sd      t3, 96(sp)
    sd      t4, 104(sp)
    sd      t5, 112(sp)
    sd      t6, 120(sp)
    
    /* Вызываем обработчик на C */
    call    handle_soft_irq
    
    /* Восстанавливаем контекст */
    ld      ra, 0(sp)
    ld      t0, 8(sp)
    ld      t1, 16(sp)
    ld      t2, 24(sp)
    ld      a0, 32(sp)
    ld      a1, 40(sp)
    ld      a2, 48(sp)
    ld      a3, 56(sp)
    ld      a4, 64(sp)
    ld      a5, 72(sp)
    ld      a6, 80(sp)
    ld      a7, 88(sp)
    ld      t3, 96(sp)
    ld      t4, 104(sp)
    ld      t5, 112(sp)
    ld      t6, 120(sp)
    addi    sp, sp, 128
    
    mret

/*
 * Обработчик таймера
 */
.global machine_timer_handler
.weak machine_timer_handler
.type machine_timer_handler, @function
machine_timer_handler:
    /* Сохраняем контекст */
    addi    sp, sp, -128
    sd      ra, 0(sp)
    sd      t0, 8(sp)
    sd      t1, 16(sp)
    sd      t2, 24(sp)
    sd      a0, 32(sp)
    sd      a1, 40(sp)
    sd      a2, 48(sp)
    sd      a3, 56(sp)
    sd      a4, 64(sp)
    sd      a5, 72(sp)
    sd      a6, 80(sp)
    sd      a7, 88(sp)
    sd      t3, 96(sp)
    sd      t4, 104(sp)
    sd      t5, 112(sp)
    sd      t6, 120(sp)
    
    /* Вызываем обработчик на C (для FreeRTOS это SysTick) */
    call    handle_timer_irq
    
    /* Восстанавливаем контекст */
    ld      ra, 0(sp)
    ld      t0, 8(sp)
    ld      t1, 16(sp)
    ld      t2, 24(sp)
    ld      a0, 32(sp)
    ld      a1, 40(sp)
    ld      a2, 48(sp)
    ld      a3, 56(sp)
    ld      a4, 64(sp)
    ld      a5, 72(sp)
    ld      a6, 80(sp)
    ld      a7, 88(sp)
    ld      t3, 96(sp)
    ld      t4, 104(sp)
    ld      t5, 112(sp)
    ld      t6, 120(sp)
    addi    sp, sp, 128
    
    mret

/*
 * Обработчик внешних прерываний (PLIC)
 */
.global machine_ext_handler
.weak machine_ext_handler
.type machine_ext_handler, @function
machine_ext_handler:
    /* Сохраняем контекст */
    addi    sp, sp, -128
    sd      ra, 0(sp)
    sd      t0, 8(sp)
    sd      t1, 16(sp)
    sd      t2, 24(sp)
    sd      a0, 32(sp)
    sd      a1, 40(sp)
    sd      a2, 48(sp)
    sd      a3, 56(sp)
    sd      a4, 64(sp)
    sd      a5, 72(sp)
    sd      a6, 80(sp)
    sd      a7, 88(sp)
    sd      t3, 96(sp)
    sd      t4, 104(sp)
    sd      t5, 112(sp)
    sd      t6, 120(sp)
    
    /* Вызываем обработчик на C */
    call    handle_external_irq
    
    /* Восстанавливаем контекст */
    ld      ra, 0(sp)
    ld      t0, 8(sp)
    ld      t1, 16(sp)
    ld      t2, 24(sp)
    ld      a0, 32(sp)
    ld      a1, 40(sp)
    ld      a2, 48(sp)
    ld      a3, 56(sp)
    ld      a4, 64(sp)
    ld      a5, 72(sp)
    ld      a6, 80(sp)
    ld      a7, 88(sp)
    ld      t3, 96(sp)
    ld      t4, 104(sp)
    ld      t5, 112(sp)
    ld      t6, 120(sp)
    addi    sp, sp, 128
    
    mret

/* =============================================================================
 * СЛАБЫЕ СИМВОЛЫ ДЛЯ ОБРАБОТЧИКОВ НА C
 * ============================================================================= */

.weak handle_soft_irq
.weak handle_timer_irq
.weak handle_external_irq

/* Заглушки по умолчанию */
handle_soft_irq:
handle_timer_irq:
handle_external_irq:
    ret

/* =============================================================================
 * ФУНКЦИЯ exit()
 * ============================================================================= */

.section .text.exit, "ax"
.global exit
.type exit, @function

exit:
    /* Бесконечный цикл при выходе */
    wfi
    j       exit

/* =============================================================================
 * ФУНКЦИИ LIBC
 * ============================================================================= */

/*
 * Слабые символы для функций libc
 * Если libc не подключена, используются эти заглушки
 */

.weak __libc_init_array
__libc_init_array:
    ret

.weak _exit
_exit:
    j       exit

/* =============================================================================
 * ГЛОБАЛЬНЫЕ СИМВОЛЫ
 * ============================================================================= */

/*
 * Глобальный указатель для релоксации
 * Должен быть определён в скрипте компоновщика
 */
.global __global_pointer$
