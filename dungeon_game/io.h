#ifndef IO_H
# define IO_H

typedef struct dungeon dungeon_t;
using namespace std;

#include <string>

void io_init_terminal(dungeon_t *d);
void io_reset_terminal(void);
void io_display(dungeon_t *d);
void io_display_all(dungeon_t *d);
void io_initial_display(dungeon_t *d);
void io_menu_display(dungeon_t *d);
void io_read_rule(dungeon_t *d);
void io_rule_display(string rule1, string rule2);
void io_handle_input(dungeon_t *d);
void io_queue_message(const char *format, ...);

#endif
