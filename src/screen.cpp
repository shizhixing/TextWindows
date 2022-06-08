#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "screen.h"
#include "utils.h"

pthread_mutex_t refresh_mutex;

screen_t::screen_t()
{
    pixels = NULL;
    setvbuf(stdout, (char*)NULL, _IONBF, 0);
    setvbuf(stderr, (char*)NULL, _IONBF, 0);
    printf("\033[?1049h");//allscreen
    printf("\033[?1002h");//enable mouse
    hide_cursor();
    struct termios terminfo;
    tcgetattr (0, &stored_settings);
    terminfo = stored_settings;
    cfmakeraw(&terminfo);
    tcsetattr(0, TCSANOW, &terminfo);
    cls();
    running = true;
    color = -1;
    bgcolor = -1;
}

screen_t::~screen_t()
{
    tcsetattr (0, TCSANOW, &stored_settings);
    show_cursor();
    printf("\033[?1002l");//disable mouse
    printf("\033[?1049l");//exit screen
    running = false;
}

void screen_t::set_font(int color, int bgcolor, bool bold)
{
    if (this->color == color && this->bold == bold && this->bgcolor == bgcolor) return;
    char s[256];
    char *p = s;
    bool d = false;
    p += sprintf(p, "\e[");
    if (this->bold != bold) {
        this->bold = bold;
        if (bold) {
            p += sprintf(p, "1");
        } else {
            p += sprintf(p, "0");
            this->color = -1;
            this->bgcolor = -1;
        }
        d = true;
    }

    if (bgcolor >= 40 && bgcolor <= 47 && this->bgcolor != bgcolor) {
        this->bgcolor = bgcolor;
        if (d) p += sprintf(p, ";");
        p += sprintf(p, "%d", bgcolor);
        d = true;
    }

    if (color >= 30 && color <= 37 && this->color != color) {
        this->color = color;
        if (d) p += sprintf(p, ";");
        p += sprintf(p, "%d", color);
        d = true;
    }
    
    printf("%sm", s);
}

void screen_t::set_pixel(int x, int y, color_t color, bgcolor_t bgcolor, bool bold, u_int32_t v)
{
    pixel_t *pixel;
    if ((x < 0 || x >= width) || (y < 0 || y >= height)) {
        return;
    }
    pixel = &pixels[y * width + x];
    pixel->color = color;
    pixel->bgcolor = bgcolor;
    pixel->bold = bold;
    pixel->v = v;
}

void screen_t::set_size(int width, int height)
{
    if (this->width == width && this->height == height) return;
    if (this->width * this->height != width * height) {
        if (pixels) {
            delete pixels;
        }
        pixels = new pixel_t[width * height];
        for (int i = 0;i < width * height;i++) {
            pixels[i].v = 0;
        }
    }

    cls();
    this->width = width;
    this->height = height;
//    fflush(stdout);
}

void screen_t::refresh(int left, int top, int right, int bottom)
{
    pthread_mutex_lock(&refresh_mutex);
    redraw(left, top, right, bottom);
    //refresh_seq++;
    pthread_mutex_unlock(&refresh_mutex);
}

pixel_t *screen_t::get_pixel(int x, int y)
{
    if ((x < 0 || x >= width) || (y < 0 || y >= height)) {
        return NULL;
    }

    return &pixels[y * width + x];
}

void screen_t::redraw(int left, int top, int right, int bottom)
{
    extern window_t *desk;
    bool start_print = false;
    int focus_x = -1, focus_y = -1;
    window_t *win;
    int tx, ty, tfx, tfy;

    left = 0;
    top = 0;
    right = width - 1;
    bottom = height - 1;

    hide_cursor();
    for (int y = top;(y < get_height()) && (y <= bottom);y++ ) {
        for (int x = left;(x < get_width()) && (x <= right);x++) {
            win = desk->get_win_on_pos(x, y);
            pixel_t new_pixel = desk->get_pixel(x, y);
            pixel_t *old_pixel = get_pixel(x, y);
            if (!old_pixel) continue;
            if (win->is_focused() && win->top_win()->actived) {
                if (win->get_focus_pos(tfx, tfy)) {
                    win->get_screen_pos(tx, ty);
                    if (tx + tfx == x && ty + tfy == y) {
                        focus_x = x;
                        focus_y = y;
                    }
                }
            }

            pixel_t pixel;
            
            if (new_pixel.v == 0) new_pixel = pixel;
            if (new_pixel != *old_pixel) {
                if (start_print == false) {
                    start_print = true;
                    set_cursor_pos(x, y);
                }
                *old_pixel = new_pixel;
                if (new_pixel.v != (u_int32_t)-1) {
                    set_pixel(x, y, new_pixel.color, new_pixel.bgcolor, new_pixel.bold, new_pixel.v);
                    set_font(new_pixel.color, new_pixel.bgcolor, new_pixel.bold);
                    u_int8_t t[4];
                    t[0] = (u_int8_t)(new_pixel.v >> 24);
                    t[1] = (u_int8_t)((new_pixel.v >> 16) & 0xff);
                    t[2] = (u_int8_t)((new_pixel.v >> 8) & 0xff);
                    t[3] = (u_int8_t)(new_pixel.v & 0xff);
                    if (t[0] != 0) {
                        printf("%c%c%c%c", t[0], t[1], t[2], t[3]);
                    } else if (t[1] != 0) {
                        printf("%c%c%c", t[1], t[2], t[3]);
                    } else if (t[2] != 0) {
                        printf("%c%c", t[2], t[3]);
                    } else if (t[3] != 0) {
                        printf("%c", t[3]);
                    }
                };
            } else if (start_print) {
                start_print = false;
            }
        }
    }
    static int no_count = 0;

    if (focus_x != -1 && focus_y != -1) {
        set_cursor_pos(focus_x, focus_y);
        show_cursor();
    }
}

pthread_t key_thread_tid;
volatile int g_key;
void *key_thread(void *arg)
{
    g_key = getchar();
    return NULL;
}

int screen_t::getkey()
{
    unsigned int r = 0;
    int i;
    int c;
    int alt = 0;
    for (;;) {
        if (key_thread_tid) {
            pthread_join(key_thread_tid, NULL);
            key_thread_tid = 0;
            c = g_key;
        } else {
            c = getchar();
        }
//        printf("%c", c);
//        if (c == 'q') return c;
//        continue;
        if (c == -1) continue;
        if (c == '\033') {/* ESC */
            int timer;
            g_key = 0;
            pthread_create(&key_thread_tid, NULL, key_thread, NULL);
            for (timer = 50;timer > 0;timer--) {
                if (g_key) {
                    pthread_join(key_thread_tid, NULL);
                    key_thread_tid = 0;
                    break;
                }
                usleep(1000);
            }
            if (timer == 0) break;
            c = g_key;
            for (;c == '\033';) {
                alt = 0x80000000;
                c = getchar();
            }

            if (c == '[' || c == 'O') {
                r = c;
                c = getchar();
                if (c != 0x4d) {/* key */
                    while (c != -1 && c != 0x41 && c != 0x42 && c != 0x43 && c != 0x44 && c != 0x45 && c != 'Z' && c != 0x7e) {
                        r = (r << 8) | c;
                        c  = getchar();
                    }
                    if (c == -1) r = 0;
                    else r = (r << 8) | c;
                } else {/* mouse */
                    r = c;
                    for (i = 0;i < 3 && c != -1;i++) {
                        c = getchar();
                        r = (r << 8) | c;
                    }
                    if (c == -1) r = 0;
                }
                
            } else {
                r = 0x80000000 + c;
            }
            switch (r) {
                case 0x5b5b41:
                    r = KEY_F1;
                    break;
                case 0x5b5b42:
                    r = KEY_F2;
                    break;
                case 0x5b5b43:
                    r = KEY_F3;
                    break;
                case 0x5b5b44:
                    r = KEY_F4;
                    break;
                case 0x5b5b45:
                    r = KEY_F5;
                    break;
                case 0x5b337e:
                    r = KEY_DEL;
                    break;
                default:
                    break;
            }
            r = r + alt;
            if (r) return r;
            return '\033';
            break;
        } else if (c == 0x7f) {
             return 8;
        } else if ((c & 0xe0) == 0xe0) {
            r = c;
            c = getchar();
            r = (r << 8) | c;
            c = getchar();
            r = (r << 8) | c;
            return r;
        } else {
            if (c == 0xd) c = 0xa;
            break;
        }
    }
    return c;
}

#define IS_MOUSE_EVENT(key) (((key) >> 24) == 0x4d)
#define MOUSE_KEY_UP 3

#define MOUSE_DOWN 0x20
#define MOUSE_DRAG 0x40
#define MOUSE_WHEEL 0x60
extern window_t *desk;

static pthread_mutex_t msg_mutex;

void screen_t::run()
{
    for (int key = getkey();;key = getkey()) {
        pthread_mutex_lock(&msg_mutex);
        if (IS_MOUSE_EVENT(key)) {
            unsigned char m_e;
            static unsigned char m_pk = 255;
            unsigned char m_k;
            unsigned char m_act;
            unsigned char m_ctrl;
            int m_x;
            int m_y;

            m_e = (key >> 16) & 0xff;
            m_x = ((key >> 8) & 0xff) - 0x21;
            m_y = (key & 0xff) - 0x21;
            m_k = m_e & MOUSE_KEY_UP;
            m_act = m_e & (MOUSE_DOWN | MOUSE_DRAG);
            m_ctrl = m_e & (MOUSE_SHIFT | MOUSE_ALT | MOUSE_CTRL);
            if (m_x < 0 || m_x >= desk->width || m_y < 0 || m_y >= desk->height) {
                pthread_mutex_unlock(&msg_mutex);
                continue;
            }
            if (m_act == MOUSE_DOWN) {
                if (m_k == MOUSE_KEY_UP) {/* mouse key up */
                    if (m_pk != 255) {
                        desk->on_inner_mouse_up(m_pk, m_x, m_y);
                        desk->refresh();
                        m_pk = 255;
                    }
                } else {/* mouse key down */
                    desk->on_inner_mouse_down(m_k, m_x, m_y, m_ctrl);
                    desk->refresh();
                    m_pk = m_k;
                }
            } else if (m_act == MOUSE_DRAG) {
                desk->on_inner_mouse_drag(m_pk, m_x, m_y, m_ctrl);
                desk->refresh();
            } else if (m_act == MOUSE_WHEEL) {
                desk->on_inner_mouse_wheel(m_k, m_x, m_y, m_ctrl);
                desk->refresh();
            } else {
                printf("??\r\n");
            }
        } else {
            desk->on_inner_keybd_event(key);
            desk->refresh();
        }
        pthread_mutex_unlock(&msg_mutex);
        if (key == 'q') break;
        void start_app(char *fullpath, char *workdir);
        if (key == 'a') start_app("/home/szx/TextWindows/app/build/test", "/home/szx/TextWindows/app/build");
    }
}

extern screen_t *screen;

void screen_refresh(int left, int top, int right, int bottom)
{
    screen->refresh(left, top, right, bottom);
}

void window_create(window_t* win)
{
}

void window_destroy(window_t* win)
{
}

void window_set_pos(window_t* win)
{
}

void window_set_size(window_t* win)
{
}

void window_set_style(window_t* win)
{
}

void window_set_text(window_t* win)
{
}

void window_sync_pixels(window_t* win)
{
}

void window_refresh(window_t *win)
{   
}

void window_set_focused(window_t *win, int fx, int fy)
{
}

struct app_arg_t {
    pid_t pid;
    int pipe_write;
    int pipe_read;
};

static char *pipe_readln(int pd, char *s, int size)
{
    char *p = s;
    for (;;) {
        if (1 != read(pd, p, 1)) return NULL;
        if (*p == '\n' || *p == 0) {
            *p = 0;
            return s;
        }
        p++;
        size--;
    }
    return NULL;
}

class dup_win_t : public window_t {
    public:
        int fx, fy;
        dup_win_t(window_t *parent):window_t(parent) {
            set_can_focuse();
            fx = -1;
            fy = -1;
        };
        bool get_focus_pos(int &x, int &y) {
            x = fx;
            y = fy;
            return is_focused();
        }
};

void *app_thread(void *arg)
{
    app_arg_t *app_arg = (app_arg_t *)arg;
    char sbuf[256];
    char sptr[1024];
    int n;
    for (;;) {
        dup_win_t *win = NULL;
        int x, y;
        int left, top, width, height, style;
        int size;
        int color, bgcolor, bold;
        u_int32_t v;

        if (pipe_readln(app_arg->pipe_read, sptr, sizeof(sptr)) == NULL) {
            kill(app_arg->pid, 9);
            break;
        }
        FILE *f = fopen("/home/szx/TextWindows/build/log", "a");
        if (!f) f = fopen("/home/szx/TextWindows/build/log", "w");
        fprintf(f, "%s\n", sptr);
        fclose(f);
        pthread_mutex_lock(&msg_mutex);
        if (sscanf(sptr, "window_create %p", &win) == 1) {
            dup_win_t *w = new dup_win_t(desk);
            w->dupwin = win;
            w->set_size(40, 10);
            w->pid = app_arg->pid;
            w->pipe_write = app_arg->pipe_write;
            n = sprintf(sbuf, "set syswin %p %p\n", win, w);
            write(app_arg->pipe_write, sbuf, n);
        } else if (sscanf(sptr, "window_destroy %p", &win) == 1) {
            if (win) delete win;
        } else if (sscanf(sptr, "window_set_pos %p %d %d", &win, &left, &top) == 3) {
            if (win) {
                win->set_pos(left, top);
            }
        } else if (sscanf(sptr, "window_set_size %p %d %d", &win, &width, &height) == 3) {
            if (win) {
                win->set_size(width, height);
            }
        } else if (sscanf(sptr, "window_set_style %p %d", &win, &style) == 2) {
            if (win) {
                win->set_style((win_style_t)style);
            }
        } else if (sscanf(sptr, "window_set_text %p", &win) == 1) {
            if (win) {
                win->set_text(strstr(sptr + 16, " ") + 1);
            }
        } else if (sscanf(sptr, "window_set_focused %p %d %d", &win, &x, &y) == 3) {
            if (win) {
                win->fx = x;
                win->fy = y;
                desk->refresh();
            }
        } else if (sscanf(sptr, "window_do_refresh %p", &win) == 1) {
            if (win) {
                desk->refresh();
            }
        } else if (sscanf(sptr, "window_refresh %p %d", &win, &size) == 2) {
            if (win) {
                u_int8_t *ptr = (u_int8_t *)win->pixels;
                size = size * sizeof(pixel_t);
                u_int8_t *p = new u_int8_t[size];
                int i, n;
                for (i = 0;i < size;i += n) {
                    n = read(app_arg->pipe_read, p + i, size - i);
                    if (n <= 0) break;
                }
                if (i == size) {
                    int csize = win->width * win->height * sizeof(pixel_t);
                    if (csize == size) {
                        memcpy(ptr, p, size);
                    }
                }
                delete p;
                win->refresh();
                desk->refresh();
            }
        } else if (sscanf(sptr, "screen_refresh %d %d %d %d", &left, &top, &width, &height) == 4) {
            screen_refresh(left, top, width, height);
        } else {
            printf("%s\r\n", sptr);
        }
        pthread_mutex_unlock(&msg_mutex);
    }
    int status;
    waitpid(app_arg->pid, &status, 0);
    printf("app exit %d\n", WEXITSTATUS(status));
    close(app_arg->pipe_read);
    close(app_arg->pipe_write);
    window_t *t, *next;
    for (t = (window_t *)desk->child.dl_first_node();t;t = next) {
        next = (window_t *)t->next;
        if (t->pid == app_arg->pid) {
            delete t;
        }
    }
    delete app_arg;
    pthread_mutex_lock(&msg_mutex);
    desk->refresh();
    pthread_mutex_unlock(&msg_mutex);
    return NULL;
}

void start_app(char *fullpath, char *workdir)
{
    int pipe_ptoc[2];
    int pipe_ctop[2];

    if (pipe(pipe_ptoc) < 0) return;
    if (pipe(pipe_ctop) < 0) return;

    pid_t pid = fork (); 

    if (pid == 0) {/* child */
        close(pipe_ctop[0]);
        close(pipe_ptoc[1]);
        if (dup2(pipe_ptoc[0],STDIN_FILENO) != STDIN_FILENO) {
            printf("fail %d\n", __LINE__);
            exit(-2);
        }
        if (dup2(pipe_ctop[1],STDOUT_FILENO) != STDOUT_FILENO) {
            printf("fail %d\n", __LINE__);
            exit(-1);
        }
        close(pipe_ctop[1]);
        close(pipe_ptoc[0]);
        chdir(workdir);
        execl(fullpath, "", NULL);
        exit (126);
    } else if (pid > 0) {/* parent */
        close(pipe_ctop[1]);
        close(pipe_ptoc[0]);
        app_arg_t *app_arg = new app_arg_t();
        app_arg->pid = pid;
        app_arg->pipe_read = pipe_ctop[0];
        app_arg->pipe_write = pipe_ptoc[1];
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, app_thread, (void *)app_arg);
    } else {
        /* fail */
        return;
    }
}