#ifndef SSD1306_H
#define SSD1306_H
#include <stdint.h>
typedef struct {
    uint32_t failures, recoveries, frames, last_us, max_us;
    uint32_t last_hal_status, last_hal_error, last_failed_us;
    uint8_t online;
} oled_stats_t;
/** @brief Init/retry display in MenuTask. No framebuffer allocation. */
int oled_init(void);
/** @brief Flush changed pages, or force all pages; bounded I2C transfers. */
int oled_flush(int full);
/** @brief Clear local pixels; does not transmit a visible blank frame. */
void oled_clear(void);
/** @brief Clipped drawing; color 0=clear,1=set,2=invert. */
void oled_pixel(int x,int y,unsigned color);
void oled_line(int x0,int y0,int x1,int y1,unsigned color);
void oled_rect(int x,int y,int w,int h,unsigned color,int filled);
/** @brief ASCII font selector: 0=6x8,1=8x16. */
void oled_text(int x,int y,const char *s,unsigned large);
/** @brief Draw one of eight Chinese glyphs: 报警布防撤故障 (indices documented in font). */
void oled_hanzi(int x,int y,unsigned index);
const oled_stats_t *oled_stats(void);
#endif
