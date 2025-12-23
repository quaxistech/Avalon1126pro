/**
 * =============================================================================
 * @file    work.c
 * @brief   Avalon A1126pro - Управление заданиями (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "work.h"
#include "cgminer.h"

static const char *TAG = "Work";

work_t *create_work(void)
{
    work_t *work = calloc(1, sizeof(work_t));
    if (work) {
        work->timestamp = time(NULL);
    }
    return work;
}

void free_work(work_t *work)
{
    if (work) {
        free(work);
    }
}

int work_to_header(work_t *work)
{
    if (!work) return -1;
    /* TODO: Формирование заголовка блока */
    return 0;
}
