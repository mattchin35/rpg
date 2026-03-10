#include "rpg_legacy.h"

uint16_t rgb_to_uint(int red, int green, int blue){
    return (((31 * (red + 4)) / 255) << 11) |
           (((63 * (green + 2)) / 255) << 5) |
           ((31 * (blue + 4)) / 255);
}

uint24_t rgb_to_uint_24bit(int red, int green, int blue){
    uint24_t result;
    result.red = red;
    result.green = green;
    result.blue = blue;
    return result;
}

int gcd(int a, int b){
    if(a == 0 || b == 0){
        return 0;
    }
    if(a == b){
        return a;
    }
    if(a > b){
        return gcd(a - b, b);
    }
    return gcd(a, b - a);
}

struct timespec get_current_time(int* status){
    struct timespec t;
    if(clock_gettime(CLOCK_REALTIME, &t)){
        *status = -1;
        PyErr_SetString(PyExc_OSError, "Failed realtime clock_gettime call");
    }else{
        *status = 0;
    }
    return t;
}

double degrees_to_radians(double angle){
    angle = ((int)(angle) % 360 + 360) % 360;
    switch((int)(angle)){
        case(0):
            angle = ANGLE_0;
            break;
        case(90):
            angle = ANGLE_90;
            break;
        case(180):
            angle = ANGLE_180;
            break;
        case(270):
            angle = ANGLE_270;
            break;
        default:
            angle = (180 - angle) * M_PI / 180;
    }
    return angle;
}

long cmp_times(struct timespec time1, struct timespec time2){
    long long total_nsec1 = time1.tv_nsec + 1000000000 * (long long)(time1.tv_sec);
    long long total_nsec2 = time2.tv_nsec + 1000000000 * (long long)(time2.tv_sec);
    if(total_nsec1 > total_nsec2) {
        printf("Compare time error: time 2 occoured before from 1");
        exit(1);
    }
    return (total_nsec2 - total_nsec1) / 1000;
}

int int_round(float x) {
    if (x < 0.0) {
        return (int)(x - 0.5);
    } else {
        return (int)(x + 0.5);
    }
}

float mean_long(long a[], int n) {
    int i;
    long sum = 0;
    for (i = 0; i < n; i++) {
        sum += a[i];
    }
    return ((float) sum) / n;
}

float std_long(long a[], int n) {
    float mean = mean_long(a, n);
    float error_sum = 0;
    float error;
    int i;
    for (i = 0; i < n; i++) {
        error = mean - a[i];
        error_sum += error * error;
    }
    return (float) sqrt(error_sum / n);
}

int get_refresh_rate(void) {
    int fb = open("/dev/fb0", O_RDWR);
    int n_reps = 11;
    struct timespec times[n_reps];
    int clock_status;
    __u32 dummy = 0;

    int i;
    for (i = 0; i < n_reps; i++) {
        ioctl(fb, FBIO_WAITFORVSYNC, &dummy);
        times[i] = get_current_time(&clock_status);
    }

    close(fb);

    long delta_usecs[n_reps - 1];
    for (i = 0; i < n_reps - 1; i++) {
        delta_usecs[i] = cmp_times(times[i], times[i + 1]);
    }

    return int_round(1 / (mean_long(delta_usecs, n_reps - 1) / 1000000));
}

double gaussian(int radius, int sigma) {
    return exp(-((radius * radius) / (double) (2 * sigma * sigma)));
}

void flip_buffer(fb_config* fb0){
    fb0->current_buffer = !fb0->current_buffer;
    int fd = open("/dev/vcio", O_RDWR | O_SYNC);
    if(fd == -1){
        perror("VCIO OPEN ERROR: ");
        return;
    }

    volatile uint32_t property[32] __attribute__((aligned(16))) =
    {
    0x00000000,
    0x00000000,
    0x48009,
    8,
    8,
    0,
    0,
    0
    };
    property[0] = 8 * sizeof(property[0]);
    if(fb0->current_buffer != 0){
        property[6] = fb0->height;
    }

    if(ioctl(fd, _IOWR(100, 0, char *), property) == -1){
        perror("BUFFER FLIP IOCTL ERROR");
    }
    close(fd);
}

int* get_current_offset(fb_config fb0){
    int fd = open("/dev/vcio", O_RDWR | O_SYNC);
    if(fd == -1){
        perror("VCIO OPEN ERROR: ");
        return NULL;
    }

    volatile uint32_t property[32] __attribute__((aligned(16))) =
    {
    0x00000000,
    0x00000000,
    0x00040009,
    8,
    8,
    0,
    0,
    0
    };
    property[0] = 8 * sizeof(property[0]);

    if(ioctl(fd, _IOWR(100, 0, char *), property) == -1){
        perror("BUFFER FLIP IOCTL ERROR");
    }
    close(fd);
    int *result = malloc(2 * sizeof(int));
    result[0] = property[5];
    result[1] = property[6];
    return result;
}
