#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* constants */
// MBG : Menu Button Gap
#define MBG_MAIN 27
#define MBG_SIGN_IN 27
#define MBG_SIGN_UP 41
#define MBG_LOGIN 41
#define MBG_WITHDRAWAL 43
#define MBG_STATISTICS 0
#define MBG_IN_GAME 0

// MBX : Menu Button X position
#define MBX_MAIN 10
#define MBX_SIGN_IN 10
#define MBX_SIGN_UP 17
#define MBX_LOGIN 17
#define MBX_WITHDRAWAL 15
#define MBX_STATISTICS_WAITING 35
#define MBX_STATISTICS_OK 38
#define MBX_IN_GAME 5

// MBY : Menu Button Y position
#define MBY 3
#define MBY_IN_GAME 16

// length of id and password
#define MAX_ID_LENGTH 30
#define MAX_PASSWORD_LENGTH 30

// file name of user information
#define FILE_NAME "users.txt"

// length of usrinfo structure
#define INFO_LENGTH 1 * MAX_ID_LENGTH + 1 * MAX_PASSWORD_LENGTH + 2 * (sizeof(int))
// length of message structure
#define MSG_LENGTH INFO_LENGTH + 7 * (sizeof(int))

// row and column length of game board
#define BOARD_ROW 6
#define BOARD_COL 6

/* global variables */
WINDOW *screen = NULL;
WINDOW *menubar = NULL;

// window state
typedef enum _window
{
	MAIN,
	SIGN_IN,
	SIGN_UP,
	LOGIN,
	WITHDRAWAL,
	STATISTICS_WAITING,
	STATISTICS_OK,
	IN_GAME
} window;

// process id or password
typedef enum _func
{
	PROC_KEY,
	ID,
	PASSWORD,
	W_CONTROL
} func;

// user information structure
typedef struct _usrinfo
{
	char id[MAX_ID_LENGTH];
	char password[MAX_PASSWORD_LENGTH];
	int win;
	int lose;
} usrinfo;

// structure for socket communication
typedef struct _message
{
	usrinfo user;
	func fname;
	int win;
	int is_password_matched;
	int exist_id;
	int is_now_logined;
	int playing_num;
	int is_ok_clicked;
} message;

// for check current window
int win_num = MAIN;

// menu arrays
char *menu_main[] = {"SIGN IN", "SIGN UP", "EXIT"};
char *menu_signin[] = {"PLAY", "SIGN OUT", "WITHDRAWAL"};
char *menu_signup[] = {"SIGN UP", "BACK"};
char *menu_login[] = {"SIGN IN", "BACK"};
char *menu_withdrawal[] = {"WITHDRAWAL", "BACK"};
char *menu_statistics_waiting[] = {"WAITING"};
char *menu_statistics_ok[] = {"OK"};
char *menu_ingame[] = {"NEXT TURN", "REGAME", "EXIT"};

// game board for othello
char game_board[6][6];

// current position in game
int cur_row;
int cur_col;

// if cursor is on menubar : 1, else : 0
int is_on_menubar = 0;
// if id exist in file
int exist_id = 0;
// if password is matched
int is_password_matched = 0;
// if id is logined another client
int is_now_logined = 0;
// if ok button is clicked on statistics
int is_ok_clicked = 0;

// for socket
int sock;

// save userid
usrinfo *id_user;
// login_user
usrinfo *login_user;
// logiend user(1p)
usrinfo *logined_user;

// number of players
int playing_num = 1;

// id of client(access order)
int clnt_id;

/* function declarations */
// socket error handling
void error_handling(char *);
// initailize function
void init();
// finalize function
void finalize();
// mangaer function
void othello_manager();
// window drawing functions
void window_main();
void window_signin();
void window_signup();
void window_login();
void window_withdrawal();
void window_statistics_waiting();
void window_statistics_ok();
void window_ingame();
// toolkit functions
void set_usrinfo(usrinfo *);
double calc_percentage(usrinfo *);
void process_key_input(int, int, int *);
void menubar_highlighter(char *[], int, int, int);
void process_id_io(int *, usrinfo *);
void process_password_io(int *, usrinfo *);
int file_search(int, int, char *);
void set_game();
void game_cursor_highlighter();
// controller functions
void window_controller(int *);
void game_controller();

/* ##### main ##### */
int main(int argc, char const *argv[])
{
	// for setting client id
	message *write_msg = (message *)malloc(MSG_LENGTH);
	message *read_msg = (message *)malloc(MSG_LENGTH);

	memset(write_msg, 0x00, MSG_LENGTH);
	memset(read_msg, 0x00, MSG_LENGTH);

	// socket
	struct sockaddr_in serv_addr;

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		error_handling("socket() error");

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error!");

	if (argc != 3)
	{
		printf("Usage : %s <IP> <port>\n", argv[0]);
		exit(1);
	}

	// othello
	id_user = (usrinfo *)malloc(sizeof(usrinfo));
	set_usrinfo(id_user);

	login_user = (usrinfo *)malloc(sizeof(usrinfo));
	set_usrinfo(login_user);

	logined_user = (usrinfo *)malloc(sizeof(usrinfo));
	set_usrinfo(logined_user);

	init();

	// main window
	window_main();

	othello_manager();

	endwin();

	free(login_user);
	close(sock);
	return 0;
}

// socket error handling
void error_handling(char *err_msg)
{
	fputs(err_msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

// initailize data
void init()
{
	initscr();			  // start curses mode
	noecho();			  // don't echoing to terminal
	cbreak();			  // for read key immediately(no line buffering)
	keypad(stdscr, TRUE); // return integer keycode
	curs_set(0);		  // set cursor invisible

	// to query the terminal capability for colors
	if (has_colors() == FALSE)
	{
		puts("Terminal does not supported colors!");
		endwin();
		return;
	}
	else
	{
		// set color pair
		start_color();
		init_pair(1, COLOR_BLUE, COLOR_WHITE);
		init_pair(2, COLOR_WHITE, COLOR_BLUE);
	}

	refresh();
}

// finalize ncurses
void finalize()
{
	wclear(screen);
	wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);
	delwin(screen);
	delwin(menubar);
	endwin();
}

// ##### manager of othello #####
void othello_manager()
{
	int ch = 0;
	int key_position = 0;
	int menu_size = 0;

	while (1)
	{
		if (menubar == NULL)
		{
			printf("menubar is null\n");
			close(sock);
			exit(1);
		}

		switch (win_num)
		{
		case MAIN:
			menu_size = sizeof(menu_main) / sizeof(char *);
			curs_set(0);
			menubar_highlighter(menu_main, MBG_MAIN, menu_size, key_position);
			break;
		case SIGN_IN:
			menu_size = sizeof(menu_signin) / sizeof(char *);
			menubar_highlighter(menu_signin, MBG_SIGN_IN, menu_size, key_position);
			break;
		case SIGN_UP:
			menu_size = sizeof(menu_signup) / sizeof(char *);
			if (is_on_menubar == 0)
				curs_set(1);
			else if (is_on_menubar == 1)
			{
				curs_set(0);
				menubar_highlighter(menu_signup, MBG_SIGN_UP, menu_size, key_position);
			}
			break;
		case LOGIN:
			menu_size = sizeof(menu_login) / sizeof(char *);
			if (is_on_menubar == 0)
				curs_set(1);
			else if (is_on_menubar == 1)
			{
				curs_set(0);
				menubar_highlighter(menu_login, MBG_LOGIN, menu_size, key_position);
			}
			break;
		case WITHDRAWAL:
			menu_size = sizeof(menu_withdrawal) / sizeof(char *);
			if (is_on_menubar == 0)
				curs_set(1);
			else if (is_on_menubar == 1)
			{
				curs_set(0);
				menubar_highlighter(menu_withdrawal, MBG_WITHDRAWAL, menu_size, key_position);
			}
			break;
		case STATISTICS_WAITING:
			menu_size = sizeof(menu_statistics_waiting) / sizeof(char *);
			curs_set(0);
			menubar_highlighter(menu_statistics_waiting, MBG_STATISTICS, menu_size, key_position);
			break;
		case STATISTICS_OK:
			menu_size = sizeof(menu_statistics_ok) / sizeof(char *);
			curs_set(0);
			menubar_highlighter(menu_statistics_ok, MBG_STATISTICS, menu_size, key_position);
			break;
		case IN_GAME:
			menu_size = sizeof(menu_ingame) / sizeof(char *);
			curs_set(0);

			menubar_highlighter(menu_ingame, MBG_IN_GAME, menu_size, key_position);
			break;
		}
		wrefresh(screen);
		wrefresh(menubar);
		if (win_num == STATISTICS_WAITING || win_num == STATISTICS_OK)
			process_key_input('A', menu_size, &key_position);
		else
			process_key_input(getch(), menu_size, &key_position);
	}
}

// ##### toolkit for make functions #####
// initialize usrinfo structure
void set_usrinfo(usrinfo *user)
{
	memset(user->id, 0x00, MAX_ID_LENGTH);
	memset(user->password, 0x00, MAX_PASSWORD_LENGTH);
	user->win = 0;
	user->lose = 0;
}

// calculate percentage of victory and return
double calc_percentage(usrinfo *user)
{
	double percentage;
	if (user->win == 0)
		return 0;
	percentage = 100 * ((double)(user->win) / ((user->win) + (user->lose)));
	return percentage;
}

// process input from keyboad
void process_key_input(int ch, int menu_size, int *p_key_pos)
{
	usrinfo *user;

	int cur_y = 6 + (cur_row * 2);
	int cur_x = 17 + (cur_col * 4);

	message *write_msg = (message *)malloc(MSG_LENGTH);
	message *read_msg = (message *)malloc(MSG_LENGTH);

	memset(write_msg, 0x00, MSG_LENGTH);
	memset(read_msg, 0x00, MSG_LENGTH);

	switch (win_num)
	{
	case MAIN:
		if (ch == KEY_LEFT)
		{
			if ((*p_key_pos) == 0)
				(*p_key_pos) = menu_size - 1;
			else
				--(*p_key_pos);
		}
		else if (ch == KEY_RIGHT)
		{
			if ((*p_key_pos) == menu_size - 1)
				(*p_key_pos) = 0;
			else
				++(*p_key_pos);
		}
		else if (ch == 10)
		{
			window_controller(p_key_pos);
		}
		break;
	case SIGN_IN:
		if (ch == KEY_LEFT)
		{
			if ((*p_key_pos) == 0)
				(*p_key_pos) = menu_size - 1;
			else
				--(*p_key_pos);
		}
		else if (ch == KEY_RIGHT)
		{
			if ((*p_key_pos) == menu_size - 1)
				(*p_key_pos) = 0;
			else
				++(*p_key_pos);
		}
		else if (ch == 10)
		{
			window_controller(p_key_pos);
		}
		break;
	case SIGN_UP:
		if (is_on_menubar == 0)
		{
			user = (usrinfo *)malloc(sizeof(usrinfo));
			set_usrinfo(user);

			wmove(screen, 9, 37);
			wrefresh(screen);
			process_id_io(&ch, user);

			wmove(screen, 11, 37); // move to password
			wrefresh(screen);
			ch = 0;

			process_password_io(&ch, user);
			move(18 + MBY, MBX_SIGN_UP); // move to signin
			wmove(menubar, MBY, MBX_SIGN_UP);
			wrefresh(menubar);

			strcpy(login_user->id, user->id);
			strcpy(id_user->id, login_user->id);

			is_on_menubar = 1;

			free(user);
		}
		else if (is_on_menubar == 1)
		{
			if (ch == KEY_LEFT)
			{
				if ((*p_key_pos) == 0)
					(*p_key_pos) = menu_size - 1;
				else
					--(*p_key_pos);
			}
			else if (ch == KEY_RIGHT)
			{
				if ((*p_key_pos) == menu_size - 1)
					(*p_key_pos) = 0;
				else
					++(*p_key_pos);
			}
			else if (ch == 10)
			{
				window_controller(p_key_pos);
				is_on_menubar = 0;
			}
		}
		break;
	case LOGIN:
		if (is_on_menubar == 0)
		{
			user = (usrinfo *)malloc(sizeof(usrinfo));
			set_usrinfo(user);

			wmove(screen, 9, 37);
			wrefresh(screen);
			process_id_io(&ch, user);

			wmove(screen, 11, 37); // move to password
			wrefresh(screen);
			ch = 0;

			process_password_io(&ch, user);
			move(18 + MBY, MBX_LOGIN); // move to signin
			wmove(menubar, MBY, MBX_LOGIN);
			wrefresh(menubar);

			is_on_menubar = 1;

			strcpy(login_user->id, user->id);
			strcpy(id_user->id, login_user->id);

			free(user);
		}
		else if (is_on_menubar == 1)
		{
			if (ch == KEY_LEFT)
			{
				if ((*p_key_pos) == 0)
					(*p_key_pos) = menu_size - 1;
				else
					--(*p_key_pos);
			}
			else if (ch == KEY_RIGHT)
			{
				if ((*p_key_pos) == menu_size - 1)
					(*p_key_pos) = 0;
				else
					++(*p_key_pos);
			}
			else if (ch == 10)
			{
				window_controller(p_key_pos);
				is_on_menubar = 0;
			}
		}
		break;
	case WITHDRAWAL:
		if (is_on_menubar == 0)
		{
			user = (usrinfo *)malloc(sizeof(usrinfo));
			set_usrinfo(user);

			// just assignment login_user->id
			strcpy(user->id, login_user->id);

			wmove(screen, 11, 35); // move to password
			wrefresh(screen);

			// checking password
			process_password_io(&ch, user);
			move(18 + MBY, MBX_WITHDRAWAL); // move to signin
			wmove(menubar, MBY, MBX_WITHDRAWAL);
			wrefresh(menubar);

			is_on_menubar = 1;

			free(user);
		}
		else if (is_on_menubar == 1)
		{
			if (ch == KEY_LEFT)
			{
				if ((*p_key_pos) == 0)
					(*p_key_pos) = menu_size - 1;
				else
					--(*p_key_pos);
			}
			else if (ch == KEY_RIGHT)
			{
				if ((*p_key_pos) == menu_size - 1)
					(*p_key_pos) = 0;
				else
					++(*p_key_pos);
			}
			else if (ch == 10)
			{
				window_controller(p_key_pos);
				is_on_menubar = 0;
			}
		}

		break;
	case STATISTICS_WAITING:
		window_controller(p_key_pos);
		break;
	case STATISTICS_OK:
		while (!is_ok_clicked)
		{
			if (getch() == 10)
				break;
		}
		window_controller(p_key_pos);
		break;
	case IN_GAME:
		if (is_on_menubar == 0)
		{
			if (ch == KEY_LEFT)
			{
				if (cur_col == 0)
					cur_col = 5;
				else
					--cur_col;
			}
			else if (ch == KEY_RIGHT)
			{
				if (cur_col == 5)
					cur_col = 0;
				else
					++cur_col;
			}
			else if (ch == KEY_UP)
			{
				if (cur_row == 0)
					cur_row = 5;
				else
					--cur_row;
			}
			else if (ch == KEY_DOWN)
			{
				if (cur_row == 5)
					cur_row = 0;
				else
					++cur_row;
			}
			else if (ch == 'n')
			{
				is_on_menubar = 1;
				(*p_key_pos) = 0;
			}
			else if (ch == 'r')
			{
				is_on_menubar = 1;
				(*p_key_pos) = 1;
			}
			else if (ch == 'x')
			{
				is_on_menubar = 1;
				(*p_key_pos) = 2;
			}
			game_controller();
		}
		else if (is_on_menubar == 1)
		{
			if (ch == 'g')
			{
				is_on_menubar = 0;
				(*p_key_pos) = 1;

				game_controller();
			}
			else if (ch == KEY_UP)
			{
				if ((*p_key_pos) == 0)
					(*p_key_pos) = menu_size - 1;
				else
					--(*p_key_pos);
			}
			else if (ch == KEY_DOWN)
			{
				if ((*p_key_pos) == menu_size - 1)
					(*p_key_pos) = 0;
				else
					++(*p_key_pos);
			}
			else if (ch == 10)
			{
				window_controller(p_key_pos);
			}
		}
		break;
	}
	wrefresh(screen);
	wrefresh(menubar);
	free(write_msg);
	free(read_msg);
}

// highlight menubar
void menubar_highlighter(char *menu_arr[], int menu_gap, int menu_size, int key_pos)
{
	int y, x, i;

	y = MBY;
	switch (win_num)
	{
	case MAIN:
		x = MBX_MAIN;
		break;
	case SIGN_IN:
		x = MBX_SIGN_IN;
		break;
	case SIGN_UP:
		x = MBX_SIGN_UP;
		break;
	case LOGIN:
		x = MBX_LOGIN;
		break;
	case WITHDRAWAL:
		x = MBX_WITHDRAWAL;
		break;
	case STATISTICS_WAITING:
		x = MBX_STATISTICS_WAITING;
		break;
	case STATISTICS_OK:
		x = MBX_STATISTICS_OK;
		break;
	case IN_GAME:
		x = MBX_IN_GAME;
		y = MBY_IN_GAME;
		break;
	}

	wmove(menubar, y, x);
	if (win_num == IN_GAME)
	{
		if (is_on_menubar == 1)
		{
			if (key_pos == 0)
			{
				// next turn
				wattron(menubar, A_REVERSE);
				wattron(menubar, A_UNDERLINE);
				mvwprintw(menubar, y - 4, x, "N");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "EXT TURN");
				wattroff(menubar, A_REVERSE);
				// regame
				wattron(menubar, A_UNDERLINE);
				mvwprintw(menubar, y - 2, x, "R");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "EGAME");
				// exit
				mvwprintw(menubar, y, x, "E");
				wattron(menubar, A_UNDERLINE);
				wprintw(menubar, "X");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "IT");
			}
			else if (key_pos == 1)
			{
				// next turn
				wattron(menubar, A_UNDERLINE);
				mvwprintw(menubar, y - 4, x, "N");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "EXT TURN");
				// regame
				wattron(menubar, A_REVERSE);
				wattron(menubar, A_UNDERLINE);
				mvwprintw(menubar, y - 2, x, "R");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "EGAME");
				wattroff(menubar, A_REVERSE);
				// exit
				mvwprintw(menubar, y, x, "E");
				wattron(menubar, A_UNDERLINE);
				wprintw(menubar, "X");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "IT");
			}
			else if (key_pos == 2)
			{
				// next turn
				wattron(menubar, A_UNDERLINE);
				mvwprintw(menubar, y - 4, x, "N");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "EXT TURN");
				// regame
				wattron(menubar, A_UNDERLINE);
				mvwprintw(menubar, y - 2, x, "R");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "EGAME");
				//exit
				wattron(menubar, A_REVERSE);
				mvwprintw(menubar, y, x, "E");
				wattron(menubar, A_UNDERLINE);
				wprintw(menubar, "X");
				wattroff(menubar, A_UNDERLINE);
				wprintw(menubar, "IT");
				wattroff(menubar, A_REVERSE);
			}
		}
		else if (is_on_menubar == 0)
		{
			// next turn
			wattron(menubar, A_UNDERLINE);
			mvwprintw(menubar, y - 4, x, "N");
			wattroff(menubar, A_UNDERLINE);
			wprintw(menubar, "EXT TURN");
			// regame
			wattron(menubar, A_UNDERLINE);
			mvwprintw(menubar, y - 2, x, "R");
			wattroff(menubar, A_UNDERLINE);
			wprintw(menubar, "EGAME");
			// exit
			mvwprintw(menubar, y, x, "E");
			wattron(menubar, A_UNDERLINE);
			wprintw(menubar, "X");
			wattroff(menubar, A_UNDERLINE);
			wprintw(menubar, "IT");
		}
	}
	else if (menu_size == 1)
	{
		if (!strcmp(menu_arr[0], "WAITING")) // don't highlight waiting
		{
			mvwprintw(menubar, y, x, "%s", menu_arr[0]);
		}
		else if (is_on_menubar == 1) // just highlighting one button
		{
			wattron(menubar, A_REVERSE);
			mvwprintw(menubar, y, x, "%s", menu_arr[0]);
			wattroff(menubar, A_REVERSE);
		}
		else if (is_on_menubar == 0)
		{
			mvwprintw(menubar, y, x, "%s", menu_arr[0]);
		}
	}
	else
	{
		for (i = 0; i < menu_size; i++)
		{
			if (key_pos == i)
			{
				wattron(menubar, A_REVERSE);
				mvwprintw(menubar, y, x + (i * menu_gap), "%s", menu_arr[i]);
				wattroff(menubar, A_REVERSE);
			}
			else
				mvwprintw(menubar, y, x + (i * menu_gap), "%s", menu_arr[i]);
		}
	}
	wrefresh(menubar);
}

// process id reading and writing on file
void process_id_io(int *ch, usrinfo *user)
{
	//int fd = 0;
	int y, x, len;
	char temp;

	message *write_msg = (message *)malloc(MSG_LENGTH);
	message *read_msg = (message *)malloc(MSG_LENGTH);

	memset(write_msg, 0x00, MSG_LENGTH);
	memset(read_msg, 0x00, MSG_LENGTH);

	while ((*ch) != 10) // if not enter
	{
		if ((*ch) == 127) // if backspace
		{
			getyx(screen, y, x);
			if (x == 37)
			{
				(*ch) = getch();
				continue;
			}
			mvwprintw(screen, y, x - 1, " ");
			wmove(screen, y, x - 1);
			wrefresh(screen);

			// delete one character at id
			len = strlen(user->id);
			user->id[len - 1] = 0x00;

			(*ch) = getch();
			continue;
		}
		temp = (*ch);
		wprintw(screen, "%c", temp);
		wrefresh(screen);
		strcat(user->id, &temp);
		(*ch) = getch();
	}
	wrefresh(screen);

	// send usrinfo to server
	memcpy(&(write_msg->user), user, INFO_LENGTH);
	write_msg->fname = ID;
	write_msg->win = win_num;
	write_msg->is_password_matched = is_password_matched;
	write_msg->exist_id = exist_id;
	write_msg->is_now_logined = 0;

	write(sock, (message *)write_msg, MSG_LENGTH);

	// read msg from server
	read(sock, (message *)read_msg, MSG_LENGTH);

	// set global variables to read_msg
	is_password_matched = read_msg->is_password_matched;
	exist_id = read_msg->exist_id;
	is_now_logined = read_msg->is_now_logined;
}

// process password reading and writing on file
void process_password_io(int *ch, usrinfo *user)
{
	int nbytes = 1;
	int fd, cur_pos, y, x, len;
	char temp;

	message *write_msg = (message *)malloc(MSG_LENGTH);
	message *read_msg = (message *)malloc(MSG_LENGTH);

	usrinfo *info_buf = (usrinfo *)malloc(INFO_LENGTH);
	usrinfo *null_buf = (usrinfo *)malloc(INFO_LENGTH);

	memset(write_msg, 0x00, MSG_LENGTH);
	memset(read_msg, 0x00, MSG_LENGTH);

	set_usrinfo(info_buf);
	set_usrinfo(null_buf);

	if (win_num == WITHDRAWAL)
	{
		while ((*ch) != 10)
		{
			if ((*ch) == 127) // if backspace
			{
				getyx(screen, y, x);
				if (x == 35)
				{
					(*ch) = getch();
					continue;
				}
				mvwprintw(screen, y, x - 1, " ");
				wmove(screen, y, x - 1);
				wrefresh(screen);

				// delete one character at password
				len = strlen(user->password);
				user->password[len - 1] = 0x00;
				user->password[len - 2] = 0x00;

				(*ch) = getch();
				continue;
			}

			temp = (*ch);
			wprintw(screen, "*");
			wrefresh(screen);
			strcat(user->password, &temp);

			wrefresh(screen);
			(*ch) = getch();
		}
	}
	else
	{
		while ((*ch) != 10)
		{
			(*ch) = getch();
			if ((*ch) == 10)
				break;
			if ((*ch) == 127)
			{
				getyx(screen, y, x);
				if (x == 37)
				{
					(*ch) = 0;
					continue;
				}
				mvwprintw(screen, y, x - 1, " ");
				wmove(screen, y, x - 1);

				// delete one character at password
				len = strlen(user->password);
				user->password[len - 1] = 0x00;
				user->password[len - 2] = 0x00;

				wrefresh(screen);

				(*ch) = 0;
				continue;
			}

			temp = (*ch);
			wprintw(screen, "*");
			wrefresh(screen);
			strcat(user->password, &temp);

			wrefresh(screen);
		}
	}
	wrefresh(screen);

	// send msg to server
	memcpy(&(write_msg->user), user, INFO_LENGTH);
	write_msg->fname = PASSWORD;
	write_msg->win = win_num;
	write_msg->is_password_matched = is_password_matched;
	write_msg->exist_id = exist_id;
	write_msg->is_now_logined = is_now_logined;

	write(sock, (message *)write_msg, MSG_LENGTH);

	// read msg from server
	read(sock, (message *)read_msg, MSG_LENGTH);

	// set global variables to read_msg
	is_password_matched = read_msg->is_password_matched;
	exist_id = read_msg->exist_id;
	is_now_logined = read_msg->is_now_logined;

	free(write_msg);
	free(read_msg);
}

// search file by usrifo pointer
int file_search(int fd, int length, char *target)
{
	int nbytes = 1;

	usrinfo *buf = (usrinfo *)malloc(sizeof(usrinfo));

	lseek(fd, 0, SEEK_SET);

	while (nbytes > 0)
	{
		set_usrinfo(buf);

		nbytes = read(fd, (usrinfo *)buf, length);
		if (!strcmp(buf->id, target))
		{
			free(buf);
			return 0;
		}
	}

	free(buf);
	return -1;
}

// initialize game
void set_game()
{
	int i, start_x, start_y;

	for (i = 0; i < BOARD_ROW; i++)
		memset(game_board[i], ' ', BOARD_COL);
	game_board[2][2] = 'O';
	game_board[2][3] = 'X';
	game_board[3][2] = 'X';
	game_board[3][3] = 'O';

	cur_row = 2;
	cur_col = 2;

	game_cursor_highlighter();
}

// highlight cursor on game board
void game_cursor_highlighter()
{
	int i, j, cur_x, cur_y;

	cur_y = 6;
	cur_x = 17;

	move(cur_y, cur_x);
	wmove(screen, cur_y, cur_x);
	wrefresh(screen);

	for (i = 0; i < 6; i++) // row
	{
		for (j = 0; j < 6; j++) // col
		{
			cur_y = 6 + (i * 2);
			cur_x = 17 + (j * 4);

			if (is_on_menubar == 0 && i == cur_row && j == cur_col)
			{
				wattron(screen, A_REVERSE);
				mvwprintw(screen, cur_y, cur_x - 1, " %c ", game_board[i][j]);
				wattroff(screen, A_REVERSE);
			}
			else
				mvwprintw(screen, cur_y, cur_x - 1, " %c ", game_board[i][j]);
		}
	}
	wrefresh(screen);
}

// ##### controller functions #####
// change window
void window_controller(int *p_key_pos)
{
	message *write_msg = (message *)malloc(MSG_LENGTH);
	message *read_msg = (message *)malloc(MSG_LENGTH);

	memset(write_msg, 0x00, MSG_LENGTH);
	memset(read_msg, 0x00, MSG_LENGTH);

	switch (win_num)
	{
	case MAIN:
		if ((*p_key_pos) == 0) // sign in
		{
			window_login();
			(*p_key_pos) = 0;
			is_on_menubar = 0;
		}
		else if ((*p_key_pos) == 1) // sign up
		{
			window_signup();
			(*p_key_pos) = 0;
			is_on_menubar = 0;
		}
		else if ((*p_key_pos) == 2) // exit
		{
			finalize();
			exit(0);
		}
		break;
	case SIGN_IN:
		if ((*p_key_pos) == 0) // play
		{
			// send message to server
			write_msg->fname = W_CONTROL;
			write_msg->win = win_num + 10;
			memcpy(&(write_msg->user), login_user, INFO_LENGTH);
			write(sock, (message *)write_msg, MSG_LENGTH);
			// for good communication
			read(sock, (message *)read_msg, MSG_LENGTH);

			playing_num = read_msg->playing_num;
			clnt_id = read_msg->playing_num;

			memcpy(id_user, login_user, INFO_LENGTH);

			if (clnt_id == 1) // player who click play first
			{
				memcpy(logined_user, login_user, INFO_LENGTH);
				memcpy(login_user, &(read_msg->user), INFO_LENGTH);
			}
			else if (clnt_id == 2)
			{
				memcpy(logined_user, &(read_msg->user), INFO_LENGTH);
			}

			window_statistics_waiting();
			(*p_key_pos) = 0;
			is_on_menubar = 1;
		}
		else if ((*p_key_pos) == 1) // sign out
		{
			// send signout success to server
			write_msg->fname = W_CONTROL;
			write_msg->win = win_num;
			memcpy(&(write_msg->user), id_user, INFO_LENGTH);
			write(sock, (message *)write_msg, MSG_LENGTH);
			// for good communication
			read(sock, (message *)read_msg, MSG_LENGTH);

			memset(login_user->id, 0x00, MAX_ID_LENGTH);
			window_main();
			(*p_key_pos) = 0;
			is_on_menubar = 1;
		}
		else if ((*p_key_pos) == 2) // withdrawal
		{
			window_withdrawal();
			(*p_key_pos) = 0;
			is_on_menubar = 0;
		}
		break;
		break;
	case SIGN_UP:
		if ((*p_key_pos) == 0) // sign up
		{
			// if there are same id
			if (exist_id)
			{
				move(23, 0);
				wmove(menubar, 5, 0);
				wrefresh(menubar);

				mvwprintw(menubar, 5, 0, ">>> %s has already exist in DB! (Press any key...)", login_user->id);
				wrefresh(menubar);
				getch();
				window_signup();
				(*p_key_pos) = 0;
				is_on_menubar = 0;
			}
			else
			{
				move(23, 0);
				wmove(menubar, 5, 0);
				wrefresh(menubar);

				mvwprintw(menubar, 5, 0, ">>> Welcome to OTHELLO World! (Press any key...)");
				wrefresh(menubar);

				// send signup success to server
				write_msg->fname = W_CONTROL;
				write_msg->win = win_num;
				memcpy(&(write_msg->user), login_user, INFO_LENGTH);
				write(sock, (message *)write_msg, MSG_LENGTH);
				// for good communication
				read(sock, (message *)read_msg, MSG_LENGTH);

				getch();
				window_main();
				(*p_key_pos) = 0;
				is_on_menubar = 1;
			}
		}
		else if ((*p_key_pos) == 1) // back
		{
			window_main();
			(*p_key_pos) = 0;
			is_on_menubar = 1;
		}
		break;
	case LOGIN:
		if ((*p_key_pos) == 0) // sign in
		{
			if (is_now_logined) // if the id is logged in already
			{
				move(23, 0);
				wmove(menubar, 5, 0);
				wrefresh(menubar);
				mvwprintw(menubar, 5, 0, ">>> Id %s is already logged in. (Press any key...)", login_user->id);
				wrefresh(menubar);
				getch();
				window_login();
				(*p_key_pos) = 0;
				is_on_menubar = 0;
			}
			else if (exist_id) // if there are same id
			{
				if (is_password_matched == 1)
				{
					// send login success to server
					write_msg->fname = W_CONTROL;
					write_msg->win = win_num;
					memcpy(&(write_msg->user), login_user, INFO_LENGTH);
					write(sock, (message *)write_msg, MSG_LENGTH);
					// for good communication
					read(sock, (message *)read_msg, MSG_LENGTH);

					window_signin();
					(*p_key_pos) = 0;
					is_on_menubar = 1;
				}
				else if (is_password_matched == 0)
				{
					move(23, 0);
					wmove(menubar, 5, 0);
					wrefresh(menubar);
					mvwprintw(menubar, 5, 0, ">>> Password isn't matched. (Press any key...)");
					wrefresh(menubar);
					getch();
					window_login();
					(*p_key_pos) = 0;
					is_on_menubar = 0;
				}
			}
			else
			{
				move(23, 0);
				wmove(menubar, 5, 0);
				wrefresh(menubar);
				mvwprintw(menubar, 5, 0, ">>> There is no same id. (Press any key...)");
				wrefresh(menubar);
				getch();
				window_login();
				(*p_key_pos) = 0;
				is_on_menubar = 0;
			}
		}
		else if ((*p_key_pos) == 1) // back
		{
			window_main();
			(*p_key_pos) = 0;
			is_on_menubar = 1;
		}
		break;
	case WITHDRAWAL:
		if ((*p_key_pos) == 0)
		{
			// if password is matched
			if (is_password_matched == 1)
			{
				move(23, 0);
				wmove(menubar, 5, 0);
				wrefresh(menubar);
				mvwprintw(menubar, 5, 0, ">>> Withdrawal success. (Press any key...)");
				wrefresh(menubar);

				// send withdrawal success to server
				write_msg->fname = W_CONTROL;
				write_msg->win = win_num;
				memcpy(&(write_msg->user), id_user, INFO_LENGTH);
				write(sock, (message *)write_msg, MSG_LENGTH);
				// for good communication
				read(sock, (message *)read_msg, MSG_LENGTH);

				getch();
				window_main();
				(*p_key_pos) = 0;
				is_on_menubar = 1;
			}
			else if (is_password_matched == 0)
			{
				move(23, 0);
				wmove(menubar, 5, 0);
				wrefresh(menubar);
				mvwprintw(menubar, 5, 0, ">>> Password isn't matched. (Press any key...)");
				wrefresh(menubar);
				getch();
				window_withdrawal();
				(*p_key_pos) = 0;
				is_on_menubar = 0;
			}
		}
		else if ((*p_key_pos) == 1)
		{
			window_signin();
			(*p_key_pos) = 0;
			is_on_menubar = 1;
		}
		break;
	case STATISTICS_WAITING:
		// waiting for 2 clients are connected
		while (playing_num == 1)
		{
			write_msg->fname = W_CONTROL;
			write_msg->win = win_num;
			write(sock, (message *)write_msg, MSG_LENGTH);

			read(sock, (message *)read_msg, MSG_LENGTH);
			playing_num = read_msg->playing_num;
		}
		window_statistics_ok();
		break;
	case STATISTICS_OK:
		if ((*p_key_pos) == 0)
		{
			is_on_menubar = 0;
			(*p_key_pos) = 1;

			// synchronize ok button click
			while (!is_ok_clicked)
			{
				//getch();
				write_msg->fname = W_CONTROL;
				write_msg->win = win_num;
				write(sock, (message *)write_msg, MSG_LENGTH);

				read(sock, (message *)read_msg, MSG_LENGTH);
				is_ok_clicked = read_msg->is_ok_clicked;
			}

			window_ingame();

			set_game();
		}
		break;
	case IN_GAME:
		if ((*p_key_pos) == 0) // next turn
		{
		}
		else if ((*p_key_pos) == 1) // regame
		{
		}
		else if ((*p_key_pos) == 2) // exit
		{
			window_signin();
			(*p_key_pos) = 0;
			is_on_menubar = 1;
		}
		break;
	default:
		return;
	}

	free(write_msg);
	free(read_msg);
}

// control the game
void game_controller()
{
	game_cursor_highlighter(game_board);
}

// ##### draw windows for othello #####
// main
void window_main()
{
	win_num = MAIN;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 6, 28, "System Software Practice");
	mvwprintw(screen, 9, 36, "OTHELLO");

	mvwprintw(screen, 14, 65, "2017203081");
	mvwprintw(screen, 16, 65, "Jeongsang Im");

	// print menubar contents
	mvwprintw(menubar, MBY, MBX_MAIN, "SIGN IN");
	mvwprintw(menubar, MBY, MBX_MAIN + MBG_MAIN, "SIGN UP");
	mvwprintw(menubar, MBY, MBX_MAIN + (2 * MBG_MAIN), "EXIT");

	// move cursor to SIGN IN
	wmove(menubar, 3, 10);

	// refresh screens
	wrefresh(screen);
	wrefresh(menubar);
}

// sign in
void window_signin()
{
	win_num = SIGN_IN;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 6, 28, "PLAYER ID : %s", id_user->id);

	// print menubar contents
	mvwprintw(menubar, MBY, MBX_SIGN_IN, "PLAY");
	mvwprintw(menubar, MBY, MBX_SIGN_IN + MBG_SIGN_IN, "SIGN OUT");
	mvwprintw(menubar, MBY, MBX_SIGN_IN + (2 * MBG_SIGN_IN), "WITHDRAWAL");

	// move cursor to PLAY
	move(18 + MBY, MBX_SIGN_IN);

	wrefresh(screen);
	wrefresh(menubar);
}

// sign up
void window_signup()
{
	win_num = SIGN_UP;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 7, 37, "SIGN UP");
	mvwprintw(screen, 9, 32, "ID : ");
	mvwprintw(screen, 11, 26, "PASSWORD : ");

	// print menubar contens
	mvwprintw(menubar, MBY, MBX_SIGN_UP, "SIGN UP");
	mvwprintw(menubar, MBY, MBX_SIGN_UP + MBG_SIGN_UP, "BACK");

	// move cursor to ID :
	move(9, 37);

	wrefresh(screen);
	wrefresh(menubar);
}

// login
void window_login()
{
	win_num = LOGIN;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 7, 37, "SIGN IN");
	mvwprintw(screen, 9, 32, "ID : ");
	mvwprintw(screen, 11, 26, "PASSWORD : ");

	// print menubar contens
	mvwprintw(menubar, MBY, MBX_LOGIN, "SIGN IN");
	mvwprintw(menubar, MBY, MBX_LOGIN + MBG_LOGIN, "BACK");

	// move cursor to ID :
	move(9, 37);

	wrefresh(screen);
	wrefresh(menubar);
}

// withdrawal
void window_withdrawal()
{
	win_num = WITHDRAWAL;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 7, 35, "WITHDRAWAL");
	mvwprintw(screen, 9, 29, "PLAYER ID : %s", id_user->id);
	mvwprintw(screen, 11, 23, "PASSWORD : ");

	// print menubar contens
	mvwprintw(menubar, MBY, MBX_WITHDRAWAL, "WITHDRAWAL");
	mvwprintw(menubar, MBY, MBX_WITHDRAWAL + MBG_WITHDRAWAL, "BACK");

	// move cursor to PASSWORD :
	move(11, 35);

	wrefresh(screen);
	wrefresh(menubar);
}

// statistics waiting(when 1 client is connected)
void window_statistics_waiting()
{
	win_num = STATISTICS_WAITING;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 7, 9, "PLAYER1 ID : %s", logined_user->id);
	mvwprintw(screen, 9, 14, "STATISTICS");
	mvwprintw(screen, 11, 7, "WIN : %d / LOSE : %d (%.3lf%)", logined_user->win, logined_user->lose, calc_percentage(logined_user));

	mvwprintw(screen, 7, 49, "PLAYER2 ID :   ");
	mvwprintw(screen, 9, 54, "STATISTICS");
	mvwprintw(screen, 11, 47, "WIN :    / LOSE :    (      %)");

	// print menubar content
	mvwprintw(menubar, MBY, MBX_STATISTICS_WAITING, "WAITING");

	// move cursor to OK
	move(18 + MBY, MBX_STATISTICS_WAITING);

	wrefresh(screen);
	wrefresh(menubar);
}

// statistics ok(when 2 client is connected)
void window_statistics_ok()
{
	win_num = STATISTICS_OK;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(18, 80, 0, 0);
	menubar = newwin(6, 80, 18, 0);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 7, 9, "PLAYER1 ID : %s", logined_user->id);
	mvwprintw(screen, 9, 14, "STATISTICS");
	mvwprintw(screen, 11, 7, "WIN : %d / LOSE : %d (%.3lf%)", logined_user->win, logined_user->lose, calc_percentage(logined_user));

	mvwprintw(screen, 7, 49, "PLAYER2 ID : %s", login_user->id);
	mvwprintw(screen, 9, 54, "STATISTICS");
	mvwprintw(screen, 11, 47, "WIN : %d / LOSE : %d (%.3lf%)", login_user->win, login_user->lose, calc_percentage(login_user));

	// print menubar content
	mvwprintw(menubar, MBY, MBX_STATISTICS_OK, "OK");

	// move cursor to OK
	move(18 + MBY, MBX_STATISTICS_OK);

	wrefresh(screen);
	wrefresh(menubar);
}

// in game
void window_ingame()
{
	win_num = IN_GAME;

	// delete previous window
	if (screen != NULL)
		wclear(screen);
	if (menubar != NULL)
		wclear(menubar);
	wrefresh(screen);
	wrefresh(menubar);

	delwin(screen);
	delwin(menubar);

	// make windows
	screen = newwin(24, 60, 0, 0);
	menubar = newwin(24, 20, 0, 60);

	// set window color
	wbkgd(screen, COLOR_PAIR(1));
	wbkgd(menubar, COLOR_PAIR(2));

	// print screen contents
	mvwprintw(screen, 5, 15, "+---+---+---+---+---+---+");
	mvwprintw(screen, 6, 15, "|   |   |   |   |   |   |");
	mvwprintw(screen, 7, 15, "+---+---+---+---+---+---+");
	mvwprintw(screen, 8, 15, "|   |   |   |   |   |   |");
	mvwprintw(screen, 9, 15, "+---+---+---+---+---+---+");
	mvwprintw(screen, 10, 15, "|   |   | O | X |   |   |");
	mvwprintw(screen, 11, 15, "+---+---+---+---+---+---+");
	mvwprintw(screen, 12, 15, "|   |   | X | O |   |   |");
	mvwprintw(screen, 13, 15, "+---+---+---+---+---+---+");
	mvwprintw(screen, 14, 15, "|   |   |   |   |   |   |");
	mvwprintw(screen, 15, 15, "+---+---+---+---+---+---+");
	mvwprintw(screen, 16, 15, "|   |   |   |   |   |   |");
	mvwprintw(screen, 17, 15, "+---+---+---+---+---+---+");

	// print players
	mvwprintw(menubar, MBY_IN_GAME - 8, MBX_IN_GAME, "%s(O) : 2", logined_user->id);
	mvwprintw(menubar, MBY_IN_GAME - 7, MBX_IN_GAME, "%s(X) : 2", login_user->id);

	// print menubar contents
	mvwprintw(menubar, MBY_IN_GAME - 4, MBX_IN_GAME, "NEXT TURN");
	mvwprintw(menubar, MBY_IN_GAME - 2, MBX_IN_GAME, "REGAME");
	mvwprintw(menubar, MBY_IN_GAME, MBX_IN_GAME, "EXIT");

	// move cursor to gameboard
	move(10, 25);

	wrefresh(screen);
	wrefresh(menubar);
}
