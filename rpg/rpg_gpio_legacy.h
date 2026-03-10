#ifndef RPG_GPIO_LEGACY_H
#define RPG_GPIO_LEGACY_H

#define RPG_GPIO_INPUT 0
#define RPG_GPIO_OUTPUT 1
#define RPG_GPIO_LOW 0
#define RPG_GPIO_HIGH 1

int rpg_legacy_gpio_setup(void);
void rpg_legacy_gpio_pin_mode(int pin, int mode);
void rpg_legacy_gpio_digital_write(int pin, int value);
int rpg_legacy_gpio_digital_read(int pin);

#endif
