/**
 * =============================================================================
 * @file    work.h
 * @brief   Avalon A1126pro - Управление заданиями (заголовочный файл)
 * @version 1.0
 * @date    2024
 * =============================================================================
 */

#ifndef __WORK_H__
#define __WORK_H__

#include "cgminer.h"

work_t *create_work(void);
void free_work(work_t *work);
int work_to_header(work_t *work);

#endif /* __WORK_H__ */
