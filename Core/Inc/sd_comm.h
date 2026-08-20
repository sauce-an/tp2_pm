/*
 * sd_comm.h
 *
 *  Created on: Mar 30, 2026
 *      Author: Lauren Moffatt
 */

#ifndef INC_SD_COMM_H_
#define INC_SD_COMM_H_

#include "main.h"
#include "menu.h"
#include "fatfs.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

extern SPI_HandleTypeDef hspi3;

/*SD card functions */
void SD_start_logging(void);
void SD_stop_logging(void);
void SD_toggle_logging(void);
void SD_log_measurement(void);

#endif /* INC_SD_COMM_H_ */
