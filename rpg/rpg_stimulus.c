#include "rpg_legacy.h"

void* squarewave(int x, int y, int t, int wavelength, int speed, double angle, double cosine, double sine, double weight, double contrast, int background, int colormode){
    unsigned short black = 0;
    unsigned short white = 255;
    double brightness;
    double x_prime, int_part, frac_part;
    if(angle == ANGLE_0){
        x_prime = -x + speed * t;
    }else if(angle == ANGLE_90){
        x_prime = y + speed * t;
    }else if(angle == ANGLE_180){
        x_prime = x + speed * t;
    }else if(angle == ANGLE_270){
        x_prime = -y + speed * t;
    }else{
        x_prime = (cosine * x + sine * y) + speed * t;
    }
    frac_part = modf(x_prime, &int_part);
    brightness = ((((double)((((int)(int_part)) % wavelength + wavelength) % wavelength) + frac_part)) / wavelength);
    if(brightness < 0.5){
        brightness = white;
    } else {
        brightness = black;
    }

    brightness = contrast * weight * (brightness - 127) + 127;
    void* pixel_ptr;
    if(colormode == RGB888MODE){
        uint24_t* pixel_ptr_24 = malloc(sizeof(uint24_t));
        *pixel_ptr_24 = rgb_to_uint_24bit(brightness, brightness, brightness);
        pixel_ptr = pixel_ptr_24;
    }
    else{
        uint16_t* pixel_ptr_16 = malloc(sizeof(uint16_t));
        *pixel_ptr_16 = rgb_to_uint(brightness, brightness, brightness);
        pixel_ptr = pixel_ptr_16;
    }
    return pixel_ptr;
}

void* sinewave(int x, int y, int t, int wavelength, int speed, double angle, double cosine, double sine, double weight, double contrast, int background, int colormode){
    double brightness;
    double x_prime;
    if(angle == ANGLE_0){
        x_prime = -x + speed * t;
    }else if(angle == ANGLE_90){
        x_prime = y + speed * t;
    }else if(angle == ANGLE_180){
        x_prime = x + speed * t;
    }else if(angle == ANGLE_270){
        x_prime = -y + speed * t;
    }else{
        x_prime = (cosine * x + sine * y) + (speed * t);
    }

    brightness = contrast * weight * 127 * sin(2 * M_PI * (x_prime) / wavelength) + 127;
    int bright_int = brightness;
    void* pixel_ptr;
    if(colormode == RGB888MODE){
        uint24_t* pixel_ptr_24 = malloc(sizeof(uint24_t));
        *pixel_ptr_24 = rgb_to_uint_24bit(bright_int, bright_int, bright_int);
        pixel_ptr = pixel_ptr_24;
    }
    else{
        uint16_t* pixel_ptr_16 = malloc(sizeof(uint16_t));
        *pixel_ptr_16 = rgb_to_uint(bright_int, bright_int, bright_int);
        pixel_ptr = pixel_ptr_16;
    }
    return pixel_ptr;
}

void* gabor(int x, int y, int t, int wavelength, int speed, double angle, double cosine, double sine, double weight, double contrast, int background, int colormode) {
    double brightness, x_prime, amplitude;

    if(angle == ANGLE_0){
        x_prime = -x + speed * t;
    }else if(angle == ANGLE_90){
        x_prime = y + speed * t;
    }else if(angle == ANGLE_180){
        x_prime = x + speed * t;
    }else if(angle == ANGLE_270){
        x_prime = -y + speed * t;
    }else{
        x_prime = (cosine * x + sine * y) + (speed * t);
    }
    if (background < 128) {
        amplitude = contrast * weight * background;
    } else {
        amplitude = contrast * weight * (255 - background);
    }
    brightness = amplitude * sin(2 * M_PI * (x_prime) / wavelength) + background;
    void* pixel_ptr;
    if(colormode == RGB888MODE){
        uint24_t* pixel_ptr_24 = malloc(sizeof(uint24_t));
        *pixel_ptr_24 = rgb_to_uint_24bit(brightness, brightness, brightness);
        pixel_ptr = pixel_ptr_24;
    }
    else{
        uint16_t* pixel_ptr_16 = malloc(sizeof(uint16_t));
        *pixel_ptr_16 = rgb_to_uint(brightness, brightness, brightness);
        pixel_ptr = pixel_ptr_16;
    }
    return pixel_ptr;
}

void* circle(int waveform, int radius, int padding, int point_radius, int j, int i, int t, int wavelength, int speed,
             double angle, double cosine, double sine, double weight, double contrast, int background,
             int colormode){
    void* pixel_ptr;
    uint16_t* pixel_ptr_16;
    uint24_t* pixel_ptr_24;
    int circle_case;
    if(point_radius > radius + padding) {
        circle_case = OUTSIDEMASK;
    }
    else if(point_radius <= radius) {
        circle_case = INSIDEMASK;
        weight = 1;
    } else {
        circle_case = INSIDEMASK;
    }
    switch(circle_case | colormode | waveform){
        case(OUTSIDEMASK | RGB888MODE | SQUARE):
        case(OUTSIDEMASK | RGB888MODE | SINE):
            pixel_ptr_24 = malloc(sizeof(uint24_t));
            *pixel_ptr_24 = rgb_to_uint_24bit(background, background, background);
            break;
        case(OUTSIDEMASK | RGB565MODE | SQUARE):
        case(OUTSIDEMASK | RGB565MODE | SINE):
            pixel_ptr_16 = malloc(sizeof(uint16_t));
            *pixel_ptr_16 = rgb_to_uint(background, background, background);
            break;
        case(INSIDEMASK | RGB888MODE | SQUARE):
            pixel_ptr_24 = squarewave(j, i, t, wavelength, speed, angle, cosine, sine, weight, contrast, background, colormode);
            break;
        case(INSIDEMASK | RGB888MODE | SINE):
            pixel_ptr_24 = sinewave(j, i, t, wavelength, speed, angle, cosine, sine, weight, contrast, background, colormode);
            break;
        case(INSIDEMASK | RGB565MODE | SQUARE):
            pixel_ptr_16 = squarewave(j, i, t, wavelength, speed, angle, cosine, sine, weight, contrast, background, colormode);
            break;
        case(INSIDEMASK | RGB565MODE | SINE):
            pixel_ptr_16 = sinewave(j, i, t, wavelength, speed, angle, cosine, sine, weight, contrast, background, colormode);
            break;
    }
    if(colormode == RGB888MODE){
        pixel_ptr = pixel_ptr_24;
    }else{
        pixel_ptr = pixel_ptr_16;
    }
    return pixel_ptr;
}

void * build_frame(int t, double angle, fb_config framebuffer, int wavelength, int speed, int waveform,
                   double contrast, int background, int center_j, int center_i, int sigma, int radius, int padding,
                   int colormode){

    angle = degrees_to_radians(angle);
    int grating_type;
    if (radius == 0 && sigma == 0){
        grating_type = FULLSCREEN;
    }else if(sigma == 0){
        grating_type = CIRCLE;
    }else{
        grating_type = GABOR;
    }
    double sine = sin(angle);
    double cosine = cos(angle);
    void* array_start = malloc(framebuffer.size);
    uint24_t *write_location_24, *read_location_24;
    uint16_t *write_location_16, *read_location_16;
    write_location_24 = write_location_16 = array_start;
    int i, j;
    for(i = 0; i < framebuffer.height; i++){
        for(j = 0; j < framebuffer.width; j++){
            int point_radius = (int) sqrt(((j - center_j) * (j - center_j)) + ((i - center_i) * (i - center_i)));
            double gauss_weight = gaussian(point_radius, sigma);
            double circle_weight = padding == 0 ? 1 : ((double)(radius + padding - point_radius)) / padding;
            switch(grating_type | colormode | waveform){
                case(FULLSCREEN | SQUARE | RGB888MODE):
                    read_location_24 = squarewave(j, i, t, wavelength, speed, angle, cosine, sine, 1, contrast, background, colormode);
                    break;
                case(FULLSCREEN | SQUARE | RGB565MODE):
                    read_location_16 = squarewave(j, i, t, wavelength, speed, angle, cosine, sine, 1, contrast, background, colormode);
                    break;
                case(FULLSCREEN | SINE | RGB888MODE):
                    read_location_24 = sinewave(j, i, t, wavelength, speed, angle, cosine, sine, 1, contrast, background, colormode);
                    break;
                case(FULLSCREEN | SINE | RGB565MODE):
                    read_location_16 = sinewave(j, i, t, wavelength, speed, angle, cosine, sine, 1, contrast, background, colormode);
                    break;
                case(GABOR | SINE | RGB888MODE):
                    read_location_24 = gabor(j, i, t, wavelength, speed, angle, cosine, sine, gauss_weight, contrast, background, colormode);
                    break;
                case(GABOR | SINE | RGB565MODE):
                    read_location_16 = gabor(j, i, t, wavelength, speed, angle, cosine, sine, gauss_weight, contrast, background, colormode);
                    break;
                case(CIRCLE | SQUARE | RGB888MODE):
                case(CIRCLE | SINE | RGB888MODE):
                    read_location_24 = circle(waveform, radius, padding, point_radius, j, i, t, wavelength, speed, angle, cosine, sine, circle_weight, contrast, background, colormode);
                    break;
                case(CIRCLE | SQUARE | RGB565MODE):
                case(CIRCLE | SINE | RGB565MODE):
                    read_location_16 = circle(waveform, radius, padding, point_radius, j, i, t, wavelength, speed, angle, cosine, sine, circle_weight, contrast, background, colormode);
                    break;
                default:
                    printf("ERROR:Invalid tags encountered in build_frame funnction.\n");
                    return NULL;
            }
            if(colormode == RGB888MODE){
                *write_location_24 = *read_location_24;
                free(read_location_24);
            }else{
                *write_location_16 = *read_location_16;
                free(read_location_16);
            }
            write_location_24++;
            write_location_16++;
        }
    }
    return (void *)array_start;
}

int build_grating(char * filename, double duration, double angle, double sf, double tf, double contrast, int background, int width, int height, int waveform,
                  double percent_sigma, double percent_diameter, double percent_center_left, double percent_center_top, double percent_padding, int colormode){
    int fps = get_refresh_rate(width, height);
    if (fps <= 0) {
        return 1;
    }
    printf("Refresh rate measured as: %d hz\n", fps);
    fb_config fb0;
    fb0.width = width;
    fb0.height = height;
    fb0.depth = (colormode == RGB888MODE) ? 24 : 16;
    fb0.size = (fb0.height) * (fb0.depth) * (fb0.width) / 8;
    FILE * file = fopen(filename, "wb");
    if(file == NULL){
        perror("File creation failed\n");
        PyErr_SetString(PyExc_OSError, "File creation failed.");
        return 1;
    }
    int wavelength = (fb0.width / DEGREES_SUBTENDED) / sf;

    int speed = wavelength * tf / fps;
    if(speed == 0){
        speed = 1;
    }
    double actual_tf = ((double)(speed * fps)) / wavelength;
    int sigma = fb0.width * percent_sigma / 100;
    int radius = fb0.width * percent_diameter / 200;
    int center_j = fb0.width * percent_center_left / 100;
    int center_i = fb0.height * percent_center_top / 100;
    double padding = radius * percent_padding / 100;
    if(actual_tf != tf){
        printf("Grating %s has a requested temporal frequency of %f, actual temporal frequency will be %f\n", filename, tf, actual_tf);
    }
    fileheader_t header;
    header.frames_per_second = fps;
    header.frames_per_cycle = wavelength / gcd(wavelength, speed);
    if(header.frames_per_cycle > fps * duration) {
        header.frames_per_cycle = fps * duration;
    }
    header.n_frames = fps * duration;
    header.spacial_frequency = (uint16_t)(sf);
    header.temporal_frequency = (uint16_t)(tf);
    header.width = (uint16_t)(width);
    header.height = (uint16_t)(height);
    fwrite(&header, sizeof(fileheader_t), 1, file);
    int t, clock_status;
    struct timespec time1, time2;
    time1 = get_current_time(&clock_status);
    if(clock_status){
        return -1;
    }
    uint24_t * frame_24;
    uint16_t * frame_16;
    for (t = 0; t < header.frames_per_cycle; t++){
        if(colormode == RGB888MODE){
            frame_24 = build_frame(t, angle, fb0, wavelength, speed, waveform, contrast, background, center_j, center_i, sigma, radius, padding, colormode);
            if(frame_24 == NULL){return -1;}
            fwrite(frame_24, sizeof(*frame_24), fb0.height * fb0.width, file);
            free(frame_24);
        }else{
            frame_16 = build_frame(t, angle, fb0, wavelength, speed, waveform, contrast, background, center_j, center_i, sigma, radius, padding, colormode);
            if(frame_16 == NULL){return -1;}
            fwrite(frame_16, sizeof(*frame_16), fb0.height * fb0.width, file);
            free(frame_16);
        }
        if(t == 4){
            time2 = get_current_time(&clock_status);
            if(clock_status){
                return -1;
            }
            printf("Expected time to completion: %ld seconds\n", header.frames_per_cycle * cmp_times(time1, time2) / 1000000 / 5);
        }
    }
    fclose(file);
    return 0;
}

void* load_grating(char* filename, fb_config fb0){
    int page_size = getpagesize();
    int bytes_already_read = 0;
    int read_size, frames;
    int filedes = open(filename, O_RDWR);
    if(filedes == -1){
        perror("Failed to open file");
        return NULL;
    }
    fileheader_t* header = mmap(NULL, sizeof(fileheader_t), PROT_READ, MAP_PRIVATE, filedes, 0);
    if(header == MAP_FAILED){
        perror("From mmap for header access");
        exit(1);
    }
    frames = header->frames_per_cycle;
    int file_fps = header->frames_per_second;
    int refresh_rate = get_refresh_rate((int)fb0.width, (int)fb0.height);
    if (refresh_rate <= 0) {
        munmap(header, sizeof(fileheader_t));
        close(filedes);
        return NULL;
    }
    if (refresh_rate != file_fps) {
        printf("File generated at %d FPS, but monitor running at %d HZ. This will cause inaccurate timing \n", file_fps, refresh_rate);
    }
    int file_size = frames * fb0.size + sizeof(fileheader_t);
    munmap(header, sizeof(fileheader_t));
    uint8_t *frame_data = malloc(file_size);
    while(bytes_already_read < file_size){
        read_size = 20000 * page_size;
        if(read_size + bytes_already_read >= file_size){
            read_size = file_size - bytes_already_read;
        }
        void* mmap_start = mmap(NULL, read_size, PROT_READ, MAP_PRIVATE, filedes, bytes_already_read);
        if(mmap_start == MAP_FAILED){
            perror("From MMAP attempt to read");
            exit(1);
        }
        memcpy(frame_data + bytes_already_read, mmap_start, read_size);
        bytes_already_read += read_size;
        munmap(mmap_start, read_size);
    }
    close(filedes);
    return (void*) frame_data;
}

int debug_dump_grating(void* frame_data, fb_config fb0, char* filename){
    uint24_t* frame_data_24 = frame_data;
    FILE * file = fopen(filename, "wb");
    if(file == NULL){
        perror("File creation failed\n");
        PyErr_SetString(PyExc_OSError, "File creation failed.");
        return 1;
    }
    fwrite(frame_data_24, 1, fb0.size * 60 + sizeof(fileheader_t), file);
    fclose(file);
    return 0;
}

void* load_raw(char* filename) {
    int page_size = getpagesize();
    int bytes_already_read = 0;
    int read_size;
    int fh = open(filename, O_RDWR);
    if(fh == -1) {
        perror("Failed to open file");
        return NULL;
    }
    off_t len = lseek(fh, 0, SEEK_END);
    if (len == -1) {
        printf("Checking File Length Failed.\n");
        return NULL;
    }

    uint8_t *frame_data = malloc(len);
    while(bytes_already_read < len) {
        read_size = 20000 * page_size;
        if(read_size + bytes_already_read >= len) {
            read_size = len - bytes_already_read;
        }
        void *mmap_start = mmap(NULL, read_size, PROT_READ, MAP_PRIVATE, fh, bytes_already_read);
        if(mmap_start == MAP_FAILED) {
            perror("MMAP failed to read");
            return NULL;
        }
        memcpy(frame_data + bytes_already_read, mmap_start, read_size);
        bytes_already_read += read_size;
        munmap(mmap_start, read_size);
    }
    close(fh);
    return frame_data;
}

int convert_raw(char* filename, char* new_filename, int n_frames, int width, int height, int refresh_per_frame, int colormode) {
    int fh = open(filename, O_RDWR);
    if (fh == -1) {
        perror("Failed to open file");
        return 1;
    }

    FILE * new_file = fopen(new_filename, "wb");
    if (new_file == NULL) {
        perror("Failed to open new file");
        return 1;
    }

    fileheader_raw header;
    header.n_frames = n_frames;
    header.width = width;
    header.height = height;
    header.refresh_per_frame = refresh_per_frame;
    fwrite(&header, sizeof(fileheader_raw), 1, new_file);

    off_t len = lseek(fh, 0, SEEK_END);
    if (len == -1) {
        printf("Checking File Length Failed.\n");
        return 1;
    }
    char *buffer = mmap(0, len, PROT_READ, MAP_PRIVATE, fh, 0);

    if (buffer == MAP_FAILED){
        PyErr_SetString(PyExc_OSError, "MMAP failed");
        return 1;
    }
    int i = 0;
    char r, g, b;
    uint16_t new_pixel_16;
    uint24_t new_pixel_24;
    while (i < len) {
        r = buffer[i];
        g = buffer[i + 1];
        b = buffer[i + 2];
        i += 3;
        if(colormode == RGB888MODE){
            new_pixel_24 = rgb_to_uint_24bit(r, g, b);
            fwrite(&new_pixel_24, sizeof(uint24_t), 1, new_file);
        }else{
            new_pixel_16 = rgb_to_uint(r, g, b);
            fwrite(&new_pixel_16, sizeof(uint16_t), 1, new_file);
        }
    }
    munmap(buffer, len);
    fclose(new_file);
    close(fh);
    return 0;
}

int unload_grating(void* frame_data){
    free(frame_data);
    return 0;
}

int unload_raw(uint16_t* raw_data) {
    free(raw_data);
    return 0;
}
