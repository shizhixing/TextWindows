#include <stdio.h>
#include <unistd.h>
#include <windows.h>


int main(int argc, char *argv[])
{
    window_t *main;
    text_t *text;
    main = new window_t(NULL);
    main->set_pos(50, 10);
    main->set_size(30, 10);
    main->set_text("this is APP's name");
    main->printf(1, 1, COLOR_WHITE, BGCOLOR_BLACK, "%s", "Hello");
    text = new text_t(main);
    text->set_pos(1, 4);
    text->set_size(40, 10);
    text->set_alignment(ALIGN_CLIENT);
    text->refresh();
    msg_process();
    return 0;
}
