#ifndef APP_MENU_H
#define APP_MENU_H
#include "bsp_services.h"
/** @brief Initialize MenuTask-owned UI after startup scan completes. */
void menu_init(void);
/** @brief Consume confirm/back events, including long press acceleration. */
void menu_key(const key_event_t *event);
/** @brief Run encoder, render/refresh deadline, diagnostics and OLED retry. */
void menu_poll(void);
#endif
