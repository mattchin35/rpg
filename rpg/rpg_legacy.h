#ifndef RPG_LEGACY_H
#define RPG_LEGACY_H

#include <Python.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fb.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "rpg_gpio_legacy.h"

// Sjulsonlab modification: changing this from wiringPi pin 1 to
// to wiringPi pin 11 (GPIO7, DIO1) for Yi's breakout board
// (this will probably change again)
#define FRAMEOUTPIN 11

#define ANGLE_0 -1
#define ANGLE_90 -2
#define ANGLE_180 -3
#define ANGLE_270 -4
#define SINE            0b0001
#define SQUARE          0b0000
#define RGB888MODE      0b0010
#define RGB565MODE      0b0000
#define FULLSCREEN      0b0000
#define CIRCLE          0b0100
#define GABOR           0b1000
#define INSIDEMASK      0b0000
#define OUTSIDEMASK     0b0100

#define DEGREES_SUBTENDED 80

typedef enum {
    RPG_BACKEND_UNSET = 0,
    RPG_BACKEND_LEGACY_FB = 1,
    RPG_BACKEND_DRM = 2,
} rpg_display_backend;

typedef struct {
    int framebuffer;
    void * map;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int header;
    unsigned int size;
    unsigned int orig_width;
    unsigned int orig_height;
    unsigned int orig_depth;
    int error;
    int current_buffer;
    int testing_var;
    int backend_type;
    void *backend_state;
} fb_config;

typedef struct {
    uint16_t frames_per_cycle;
    uint16_t spacial_frequency;
    uint16_t temporal_frequency;
    uint16_t frames_per_second;
    uint16_t n_frames;
    uint16_t width;
    uint16_t height;
    uint16_t _padding;
} fileheader_t;

typedef struct {
    long int width;
    long int height;
    long int refresh_per_frame;
    long int n_frames;
} fileheader_raw;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} uint24_t;

uint16_t rgb_to_uint(int red, int green, int blue);
uint24_t rgb_to_uint_24bit(int red, int green, int blue);
int gcd(int a, int b);
struct timespec get_current_time(int* status);
double degrees_to_radians(double angle);
long cmp_times(struct timespec time1, struct timespec time2);
int int_round(float x);
float mean_long(long a[], int n);
float std_long(long a[], int n);
double gaussian(int radius, int sigma);

int get_refresh_rate(int width, int height);
void flip_buffer(fb_config* fb0);
int* get_current_offset(fb_config fb0);

void* squarewave(int x, int y, int t, int wavelength, int speed, double angle, double cosine, double sine, double weight, double contrast, int background, int colormode);
void* sinewave(int x, int y, int t, int wavelength, int speed, double angle, double cosine, double sine, double weight, double contrast, int background, int colormode);
void* gabor(int x, int y, int t, int wavelength, int speed, double angle, double cosine, double sine, double weight, double contrast, int background, int colormode);
void* circle(int waveform, int radius, int padding, int point_radius, int j, int i, int t, int wavelength, int speed,
             double angle, double cosine, double sine, double weight, double contrast, int background,
             int colormode);
void* build_frame(int t, double angle, fb_config framebuffer, int wavelength, int speed, int waveform,
                  double contrast, int background, int center_j, int center_i, int sigma, int radius, int padding,
                  int colormode);

int build_grating(char * filename, double duration, double angle, double sf, double tf, double contrast, int background, int width, int height, int waveform,
                  double percent_sigma, double percent_diameter, double percent_center_left, double percent_center_top, double percent_padding, int colormode);
void* load_grating(char* filename, fb_config fb0);
int debug_dump_grating(void* frame_data, fb_config fb0, char* filename);
void* load_raw(char* filename);
int convert_raw(char* filename, char* new_filename, int n_frames, int width, int height, int refresh_per_frame, int colormode);
float* display_raw(void *frame_data, fb_config* fb0, int trig_pin, int colormode);
double* display_grating(void* frame_data, fb_config* fb0, int trig_pin, int colormode);
int unload_grating(void* frame_data);
int unload_raw(uint16_t* raw_data);
int display_color(fb_config* fb0, uint16_t color_16, uint24_t color_24, int colormode, int blocking);
int is_current_resolution(int xres, int yres);
fb_config init(int width, int height, int colormode);
int close_display(fb_config* fb0);

int legacy_get_refresh_rate(void);
void legacy_flip_buffer(fb_config* fb0);
int* legacy_get_current_offset(fb_config fb0);
int legacy_kbhit(void);
float* legacy_display_raw(void *frame_data, fb_config* fb0, int trig_pin, int colormode);
double* legacy_display_grating(void* frame_data, fb_config* fb0, int trig_pin, int colormode);
int legacy_display_color(fb_config* fb0, uint16_t color_16, uint24_t color_24, int colormode, int blocking);
int legacy_is_current_resolution(int xres, int yres);
fb_config legacy_init(int width, int height, int colormode);
int legacy_close_display(fb_config* fb0);

int drm_get_refresh_rate(int width, int height);
void drm_flip_buffer(fb_config* fb0);
int* drm_get_current_offset(fb_config fb0);
float* drm_display_raw(void *frame_data, fb_config* fb0, int trig_pin, int colormode);
double* drm_display_grating(void* frame_data, fb_config* fb0, int trig_pin, int colormode);
int drm_display_color(fb_config* fb0, uint16_t color_16, uint24_t color_24, int colormode, int blocking);
int drm_is_current_resolution(int xres, int yres);
fb_config drm_init(int width, int height, int colormode);
int drm_close_display(fb_config* fb0);

#endif
