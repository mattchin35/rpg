#include "rpg_legacy.h"

int kbhit(void) {
    static const int STDIN = 0;
    static bool is_init = false;

    if(!is_init) {
        struct termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        is_init = true;
    }

    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting;
}

float* display_raw(void *frame_data, fb_config* fb0, int trig_pin, int colormode) {
    pinMode(FRAMEOUTPIN, OUTPUT);
    digitalWrite(FRAMEOUTPIN, LOW);
    if (trig_pin > 0) {
        pinMode(trig_pin, INPUT);
        while (digitalRead(trig_pin) == 0) {
            if (kbhit()) {
                return 0;
            }
        }
    }
    fileheader_raw* header = frame_data;
    frame_data = header + 1;
    uint24_t * frame_data_24 = frame_data;
    uint16_t * frame_data_16 = frame_data;

    uint24_t * write_loc_24 = (uint24_t *)(fb0->map) + fb0->width * fb0->height;
    uint16_t * write_loc_16 = (uint16_t *)(fb0->map) + fb0->width * fb0->height;
    int t, pixel, clock_status, waits, pixel_size;
    if (colormode == RGB888MODE){pixel_size = sizeof(uint24_t);    }
    else                        {pixel_size = sizeof(uint16_t);    }
    float *frame_duration_mean = malloc(2 * sizeof(float));
    float *frame_duration_std = frame_duration_mean + 1;
    struct timespec frame_start, frame_end;
    __u32 dummy = 0;

    int n_frames = header->n_frames;
    int refresh_per_frame = header->refresh_per_frame;
    long timings[n_frames - 1];
    for (t = 0; t < n_frames; t++) {
        frame_end = frame_start;
        frame_start = get_current_time(&clock_status);
        if(clock_status) {
            return NULL;
        }
        if(!fb0->current_buffer){
            write_loc_24 = fb0->map + 3 * fb0->width * fb0->height;
            write_loc_16 = fb0->map + 2 * fb0->width * fb0->height;
        } else {
            write_loc_24 = fb0->map;
            write_loc_16 = fb0->map;
        }
        for(pixel = 0; pixel < fb0->width * fb0->height; pixel++) {
            if(colormode == RGB888MODE){
                *write_loc_24 = frame_data_24[(t * fb0->size / pixel_size) + pixel];
                write_loc_24++;
            }else{
                *write_loc_16 = frame_data_16[(t * fb0->size / pixel_size) + pixel];
                write_loc_16++;
            }
        }
        if(t == 0){
            ioctl(fb0->framebuffer, FBIO_WAITFORVSYNC, &dummy);
        }
        flip_buffer(fb0);
        for (waits = 0; waits < refresh_per_frame; waits++) {
            ioctl(fb0->framebuffer, FBIO_WAITFORVSYNC, &dummy);
            if (waits == 0) {
                digitalWrite(FRAMEOUTPIN, HIGH);
                usleep(2000);
                digitalWrite(FRAMEOUTPIN, LOW);
            }
        }
        if (t != 0) {
            timings[t - 1] = cmp_times(frame_end, frame_start);
        }
    }
    *frame_duration_mean = mean_long(timings, n_frames - 1);
    *frame_duration_std = std_long(timings, n_frames - 1);
    return frame_duration_mean;
}

double* display_grating(void* frame_data, fb_config* fb0, int trig_pin, int colormode){
    pinMode(FRAMEOUTPIN, OUTPUT);
    digitalWrite(FRAMEOUTPIN, LOW);
    if (trig_pin > 0) {
        pinMode(trig_pin, INPUT);
        while (digitalRead(trig_pin) == 0) {
            if (kbhit()) {
                return NULL;
            }
        }
    }

    fileheader_t* header = frame_data;
    frame_data = (header + 1);
    uint24_t * frame_data_24 = frame_data;
    uint16_t * frame_data_16 = frame_data;

    uint24_t * write_loc_24 = (uint24_t *)(fb0->map) + fb0->width * fb0->height;
    uint16_t * write_loc_16 = (uint16_t *)(fb0->map) + fb0->width * fb0->height;

    int t, pixel, frame, clock_status, pixel_size;
    if (colormode == RGB888MODE){pixel_size = sizeof(uint24_t);    }
    else                        {pixel_size = sizeof(uint16_t);    }

    double* frame_duration_mean = malloc(2 * sizeof(double));
    double* frame_duration_std = frame_duration_mean + 1;
    struct timespec frame_start, frame_end;
    __u32 dummy = 0;

    int n_frames = header->n_frames;
    long timings[n_frames - 1];
    for (t = 0; t < n_frames; t++){
        frame_end = frame_start;
        frame_start = get_current_time(&clock_status);
        if(clock_status) {
            return NULL;
        }

        frame = t % (header->frames_per_cycle);
        if(!fb0->current_buffer){
            write_loc_24 = fb0->map + 3 * fb0->width * fb0->height;
            write_loc_16 = fb0->map + 2 * fb0->width * fb0->height;
        } else {
            write_loc_24 = fb0->map;
            write_loc_16 = fb0->map;
        }
        for(pixel = 0; pixel < fb0->width * fb0->height; pixel++){
            if(colormode == RGB888MODE){
                *write_loc_24 = frame_data_24[(frame * fb0->size / pixel_size) + pixel];
                write_loc_24++;
            }else{
                *write_loc_16 = frame_data_16[(frame * fb0->size / pixel_size) + pixel];
                write_loc_16++;
            }
        }
        if(t == 0){
            ioctl(fb0->framebuffer, FBIO_WAITFORVSYNC, &dummy);
        }
        flip_buffer(fb0);
        ioctl(fb0->framebuffer, FBIO_WAITFORVSYNC, &dummy);
        digitalWrite(FRAMEOUTPIN, HIGH);
        usleep(2000);
        digitalWrite(FRAMEOUTPIN, LOW);
        if (t != 0) {
            timings[t - 1] = cmp_times(frame_end, frame_start);
        }
    }
    *frame_duration_mean = mean_long(timings, n_frames - 1);
    *frame_duration_std = std_long(timings, n_frames - 1);
    return frame_duration_mean;
}

int display_color(fb_config* fb0, uint16_t color_16, uint24_t color_24, int colormode, int blocking){
    __u32 dummy = 0;
    uint16_t *write_loc_16;
    uint24_t *write_loc_24;
    int pixel;
    if(!fb0->current_buffer){
        write_loc_24 = fb0->map + 3 * fb0->width * fb0->height;
        write_loc_16 = fb0->map + 2 * fb0->width * fb0->height;
    } else {
        write_loc_24 = fb0->map;
        write_loc_16 = fb0->map;
    }

    for(pixel = 0; pixel < fb0->width * fb0->height; pixel++){
        if(colormode == RGB888MODE){
            *write_loc_24 = color_24;
            write_loc_24++;
        }else{
            *write_loc_16 = color_16;
            write_loc_16++;
        }
    }

    flip_buffer(fb0);
    if(blocking){
        ioctl(fb0->framebuffer, FBIO_WAITFORVSYNC, &dummy);
        digitalWrite(FRAMEOUTPIN, HIGH);
        usleep(2000);
        digitalWrite(FRAMEOUTPIN, LOW);
    }
    return 0;
}

int is_current_resolution(int xres, int yres){
    int fd = open("/dev/vcio", 0);
    if(fd == -1){
        PyErr_SetString(PyExc_OSError, "Could not open /dev/vcio device");
        return -1;
    }
    volatile uint32_t property[32] __attribute__((aligned(16))) =
    {
    0x00000000,
    0x00000000,
    0x00040003,
    0x00000008,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
    };
    property[0] = 8 * sizeof(property[0]);
    if(ioctl(fd, _IOWR(100, 0, char*), property) == -1){
        PyErr_SetString(PyExc_OSError, "IOCTL call failed when attempting to check resolution");
        return -1;
    }
    return ((property[5] == xres) && (property[6] == yres));
}

fb_config init(int width, int height, int colormode){
    wiringPiSetup();

    fb_config fb0;
    fb0.current_buffer = 0;
    fb0.testing_var = 0;
    int fd = open("/dev/vcio", 0);
    if(fd == -1){
        perror("From open() call on /dev/vcio device");
        exit(1);
    }
    volatile uint32_t property[32] __attribute__((aligned(16))) =
    {
    0x00000000,
    0x00000000,
    0x00040003,
    0x00000008,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00040005,
    0x00000004,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000
    };
    property[0] = 12 * sizeof(property[0]);
    if(ioctl(fd, _IOWR(100, 0, char *), property) == -1){
        PyErr_SetString(PyExc_OSError, "Error from call to ioctl\n");
        fb0.error = 1;
        return fb0;
    }
    close(fd);
    fb0.orig_width = (int)(property[5]);
    fb0.orig_height = (int)(property[6]);
    fb0.orig_depth = (int)(property[10]);
    fb0.width = width;
    fb0.height = height;
    if(colormode == RGB888MODE){
        fb0.depth = 24;
    }else{
        fb0.depth = 16;
    }
    fb0.size = (fb0.height) * (fb0.depth) * (fb0.width) / 8;
    char fbset_str[80];
    sprintf(fbset_str,
        "fbset -xres %d -yres %d -vxres %d -vyres %d -depth %d",
        fb0.width, fb0.height, fb0.width, 2 * fb0.height, fb0.depth);
    if(system(fbset_str)){
        PyErr_SetString(PyExc_OSError, "Call to fbset subroutine failed.");
        fb0.error = 1;
        return fb0;
    }
    int resolution_status = is_current_resolution(width, height);
    if(resolution_status == 0){
        printf("The linux framebuffer does not support the requested resolution\n"
               "Attepting to reset resolution settings...\n");
        sprintf(fbset_str,
            "fbset -xres %d -yres %d -vxres %d -vyres %d -depth %d",
            fb0.orig_width, fb0.orig_height, fb0.orig_width,
            fb0.orig_height, fb0.orig_depth);
        if(system(fbset_str)){
            perror("Attempt failed, message from fbset");
        }
        else{
            printf("Attempt successful.\n");
        }
        PyErr_SetString(PyExc_OSError, "Requested resolution not supported");
        fb0.error = 1;
        return fb0;
    }else if(resolution_status == -1){
        fb0.error = 1;
        return fb0;
    }
    fb0.framebuffer = open("/dev/fb0", O_RDWR);
    if (fb0.framebuffer == -1){
        PyErr_SetString(PyExc_OSError, "Attempt to open /dev/fb0 (framebuffer 0) device failed");
        fb0.error = 1;
        return fb0;
    }
    fb0.map = mmap(0, 2 * fb0.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0.framebuffer, 0);
    if (fb0.map == MAP_FAILED){
        PyErr_SetString(PyExc_OSError, "Attempt to mmap /dev/fb0 device failed");
        fb0.error = 1;
        return fb0;
    }
    fb0.error = 0;
    return fb0;
}

int close_display(fb_config* fb0){
    if(fb0->current_buffer == 1){
        flip_buffer(fb0);
    }
    munmap(fb0->map, 2 * fb0->size);
    char fbset_str[80];
    sprintf(fbset_str,
        "fbset -xres %d -yres %d -vxres %d -vyres %d -depth %d",
        fb0->orig_width, fb0->orig_height, fb0->orig_width, fb0->orig_height,
        fb0->orig_depth);
    if(system(fbset_str)){
        PyErr_SetString(PyExc_OSError, "System call to reset resolution (via fbset subroutine) failed");
        return 1;
    }
    return 0;
}
