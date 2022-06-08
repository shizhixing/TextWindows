#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "screen.h"
#include "utils.h"
#include "windows.h"

screen_t *screen;

void start_app(char *fullpath, char *workdir);

class desk_t : public window_t {
    public:
        desk_t() : window_t(NULL){
            set_style(WIN_STYLE_NONE);
            desk = this;
            set_text("DESK");
        };
};

desk_t *desk;

class task_bar_t :public window_t {
    private:
        button_t start;
    public:
        task_bar_t(window_t *parent) : window_t(parent), start(this) {
            set_style(WIN_STYLE_NONE);
            set_size(10, 3);
            set_alignment(ALIGN_BOTTOM);
            start.set_alignment(ALIGN_LEFT);
            start.set_text("Start");
            start.refresh();
        };
};

task_bar_t *task_bar;

void sig_resize( int signo )
{
    struct winsize size;
    if (ioctl(0, TIOCGWINSZ, &size) < 0) {
        printf("Error\n");
    } else {
        screen->set_size(size.ws_col, size.ws_row);
        desk->set_size(size.ws_col, size.ws_row);
        desk->refresh();
    }
}

int main(int argc, char *argv[])
{
    screen = new screen_t;
    desk = new desk_t();
    task_bar = new task_bar_t(desk);
    task_bar->refresh();
    track_v_t track(desk);
    track.refresh();
    signal(SIGPIPE, SIG_IGN);
    signal(SIGWINCH, sig_resize);
    kill(getpid(), SIGWINCH);
    screen->run();
    signal(SIGWINCH, NULL);
    delete screen;
    return 0;
}
