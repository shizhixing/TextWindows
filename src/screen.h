#ifndef SCREEN
#define SCREEN

#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "windows.h"

class screen_t {
    friend void *refresh_thread(void *arg);
    friend void sig_resize( int signo );
    private:
        int width;
        int height;
        pixel_t *pixels;
        int color;
        int bgcolor;
        bool bold;
        struct termios stored_settings;
        bool running;
        u_int64_t refresh_seq;
        void cls(){
            printf("\e[1;1H\e[2J");
        }

        void hide_cursor() {
            printf("\033[?25l");
        }

        void show_cursor() {
            printf("\033[?25h");
        }

        void set_cursor_pos(int x, int y) {
            printf("\033[%d;%dH", y + 1, x + 1);
        }

        pixel_t *get_pixel(int x, int y);
        void set_font(int color, int bgcolor, bool bold);
        void set_pixel(int x, int y, color_t color, bgcolor_t bgcolor, bool bold, u_int32_t v);
        void set_size(int width, int height);
        int getkey();
        void redraw(int left, int top, int right, int bottom);
    public:
        screen_t();
        ~screen_t();
        void run();
        int get_width() {return width;};
        int get_height() {return height;};
        void refresh(int left, int top, int right, int bottom);
        void set_focused_win(void *win);
};

#endif
