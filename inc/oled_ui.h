#ifndef OLED_UI_H
#define OLED_UI_H

// Inicializa o barramento I2C e a tela SSD1306
void oled_init_display(void);

// Força a próxima rotina de atualização a redesenhar a tela
void oled_mark_dirty(void);

// Verifica se é hora de atualizar o display ou se os dados mudaram
void oled_update_if_needed(void);

#endif // OLED_UI_H