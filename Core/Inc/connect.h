/*
 * connect.h
 *
 *  Created on: Mar 30, 2026
 *      Author: Andrew Jian
 */

#ifndef INC_CONNECT_H_
#define INC_CONNECT_H_

#include <stdint.h>

// Three possible connection states
typedef enum {
    CONN_NONE = 0,
    CONN_USB,
    CONN_WIFI
} ConnectionMode_t;

// Public functions
void Connect_Init(void);
//void Connect_Update(void);                   // will be placed in while(1) loop to check for timeouts
void Connect_SetMode(ConnectionMode_t mode); // Call this when receive UART/WiFi data
ConnectionMode_t Connect_GetMode(void);
void Safe_UART_Restore(void);
// will be used my menu to see which icon to draw

#endif /* INC_CONNECT_H_ */
