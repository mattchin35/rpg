#include "rpg_gpio_legacy.h"

#include <dlfcn.h>
#include <stdio.h>

static void *wiringpi_handle = NULL;
static int (*wiringpi_setup_fn)(void) = NULL;
static void (*pin_mode_fn)(int, int) = NULL;
static void (*digital_write_fn)(int, int) = NULL;
static int (*digital_read_fn)(int) = NULL;
static int gpio_loader_attempted = 0;
static int gpio_available = 0;

static void rpg_legacy_gpio_try_load(void) {
    if (gpio_loader_attempted) {
        return;
    }
    gpio_loader_attempted = 1;

    const char *candidates[] = {
        "libwiringPi.so",
        "libwiringPi.so.3",
        NULL,
    };

    for (int i = 0; candidates[i] != NULL; ++i) {
        wiringpi_handle = dlopen(candidates[i], RTLD_LAZY | RTLD_LOCAL);
        if (wiringpi_handle != NULL) {
            break;
        }
    }

    if (wiringpi_handle == NULL) {
        return;
    }

    wiringpi_setup_fn = (int (*)(void))dlsym(wiringpi_handle, "wiringPiSetup");
    pin_mode_fn = (void (*)(int, int))dlsym(wiringpi_handle, "pinMode");
    digital_write_fn = (void (*)(int, int))dlsym(wiringpi_handle, "digitalWrite");
    digital_read_fn = (int (*)(int))dlsym(wiringpi_handle, "digitalRead");

    gpio_available = wiringpi_setup_fn != NULL &&
                     pin_mode_fn != NULL &&
                     digital_write_fn != NULL &&
                     digital_read_fn != NULL;

    if (!gpio_available) {
        dlclose(wiringpi_handle);
        wiringpi_handle = NULL;
        wiringpi_setup_fn = NULL;
        pin_mode_fn = NULL;
        digital_write_fn = NULL;
        digital_read_fn = NULL;
    }
}

int rpg_legacy_gpio_setup(void) {
    rpg_legacy_gpio_try_load();
    if (!gpio_available) {
        return -1;
    }
    return wiringpi_setup_fn();
}

void rpg_legacy_gpio_pin_mode(int pin, int mode) {
    rpg_legacy_gpio_try_load();
    if (!gpio_available) {
        return;
    }
    pin_mode_fn(pin, mode);
}

void rpg_legacy_gpio_digital_write(int pin, int value) {
    rpg_legacy_gpio_try_load();
    if (!gpio_available) {
        return;
    }
    digital_write_fn(pin, value);
}

int rpg_legacy_gpio_digital_read(int pin) {
    rpg_legacy_gpio_try_load();
    if (!gpio_available) {
        return 0;
    }
    return digital_read_fn(pin);
}
