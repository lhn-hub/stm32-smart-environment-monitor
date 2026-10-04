#ifndef OLED_UI_H
#define OLED_UI_H
/** @brief Draw an ASCII row with optional inverse highlight. */
void ui_row(unsigned row,const char *label,int selected);
/** @brief Draw a bounded progress bar; max<=0 renders an empty bar. */
void ui_progress(int x,int y,int width,int height,int value,int max);
/** @brief Draw status symbols: offline WiFi/MQTT placeholders, arm, alarm, warmup. */
void ui_status(int wifi,int mqtt,int armed,int alarm,int warmup);
#endif
