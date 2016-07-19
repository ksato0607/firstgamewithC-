#include <unistd.h>
#include <ncurses.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>

#include "io.h"
#include "move.h"
#include "path.h"
#include "pc.h"
#include "utils.h"
#include "dungeon.h"

using namespace std;
/* Same ugly hack we did in path.c */
static dungeon_t *dungeon;

typedef struct io_message {
  /* Will print " --more-- " at end of line when another message follows. *
   * Leave 10 extra spaces for that.                                      */
  char msg[71];
  struct io_message *next;
} io_message_t;

static io_message_t *io_head, *io_tail;

static void sigalrm_handler(int unused)
{
  //io_display(dungeon);
  io_display_all(dungeon);
}

static void mask_alarm(void)
{
  sigset_t s;

  sigemptyset(&s);
  sigaddset(&s, SIGALRM);
  sigprocmask(SIG_BLOCK, &s, NULL);
}

static void unmask_alarm(void)
{
  sigset_t s;

  sigemptyset(&s);
  sigaddset(&s, SIGALRM);
  sigprocmask(SIG_UNBLOCK, &s, NULL);
}

void io_init_terminal(dungeon_t *d)
{
  struct itimerval itv;

  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  start_color();

  init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
  
  itv.it_interval.tv_sec = 0;
  itv.it_interval.tv_usec = 200000; /* 1/5 seconds */
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = 200000;
  setitimer(ITIMER_REAL, &itv, NULL);
  dungeon = d;
  signal(SIGALRM, sigalrm_handler);
}

void io_reset_terminal(void)
{
  endwin();

  while (io_head) {
    io_tail = io_head;
    io_head = io_head->next;
    free(io_tail);
  }
  io_tail = NULL;
}

void io_queue_message(const char *format, ...)
{
  io_message_t *tmp;
  va_list ap;

  if (!(tmp = (io_message_t *) malloc(sizeof (*tmp)))) {
    perror("malloc");
    exit(1);
  }

  tmp->next = NULL;

  va_start(ap, format);

  vsnprintf(tmp->msg, sizeof (tmp->msg), format, ap);

  va_end(ap);

  if (!io_head) {
    io_head = io_tail = tmp;
  } else {
    io_tail->next = tmp;
    io_tail = tmp;
  }
}

static void io_print_message_queue(uint32_t y, uint32_t x, dungeon_t *d)
{
  uint32_t damage, i;
  int COLOR_HIGHLIGHT = 11;
  char input;
  init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
  
  mask_alarm();
  while (io_head) {
    io_tail = io_head;
    attron(COLOR_PAIR(COLOR_CYAN));
    mvprintw(y, x, "%-80s", io_head->msg);
    attroff(COLOR_PAIR(COLOR_CYAN));
    io_head = io_head->next;
    /* Removed the test for head that prevents "more" prompt on final *
     * message in order to prevent redraw from smashing it.           */
    attron(COLOR_PAIR(COLOR_HIGHLIGHT));
    mvprintw(y, x + 63, "%10s", " --PRESS ENTER-- ");
    attroff(COLOR_PAIR(COLOR_HIGHLIGHT));

    string highest_score="";
    if(d->nummon_beaten>get_highest_score()){
      highest_score += " HIGHEST SCORE!!!";
    }
    for (i = damage = 0; i < num_eq_slots; i++) {
      if (i == eq_slot_weapon && !d->the_pc->eq[i]) {
	damage += d->the_pc->damage->roll();
      } else if (d->the_pc->eq[i]) {
	damage += d->the_pc->eq[i]->roll_dice();
      }
    }
    
    
    mvprintw(23,0,"PC: HP = %d, SPEED = %d, POWER = %d, SCORE = %d %s",d->the_pc->hp,d->the_pc->speed, damage, d->nummon_beaten, highest_score.c_str());
    refresh();
    do{
    }while(input=getch()!='\n');
    free(io_tail);
  }
  io_tail = NULL;
  
  unmask_alarm();
}

static char distance_to_char[] = {
  '0',
  '1',
  '2',
  '3',
  '4',
  '5',
  '6',
  '7',
  '8',
  '9',
  'a',
  'b',
  'c',
  'd',
  'e',
  'f',
  'g',
  'h',
  'i',
  'j',
  'k',
  'l',
  'm',
  'n',
  'o',
  'p',
  'q',
  'r',
  's',
  't',
  'u',
  'v',
  'w',
  'x',
  'y',
  'z',
  'A',
  'B',
  'C',
  'D',
  'E',
  'F',
  'G',
  'H',
  'I',
  'J',
  'K',
  'L',
  'M',
  'N',
  'O',
  'P',
  'Q',
  'R',
  'S',
  'T',
  'U',
  'V',
  'W',
  'X',
  'Y',
  'Z'
};

void io_display_ch(dungeon_t *d)
{
  mask_alarm();
  mvprintw(11, 33, " HP:    %5d ", d->the_pc->hp);
  mvprintw(12, 33, " Speed: %5d ", d->the_pc->speed);
  mvprintw(14, 27, " Hit any key to continue. ");
  refresh();
  getch();
  unmask_alarm();
}

void io_display_tunnel(dungeon_t *d)
{
  uint32_t y, x;
  mask_alarm();
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      mvaddch(y + 1, x, (d->pc_tunnel[y][x] < 62              ?
                         distance_to_char[d->pc_tunnel[y][x]] :
                         '*'));
    }
  }
  refresh();
  while (getch() != 27 /* ESC */)
    ;
  unmask_alarm();
}

void io_display_distance(dungeon_t *d)
{
  uint32_t y, x;
  mask_alarm();
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      mvaddch(y + 1, x, (d->pc_distance[y][x] < 62              ?
                         distance_to_char[d->pc_distance[y][x]] :
                         '*'));
    }
  }
  refresh();
  while (getch() != 27 /* ESC */)
    ;
  unmask_alarm();
}

void io_display_hardness(dungeon_t *d)
{
  uint32_t y, x;
  mask_alarm();
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      /* Maximum hardness is 255.  We have 62 values to display it, but *
       * we only want one zero value, so we need to cover [1,255] with  *
       * 61 values, which gives us a divisor of 254 / 61 = 4.164.       *
       * Generally, we want to avoid floating point math, but this is   *
       * not gameplay, so we'll make an exception here to get maximal   *
       * hardness display resolution.                                   */
      mvaddch(y + 1, x, (d->hardness[y][x]                             ?
                         distance_to_char[1 + (d->hardness[y][x] / 5)] :
                         '0'));
    }
  }
  refresh();
  while (getch() != 27 /* ESC */)
    ;
  unmask_alarm();
}

void io_menu_display(dungeon_t *d)
{
  int input;
  string dungeon;
  int COLOR_HIGHLIGHT = 11;
  int read_already;
  init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
    /*  
   ___   _   _ _   _  ____ _____ ___  _   _
  |  _ \| | | | \ | |/ ___| ____/ _ \| \ | |
  | | | | | | |  \| | |  _|  _|| | | |  \| |
  | |_| | |_| | |\  | |_| | |__| |_| | |\  |
  |____/ \___/|_| \_|\____|_____\___/|_| \_|";*/
   /*
 ___ _  _ _   ___  
| _ \ || | | | __| 
| v / \/ | |_| _|  
|_|_\\__/|___|___| 
  
 ___  __  __  _ _  __ 
| _ \/  \|  \| | |/ / 
| v / /\ | | ' |   <  
|_|_\_||_|_|\__|_|\_\ 

 ___ _    __ __   __
| _,\ |  /  \\ `v' /
| v_/ |_| /\ |`. .' 
|_| |___|_||_| !_!  
*/
  int j=30;
  int highlight=0;
  do{
    mask_alarm();
    clear();
    attron(COLOR_PAIR(COLOR_GREEN));
    mvprintw(3,20," ___   _   _ _   _  ____ _____ ___  _   _ ");
    mvprintw(4,20,"|  _ \\| | | | \\ | |/ ___| ____/ _ \\| \\ | |");
    mvprintw(5,20,"| | | | | | |  \\| | |  _|  _|| | | |  \\| |");
    mvprintw(6,20,"| |_| | |_| | |\\  | |_| | |__| |_| | |\\  |");
    mvprintw(7,20,"|____/ \\___/|_| \\_|\\____|_____\\___/|_| \\_|");
    attroff(COLOR_PAIR(COLOR_GREEN));
 
    if(highlight==0){
    attron(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
    mvprintw(9,j," ___ _  _ _   ___    ");
    mvprintw(10,j,"| _ \\ || | | | __|   ");
    mvprintw(11,j,"| v / \\/ | |_| _|    ");
    mvprintw(12,j,"|_|_\\\\__/|___|___|   ");
    if(highlight==0){
      attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
 
    if(highlight==1){
    attron(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
    mvprintw(14,j," ___  __  __  _ _  __");
    mvprintw(15,j,"| _ \\/  \\|  \\| | |/ /");
    mvprintw(16,j,"| v / /\\ | | ' |   < ");
    mvprintw(17,j,"|_|_\\_||_|_|\\__|_|\\_\\");
 
    if(highlight==1){
      attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
    
    if(highlight==2){
    attron(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
  mvprintw(19,j," ___ _    __ __   __ ");
  mvprintw(20,j,"| _,\\ |  /  \\\\ `v' / ");
  mvprintw(21,j,"| v_/ |_| /\\ |`. .'  ");
  mvprintw(22,j,"|_| |___|_||_| !_!   ");
  if(highlight==2){
      attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
  }
  input = getch();
  switch(input){
  case KEY_UP: highlight = (highlight+2)%3; break;
  case KEY_DOWN: highlight = (highlight+1)%3; break;
  default: ;
  }
  refresh();
  
  if(input=='\n'&&highlight==1&&!read_already){
    read_ranking(d, 'r');
    read_already++;
  }
  else if(input=='\n'&&highlight==0){
    io_read_rule(d);  
  }
  unmask_alarm();
  }while(!(input == '\n'&&highlight==2));
  
  if(!read_already){
  read_ranking(d, 'p');
  }
}

void io_read_rule(dungeon_t *d){
  char input;
  int COLOR_HIGHLIGHT = 11;
  int i;
  string intro1 = "This is a text-based dungeon game."; 
  string intro2 = "You will be fighting with monsters.";
  string intro3 = "The mission is to beat as many monsters as possible.";
  string intro4 = "The dungeon looks like this.";
  string intro5 = "The collection of dot(.) represents a room.";
  string intro6 = "The collection of hash(#) represents a corridor.";
  string intro7 = "You are represented by @.";
  string intro8 = "You can move anywhere in the rooms or on corridors.";
  string intro9 = "Monsters are represented by alphabets.";
  string intro10 = "The strength of each monster is different.";
  string intro11 = "Some monsters use the shortest path to attack you.";
  string intro12 = "Some monsters have high HP, strong power, high speed and etc...";
  string intro13 = "When you meet monsters, you can choose to fight or run away.";
  string intro14 = "When HP gets 0, the monster or you will be dead.";
  string intro15 = "Initially, you do not wear any items, so you are not strong at all.";
  string intro16 = "However, you can pick up some items and wear them to be powerful.";
  string intro17 = "Itemas are represented by colored special characters.";
  string intro18 = "Each item has a different feature.";
  string intro19 = "If you get too much damage, you can go to a hospital to recover your HP.";
  string intro20 = "It is represented by +.";
  string intro21 = "If monsters on the floor are too strong, you can use stairs.";
  string intro22 = "> are stairs to go up and < are stairs to go down.";
  string intro23 = "Again, your mission is to beat as many monsters as possible.";
  string intro24 = "Good Luck!";
  
  
  init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
  
  clear();
  
  for(i=0; i<intro1.length(); i++){
    usleep(125000);
    mvaddch(0,i,intro1[i]);
    refresh();
  }

  for(i=0; i<intro2.length(); i++){
    usleep(125000);
    mvaddch(1,i,intro2[i]);
    refresh();
  }

  for(i=0; i<intro3.length(); i++){
    usleep(125000);
    mvaddch(2,i,intro3[i]);
    refresh();
  }

  for(i=0; i<intro4.length(); i++){
    usleep(125000);
    mvaddch(3,i,intro4[i]);
    refresh();
  }
  
  sleep(2);

  io_display_all(d);
  
  mask_alarm();
  mvprintw(0,0,"%80s","");
  attron(COLOR_PAIR(COLOR_GREEN));
  for(i=1; i<80; i++){
    mvaddch(19,i,'-');
  }
  for(i=1; i<80; i++){
    mvaddch(23,i,'-');
  }
  mvaddch(19,0,'|');
  mvaddch(19,79,'|');
  mvaddch(20,0,'|');
  mvaddch(20,79,'|');
  mvaddch(21,0,'|');
  mvaddch(21,79,'|');
  mvaddch(22,0,'|');
  mvaddch(22,79,'|');
  mvaddch(23,0,'|');
  mvaddch(23,79,'|');
  attroff(COLOR_PAIR(COLOR_GREEN));
  refresh();

  io_rule_display(intro5, intro6);
  io_rule_display(intro7, intro8);
  io_rule_display(intro9, intro10);
  io_rule_display(intro11, intro12);
  io_rule_display(intro13, intro14);
  io_rule_display(intro15, intro16);
  io_rule_display(intro17, intro18);
  io_rule_display(intro19, intro20);
  io_rule_display(intro21, intro22);
  io_rule_display(intro23, intro24);
  
  unmask_alarm();  
}

void io_rule_display(string rule1, string rule2){

  char input;
  int COLOR_HIGHLIGHT = 11;
  int i;
  
  mvprintw(20,1,"%78s","");
  mvprintw(21,1,"%78s","");
  mvprintw(22,1,"%78s","");

  for(i=0; i<rule1.length(); i++){
    usleep(125000);
    mvaddch(20,i+1,rule1[i]);
    refresh();
  }

  for(i=0; i<rule2.length(); i++){
    usleep(125000);
    mvaddch(21,i+1,rule2[i]);
    refresh();
  }
  attron(COLOR_PAIR(COLOR_HIGHLIGHT));
  mvprintw(22,73," NEXT ");
  attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
  input = getch();

}
void io_initial_display(dungeon_t *d)
{
  char input;
  string name;
  mask_alarm();

  attron(COLOR_PAIR(COLOR_CYAN));
  mvprintw(0, 0, " %-58s ", "Hit p to play, Hit r to check rankings.");
  attroff(COLOR_PAIR(COLOR_CYAN));
  
  refresh();
  int count=0;  

  do{
    input = getch();

    if(count == 0 && (input == 'r' || input=='p')){
      read_ranking(d,input);
      count++;
    }
  }while(input != 'p');
  
  //io_display(d);
  io_display_all(d);
  
  unmask_alarm();
}

void io_display_all(dungeon_t *d)
{
  uint32_t y, x;
  uint32_t damage, i;
    
  mask_alarm();
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      if (d->charmap[y][x]) {
        attron(COLOR_PAIR(d->charmap[y][x]->get_color()));
        mvaddch(y + 1, x, d->charmap[y][x]->get_symbol());
        attroff(COLOR_PAIR(d->charmap[y][x]->get_color()));
      } else if (d->objmap[y][x] /*&& d->objmap[y][x]->have_seen()*/) {
        attron(COLOR_PAIR(d->objmap[y][x]->get_color()));
        mvaddch(y + 1, x, d->objmap[y][x]->get_symbol());
        attroff(COLOR_PAIR(d->objmap[y][x]->get_color()));
      } else {
        switch (mapxy(x, y)) {
        case ter_wall:
        case ter_wall_immutable:
          mvaddch(y + 1, x, ' ');
          break;
        case ter_floor:
        case ter_floor_room:
          mvaddch(y + 1, x, '.');
          break;
        case ter_floor_hall:
          mvaddch(y + 1, x, '#');
          break;
        case ter_debug:
          mvaddch(y + 1, x, '*');
          break;
        case ter_stairs_up:
          mvaddch(y + 1, x, '<');
          break;
        case ter_stairs_down:
          mvaddch(y + 1, x, '>');
          break;
	case ter_hospital:
	  mvaddch(y + 1, x, '+');
	  break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          mvaddch(y + 1, x, '0');
        }
      }
    }
  }
  
  io_print_message_queue(0, 0,d);
  string highest_score="";
  if(d->nummon_beaten>get_highest_score()){
    highest_score += " HIGHEST SCORE!!!";
  }

  for (i = damage = 0; i < num_eq_slots; i++) {
    if (i == eq_slot_weapon && !d->the_pc->eq[i]) {
      damage += d->the_pc->damage->roll();
    } else if (d->the_pc->eq[i]) {
      damage += d->the_pc->eq[i]->roll_dice();
    }
  }

  attron(COLOR_PAIR(COLOR_CYAN));
  mvprintw(0,0,"%80s","--Hit 'h' for HELP--");
  attroff(COLOR_PAIR(COLOR_CYAN));
  mvprintw(23,0,"PC: HP = %d, SPEED = %d, POWER = %d, SCORE = %d %s",d->the_pc->hp,d->the_pc->speed, damage, d->nummon_beaten,highest_score.c_str());

  refresh();
  unmask_alarm();
}

void io_display(dungeon_t *d)
{
  uint32_t y, x;
  uint32_t illuminated;

  mask_alarm();
  clear();
  for (y = 0; y < DUNGEON_Y; y++) {
    for (x = 0; x < DUNGEON_X; x++) {
      if ((illuminated = is_illuminated(d->the_pc, y, x))) {
        attron(A_BOLD);
      }
      if (d->charmap[y][x] &&
          can_see(d,
                  character_get_pos(d->the_pc),
                  character_get_pos(d->charmap[y][x]),
                  1)) {
        attron(COLOR_PAIR(d->charmap[y][x]->get_color()));
        mvaddch(y + 1, x, d->charmap[y][x]->get_symbol());
        attroff(COLOR_PAIR(d->charmap[y][x]->get_color()));
      } else if (d->objmap[y][x] && d->objmap[y][x]->have_seen()) {
        attron(COLOR_PAIR(d->objmap[y][x]->get_color()));
        mvaddch(y + 1, x, d->objmap[y][x]->get_symbol());
        attroff(COLOR_PAIR(d->objmap[y][x]->get_color()));
      } else {
        switch (pc_learned_terrain(d->the_pc, y, x)) {
        case ter_wall:
        case ter_wall_immutable:
        case ter_unknown:
          mvaddch(y + 1, x, ' ');
          break;
        case ter_floor:
        case ter_floor_room:
          mvaddch(y + 1, x, '.');
          break;
        case ter_floor_hall:
          mvaddch(y + 1, x, '#');
          break;
        case ter_debug:
          mvaddch(y + 1, x, '*');
          break;
        case ter_stairs_up:
          mvaddch(y + 1, x, '<');
          break;
        case ter_stairs_down:
          mvaddch(y + 1, x, '>');
          break;
	case ter_hospital:
	  mvaddch(y + 1, x, '+');
	  break;
        default:
 /* Use zero as an error symbol, since it stands out somewhat, and it's *
  * not otherwise used.                                                 */
          mvaddch(y + 1, x, '0');
        }
      }
      if (illuminated) {
        attroff(A_BOLD);
      }
    }
  }

  io_print_message_queue(0, 0,d);
  string highest_score="";
  if(d->nummon_beaten>get_highest_score()){
    highest_score += " HIGHEST SCORE!!!";
  }
  mvprintw(23,0,"PC: HP = %d, SPEED = %d, SCORE = %d %s",d->the_pc->hp, d->the_pc->speed, d->nummon_beaten,highest_score.c_str());
  refresh();
  unmask_alarm();
}

void io_display_monster_list(dungeon_t *d)
{
  mvprintw(11, 33, " HP:    XXXXX ");
  mvprintw(12, 33, " Speed: XXXXX ");
  mvprintw(14, 27, " Hit any key to continue. ");
  refresh();
  getch();
}

uint32_t io_teleport_pc(dungeon_t *d)
{
  /* Just for fun. */
  pair_t dest;

  do {
    dest[dim_x] = rand_range(1, DUNGEON_X - 2);
    dest[dim_y] = rand_range(1, DUNGEON_Y - 2);

  }while(charpair(dest) || (mappair(dest)<ter_floor));
      //} while (charpair(dest) && mappair(dest)!=ter_floor);
  d->charmap[character_get_y(d->the_pc)][character_get_x(d->the_pc)] = NULL;
  d->charmap[dest[dim_y]][dest[dim_x]] = d->the_pc;

  character_set_y(d->the_pc, dest[dim_y]);
  character_set_x(d->the_pc, dest[dim_x]);

  if (mappair(dest) < ter_floor) {
    mappair(dest) = ter_floor;
  }

  pc_observe_terrain(d->the_pc, d);

  dijkstra(d);
  dijkstra_tunnel(d);
  d->the_pc->hp -= 100;
  if(d->the_pc->hp<=0){
    d->the_pc->hp=0;
    d->the_pc->alive=0;
  }
  return 0;
}

/* Adjectives to describe our monsters */
static const char *adjectives[] = {
  "A menacing ",
  "A threatening ",
  "A horrifying ",
  "An intimidating ",
  "An aggressive ",
  "A frightening ",
  "A terrifying ",
  "A terrorizing ",
  "An alarming ",
  "A frightening ",
  "A dangerous ",
  "A glowering ",
  "A glaring ",
  "A scowling ",
  "A chilling ",
  "A scary ",
  "A creepy ",
  "An eerie ",
  "A spooky ",
  "A slobbering ",
  "A drooling ",
  " A horrendous ",
  "An unnerving ",
  "A cute little ",  /* Even though they're trying to kill you, */
  "A teeny-weenie ", /* they can still be cute!                 */
  "A fuzzy ",
  "A fluffy white ",
  "A kawaii ",       /* For our otaku */
  "Hao ke ai de "    /* And for our Chinese */
  /* And there's one special case (see below) */
};
/*
static void io_scroll_monster_list(char (*s)[50], uint32_t count)
{
  uint32_t offset;
  uint32_t i;

  offset = 0;

  while (1) {
    for (i = 0; i < 13; i++) {
      mvprintw(i + 6, 19, " %-50s ", s[i + offset]);
    }
    switch (getch()) {
    case KEY_UP:
      if (offset) {
        offset--;
      }
      break;
    case KEY_DOWN:
      if (offset < (count - 13)) {
        offset++;
      }
      break;
    case 27:
      return;
    }

  }
}
*/
static bool is_vowel(const char c)
{
  return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' ||
          c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U');
}

static void io_list_monsters_display(dungeon_t *d,
                                     character **c,
                                     uint32_t count)
{
  uint32_t i;
  char input;
  char (*s)[50]; /* pointer to array of 40 char */

  (void) adjectives;

  s = (char (*)[50]) malloc((count ? count : 1) * sizeof (*s));

  mvprintw(3, 14, " %-50s ", "");
  /* Borrow the first element of our array for this string: */
  snprintf(s[0], 50, "You know of %d monsters:", count);
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(4, 14, "               %-34s ", s);
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(5,14," ___________________________________________________ ");
  mvprintw(6,14," |                                                 | ");
  mvprintw(7,14," |                                                 | ");
  mvprintw(8,14," |                                                 | ");
  mvprintw(9,14," |                                                 | ");
  mvprintw(10,14," |                                                 | ");
  mvprintw(11,14," |                                                 | ");
  mvprintw(12,14," |                                                 | ");
  mvprintw(13,14," |                                                 | ");
  mvprintw(14,14," |                                                 | ");
  mvprintw(15,14," |                                                 | ");
  
  for (i = 0; i < count; i++) {
    snprintf(s[i], 50, "%3s%s (%c): HP = %d, POWER = %d ",
             (is_vowel(character_get_name(c[i])[0]) ? "An " : "A "),
             character_get_name(c[i]),
             character_get_symbol(c[i]),character_get_hp(c[i]),character_get_attack(c[i]));

	     /*
             abs(character_get_y(c[i]) - character_get_y(d->the_pc)),
             ((character_get_y(c[i]) - character_get_y(d->the_pc)) <= 0 ?
              "North" : "South"),
             abs(character_get_x(c[i]) - character_get_x(d->the_pc)),
             ((character_get_x(c[i]) - character_get_x(d->the_pc)) <= 0 ?
	     "East" : "West"));*/
    if (count <= 13) {
      /* Handle the non-scrolling case right here. *
       * Scrolling in another function.            */
      attron(COLOR_PAIR(COLOR_GREEN));
      mvprintw(i + 6, 16, "%-48s", s[i]);
      attroff(COLOR_PAIR(COLOR_GREEN));
    }
  }

  mvprintw(16,14," |_________________________________________________| ");
  mvprintw(17,14," %-49s "," Hit any key to continue.");
  input = getch();
  /*
  if (count <= 13) {
    mvprintw(count + 6, 14, " %-50s ", "");
    mvprintw(count + 7, 14, " %-50s ", "Hit escape to continue.");
    while (getch() != 27)
      ;
  } else {
    mvprintw(14, 14, " %-50s ", "");
    mvprintw(20, 14, " %-50s ",
             "Arrows to scroll, escape to continue.");
    //io_scroll_monster_list(s, count);
  }
  */
  free(s);
}

static int compare_monster_distance(const void *v1, const void *v2)
{
  const character *const *c1 = (const character * const *) v1;
  const character *const *c2 = (const character * const *) v2;

  return (dungeon->pc_distance[character_get_y(*c1)][character_get_x(*c1)] -
          dungeon->pc_distance[character_get_y(*c2)][character_get_x(*c2)]);
}

static void io_list_monsters(dungeon_t *d)
{
  character **c;
  uint32_t x, y, count;

  mask_alarm();
  c = (character **) malloc(d->num_monsters * sizeof (*c));

  /* Get a linear list of monsters */
  for (count = 0, y = 1; y < DUNGEON_Y - 1; y++) {
    for (x = 1; x < DUNGEON_X - 1; x++) {
      if (d->charmap[y][x] &&
          d->charmap[y][x] != d->the_pc &&
          can_see(d, d->the_pc->position, character_get_pos(d->charmap[y][x]), 1)) {
        c[count++] = d->charmap[y][x];
      }
    }
  }

  /* Sort it by distance from PC */
  dungeon = d;
  qsort(c, count, sizeof (*c), compare_monster_distance);

  /* Display it */
  io_list_monsters_display(d, c, count);
  free(c);

  unmask_alarm();

  /* And redraw the dungeon */
  //io_display(d);
  io_display_all(d);
}

void io_object_to_string(object *o, char *s, uint32_t size)
{
  if (o) {
    snprintf(s, size, "%s (sp: %d, dmg: %d+%dd%d)",
             o->get_name(), o->get_speed(), o->get_damage_base(),
             o->get_damage_number(), o->get_damage_sides());
  } else {
    *s = '\0';
  }
}

uint32_t io_wear_eq(dungeon_t *d)
{
  uint32_t i;
  char s[61];
  int input;
  int COLOR_HIGHLIGHT = 11;
  init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
  int highlight=0;
  uint32_t count=d->the_pc->count_items();

  mask_alarm();
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(3,11,"%50s","");
  mvprintw(4,11,"                  %-42s","List of Wearable Items");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(5,11," ___________________________________________________________ ");
  mvprintw(6,11," |                                                         | ");
  mvprintw(7,11," |                                                         | ");
  mvprintw(8,11," |                                                         | ");
  mvprintw(9,11," |                                                         | ");
  mvprintw(10,11," |                                                         | ");
  mvprintw(11,11," |                                                         | ");
  mvprintw(12,11," |                                                         | ");
  mvprintw(13,11," |                                                         | ");
  mvprintw(14,11," |                                                         | ");
  mvprintw(15,11," |                                                         | ");
  mvprintw(16,11," |_________________________________________________________| ");

  do{
  
  for (i = 0; i < MAX_INVENTORY; i++) {
    /* We'll write 12 lines, 10 of inventory, 1 blank, and 1 prompt. *
     * We'll limit width to 60 characters, so very long object names *
     * will be truncated.  In an 80x24 terminal, this gives offsets  *
     * at 10 x and 6 y to start printing things.  Same principal in  *
     * other functions, below.                                       */
    io_object_to_string(d->the_pc->in[i], s, 61);
    if(highlight==i){
      attron(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
    mvprintw(i + 6, 13, "%c) %-54s", '0' + i, s);
    if(highlight==i){
      attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
    }
  }
  mvprintw(17, 11, " %-58s ", "");
  mvprintw(18, 11, "  %-58s ", "Wear which item (ESC to cancel)?");
  refresh();

  input = getch();  
  switch(input){
  case KEY_UP: highlight = (highlight+count-1)%count; break;
  case KEY_DOWN: highlight = (highlight+1)%count; break;
  default: ;
  }
  
  if (input == '\n' && !(d->the_pc->wear_in(highlight - 0))) {
    unmask_alarm();
    return 0;
    }
  }while(input!=27);
  

  /*
  while (1) {
    if ((key = getch()) == 27) {
      //io_display(d);
      io_display_all(d);
      unmask_alarm();
      return 1;
    }

    if (key < '0' || key > '9') {
      if (isprint(key)) {
        snprintf(s, 61, "Invalid input: '%c'.  Enter 0-9 or ESC to cancel.",
                 key);
        mvprintw(18, 10, " %-58s ", s);
      } else {
        mvprintw(18, 10, " %-58s ",
                 "Invalid input.  Enter 0-9 or ESC to cancel.");
      }
      refresh();
      continue;
    }

    if (!d->the_pc->in[key - '0']) {
      mvprintw(18, 10, " %-58s ", "Empty inventory slot.  Try again.");
      continue;
    }

    if (!d->the_pc->wear_in(key - '0')) {
      unmask_alarm();
      return 0;
    }

    snprintf(s, 61, "Can't wear %s.  Try again.",
             d->the_pc->in[key - '0']->get_name());
    mvprintw(18, 10, " %-58s ", s);
    refresh();
  }*/
  
  unmask_alarm();
  return 1;
}

void io_display_in(dungeon_t *d)
{
  
  uint32_t i;
  char s[61];

  mask_alarm();
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(4,11,"                    %-33s","List of Items");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(5,11," ______________________________________________________ ");
  mvprintw(6,11," |                                                    | ");
  mvprintw(7,11," |                                                    | ");
  mvprintw(8,11," |                                                    | ");
  mvprintw(9,11," |                                                    | ");
  mvprintw(10,11," |                                                    | ");
  mvprintw(11,11," |                                                    | ");
  mvprintw(12,11," |                                                    | ");
  mvprintw(13,11," |                                                    | ");
  mvprintw(14,11," |                                                    | ");
  mvprintw(15,11," |                                                    | ");
  mvprintw(16,11," |____________________________________________________| ");
  for (i = 0; i < MAX_INVENTORY; i++) {
    io_object_to_string(d->the_pc->in[i], s, 61);
    mvprintw(i + 6, 13, "%c) %-40s", '0' + i, s);
  }

  mvprintw(17, 11, "%-48s", "");
  mvprintw(18, 11, "  %-48s", "Hit any key to continue.");

  refresh();

  getch();
  unmask_alarm();

  //io_display(d);
  io_display_all(d);
}

uint32_t io_remove_eq(dungeon_t *d)
{
  uint32_t i, key;
  char s[61], t[61];

  mask_alarm();
  for (i = 0; i < num_eq_slots; i++) {
    sprintf(s, "[%s]", eq_slot_name[i]);
    io_object_to_string(d->the_pc->eq[i], t, 61);
    mvprintw(i + 5, 10, " %c %-9s) %-45s ", 'a' + i, s, t);
  }
  mvprintw(17, 10, " %-58s ", "");
  mvprintw(18, 10, " %-58s ", "Take off which item (ESC to cancel)?");
  refresh();

  while (1) {
    if ((key = getch()) == 27 /* ESC */) {
      //io_display(d);
     io_display_all(d);
      return 1;
    }

    if (key < 'a' || key > 'l') {
      if (isprint(key)) {
        snprintf(s, 61, "Invalid input: '%c'.  Enter a-l or ESC to cancel.",
                 key);
        mvprintw(18, 10, " %-58s ", s);
      } else {
        mvprintw(18, 10, " %-58s ",
                 "Invalid input.  Enter a-l or ESC to cancel.");
      }
      refresh();
      continue;
    }

    if (!d->the_pc->eq[key - 'a']) {
      mvprintw(18, 10, " %-58s ", "Empty equipment slot.  Try again.");
      continue;
    }

    if (!d->the_pc->remove_eq(key - 'a')) {
      unmask_alarm();
      return 0;
    }

    snprintf(s, 61, "Can't take off %s.  Try again.",
             d->the_pc->eq[key - 'a']->get_name());
    mvprintw(19, 10, " %-58s ", s);
  }

  unmask_alarm();
  return 1;
}

void io_display_help(dungeon_t *d)
{
  char input;
  int i = 0;
  mask_alarm();
  mvprintw(++i,19,"%-43s","");
  mvprintw(++i,19,"%-43s","");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(++i,0,"%35s %-44s","","SET UP KEY");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(++i,19," _________________________________________ ");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","UP KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Move Up");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","DOWN KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Move Down");

  mvprintw(i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","LEFT KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Move Left");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","RIGHT KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Move Right");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'S\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Exit the game");

  
  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'m\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ","List monsters");

  
  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'i\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "List items");

  
  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'w\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Equip items");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'e\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "List equipped items");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'g\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Teleport (-100HP)");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'<\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Go downstairs");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'>\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Go upstairs");

  mvprintw(++i,19," |");
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,21,"%-13s","\'+\' KEY:");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(i,34,"%-26s| ", "Recover in hospital(+50HP)");

  mvprintw(++i,19," |                                       | ");
  mvprintw(++i,19," |Use <,>,+ keys when @ is on the symbol | ");
  mvprintw(++i,19," |_______________________________________| ");
  mvprintw(++i,19,"%50s","");
  mvprintw(++i,19," %-49s "," Hit any key to continue.");
  ++i;
  mvprintw(++i,0,"%80s","");
  input = getch();
  unmask_alarm();
}

void io_display_eq(dungeon_t *d)
{
  uint32_t i;
  char s[61], t[61];

  mask_alarm();
  attron(COLOR_PAIR(COLOR_GREEN));
  mvprintw(3,11,"%50s","");
  mvprintw(4,11,"                  %-42s","List of Equipped Items");
  attroff(COLOR_PAIR(COLOR_GREEN));
  mvprintw(5,11," ___________________________________________________________ ");
  mvprintw(6,11," |                                                         | ");
  mvprintw(7,11," |                                                         | ");
  mvprintw(8,11," |                                                         | ");
  mvprintw(9,11," |                                                         | ");
  mvprintw(10,11," |                                                         | ");
  mvprintw(11,11," |                                                         | ");
  mvprintw(12,11," |                                                         | ");
  mvprintw(13,11," |                                                         | ");
  mvprintw(14,11," |                                                         | ");
  mvprintw(15,11," |                                                         | ");
  mvprintw(16,11," |                                                         | ");
  mvprintw(17,11," |                                                         | ");
  mvprintw(18,11," |_________________________________________________________| ");
  for (i = 0; i < num_eq_slots; i++) {
    sprintf(s, "[%s]", eq_slot_name[i]);
    io_object_to_string(d->the_pc->eq[i], t, 61);
    mvprintw(i + 6, 13, "%c %-9s) %-40s", 'a' + i, s, t);
  }
  mvprintw(19, 11, " %-58s ", "");
  mvprintw(20, 11, "  %-58s ", "Hit any key to continue.");

  refresh();

  getch();

  //io_display(d);
  io_display_all(d);
  unmask_alarm();
}

uint32_t io_drop_in(dungeon_t *d)
{
  uint32_t i, key;
  char s[61];

  mask_alarm();
  for (i = 0; i < MAX_INVENTORY; i++) {
    /* We'll write 12 lines, 10 of inventory, 1 blank, and 1 prompt. *
     * We'll limit width to 60 characters, so very long object names *
     * will be truncated.  In an 80x24 terminal, this gives offsets  *
     * at 10 x and 6 y to start printing things.                     */
      mvprintw(i + 6, 10, " %c) %-55s ", '0' + i,
               d->the_pc->in[i] ? d->the_pc->in[i]->get_name() : "");
  }
  mvprintw(16, 10, " %-58s ", "");
  mvprintw(17, 10, " %-58s ", "Drop which item (ESC to cancel)?");
  refresh();

  while (1) {
    if ((key = getch()) == 27 /* ESC */) {
      //io_display(d);
      io_display_all(d);
      unmask_alarm();
      return 1;
    }

    if (key < '0' || key > '9') {
      if (isprint(key)) {
        snprintf(s, 61, "Invalid input: '%c'.  Enter 0-9 or ESC to cancel.",
                 key);
        mvprintw(18, 10, " %-58s ", s);
      } else {
        mvprintw(18, 10, " %-58s ",
                 "Invalid input.  Enter 0-9 or ESC to cancel.");
      }
      refresh();
      continue;
    }

    if (!d->the_pc->in[key - '0']) {
      mvprintw(18, 10, " %-58s ", "Empty inventory slot.  Try again.");
      continue;
    }

    if (!d->the_pc->drop_in(d, key - '0')) {
      unmask_alarm();
      return 0;
    }

    snprintf(s, 61, "Can't drop %s.  Try again.",
             d->the_pc->in[key - '0']->get_name());
    mvprintw(18, 10, " %-58s ", s);
    refresh();
  }

  unmask_alarm();
  return 1;
}

uint32_t io_expunge_in(dungeon_t *d)
{
  uint32_t i, key;
  char s[61];

  mask_alarm();
  for (i = 0; i < MAX_INVENTORY; i++) {
    /* We'll write 12 lines, 10 of inventory, 1 blank, and 1 prompt. *
     * We'll limit width to 60 characters, so very long object names *
     * will be truncated.  In an 80x24 terminal, this gives offsets  *
     * at 10 x and 6 y to start printing things.                     */
      mvprintw(i + 6, 10, " %c) %-55s ", '0' + i,
               d->the_pc->in[i] ? d->the_pc->in[i]->get_name() : "");
  }
  mvprintw(16, 10, " %-58s ", "");
  mvprintw(17, 10, " %-58s ", "Destroy which item (ESC to cancel)?");
  refresh();

  while (1) {
    if ((key = getch()) == 27 /* ESC */) {
      //io_display(d);
      io_display_all(d);
      unmask_alarm();
      return 1;
    }

    if (key < '0' || key > '9') {
      if (isprint(key)) {
        snprintf(s, 61, "Invalid input: '%c'.  Enter 0-9 or ESC to cancel.",
                 key);
        mvprintw(18, 10, " %-58s ", s);
      } else {
        mvprintw(18, 10, " %-58s ",
                 "Invalid input.  Enter 0-9 or ESC to cancel.");
      }
      refresh();
      continue;
    }

    if (!d->the_pc->in[key - '0']) {
      mvprintw(18, 10, " %-58s ", "Empty inventory slot.  Try again.");
      continue;
    }

    if (!d->the_pc->destroy_in(key - '0')) {
      //io_display(d);
      io_display_all(d);
      
      unmask_alarm();
      return 1;
    }

    snprintf(s, 61, "Can't destroy %s.  Try again.",
             d->the_pc->in[key - '0']->get_name());
    mvprintw(18, 10, " %-58s ", s);
    refresh();
  }

  unmask_alarm();
  return 1;
}

void io_handle_input(dungeon_t *d)
{
  uint32_t fail_code;
  int key;

  do {
    switch (key = getch()) {
      //case '7':
      //case 'y':
    case KEY_HOME:
      fail_code = move_pc(d, 7);
      break;
      //case '8':
      //case 'k':
    case KEY_UP:
      fail_code = move_pc(d, 8);
      break;
      //case '9':
      //case 'u':
    case KEY_PPAGE:
      fail_code = move_pc(d, 9);
      break;
      //case '6':
      //case 'l':
    case KEY_RIGHT:
      fail_code = move_pc(d, 6);
      break;
      //case '3':
      //case 'n':
    case KEY_NPAGE:
      fail_code = move_pc(d, 3);
      break;
      //case '2':
      //case 'j':
    case KEY_DOWN:
      fail_code = move_pc(d, 2);
      break;
      //case '1':
      //case 'b':
    case KEY_END:
      fail_code = move_pc(d, 1);
      break;
      //case '4':
      //case 'h':
    case KEY_LEFT:
      fail_code = move_pc(d, 4);
      break;
      //case '5':
      //case ' ':
    case KEY_B2:
      fail_code = 0;
      break;
    case '>':
      fail_code = move_pc(d, '>');
      break;
    case '<':
      fail_code = move_pc(d, '<');
      break;
    case '+':
      fail_code = move_pc(d, '+');
      break;
    case 'S':
      d->save_and_exit = 1;
      character_reset_turn(d->the_pc);
      fail_code = 0;
      break;
      /*case 'Q':
       Extra command, not in the spec.  Quit without saving.          
      d->quit_no_save = 1;
      fail_code = 0;
      break;
    case 'T':
       New command.  Display the distances for tunnelers.           
      io_display_tunnel(d);
      fail_code = 1;
      break;
    case 'D':
       New command.  Display the distances for non-tunnelers.         
      io_display_distance(d);
      fail_code = 1;
      break;
    case 'H':
       New command.  Display the hardnesses.                          
      io_display_hardness(d);
      fail_code = 1;
      break;
    case 's':
        New command.  Return to normal display after displaying some   
        special screen.                                                
      //io_display(d);
      io_display_all(d);
      fail_code = 1;
      break;*/
    case 'g':
      /* Teleport the PC to a random place in the dungeon.              */
      io_teleport_pc(d);
      fail_code = 0;
      break;
    case 'm':
      io_list_monsters(d);
      fail_code = 1;
      break;
    case 'a'
      :
      io_display_all(d);
      fail_code = 1;
      break;
    case 'w':
      fail_code = io_wear_eq(d);
      break;
      /*case 't':
      fail_code = io_remove_eq(d);
      break;
    case 'd':
      fail_code = io_drop_in(d);
      break;
    case 'x':
      fail_code = io_expunge_in(d);
      break;*/
    case 'i':
      io_display_in(d);
      fail_code = 1;
      break;
    case 'e':
      io_display_eq(d);
      fail_code = 1;
      break;
      /*case 'c':
      io_display_ch(d);
      fail_code = 1;
      break;*/
    case 'h':
      io_display_help(d);
      fail_code = 1;
      break;
      //case 'q':
      /* Demonstrate use of the message queue.  You can use this for *
       * printf()-style debugging (though gdb is probably a better   *
       * option.  Not that it matterrs, but using this command will  *
       * waste a turn.  Set fail_code to 1 and you should be able to *
       * figure out why I did it that way.                           */
      /*io_queue_message("This is the first message.");
      io_queue_message("Since there are multiple messages, "
                       "you will see \"more\" prompts.");
      io_queue_message("You can use any key to advance through messages.");
      io_queue_message("Normal gameplay will not resume until the queue "
                       "is empty.");
      io_queue_message("Long lines will be truncated, not wrapped.");
      io_queue_message("io_queue_message() is variadic and handles "
                       "all printf() conversion specifiers.");
      io_queue_message("Did you see %s?", "what I did there");
      io_queue_message("When the last message is displayed, there will "
                       "be no \"more\" prompt.");
      io_queue_message("Have fun!  And happy printing!");
      fail_code = 0;
      break;*/
    default:
      /* Also not in the spec.  It's not always easy to figure out what *
       * key code corresponds with a given keystroke.  Print out any    *
       * unhandled key here.  Not only does it give a visual error      *
       * indicator, but it also gives an integer value that can be used *
       * for that key in this (or other) switch statements.  Printed in *
       * octal, with the leading zero, because ncurses.h lists codes in *
       * octal, thus allowing us to do reverse lookups.  If a key has a *
       * name defined in the header, you can use the name here, else    *
       * you can directly use the octal value.                          */
      mvprintw(0, 0, "Unbound key: %#o ", key);
      fail_code = 1;
    }
  } while (fail_code);
}
