#include "windows.h"

#include <stdarg.h>
#include <unistd.h>

extern void set_cur_win(void* win);
extern void screen_refresh(int left, int top, int right, int bottom);
extern void start_app(char *fullpath, char *workdir);
extern void window_create(window_t* win);
extern void window_destroy(window_t* win);
extern void window_set_pos(window_t* win);
extern void window_set_size(window_t* win);
extern void window_set_style(window_t* win);
extern void window_set_text(window_t* win);
extern void window_sync_pixels(window_t* win);
extern void window_refresh(window_t *win);
extern void window_set_focused(window_t *win, int fx, int fy);

#define IS_WIN_IN_SYS_SIDE() (this->pipe_write != 0)
#define DUP_MSG(...) {\
    char sbuf[1024];\
    char *p = sbuf;\
    int n, len = 0;\
    n = sprintf(p, "%s ", __FUNCTION__);\
    p += n;\
    len += n;\
    n = sprintf(p, "%p ", this->dupwin);\
    p += n;\
    len += n;\
    n = sprintf(p, ##__VA_ARGS__);\
    p += n;\
    len += n;\
    n = sprintf(p, "\n");\
    len += n;\
    n = write(this->pipe_write, sbuf, len);\
}

window_t::window_t(window_t* parent) {
    
    dupwin = NULL;
    pid = 0;
    pipe_write = 0;
    this->parent = NULL;
    
    actived_child = NULL;
    focused_win = NULL;
    actived = false;
    focused = false;
    can_focuse = false;
    left = 0;
    top = 0;
    width = 1;
    height = 1;
    m_ox = 0;
    m_oy = 0;
    pixels = NULL;
    old_pixels = NULL;
    style = WIN_STYLE_SIZEABLE;
    drag_type = DRAG_NONE;
    align = ALIGN_NONE;
    top_most = false;
    text = "UNNAMED";
    if (parent) desk = parent->desk; else desk = NULL;
    set_parent(parent);
}

window_t::~window_t()
{
    if (desk) {
        if (top_most == false) desk->child.dl_del_node(this);
        if (desk->actived_child == this) desk->actived_child = NULL;
        if (desk->focused_win == this) desk->focused_win = NULL;
        if (desk->mouse_win == this) desk->mouse_win = NULL;
    }
}

bool window_t::is_focused() {
    if (can_focuse) return focused;
    return false;
}

void window_t::set_focuded() {
    if (can_focuse) {
        focused = true;
        window_t *t = top_win();
        if (t->focused_win && t->focused_win != this) {
            t->focused_win->focused = false;
        }
        t->focused_win = this;
        int fx, fy;
        int tx, ty;
        get_focus_pos(fx, fy);
        get_screen_pos(tx, ty);
        window_set_focused(t, tx - t->get_left() + fx, ty - t->get_top() + fy);
    }
}

bool window_t::is_can_focuse() {
    return can_focuse;
}

void window_t::set_can_focuse() {
    can_focuse = true;
}

bool window_t::get_focus_pos(int& x, int& y) {
    return false;
}

void window_t::set_parent(window_t* parent) {
    if (this->parent) {
        if (top_most == false) this->parent->child.dl_del_node(this);
    }
    if (parent) {
        this->parent = parent;
        if (top_most == false) parent->child.dl_apppend_node(this);
        window_destroy(this);
    } else {
        window_create(this);
    }
}

bool window_t::is_top_most() {
    return top_most;
}

void window_t::set_top_most(bool top_most) {
    if (this->top_most == top_most) return;
}

window_t* window_t::get_parent() const {
    return parent;
}

void window_t::set_alignment(alignment_t align)
{
    this->align = align;
    if (parent == NULL) return;
    switch (align) {
        case ALIGN_CLIENT:
            set_pos(0, 0);
            set_size(parent->clientrect.width, parent->clientrect.height);
            break;
        case ALIGN_LEFT:
            set_pos(0, 0);
            set_size(width, parent->clientrect.height);
            break;
        case ALIGN_TOP:
            set_pos(0, 0);
            set_size(parent->clientrect.width, height);
            break;
        case ALIGN_RIGHT:
            set_size(width, parent->height);
            set_pos(parent->clientrect.width - width, 0);
            break;
        case ALIGN_BOTTOM:
            set_size(parent->clientrect.width, height);
            set_pos(0, parent->clientrect.height - height);
            break;
        default:
            break;
    }
}

alignment_t window_t::get_alignment()
{
    return align;
}

void window_t::set_size(int width, int height) {
    if (!pixels || !old_pixels || (this->width * this->height != width * height)) {
        if (pixels) {
            delete pixels;
        }
        if (old_pixels) {
            delete old_pixels;
        }
        int new_size = width * height;
        if (new_size > 0) {
            pixels = new pixel_t[width * height];
            old_pixels = new pixel_t[width * height];
            old_pixels[0].v = 0;
        } else {
            pixels = NULL;
            old_pixels = NULL;
        }
    }
    if (this->width == width &&  this->height == height) return;
    this->width = width;
    this->height = height;
    set_style(style);

    on_resize(width, height);
    for (window_t *w = (window_t *)child.dl_first_node();w;w = (window_t *)w->next) {
        w->set_alignment(w->get_alignment());
        w->refresh();
    }

    window_set_size(this);
}

int window_t::get_left() {
    return left;
}

int window_t::get_top() {
    return top;
}

int window_t::get_width() {
    return width;
}

int window_t::get_height() {
    return height;
}

void window_t::set_pos(int left, int top) {
    this->left = left;
    this->top = top;
    window_set_pos(this);
}

void window_t::get_screen_pos(int& x, int& y) {
    int tx = get_left(), ty = get_top();
    window_t* p;
    for (p = get_parent(); p; p = p->get_parent()) {
        tx = tx + p->clientrect.left + p->get_left();
        ty = ty + p->clientrect.top + p->get_top();
    }
    x = tx;
    y = ty;
}

window_t *window_t::top_win()
{
    window_t *w;
    for (w = this;w && w->parent && w->parent != desk;w = w->parent);
    return w;
}

pixel_t window_t::get_pixel(int x, int y) {
    pixel_t p;
    p.v = 0;
    if ((x < 0 || x >= width) || (y < 0 || y >= height)) {
        return p;
    }

    return pixels[y * width + x];
}

pixel_t* window_t::get_pixel_ptr(int x, int y) {
    if ((x < 0 || x >= width) || (y < 0 || y >= height)) {
        return NULL;
    }

    return &pixels[y * width + x];
}

bool window_t::in_clientrect(int x, int y) {
    if ((x >= clientrect.left) && (x < clientrect.left + clientrect.width) &&
        (y >= clientrect.top) && y < (clientrect.top + clientrect.height)) {
        return true;
    }
    return false;
}

void window_t::set_pixel(int x,
                         int y,
                         color_t color,
                         bgcolor_t bgcolor,
                         bool bold,
                         u_int32_t v) {
    pixel_t* pixel;
    if ((x < 0 || x >= width) || (y < 0 || y >= height)) {
        return;
    }
    pixel = &pixels[y * width + x];
    pixel->color = color;
    pixel->bgcolor = bgcolor;
    pixel->bold = bold;
    pixel->v = v;
    int tx, ty;
    this->get_screen_pos(tx, ty);
    if (text.c_str()[0] == '1' &&  x == width - 1 && y == 0) {
//        pixel->v = 'f';
    }
}

void window_t::set_text(string text) {
    this->text = text;
    window_set_text(this);
}

string window_t::get_text() {
    return text;
}

window_t* window_t::get_win_on_pos(int x, int y) {
    if ((x < 0 || x >= width) || (y < 0 || y >= height)) {
        return NULL;
    }

    if (in_clientrect(x, y)) {
        x = x - clientrect.left;
        y = y - clientrect.top;
        for (window_t* t = (window_t*)child.dl_first_node(); t;
             t = (window_t*)t->next) {
            if ((x >= t->left) && (x < t->left + t->width) && 
                (y >= t->top) && (y < t->top + t->height)) {
                return t->get_win_on_pos(x - t->left, y - t->top);
            }
        }
    }

    return this;
}

void window_t::refresh() {
    if (pixels == NULL) return;
    on_inner_paint();
    window_t *desk;
    if (this->parent == NULL) {
        for (int y = 0;y < height;y++) {
            for (int x = 0;x < width;x++) {
                window_t *win = get_win_on_pos(x, y);
                if (win != this) {
                    int tx, ty;
                    win->get_screen_pos(tx, ty);
                    if (x == width - 1) {
                        pixel_t np = win->get_pixel(x + 1 + get_left() - tx, y + get_top() - ty);
                        if (np.v == (u_int32_t)-1) {
                            *get_pixel_ptr(x, y) = pixel_t();
                        } else {
                            *get_pixel_ptr(x, y) = win->get_pixel(x + get_left() - tx, y + get_top() - ty);
                        }
                    } else {
                        *get_pixel_ptr(x, y) = win->get_pixel(x + get_left() - tx, y + get_top() - ty);
                    }
                } else {
                    pixel_t pixel;
                    pixel_t *p = get_pixel_ptr(x, y);
                    if (*p != pixel) *p = pixel;
                }
            }
        }
        window_refresh(this);
        screen_refresh(0, 0, get_width() - 1, get_height() - 1);
    } else {
 //       for (desk = this;desk->parent != NULL;desk = desk->parent);
 //       desk->refresh();
    }
}

void window_t::set_actived(bool actived) {
    window_t *p;
    if (this->parent != this->desk) return;
    if (actived && desk && desk->actived_child == this) return;
    if (desk && desk->actived_child) {
        if (desk->actived_child->actived == true) {
            desk->actived_child->actived = false;
            desk->actived_child->refresh();
        }
        desk->actived_child = NULL;
    }
    if (this->actived != actived) {
        this->actived = actived;
        if (actived && desk) desk->actived_child = this;
        refresh();
    }
}

void window_t::on_inner_paint() {
    if (style != WIN_STYLE_NONE) {
        draw_vertical_line(0, 1, height - 2);
        draw_vertical_line(width - 1, 1, height - 2);
        draw_horizontal_line(1, width - 2, 0);
        draw_horizontal_line(1, width - 2, height - 1);
        draw_lefttop_angle(0, 0);
        draw_rightttop_angle(width - 1, 0);

        if (style == WIN_STYLE_SIZEABLE) {
            color_t color;
            if (actived)
                color = COLOR_YELLOW;
            else
                color = COLOR_WHITE;
            for (int x = 1; x < width - 1; x++) {
                set_pixel(x, 1, color, BGCOLOR_BLACK, 0, ' ');
            }

            printf(1, 1, color, BGCOLOR_BLACK, actived, "%s", (char*)text.c_str());
            draw_horizontal_line(1, width - 2, 2);
            draw_left_edge(0, 2);
            draw_right_edge(width - 1, 2);
        }

        draw_leftbottom_angle(0, height - 1);
        draw_righttbottom_angle(width - 1, height - 1);
    }
    on_paint();
}

void window_t::on_inner_keybd_event(int key) {
    if (IS_WIN_IN_SYS_SIDE()) {/* in system side */
        DUP_MSG("%d", key);
        return;
    }
    if (actived_child) actived_child->on_inner_keybd_event(key);
    else if (focused_win) focused_win->on_inner_keybd_event(key);
    else {
        if (focused_win)
            focused_win->on_inner_keybd_event(key);
        else {
            on_keybd_event(key);
        }

        refresh();
    }
}

void window_t::on_inner_mouse_up(int key, int x, int y) {
    if (actived_child) {
        actived_child->on_inner_mouse_up(key, x, y);
        return;
    } else {
        window_t *top_mouse_win = top_win()->mouse_win;
        if (top_mouse_win) {
            if (top_mouse_win != this) {
                top_mouse_win->on_inner_mouse_up(key, x, y);
                return;
            }
        } else {
            return;
        }
    }
    if (IS_WIN_IN_SYS_SIDE()) {/* in system side */
        DUP_MSG("%d %d %d", key, x, y);
        return;
    }
    int tx, ty;
    get_screen_pos(tx, ty);
    x -= tx;
    y -= ty;

    drag_type = DRAG_NONE;
    on_mouse_up(key, x, y);
    refresh();
}

void window_t::on_inner_mouse_down(int key, int x, int y, u_int8_t ctrl) {
    top_win()->set_actived(true);
    int tx, ty;
    get_screen_pos(tx, ty);
    window_t* win = get_win_on_pos(x - tx, y - ty);
    if (win != this) {
        for (window_t* p = win; p->get_parent(); p = p->get_parent()) {
            if (p->top_most == false) {
                p->get_parent()->child.dl_bring_first(p);
            }
        }

        win->on_inner_mouse_down(key, x, y, ctrl);
    } else if (win == this) {
        if (desk == this) {
            if (actived_child) {
                actived_child->set_actived(false);
                return;
            }
        }
        top_win()->mouse_win = this;
        set_focuded();
        if (IS_WIN_IN_SYS_SIDE()) {/* in system side */
            DUP_MSG("%d %d %d %d", key, x, y, ctrl);
            return;
        }
        int tx, ty;
        get_screen_pos(tx, ty);
        x = x - tx;
        y = y - ty;
        m_ox = x;
        m_oy = y;
        if (style == WIN_STYLE_SIZEABLE && key == MOUSE_LEFT) {
            if (x == 0 && y == 0) {
                drag_type = DRAG_SIZE_TOP_LEFT;
            } else if (x == width - 1 && y == 0) {
                drag_type = DRAG_SIZE_TOP_RIGHT;
            } else if (x == 0 && y == height - 1) {
                drag_type = DRAG_SIZE_BOTTOM_LEFT;
            } else if (x == width - 1 && y == height - 1) {
                drag_type = DRAG_SIZE_BOTTOM_RIGHT;
            } else if (x == 0) {
                drag_type = DRAG_SIZE_LEFT;
            } else if (x == width - 1) {
                drag_type = DRAG_SIZE_RIGHT;
            } else if (y == 0) {
                drag_type = DRAG_SIZE_TOP;
            } else if (y == height - 1) {
                drag_type = DRAG_SIZE_BOTTOM;
            } else if (x > 0 && x < width - 1 && (y == 1 || y == 2)) {
                drag_type = DRAG_POS;
            }
        }

        on_mouse_down(key, x, y, ctrl);
        refresh();
    }
}

void window_t::on_inner_mouse_drag(int key, int x, int y, u_int8_t ctrl) {
    if (actived_child) {
        actived_child->on_inner_mouse_drag(key, x, y, ctrl);
        return;
    } else {
        window_t *top_mouse_win = top_win()->mouse_win;
        if (top_mouse_win) {
            if (top_mouse_win != this) {
                top_mouse_win->on_inner_mouse_drag(key, x, y, ctrl);
                return;
            }
        } else {
            return;
        }
    }
    if (IS_WIN_IN_SYS_SIDE()) {/* in system side */
        DUP_MSG("%d %d %d %d", key, x, y, ctrl);
        return;
    }

    const int min_width = 8;
    const int min_height = 3;
    int left = this->left, top = this->top;
    int width = this->width, height = this->height;
    int tx, ty;
    get_screen_pos(tx, ty);
    x -= tx;
    y -= ty;
    int dx = x - m_ox, dy = y - m_oy;
    bool do_refresh = false;

    if (key != MOUSE_LEFT) return;
    if (drag_type == DRAG_POS) {
        left += dx;
        top += dy;
    }

    if (drag_type == DRAG_SIZE_LEFT || drag_type == DRAG_SIZE_TOP_LEFT ||
        drag_type == DRAG_SIZE_BOTTOM_LEFT) {
        if (width - dx < min_width) dx = width - min_width;
        width -= dx;
        left += dx;
    }

    if (drag_type == DRAG_SIZE_RIGHT || drag_type == DRAG_SIZE_TOP_RIGHT ||
        drag_type == DRAG_SIZE_BOTTOM_RIGHT) {
        if (width + dx < min_width) dx = min_width - width;
        width += dx;
        m_ox = m_ox + dx;
    }

    if (drag_type == DRAG_SIZE_TOP || drag_type == DRAG_SIZE_TOP_LEFT ||
        drag_type == DRAG_SIZE_TOP_RIGHT) {
        if (height - dy < min_height) dy = height - min_height;
        top += dy;
        height -= dy;
    }

    if (drag_type == DRAG_SIZE_BOTTOM || drag_type == DRAG_SIZE_BOTTOM_LEFT ||
        drag_type == DRAG_SIZE_BOTTOM_RIGHT) {
        if (height + dy < min_height) dy = min_height - height;
        height += dy;
        m_oy = m_oy + dy;
    }

    if ((left != this->left) || (top != this->top)) {
        set_pos(left, top);
    }

    if ((width != this->width) || (height != this->height)) {
        set_size(width, height);
        do_refresh = true;
    }

    on_mouse_drag(key, x, y, ctrl);

    if (do_refresh) refresh();
}

void window_t::on_inner_mouse_wheel(int key, int x, int y, u_int8_t ctrl) {
    if (IS_WIN_IN_SYS_SIDE()) {/* in system side */
        DUP_MSG("%d %d %d %d", key, x, y, ctrl);
        return;
    }
}

#define UNICODE(x) (0xe20000 + (x))
#define U_TABLE(x) (UNICODE(x) + 0x6f80)

void window_t::draw_vertical_line(int x,
                                  int y_begin,
                                  int y_end,
                                  color_t color,
                                  bgcolor_t bgcolor) {
    int b;
    int e;
    if (y_begin <= y_end) {
        b = y_begin;
        e = y_end;
    } else {
        b = y_end;
        e = y_begin;
    }
    for (int y = b; y <= e; y++) {
        set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2502));
    }
}

void window_t::draw_horizontal_line(int x_begin,
                                    int x_end,
                                    int y,
                                    color_t color,
                                    bgcolor_t bgcolor) {
    int b;
    int e;
    if (x_begin <= x_end) {
        b = x_begin;
        e = x_end;
    } else {
        b = x_end;
        e = x_begin;
    }
    for (int x = b; x <= e; x++) {
        set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2500));
    }
}

void window_t::draw_lefttop_angle(int x,
                                  int y,
                                  color_t color,
                                  bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x250c));
}

void window_t::draw_rightttop_angle(int x,
                                    int y,
                                    color_t color,
                                    bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2510));
}

void window_t::draw_leftbottom_angle(int x,
                                     int y,
                                     color_t color,
                                     bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2514));
}

void window_t::draw_righttbottom_angle(int x,
                                       int y,
                                       color_t color,
                                       bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2518));
}

void window_t::draw_cross(int x, int y, color_t color, bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x253c));
}

void window_t::draw_left_edge(int x, int y, color_t color, bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x251c));
}

void window_t::draw_right_edge(int x, int y, color_t color, bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2524));
}

void window_t::draw_top_edge(int x, int y, color_t color, bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x252c));
}

void window_t::draw_bottom_edge(int x,
                                int y,
                                color_t color,
                                bgcolor_t bgcolor) {
    set_pixel(x, y, color, bgcolor, 0, U_TABLE(0x2534));
}

void window_t::set_style(win_style_t style) {
    this->style = style;
    switch (style) {
        case WIN_STYLE_SIZEABLE:
            clientrect.left = 1;
            clientrect.top = 3;
            clientrect.width = width - 2;
            clientrect.height = height - 4;
            break;
        case WIN_STYLE_PANNEL:
            clientrect.left = 1;
            clientrect.top = 1;
            clientrect.width = width - 2;
            clientrect.height = height - 2;
            break;
        default:
        case WIN_STYLE_NONE:
            clientrect.left = 0;
            clientrect.top = 0;
            clientrect.width = width;
            clientrect.height = height;
            break;
    }
    window_set_style(this);
}

win_style_t window_t::get_style()
{
    return style;
}

void window_t::printf(int x,
                      int y,
                      color_t color,
                      bgcolor_t bgcolor,
                      bool bold,
                      char* fmt,
                      ...) {
    unsigned char buf[256];
    unsigned char* p = buf;
    va_list ap;
    int val;
    char* s = 0;
    pixel_t* pixel;

    va_start(ap, fmt);
    while (*fmt) {
        switch (*fmt) {
            case '%':
                fmt++;
                switch (*fmt) {
                    case 'd':
                        val = va_arg(ap, int);
                        p += sprintf((char*)p, "%d", val);
                        break;
                    case 'x':
                        val = va_arg(ap, int);
                        p += sprintf((char*)p, "%x", val);
                        break;
                    case 's':
                        s = va_arg(ap, char*);
                        p += sprintf((char*)p, "%s", s);
                        break;
                    case 'p':
                        s = va_arg(ap, char*);
                        p += sprintf((char*)p, "%p", s);
                        break;
                    case 'c':
                        *p = va_arg(ap, int);
                        p++;
                        break;
                    default:
                        break;
                }
                break;

            default:
                *p = *fmt;
                p++;
                break;
        }

        fmt++;
    }
    *p = 0;
    va_end(ap);

    p = buf;
    while (*p) {
        if (*p == '\n' || *p == '\r') {
            x = width - 1;
        } else if (*p == 0xe2) {
            pixel = get_pixel_ptr(x, y);
            if (pixel) {
                pixel->v = 0xe20000 | (p[1] * 256) | p[2];
                pixel->bold = bold;
                pixel->color = color;
                pixel->bgcolor = bgcolor;
                p = p + 2;
            }
        } else {
            pixel = get_pixel_ptr(x, y);
            if (pixel) {
                pixel->v = *p;
                pixel->bold = bold;
                pixel->color = color;
                pixel->bgcolor = bgcolor;
            }
        }
        p++;
        x++;
        if (x >= width) {
            x = 0;
            y++;
            if (y >= height) {
                break;
            }
        }
    }
}
/* button_t begin */
button_t::button_t(window_t* parent) : window_t(parent) {
    set_style(WIN_STYLE_PANNEL);
    set_size(8, 3);
}

void button_t::set_on_click(void (window_t::*on_click)(window_t*)) {
    this->on_click = on_click;
}

void button_t::on_paint() {
    int len = get_text().length();
    int x;
    for (x = 1; x < get_width() - 1; x++) {
        set_pixel(x, clientrect.height / 2 + 1, COLOR_WHITE, BGCOLOR_BLACK,
                  false, ' ');
    }

    x = (get_width() - len) / 2;
    if (x < 1) x = 1;
    if (mouse_down) {
        for (int i = 0; i < get_width(); i++) {
            pixel_t* pixel = get_pixel_ptr(i, 0);
            pixel->bold = false;
            pixel = get_pixel_ptr(i, get_height() - 1);
            pixel->bold = true;
        }
        for (int i = 0; i < get_height(); i++) {
            pixel_t* pixel = get_pixel_ptr(0, i);
            pixel->bold = false;
            pixel = get_pixel_ptr(get_width() - 1, i);
            pixel->bold = true;
        }
    } else {
        for (int i = 0; i < get_width(); i++) {
            pixel_t* pixel = get_pixel_ptr(i, 0);
            pixel->bold = true;
            pixel = get_pixel_ptr(i, get_height() - 1);
            pixel->bold = false;
        }
        for (int i = 0; i < get_height(); i++) {
            pixel_t* pixel = get_pixel_ptr(0, i);
            pixel->bold = true;
            pixel = get_pixel_ptr(get_width() - 1, i);
            pixel->bold = false;
        }
    }
    printf(x, clientrect.height / 2 + 1, COLOR_WHITE, BGCOLOR_BLACK,
           !mouse_down, "%s", get_text().c_str());
}

void button_t::on_mouse_down(int key, int x, int y, u_int8_t ctrl) {
    if (key == MOUSE_LEFT) mouse_down = true;
}

void button_t::on_mouse_up(int key, int x, int y) {
    if (mouse_down && x >= 0 && x < get_width() && y >= 0 && y < get_height()) {
        if (on_click) {
            (get_parent()->*on_click)(this);
        }
    }
    mouse_down = false;
}

void button_t::on_mouse_drag(int key, int x, int y, u_int8_t ctrl) {
    bool stored_down = mouse_down;
    if (x < 0 || x >= get_width() || y < 0 || y >= get_height()) {
        mouse_down = false;
    } else {
        mouse_down = true;
    }
    if (stored_down != mouse_down) refresh();
}
/* button_t end */

/* track_v_t begin */
track_v_t::track_v_t(window_t *parent): window_t(parent) {
    set_style(WIN_STYLE_NONE);
    set_size(1, 10);
}

int track_v_t::getlen() {
    return len;
}

void track_v_t::setlen(int len) {
    this->len = len;
}

int track_v_t::getpos() {
    return pos;
}

void track_v_t::setpos(int pos) {
    this->pos = pos;
}

void track_v_t::on_paint() {
    set_pixel(0, 0, COLOR_WHITE, BGCOLOR_BLACK, false, UNICODE(0x88a7));
    for (int i = 1;i < get_height() - 1;i++) {
        set_pixel(0, i, COLOR_WHITE, BGCOLOR_BLACK, false, U_TABLE(0x2502));
    }
    set_pixel(0, get_height() - 1, COLOR_WHITE, BGCOLOR_BLACK, false, UNICODE(0x88a8));
}

void track_v_t::on_mouse_down(int key, int x, int y, u_int8_t ctrl) {

}

void track_v_t::on_mouse_drag(int key, int x, int y, u_int8_t ctrl) {

}

/* track_v_t end*/

/* text_t begin */
struct char_t {
    u_int32_t value;
    color_t color;
    bgcolor_t bgcolor;
    bool bold;
    char_t() {
        value = 0;
        color = COLOR_WHITE;
        bgcolor = BGCOLOR_BLACK;
        bold = false;
    };
};
class row_t : public dlnode_t {
   private:
    row_t* right;
    row_t* left;
#define MAX_SEC_LEN 10
   public:
    char_t v[MAX_SEC_LEN + 1];

   private:
    int sec_len() {
        int n;
        for (n = 0; v[n].value != 0; n++)
            ;
        return n;
    }
    void vmemset(char_t *d, char_t &v, int count) {
        for (int i = 0;i < count;i++) {
            d[i] = v;
        }
    }
    void vcopy(char_t* dst, char_t* src) {
        while (src->value) {
            *dst = *src;
            dst++;
            src++;
        }
        dst->value = 0;
    }
    row_t* get_insert_pos(int& pos) {
        row_t* sec = this;
        int ins = 0;
        for (; pos;) {
            if (sec->v[ins].value == 0) {
                if (sec->right) {
                    sec = sec->right;
                    ins = 0;
                } else {
                    break;
                }
            }
            ins++;
            pos--;
        }
        pos = ins;
        return sec;
    }

   public:
    int len() {
        int ret = 0;
        for (row_t* sec = this; sec; sec = sec->right) {
            ret += sec->sec_len();
        }
        return ret;
    }
    char_t get_value(int pos) {
        row_t* sec = get_insert_pos(pos);
        char_t v;
        if (sec->v[pos].value == 0) {
            if (sec->right) {
                sec = sec->right;
                pos = 0;
            } else {
                return v;
            }
        }

        return sec->v[pos];
    };

    void insert(u_int32_t v, int pos) {
        row_t* sec = get_insert_pos(pos);

        if (pos == MAX_SEC_LEN || sec->v[pos].value != 0) {
            int n = sec->sec_len();
            if (n < MAX_SEC_LEN) {
                for (int i = n + 1; i > pos; i--) {
                    sec->v[i] = sec->v[i - 1];
                }
            } else {
                row_t* n_sec = new row_t();
                n_sec->right = sec->right;
                if (sec->right) sec->right->left = n_sec;
                n_sec->left = sec;
                sec->right = n_sec;
                if (pos == MAX_SEC_LEN) {
                    sec = n_sec;
                    pos = 0;
                } else {
                    vcopy(n_sec->v, sec->v + pos);
                    sec->v[pos + 1].value = 0;
                }
            }
        } else {
            sec->v[pos + 1].value = 0;
        }
        sec->v[pos].value = v;
    };

    void inser_new_row(dllist_t& list, int pos) {
        row_t* sec = get_insert_pos(pos);
        row_t* row = sec;
        while (row->left)
            row = row->left;
        row_t* n_sec;
        if (sec->v[pos].value != 0 || !sec->right) {
            n_sec = new row_t();
            vcopy(n_sec->v, sec->v + pos);
            sec->v[pos].value = 0;
            n_sec->right = sec->right;
            if (sec->right) {
                sec->right->left = n_sec;
                sec->right = NULL;
            }
        } else {
            n_sec = sec->right;
            sec->right = NULL;
            n_sec->left = NULL;
        }

        list.dl_insert_node(row, n_sec);
    };

    bool del(int pos) {
        row_t* sec = get_insert_pos(pos);
        while (sec->v[pos].value == 0) {
            if (sec->right) {
                sec = sec->right;
                pos = 0;
            } else {
                return false;
            }
        }
        vcopy(sec->v + pos, sec->v + pos + 1);
        if (sec->v[0].value == 0) {
            if (sec->left) {
                sec->left->right = sec->right;
                if (sec->right) {
                    sec->right->left = sec->left;
                }
                delete sec;
            } else if (sec->right) {
                row_t* right = sec->right;
                vcopy(sec->v, right->v);
                sec->right = right->right;
                if (right->right) right->right->left = sec;
                delete right;
            }
        }
        return true;
    };

    void join(row_t* row) {
        row_t* sec = this;
        while (sec->right)
            sec = sec->right;
        if (sec->v[0].value == 0) { /* this must be only one in this row*/
            vcopy(sec->v, row->v);
            sec->right = row->right;
            if (row->right) row->right->left = sec;
            delete row;
        } else {
            sec->right = row;
            row->left = sec;
        }
    };

    row_t* get_right() { return right; }
};

text_t::text_t(window_t* parent) : window_t(parent)/*, track_v(this)*/ {
    set_style(WIN_STYLE_PANNEL);
    set_can_focuse();
    row_start = 0;
    ins_pos = 0;
    ins_row = 0;
    column_start = 0;
    row_t* row = new (row_t);
    rowlist.dl_apppend_node(row);
}

text_t::~text_t() {
    
}

void text_t::on_paint() {
    char_t v;
    row_t* row = (row_t*)rowlist.dl_first_node();
    int col, ro;
    for (int i = 0; i < row_start; i++) {
        row = (row_t*)row->next;
    }
    for (int y = 0; y < clientrect.height; y++) {
        ro = y + row_start;
        for (int x = 0; x < clientrect.width; x++) {
            if (row) {
                col = x + column_start;
                v = row->get_value(col);
                if (v.value == 0) v.value = ' ';
                else {
                    if ((ro > sel_r_begin && ro < sel_r_end) || (ro > sel_r_end && ro < sel_r_begin)) {
                        v.color = COLOR_WHITE;
                        v.bgcolor = BGCOLOR_BLUE;
                    } else if (ro == sel_r_begin || ro == sel_r_end) {
                        if (sel_r_begin == sel_r_end) {
                            if ((sel_c_begin != sel_c_end) && 
                                ((col >= sel_c_begin && col < sel_c_end) || (col >= sel_c_end && col < sel_c_begin))) {
                                v.color = COLOR_WHITE;
                                v.bgcolor = BGCOLOR_BLUE;
                            }
                        } else if (sel_r_begin < sel_r_end) {
                            if ((ro == sel_r_begin && col >= sel_c_begin) || (ro == sel_r_end && col < sel_c_end)) {
                                v.color = COLOR_WHITE;
                                v.bgcolor = BGCOLOR_BLUE;
                            }
                        } else {
                            if ((ro == sel_r_end && col >= sel_c_end) || (ro == sel_r_begin && col < sel_c_begin)) {
                                v.color = COLOR_WHITE;
                                v.bgcolor = BGCOLOR_BLUE;
                            }
                        }
                    }
                }
                if (x == 0 && (v.value == (u_int32_t)-1)) v.value = ' ';
                if (x == clientrect.width - 1) {
                    if ((v.value & 0xf00000) == 0xe00000) v.value = ' ';
                }
            } else {
                v.value = ' ';
            }
            set_pixel(x + clientrect.left, y + clientrect.top, v.color,
                      v.bgcolor, false, v.value);
        }
        if (row) row = (row_t*)row->next;
    }
}

void text_t::on_keybd_event(int key) {
    row_t* row;
    int i = 0;
    for (row = (row_t*)rowlist.dl_first_node(); row && row->next && i < ins_row;row = (row_t*)row->next)i++;

    switch (key) {
        case KEY_LEFT:
            if (ins_pos > row->len()) ins_pos = row->len();
            if (ins_pos > 0) {
                ins_pos--;
                if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos--;
            } else if (ins_row > 0) {
                ins_row--;
                row = (row_t*)row->prev;
                ins_pos = row->len();
            }
            break;
        case KEY_RIGHT:
            if (ins_pos < row->len()) {
                ins_pos++;
                if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos++;
            } else {
                if (ins_row < (int)(rowlist.count - 1)) {
                    ins_row++;
                    ins_pos = 0;
                }
            }
            break;
        case KEY_UP:
            if (ins_row > 0) {
                ins_row--;
                row = (row_t*)row->prev;
                if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos--;
            }
            break;
        case KEY_DOWN:
            if (ins_row < (int)(rowlist.count - 1)) {
                ins_row++;
                row = (row_t*)row->next;
                if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos--;
            }
            break;
        case KEY_BS:
            if (ins_pos > row->len()) ins_pos = row->len();
            if (ins_pos > 0) {
                ins_pos--;
                if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos--;
            } else if (ins_row > 0) {
                ins_row--;
                row = (row_t*)row->prev;
                ins_pos = row->len();
            } else {
                break;
            }
        case KEY_DEL:
            if (ins_pos > row->len()) ins_pos = row->len();
            if (row->del(ins_pos)) {
                if (row->get_value(ins_pos).value == (u_int32_t)-1)
                    row->del(ins_pos);
            } else {
                if (row->next) {
                    row_t* t = (row_t*)row->next;
                    rowlist.dl_del_node(t);
                    row->join(t);
                }
            }
            break;
        case KEY_ENTER:
            row->inser_new_row(rowlist, ins_pos);
            ins_pos = 0;
            ins_row++;
            break;
        case KEY_HOME:
            ins_pos = 0;
            break;
        case KEY_END:
            ins_pos = row->len();
            break;

        default:
            if (ins_pos > row->len()) ins_pos = row->len();
            row->insert(key, ins_pos++);
            if ((key & 0xf00000) == 0xe00000) { /* Chinese charactor */
                row->insert((u_int32_t)-1, ins_pos++);
            }
            break;
    }
    int fx, fy;
    get_focus_pos(fx, fy);
    for (;;) {
        if (fx < clientrect.left) {
            column_start--;
            fx++;
        }
        if (fx >= clientrect.left + clientrect.width) {
            column_start++;
            fx--;
        }
        if ((fx == clientrect.left + clientrect.width)
            && (row->get_value(column_start + clientrect.width).value == (u_int32_t)-1)) {
            column_start++;
        }

        if (fx == clientrect.left + clientrect.width - 1) {
            if (row->get_value(column_start + clientrect.width).value == (u_int32_t)-1) {
                column_start++;
                fx--;
            }
        }
        
        if (fy < clientrect.top) {
            row_start--;
            fy++;
        }
        if (fy >= clientrect.top + clientrect.height) {
            row_start++;
            fy--;
        }
        if (get_focus_pos(fx, fy)) break;
    }

    int tx, ty;
    get_screen_pos(tx, ty);
    window_t *t = top_win();
    window_set_focused(t, tx - t->get_left() + fx, ty - t->get_top() + fy);
}

void text_t::on_mouse_down(int key, int x, int y, u_int8_t ctrl) {
    if (x < clientrect.left || x >= clientrect.left + clientrect.width || y < clientrect.top || y >= clientrect.top + clientrect.height) return;
    if (key == MOUSE_LEFT && ctrl == 0) {
        ins_pos = column_start + x - clientrect.left;
        ins_row = row_start + y - clientrect.top;
        int i = 0;
        row_t *row;
        for (row = (row_t*)rowlist.dl_first_node(); row && row->next && i < ins_row;row = (row_t*)row->next)i++;
        ins_row = i;
        int len = row->len();
        if (ins_pos > len) ins_pos = len;
        if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos--;
        int fx, fy;
        get_focus_pos(fx, fy);
        int tx, ty;
        get_screen_pos(tx, ty);
        window_t *t = top_win();
        sel_r_end = sel_r_begin = ins_row;
        sel_c_end = sel_c_begin = ins_pos;
        window_set_focused(t, tx - t->get_left() + fx, ty - t->get_top() + fy);
    }
}

void text_t::on_mouse_drag(int key, int x, int y, u_int8_t ctrl) {
    if (x < clientrect.left || x >= clientrect.left + clientrect.width || y < clientrect.top || y >= clientrect.top + clientrect.height) return;
    if (key == MOUSE_LEFT && ctrl == 0) {
        ins_pos = column_start + x - clientrect.left;
        ins_row = row_start + y - clientrect.top;
        int i = 0;
        row_t *row;
        for (row = (row_t*)rowlist.dl_first_node(); row && row->next && i < ins_row;row = (row_t*)row->next)i++;
        ins_row = i;
        int len = row->len();
        if (ins_pos < 0) ins_pos = 0;
        if (ins_pos > len) ins_pos = len;
        if (row->get_value(ins_pos).value == (u_int32_t)-1) ins_pos--;
        int fx, fy;
        get_focus_pos(fx, fy);
        int tx, ty;
        get_screen_pos(tx, ty);
        window_t *t = top_win();
        for (;;) {
            if (fx < clientrect.left) {
                column_start--;
                fx++;
            }
            if (fx >= clientrect.left + clientrect.width) {
                column_start++;
                fx--;
            }
            if ((fx == clientrect.left + clientrect.width)
                && (row->get_value(column_start + clientrect.width).value == (u_int32_t)-1)) {
                column_start++;
            }

            if (fx == clientrect.left + clientrect.width - 1) {
                if (row->get_value(column_start + clientrect.width).value == (u_int32_t)-1) {
                    column_start++;
                    fx--;
                }
            }
            
            if (fy < clientrect.top) {
                row_start--;
                fy++;
            }
            if (fy >= clientrect.top + clientrect.height) {
                row_start++;
                fy--;
            }
            if (get_focus_pos(fx, fy)) break;
        }
        sel_r_end = ins_row;
        sel_c_end = ins_pos;
        refresh();
        window_set_focused(t, tx - t->get_left() + fx, ty - t->get_top() + fy);
    }
}

bool text_t::get_focus_pos(int& x, int& y) {
    row_t* row;
    int i = 0;
    int tx, ty;
    for (row = (row_t*)rowlist.dl_first_node(); row && i < ins_row;row = (row_t*)row->next)i++;
    tx = row->len();
    if (tx > ins_pos) tx = ins_pos;
    ty = ins_row;
    x = clientrect.left + tx - column_start;
    y = clientrect.top + ty - row_start;
    if (x < clientrect.left || x >= clientrect.left + clientrect.width ||
        y < clientrect.top || y >= clientrect.top + clientrect.height)
        return false;
    return true;
}
