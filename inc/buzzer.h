#ifndef BUZZER_H // Verifica se BUZZER_H não foi definido
#define BUZZER_H // Define BUZZER_H para evitar inclusões múltiplas

#include <stdbool.h> // Inclui a biblioteca para o tipo booleano

// Função para inicializar o buzzer
void buzzer_init(void);

// Função para atualizar o estado do buzzer
void buzzer_update(void);

// Função para definir o estado de mudo do buzzer
void buzzer_set_muted(bool muted); // Recebe um valor booleano para definir se o buzzer está mudo

#endif // Fim da verificação de inclusão
