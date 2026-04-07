#ifndef INPUTS_H
#define INPUTS_H

// Inicializa os pinos de botão e configura as interrupções (IRQ)
void inputs_init(void);

// Agora lê apenas o ADC do Joystick
void read_inputs(void);

#endif // INPUTS_H