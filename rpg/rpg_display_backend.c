#include "rpg_legacy.h"

static rpg_display_backend requested_backend(void) {
    const char *backend = getenv("RPG_DISPLAY_BACKEND");
    if (backend != NULL && strcmp(backend, "legacy") == 0) {
        return RPG_BACKEND_LEGACY_FB;
    }
    if (backend != NULL && strcmp(backend, "drm") == 0) {
        return RPG_BACKEND_DRM;
    }
    return RPG_BACKEND_DRM;
}

int get_refresh_rate(int width, int height) {
    if (requested_backend() == RPG_BACKEND_DRM) {
        return drm_get_refresh_rate(width, height);
    }
    return legacy_get_refresh_rate();
}

void flip_buffer(fb_config* fb0) {
    if (fb0 != NULL && fb0->backend_type == RPG_BACKEND_DRM) {
        drm_flip_buffer(fb0);
        return;
    }
    legacy_flip_buffer(fb0);
}

int* get_current_offset(fb_config fb0) {
    if (fb0.backend_type == RPG_BACKEND_DRM) {
        return drm_get_current_offset(fb0);
    }
    return legacy_get_current_offset(fb0);
}

float* display_raw(void *frame_data, fb_config* fb0, int trig_pin, int colormode) {
    if (fb0 != NULL && fb0->backend_type == RPG_BACKEND_DRM) {
        return drm_display_raw(frame_data, fb0, trig_pin, colormode);
    }
    return legacy_display_raw(frame_data, fb0, trig_pin, colormode);
}

double* display_grating(void* frame_data, fb_config* fb0, int trig_pin, int colormode) {
    if (fb0 != NULL && fb0->backend_type == RPG_BACKEND_DRM) {
        return drm_display_grating(frame_data, fb0, trig_pin, colormode);
    }
    return legacy_display_grating(frame_data, fb0, trig_pin, colormode);
}

int display_color(fb_config* fb0, uint16_t color_16, uint24_t color_24, int colormode, int blocking) {
    if (fb0 != NULL && fb0->backend_type == RPG_BACKEND_DRM) {
        return drm_display_color(fb0, color_16, color_24, colormode, blocking);
    }
    return legacy_display_color(fb0, color_16, color_24, colormode, blocking);
}

int is_current_resolution(int xres, int yres) {
    if (requested_backend() == RPG_BACKEND_DRM) {
        return drm_is_current_resolution(xres, yres);
    }
    return legacy_is_current_resolution(xres, yres);
}

fb_config init(int width, int height, int colormode) {
    if (requested_backend() == RPG_BACKEND_DRM) {
        return drm_init(width, height, colormode);
    }
    return legacy_init(width, height, colormode);
}

int close_display(fb_config* fb0) {
    if (fb0 != NULL && fb0->backend_type == RPG_BACKEND_DRM) {
        return drm_close_display(fb0);
    }
    return legacy_close_display(fb0);
}
