/**
 * =============================================================================
 * @file    k210.ld
 * @brief   Скрипт компоновщика для Kendryte K210
 * =============================================================================
 * 
 * Определяет карту памяти и размещение секций для процессора K210.
 * 
 * Карта памяти K210:
 * - SRAM:      0x80000000 - 0x807FFFFF (8 МБ)
 * - AI SRAM:   0x80800000 - 0x80BFFFFF (4 МБ)
 * - Flash XIP: 0x90000000 - 0x9FFFFFFF (до 16 МБ)
 * 
 * Для прошивки майнера используется:
 * - Flash: 8 МБ для кода и данных
 * - SRAM: 8 МБ для динамической памяти
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

/* -----------------------------------------------------------------------------
 * ТОЧКА ВХОДА
 * ----------------------------------------------------------------------------- */

/* Точка входа программы */
ENTRY(_start)

/* -----------------------------------------------------------------------------
 * ОПРЕДЕЛЕНИЕ ПАМЯТИ
 * ----------------------------------------------------------------------------- */

MEMORY
{
    /* 
     * Flash память (XIP - Execute In Place)
     * Начало: 0x90000000
     * Размер: 8 МБ
     * 
     * Разметка:
     * 0x00000 - 0x00FFF : kflash заголовок (4 КБ)
     * 0x01000 - 0x3FFFFF : Слот A (код и данные, ~4 МБ)
     * 0x400000 - 0x7FFFFF : Слот B (резерв для OTA, ~4 МБ)
     */
    FLASH (rx)  : ORIGIN = 0x90001000, LENGTH = 4M - 4K
    
    /*
     * SRAM (оперативная память)
     * Начало: 0x80000000
     * Размер: 8 МБ
     * 
     * Используется для:
     * - Стек
     * - Куча (heap)
     * - Данные
     * - BSS
     */
    RAM (rwx)   : ORIGIN = 0x80000000, LENGTH = 8M
    
    /*
     * AI SRAM (дополнительная память)
     * Можно использовать если не используется AI ускоритель
     */
    AI_RAM (rwx) : ORIGIN = 0x80800000, LENGTH = 4M
}

/* Начальный размер стека */
__stack_size = 64K;

/* Размер кучи FreeRTOS */
__heap_size = 4M;

/* -----------------------------------------------------------------------------
 * РАЗМЕЩЕНИЕ СЕКЦИЙ
 * ----------------------------------------------------------------------------- */

SECTIONS
{
    /* -------------------------------------------------------------------------
     * СЕКЦИИ ВО FLASH
     * ------------------------------------------------------------------------- */
    
    /*
     * Векторы прерываний - должны быть в начале
     */
    .vectors : ALIGN(4)
    {
        __vectors_start = .;
        KEEP(*(.vectors))
        __vectors_end = .;
    } > FLASH
    
    /*
     * Код программы
     */
    .text : ALIGN(4)
    {
        __text_start = .;
        
        *(.text.unlikely .text.unlikely.*)
        *(.text.startup .text.startup.*)
        *(.text .text.*)
        
        /* Конструкторы C++ */
        KEEP(*(.init))
        KEEP(*(.fini))
        
        /* Таблица исключений C++ */
        *(.eh_frame_hdr)
        *(.eh_frame)
        
        __text_end = .;
    } > FLASH
    
    /*
     * Константные данные (только для чтения)
     */
    .rodata : ALIGN(4)
    {
        __rodata_start = .;
        
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
        
        /* Строковые литералы */
        *(.rodata.str1.1)
        *(.rodata.str1.4)
        
        __rodata_end = .;
    } > FLASH
    
    /*
     * Инициализаторы глобальных объектов
     */
    .preinit_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH
    
    .init_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH
    
    .fini_array : ALIGN(4)
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH
    
    /* Сохраняем конец загруженных данных для копирования в RAM */
    __data_load_start = .;
    
    /* -------------------------------------------------------------------------
     * СЕКЦИИ В RAM
     * ------------------------------------------------------------------------- */
    
    /*
     * Инициализированные данные
     * Копируются из Flash в RAM при старте
     */
    .data : ALIGN(8)
    {
        __data_start = .;
        
        *(.data .data.*)
        *(.sdata .sdata.*)
        
        /* Для FreeRTOS */
        *(.data.privileged)
        
        __data_end = .;
    } > RAM AT > FLASH
    
    /*
     * Неинициализированные данные (BSS)
     * Заполняются нулями при старте
     */
    .bss (NOLOAD) : ALIGN(8)
    {
        __bss_start = .;
        
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        
        /* Для FreeRTOS */
        *(.bss.privileged)
        
        __bss_end = .;
    } > RAM
    
    /*
     * Куча (heap) для динамической памяти
     */
    .heap (NOLOAD) : ALIGN(16)
    {
        __heap_start = .;
        . = . + __heap_size;
        __heap_end = .;
    } > RAM
    
    /*
     * Стек
     * Растёт вниз от конца RAM
     */
    .stack (NOLOAD) : ALIGN(16)
    {
        __stack_bottom = .;
        . = . + __stack_size;
        __stack_top = .;
    } > RAM
    
    /* -------------------------------------------------------------------------
     * ДОПОЛНИТЕЛЬНЫЕ СЕКЦИИ
     * ------------------------------------------------------------------------- */
    
    /*
     * Отладочная информация (не загружается в устройство)
     */
    .debug_info     0 : { *(.debug_info) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_ranges   0 : { *(.debug_ranges) }
    
    /* Отбрасываем ненужные секции */
    /DISCARD/ :
    {
        *(.note.*)
        *(.comment)
        *(.ARM.attributes)
        *(.riscv.attributes)
    }
}

/* -----------------------------------------------------------------------------
 * ЭКСПОРТИРУЕМЫЕ СИМВОЛЫ
 * ----------------------------------------------------------------------------- */

/* Размер секций */
__text_size = __text_end - __text_start;
__rodata_size = __rodata_end - __rodata_start;
__data_size = __data_end - __data_start;
__bss_size = __bss_end - __bss_start;

/* Указатель стека для crt0 */
PROVIDE(__stack = __stack_top);

/* Для C runtime */
PROVIDE(end = __heap_start);
PROVIDE(_end = __heap_start);

/* Для FreeRTOS */
PROVIDE(_heap_start = __heap_start);
PROVIDE(_heap_end = __heap_end);
PROVIDE(_stack_size = __stack_size);

/* Размер используемой Flash */
PROVIDE(__flash_used = __data_load_start + __data_size - ORIGIN(FLASH));

/* Размер используемой RAM */
PROVIDE(__ram_used = __stack_top - ORIGIN(RAM));
