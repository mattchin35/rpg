#include "rpg_legacy.h"

#include <errno.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#ifndef DRM_MODE_CONNECTED
#define DRM_MODE_CONNECTED 1
#endif

typedef struct {
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    uint64_t map_offset;
    void *map;
} rpg_drm_buffer;

typedef struct {
    int card_fd;
    char card_path[64];
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t crtc_id;
    struct drm_mode_modeinfo mode;
    struct drm_mode_crtc original_crtc;
    int has_original_crtc;
    rpg_drm_buffer buffers[2];
} rpg_drm_state;

static const char *resolve_drm_card_path(void) {
    const char *env_path = getenv("RPG_DRM_CARD");
    if (env_path != NULL && env_path[0] != '\0') {
        return env_path;
    }
    return "/dev/dri/card0";
}

static rpg_drm_state *get_drm_state(fb_config *fb0) {
    if (fb0 == NULL) {
        return NULL;
    }
    return (rpg_drm_state *)fb0->backend_state;
}

static void set_drm_error_from_errno(const char *message) {
    PyErr_Format(PyExc_OSError, "%s: %s", message, strerror(errno));
}

static int drm_ioctl_checked(int fd, unsigned long request, void *arg, const char *message) {
    if (ioctl(fd, request, arg) == -1) {
        set_drm_error_from_errno(message);
        return -1;
    }
    return 0;
}

static int drm_wait_vblank(rpg_drm_state *state) {
    union drm_wait_vblank vblank;
    memset(&vblank, 0, sizeof(vblank));
    vblank.request.type = _DRM_VBLANK_RELATIVE;
    vblank.request.sequence = 1;
    if (ioctl(state->card_fd, DRM_IOCTL_WAIT_VBLANK, &vblank) == -1) {
        return -1;
    }
    return 0;
}

static int drm_set_crtc(rpg_drm_state *state, uint32_t fb_id) {
    struct drm_mode_crtc crtc;
    uint32_t connector_id = state->connector_id;
    memset(&crtc, 0, sizeof(crtc));
    crtc.crtc_id = state->crtc_id;
    crtc.fb_id = fb_id;
    crtc.set_connectors_ptr = (uintptr_t)&connector_id;
    crtc.count_connectors = 1;
    crtc.mode_valid = 1;
    crtc.mode = state->mode;
    return drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_SETCRTC, &crtc, "DRM set CRTC failed");
}

static int drm_destroy_buffer(rpg_drm_state *state, rpg_drm_buffer *buffer) {
    if (buffer->map != NULL && buffer->size > 0) {
        munmap(buffer->map, buffer->size);
        buffer->map = NULL;
    }
    if (buffer->fb_id != 0) {
        uint32_t fb_id = buffer->fb_id;
        ioctl(state->card_fd, DRM_IOCTL_MODE_RMFB, &fb_id);
        buffer->fb_id = 0;
    }
    if (buffer->handle != 0) {
        struct drm_mode_destroy_dumb destroy_request;
        memset(&destroy_request, 0, sizeof(destroy_request));
        destroy_request.handle = buffer->handle;
        ioctl(state->card_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
        buffer->handle = 0;
    }
    return 0;
}

static int drm_create_buffer(rpg_drm_state *state, uint32_t width, uint32_t height, uint32_t depth, rpg_drm_buffer *buffer) {
    struct drm_mode_create_dumb create_request;
    struct drm_mode_map_dumb map_request;
    struct drm_mode_fb_cmd fb_request;

    memset(&create_request, 0, sizeof(create_request));
    create_request.width = width;
    create_request.height = height;
    create_request.bpp = depth;
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_request, "DRM create dumb buffer failed")) {
        return -1;
    }

    memset(buffer, 0, sizeof(*buffer));
    buffer->handle = create_request.handle;
    buffer->pitch = create_request.pitch;
    buffer->size = create_request.size;

    memset(&fb_request, 0, sizeof(fb_request));
    fb_request.width = width;
    fb_request.height = height;
    fb_request.pitch = create_request.pitch;
    fb_request.bpp = depth;
    fb_request.depth = depth;
    fb_request.handle = create_request.handle;
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_ADDFB, &fb_request, "DRM add framebuffer failed")) {
        drm_destroy_buffer(state, buffer);
        return -1;
    }
    buffer->fb_id = fb_request.fb_id;

    memset(&map_request, 0, sizeof(map_request));
    map_request.handle = create_request.handle;
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_request, "DRM map dumb buffer failed")) {
        drm_destroy_buffer(state, buffer);
        return -1;
    }
    buffer->map_offset = map_request.offset;

    buffer->map = mmap(0, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, state->card_fd, map_request.offset);
    if (buffer->map == MAP_FAILED) {
        buffer->map = NULL;
        set_drm_error_from_errno("DRM dumb buffer mmap failed");
        drm_destroy_buffer(state, buffer);
        return -1;
    }

    memset(buffer->map, 0, buffer->size);
    return 0;
}

static int drm_pick_connector_and_mode(
    rpg_drm_state *state,
    uint32_t requested_width,
    uint32_t requested_height,
    uint32_t *crtc_ids,
    uint32_t crtc_count,
    uint32_t *connector_ids,
    uint32_t connector_count
) {
    uint32_t connector_index;
    for (connector_index = 0; connector_index < connector_count; connector_index++) {
        struct drm_mode_get_connector connector;
        struct drm_mode_get_encoder encoder;
        uint32_t *encoders = NULL;
        struct drm_mode_modeinfo *modes = NULL;
        uint32_t mode_index;

        memset(&connector, 0, sizeof(connector));
        connector.connector_id = connector_ids[connector_index];
        if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector, "DRM get connector metadata failed")) {
            return -1;
        }

        if (connector.count_modes == 0 || connector.count_encoders == 0) {
            continue;
        }

        encoders = calloc(connector.count_encoders, sizeof(uint32_t));
        modes = calloc(connector.count_modes, sizeof(struct drm_mode_modeinfo));
        if (encoders == NULL || modes == NULL) {
            free(encoders);
            free(modes);
            PyErr_NoMemory();
            return -1;
        }

        connector.encoders_ptr = (uintptr_t)encoders;
        connector.modes_ptr = (uintptr_t)modes;
        if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector, "DRM get connector details failed")) {
            free(encoders);
            free(modes);
            return -1;
        }

        if (connector.connection != DRM_MODE_CONNECTED) {
            free(encoders);
            free(modes);
            continue;
        }

        for (mode_index = 0; mode_index < connector.count_modes; mode_index++) {
            if (modes[mode_index].hdisplay != requested_width || modes[mode_index].vdisplay != requested_height) {
                continue;
            }

            memset(&encoder, 0, sizeof(encoder));
            encoder.encoder_id = connector.encoder_id != 0 ? connector.encoder_id : encoders[0];
            if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_GETENCODER, &encoder, "DRM get encoder failed")) {
                free(encoders);
                free(modes);
                return -1;
            }

            state->connector_id = connector.connector_id;
            state->encoder_id = encoder.encoder_id;
            state->crtc_id = encoder.crtc_id != 0 ? encoder.crtc_id : crtc_ids[0];
            state->mode = modes[mode_index];

            if (state->crtc_id == 0 && encoder.possible_crtcs != 0) {
                uint32_t crtc_index;
                for (crtc_index = 0; crtc_index < crtc_count; crtc_index++) {
                    if (encoder.possible_crtcs & (1u << crtc_index)) {
                        state->crtc_id = crtc_ids[crtc_index];
                        break;
                    }
                }
            }

            free(encoders);
            free(modes);
            return 0;
        }

        free(encoders);
        free(modes);
    }

    PyErr_Format(
        PyExc_OSError,
        "No connected DRM connector reported an exact %ux%u mode",
        requested_width,
        requested_height
    );
    return -1;
}

static int drm_load_resources(rpg_drm_state *state, uint32_t requested_width, uint32_t requested_height) {
    struct drm_mode_card_res resources;
    uint32_t *connector_ids = NULL;
    uint32_t *crtc_ids = NULL;
    int status = -1;

    memset(&resources, 0, sizeof(resources));
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_GETRESOURCES, &resources, "DRM get resources failed")) {
        return -1;
    }

    connector_ids = calloc(resources.count_connectors, sizeof(uint32_t));
    crtc_ids = calloc(resources.count_crtcs, sizeof(uint32_t));
    if ((resources.count_connectors > 0 && connector_ids == NULL) || (resources.count_crtcs > 0 && crtc_ids == NULL)) {
        free(connector_ids);
        free(crtc_ids);
        PyErr_NoMemory();
        return -1;
    }

    resources.connector_id_ptr = (uintptr_t)connector_ids;
    resources.crtc_id_ptr = (uintptr_t)crtc_ids;
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_GETRESOURCES, &resources, "DRM get resource lists failed")) {
        goto cleanup;
    }

    if (resources.count_connectors == 0 || resources.count_crtcs == 0) {
        PyErr_SetString(PyExc_OSError, "DRM device reported no connectors or CRTCs");
        goto cleanup;
    }

    status = drm_pick_connector_and_mode(
        state,
        requested_width,
        requested_height,
        crtc_ids,
        resources.count_crtcs,
        connector_ids,
        resources.count_connectors
    );

cleanup:
    free(connector_ids);
    free(crtc_ids);
    return status;
}

static int drm_store_original_crtc(rpg_drm_state *state) {
    memset(&state->original_crtc, 0, sizeof(state->original_crtc));
    state->original_crtc.crtc_id = state->crtc_id;
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_MODE_GETCRTC, &state->original_crtc, "DRM get original CRTC failed")) {
        return -1;
    }
    state->has_original_crtc = 1;
    return 0;
}

static void *drm_back_buffer_map(rpg_drm_state *state, fb_config *fb0) {
    uint32_t next_index = fb0->current_buffer == 0 ? 1 : 0;
    return state->buffers[next_index].map;
}

static uint32_t drm_back_buffer_index(fb_config *fb0) {
    return fb0->current_buffer == 0 ? 1 : 0;
}

static void drm_emit_frame_pulse(void) {
    if (rpg_legacy_gpio_setup() != 0) {
        return;
    }
    rpg_legacy_gpio_pin_mode(FRAMEOUTPIN, RPG_GPIO_OUTPUT);
    rpg_legacy_gpio_digital_write(FRAMEOUTPIN, RPG_GPIO_HIGH);
    usleep(2000);
    rpg_legacy_gpio_digital_write(FRAMEOUTPIN, RPG_GPIO_LOW);
}

int drm_get_refresh_rate(void) {
    const char *path = resolve_drm_card_path();
    int fd = open(path, O_RDWR | O_CLOEXEC);
    rpg_drm_state state;

    if (fd < 0) {
        set_drm_error_from_errno("Could not open DRM device for refresh-rate query");
        return -1;
    }

    memset(&state, 0, sizeof(state));
    state.card_fd = fd;
    if (drm_load_resources(&state, 1280, 720) == 0 || !PyErr_Occurred()) {
        close(fd);
        if (state.mode.vrefresh != 0) {
            return state.mode.vrefresh;
        }
    }
    close(fd);
    if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_OSError, "Unable to determine DRM refresh rate");
    }
    return -1;
}

void drm_flip_buffer(fb_config* fb0) {
    rpg_drm_state *state = get_drm_state(fb0);
    uint32_t next_index;
    if (state == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "DRM backend state is not initialized");
        return;
    }
    next_index = drm_back_buffer_index(fb0);
    if (drm_set_crtc(state, state->buffers[next_index].fb_id) == 0) {
        fb0->current_buffer = next_index;
        fb0->map = state->buffers[next_index].map;
    }
}

int* drm_get_current_offset(fb_config fb0) {
    int *result = malloc(2 * sizeof(int));
    (void)fb0;
    if (result == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    result[0] = 0;
    result[1] = 0;
    return result;
}

float* drm_display_raw(void *frame_data, fb_config* fb0, int trig_pin, int colormode) {
    rpg_drm_state *state = get_drm_state(fb0);
    fileheader_raw* header = frame_data;
    uint24_t * frame_data_24;
    uint16_t * frame_data_16;
    int t, pixel, clock_status, waits, pixel_size;
    float *frame_duration_mean;
    float *frame_duration_std;
    struct timespec frame_start, frame_end;
    long *timings;

    if (state == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "DRM backend state is not initialized");
        return NULL;
    }
    if (trig_pin > 0) {
        PyErr_SetString(PyExc_NotImplementedError, "DRM backend trigger GPIO support has not been implemented yet");
        return NULL;
    }

    frame_data = header + 1;
    frame_data_24 = frame_data;
    frame_data_16 = frame_data;
    pixel_size = (colormode == RGB888MODE) ? (int)sizeof(uint24_t) : (int)sizeof(uint16_t);
    frame_duration_mean = malloc(2 * sizeof(float));
    if (frame_duration_mean == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    frame_duration_std = frame_duration_mean + 1;

    timings = malloc((header->n_frames > 1 ? header->n_frames - 1 : 1) * sizeof(long));
    if (timings == NULL) {
        free(frame_duration_mean);
        PyErr_NoMemory();
        return NULL;
    }

    for (t = 0; t < header->n_frames; t++) {
        void *back_buffer = drm_back_buffer_map(state, fb0);
        frame_end = frame_start;
        frame_start = get_current_time(&clock_status);
        if (clock_status) {
            free(timings);
            free(frame_duration_mean);
            return NULL;
        }

        for (pixel = 0; pixel < (int)(fb0->width * fb0->height); pixel++) {
            if (colormode == RGB888MODE) {
                ((uint24_t *)back_buffer)[pixel] = frame_data_24[(t * fb0->size / pixel_size) + pixel];
            } else {
                ((uint16_t *)back_buffer)[pixel] = frame_data_16[(t * fb0->size / pixel_size) + pixel];
            }
        }

        drm_flip_buffer(fb0);
        if (PyErr_Occurred()) {
            free(timings);
            free(frame_duration_mean);
            return NULL;
        }

        for (waits = 0; waits < header->refresh_per_frame; waits++) {
            drm_wait_vblank(state);
            if (waits == 0) {
                drm_emit_frame_pulse();
            }
        }

        if (t != 0) {
            timings[t - 1] = cmp_times(frame_end, frame_start);
        }
    }

    *frame_duration_mean = mean_long(timings, header->n_frames > 1 ? header->n_frames - 1 : 1);
    *frame_duration_std = std_long(timings, header->n_frames > 1 ? header->n_frames - 1 : 1);
    free(timings);
    return frame_duration_mean;
}

double* drm_display_grating(void* frame_data, fb_config* fb0, int trig_pin, int colormode) {
    rpg_drm_state *state = get_drm_state(fb0);
    fileheader_t* header = frame_data;
    uint24_t * frame_data_24;
    uint16_t * frame_data_16;
    int t, pixel, frame, clock_status, pixel_size;
    double* frame_duration_mean;
    double* frame_duration_std;
    struct timespec frame_start, frame_end;
    long *timings;

    if (state == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "DRM backend state is not initialized");
        return NULL;
    }
    if (trig_pin > 0) {
        PyErr_SetString(PyExc_NotImplementedError, "DRM backend trigger GPIO support has not been implemented yet");
        return NULL;
    }

    frame_data = header + 1;
    frame_data_24 = frame_data;
    frame_data_16 = frame_data;
    pixel_size = (colormode == RGB888MODE) ? (int)sizeof(uint24_t) : (int)sizeof(uint16_t);
    frame_duration_mean = malloc(2 * sizeof(double));
    if (frame_duration_mean == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    frame_duration_std = frame_duration_mean + 1;

    timings = malloc((header->n_frames > 1 ? header->n_frames - 1 : 1) * sizeof(long));
    if (timings == NULL) {
        free(frame_duration_mean);
        PyErr_NoMemory();
        return NULL;
    }

    for (t = 0; t < header->n_frames; t++) {
        void *back_buffer = drm_back_buffer_map(state, fb0);
        frame_end = frame_start;
        frame_start = get_current_time(&clock_status);
        if (clock_status) {
            free(timings);
            free(frame_duration_mean);
            return NULL;
        }

        frame = t % (header->frames_per_cycle);
        for (pixel = 0; pixel < (int)(fb0->width * fb0->height); pixel++) {
            if (colormode == RGB888MODE) {
                ((uint24_t *)back_buffer)[pixel] = frame_data_24[(frame * fb0->size / pixel_size) + pixel];
            } else {
                ((uint16_t *)back_buffer)[pixel] = frame_data_16[(frame * fb0->size / pixel_size) + pixel];
            }
        }

        drm_flip_buffer(fb0);
        if (PyErr_Occurred()) {
            free(timings);
            free(frame_duration_mean);
            return NULL;
        }

        drm_wait_vblank(state);
        drm_emit_frame_pulse();
        if (t != 0) {
            timings[t - 1] = cmp_times(frame_end, frame_start);
        }
    }

    *frame_duration_mean = mean_long(timings, header->n_frames > 1 ? header->n_frames - 1 : 1);
    *frame_duration_std = std_long(timings, header->n_frames > 1 ? header->n_frames - 1 : 1);
    free(timings);
    return frame_duration_mean;
}

int drm_display_color(fb_config* fb0, uint16_t color_16, uint24_t color_24, int colormode, int blocking) {
    rpg_drm_state *state = get_drm_state(fb0);
    void *back_buffer;
    int pixel;

    if (state == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "DRM backend state is not initialized");
        return 1;
    }

    back_buffer = drm_back_buffer_map(state, fb0);
    for (pixel = 0; pixel < (int)(fb0->width * fb0->height); pixel++) {
        if (colormode == RGB888MODE) {
            ((uint24_t *)back_buffer)[pixel] = color_24;
        } else {
            ((uint16_t *)back_buffer)[pixel] = color_16;
        }
    }

    drm_flip_buffer(fb0);
    if (PyErr_Occurred()) {
        return 1;
    }
    if (blocking) {
        drm_wait_vblank(state);
        drm_emit_frame_pulse();
    }
    return 0;
}

int drm_is_current_resolution(int xres, int yres) {
    const char *path = resolve_drm_card_path();
    int fd = open(path, O_RDWR | O_CLOEXEC);
    rpg_drm_state state;
    int matches = 0;

    if (fd < 0) {
        set_drm_error_from_errno("Could not open DRM device for resolution query");
        return -1;
    }

    memset(&state, 0, sizeof(state));
    state.card_fd = fd;
    if (drm_load_resources(&state, (uint32_t)xres, (uint32_t)yres) == 0) {
        matches = 1;
    }
    close(fd);
    if (!matches && !PyErr_Occurred()) {
        PyErr_Format(PyExc_OSError, "No exact DRM mode found for %dx%d", xres, yres);
        return -1;
    }
    return matches;
}

fb_config drm_init(int width, int height, int colormode) {
    fb_config fb0;
    rpg_drm_state *state = calloc(1, sizeof(rpg_drm_state));
    struct drm_get_cap capability;

    memset(&fb0, 0, sizeof(fb0));
    fb0.width = width;
    fb0.height = height;
    fb0.depth = (colormode == RGB888MODE) ? 24 : 16;
    fb0.size = (fb0.height) * (fb0.depth) * (fb0.width) / 8;
    fb0.backend_type = RPG_BACKEND_DRM;
    fb0.backend_state = state;
    fb0.error = 1;

    if (state == NULL) {
        PyErr_NoMemory();
        return fb0;
    }

    snprintf(state->card_path, sizeof(state->card_path), "%s", resolve_drm_card_path());
    state->card_fd = open(state->card_path, O_RDWR | O_CLOEXEC);
    if (state->card_fd < 0) {
        set_drm_error_from_errno("Could not open DRM device");
        return fb0;
    }

    memset(&capability, 0, sizeof(capability));
    capability.capability = DRM_CAP_DUMB_BUFFER;
    if (drm_ioctl_checked(state->card_fd, DRM_IOCTL_GET_CAP, &capability, "DRM dumb-buffer capability query failed")) {
        return fb0;
    }
    if (capability.value == 0) {
        PyErr_SetString(PyExc_OSError, "DRM device does not support dumb buffers");
        return fb0;
    }

    if (drm_load_resources(state, (uint32_t)width, (uint32_t)height)) {
        return fb0;
    }
    if (drm_store_original_crtc(state)) {
        return fb0;
    }
    if (drm_create_buffer(state, (uint32_t)width, (uint32_t)height, fb0.depth, &state->buffers[0])) {
        return fb0;
    }
    if (drm_create_buffer(state, (uint32_t)width, (uint32_t)height, fb0.depth, &state->buffers[1])) {
        drm_destroy_buffer(state, &state->buffers[0]);
        return fb0;
    }
    if (drm_set_crtc(state, state->buffers[0].fb_id)) {
        drm_destroy_buffer(state, &state->buffers[1]);
        drm_destroy_buffer(state, &state->buffers[0]);
        return fb0;
    }

    fb0.framebuffer = state->card_fd;
    fb0.map = state->buffers[0].map;
    fb0.current_buffer = 0;
    fb0.error = 0;
    return fb0;
}

int drm_close_display(fb_config* fb0) {
    rpg_drm_state *state = get_drm_state(fb0);
    if (state == NULL) {
        return 0;
    }

    if (state->has_original_crtc) {
        ioctl(state->card_fd, DRM_IOCTL_MODE_SETCRTC, &state->original_crtc);
    }

    drm_destroy_buffer(state, &state->buffers[0]);
    drm_destroy_buffer(state, &state->buffers[1]);
    if (state->card_fd >= 0) {
        close(state->card_fd);
    }
    free(state);
    fb0->backend_state = NULL;
    return 0;
}
