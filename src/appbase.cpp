#include <unistd.h>
#include "windows.h"

void screen_refresh(int left, int top, int right, int bottom)
{
//    printf("screen_refresh %d %d %d %d\n", left, top, right, bottom);
//    fflush(stdout);
}

void start_app(char *fullpath, char *workdir)
{
}

void window_create(window_t* win)
{
    printf("window_create %p\n", win);
    fflush(stdout);
}

void window_destroy(window_t* win)
{
    if (!win->dupwin) return;
    printf("window_destroy %p\n", win->dupwin);
    fflush(stdout);
}

void window_set_pos(window_t* win)
{
    if (!win->dupwin) return;
    printf("window_set_pos %p %d %d\n", win->dupwin, win->get_left(), win->get_top());
    fflush(stdout);
}

void window_set_size(window_t* win)
{
    if (!win->dupwin) return;
    printf("window_set_size %p %d %d\n", win->dupwin, win->get_width(), win->get_height());
    fflush(stdout);
}

void window_set_style(window_t* win)
{
    if (!win->dupwin) return;
    printf("window_set_style %p %d\n", win->dupwin, (int)win->get_style());
    fflush(stdout);
}

void window_set_text(window_t* win)
{
    if (!win->dupwin) return;
    printf("window_set_text %p %s\n", win->dupwin, win->get_text().c_str());
    fflush(stdout);
}

void window_refresh(window_t *win)
{
    if (!win->dupwin) return;
    pixel_t pixel;
    char s[256];
    int i, n, size = win->get_width() * win->get_height();
    bool refresh = false;
    for (i = 0;i < size;i++) {
        if (win->old_pixels[i] != win->pixels[i]) {
            refresh = true;
            win->old_pixels[i] = win->pixels[i];
        }
    }
    if (!refresh) {
        n = sprintf(s, "window_do_refresh %p\n", win->dupwin);
        if (n != write(STDOUT_FILENO, s, n)) exit(0);
        return;
    }
    size *= sizeof(pixel_t);
    n = sprintf(s, "window_refresh %p %d\n", win->dupwin, win->get_width() * win->get_height());
    if (n != write(STDOUT_FILENO, s, n)) exit(0);

    for (i = 0;i < size;i = i + n) {
        n =  write(STDOUT_FILENO, win->pixels, size - i);
        if (n < 0) exit(0);
    }
}  

void window_set_focused(window_t *win, int fx, int fy)
{
    if (!win->dupwin) return;
    printf("window_set_focused %p %d %d\n", win->dupwin, fx, fy);
    fflush(stdout);
}

void msg_process()
{
    char s[256];
    for (;;) {
        window_t* win = NULL;
        window_t *dupwin = NULL;
        int width, height;
        int key;
        int x, y;
        int ctrl;

        if (!fgets(s, sizeof(s), stdin)) break;
        
        if (sscanf(s, "set syswin %p %p", &win, &dupwin) == 2) {
            if (win) {
                win->dupwin = dupwin;
                window_set_pos(win);
                window_set_size(win);
                window_set_style(win);
                window_set_text(win);
                win->old_pixels[0].v = 0;
                win->refresh();
            }
        } else if (sscanf(s, "on_inner_keybd_event %p %d", &win, &key) == 2) {
            if (win) {
                win->on_inner_keybd_event(key);
                win->refresh();
            }
        } else if (sscanf(s, "on_inner_mouse_up %p %d %d %d", &win, &key, &x, &y) == 4) {
            if (win) {
                win->on_inner_mouse_up(key, x, y);
                win->refresh();
            }
        } else if (sscanf(s, "on_inner_mouse_down %p %d %d %d %d", &win, &key, &x, &y, &ctrl) == 5) {
            if (win) {
                win->on_inner_mouse_down(key, x, y, ctrl);
                win->refresh();
            }
        } else if (sscanf(s, "on_inner_mouse_drag %p %d %d %d %d", &win, &key, &x, &y, &ctrl) == 5) {
            if (win) {
                win->on_inner_mouse_drag(key, x, y, ctrl);
                win->refresh();
            }
        } else if (sscanf(s, "on_inner_mouse_wheel %p %d %d %d %d", &win, &key, &x, &y, &ctrl) == 5) {
            if (win) {
                win->on_inner_mouse_wheel(key, x, y, ctrl);
                win->refresh();
            }
        } else {
            printf("%s\n", s);
            fflush(stdout);
        }
    }
}