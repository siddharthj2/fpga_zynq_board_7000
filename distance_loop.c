/*
* Copyright (C) 2013-2022  Xilinx, Inc.  All rights reserved.
* Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/

// auto_select_dist.cpp
// distance_loop.c
// Simple V4L2‑based distance estimator in a continuous loop.
//
// Compile on PC:
//   gcc distance_loop.c -o distance_loop -lm
//
// In PetaLinux recipe (cross‑compile), recipe will invoke this directly.

// distance_loop.c
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <time.h>               // nanosleep, timespec
#include <sys/time.h>           // timeval
#include <linux/videodev2.h>    // v4l2 headers

// -------- CONFIG --------
#define DEVICE      "/dev/video0"
#define WIDTH       640
#define HEIGHT      480
#define PIX_FMT     V4L2_PIX_FMT_GREY
#define THRESHOLD   30    // brightness threshold

static const float real_widths_cm[] = {
    0.0f,
    5.0f,   // object #1 = 5 cm
    7.2f    // object #2 = 7.2 cm
};
static const int num_objects = sizeof(real_widths_cm)/sizeof(real_widths_cm[0]) - 1;
#define FOCAL_PIXELS 400.0f

struct buffer {
    void   *start;
    size_t  length;
};

int main(){
    int fd = open(DEVICE, O_RDWR);
    if(fd < 0){
        perror("Opening video device");
        return 1;
    }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = PIX_FMT;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0){
        perror("VIDIOC_S_FMT");
        close(fd);
        return 1;
    }

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0){
        perror("VIDIOC_REQBUFS");
        close(fd);
        return 1;
    }

    struct v4l2_buffer buf = {0};
    buf.type = req.type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0){
        perror("VIDIOC_QUERYBUF");
        close(fd);
        return 1;
    }

    struct buffer buffers[1];
    buffers[0].length = buf.length;
    buffers[0].start  = mmap(NULL, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if(buffers[0].start == MAP_FAILED){
        perror("mmap");
        close(fd);
        return 1;
    }

    if(ioctl(fd, VIDIOC_STREAMON, &buf.type) < 0){
        perror("VIDIOC_STREAMON");
        munmap(buffers[0].start, buffers[0].length);
        close(fd);
        return 1;
    }

    printf("Select object:\n");
    for(int i=1; i<=num_objects; i++){
        printf("  %d) %.1f cm\n", i, real_widths_cm[i]);
    }

    int choice = 1;
    printf("Enter choice [1-%d]: ", num_objects);
    if(scanf("%d",&choice)!=1 || choice<1 || choice>num_objects){
        printf("Invalid, defaulting to 1\n");
        choice = 1;
    }
    float real_w = real_widths_cm[choice];

    while(1){
        if(ioctl(fd, VIDIOC_QBUF, &buf) < 0){
            perror("VIDIOC_QBUF");
            break;
        }
        if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0){
            perror("VIDIOC_DQBUF");
            break;
        }

        unsigned char *frame = buffers[0].start;
        int minx = WIDTH, maxx = 0;

        for(int y=0; y<HEIGHT; y++){
            for(int x=0; x<WIDTH; x++){
                if(frame[y*WIDTH + x] > THRESHOLD){
                    if(x < minx) minx = x;
                    if(x > maxx) maxx = x;
                }
            }
        }

        if(minx < maxx){
            int pixel_w = maxx - minx;
            float dist = (real_w * FOCAL_PIXELS) / pixel_w;
            printf("\rPixels: %3d | Distance: %6.2f cm", pixel_w, dist);
        } else {
            printf("\rNo object detected                         ");
        }
        fflush(stdout);

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000}; // 100ms
        nanosleep(&ts, NULL);
    }

    ioctl(fd, VIDIOC_STREAMOFF, &buf.type);
    munmap(buffers[0].start, buffers[0].length);
    close(fd);
    return 0;
}

