#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

/* constants */
// buf size for socket communication
#define BUFSIZE 1024

// length of id and password
#define MAX_ID_LENGTH 30
#define MAX_PASSWORD_LENGTH 30

// file name of user information
#define FILE_NAME "users.txt"

// length of usrinfo structure
#define INFO_LENGTH 1 * MAX_ID_LENGTH + 1 * MAX_PASSWORD_LENGTH + 2 * (sizeof(int))
// length of message structure
#define MSG_LENGTH INFO_LENGTH + 7 * (sizeof(int))

// window states
typedef enum _window
{
	MAIN,
	SIGN_IN,
	SIGN_UP,
	LOGIN,
	WITHDRAWAL,
	STATISTICS_WAITING,
	STATISTICS_OK,
	IN_GAME,

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

// ## global variable ##
// number of accepted client
int clnt_num = 0;

// for check current window
int win_num = MAIN;

// now logined users
usrinfo now_logins[2];
// number of users who clicked play button
int playing_num = 0;

// mutex for syncronization
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* function declarations */
void error_handling(char *);
int file_search(int, int, char *);
void set_usrinfo(usrinfo *);
void delete_now_logins(usrinfo *);
int search_now_logins(usrinfo *);
// socket generate thread
void *thr_func(void *);

/* ##### main ##### */
int main(int argc, char **argv)
{
	int serv_sock;

	struct sockaddr_in serv_addr;

	// thread ids
	pthread_t tid[clnt_num];

	int clnt_addr_size;
	int clnt_sock[clnt_num];

	struct sockaddr_in clnt_addr;

	if (argc != 2)
	{
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("bind() error");

	while (1)
	{
		if (listen(serv_sock, 2) == -1)
			error_handling("listen() error");

		clnt_addr_size = sizeof(clnt_addr);
		clnt_sock[clnt_num] = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
		if (clnt_sock[clnt_num] == -1)
			error_handling("accept() error");
		else
			printf("CONNECT: %d\n", clnt_num + 1);

		pthread_create(&tid[clnt_num], NULL, &thr_func, &clnt_sock[clnt_num]);

		clnt_num++;
	}

	return 0;
}

void *thr_func(void *arg)
{
	int idx;

	// client id
	int clnt_id;

	int str_len;

	int clnt_sock = (*(int *)arg);

	int fd;
	int cur_pos;
	int nbytes = 1;

	usrinfo *info_buf = (usrinfo *)malloc(INFO_LENGTH);
	usrinfo *null_buf = (usrinfo *)malloc(INFO_LENGTH);

	message *read_msg = (message *)malloc(MSG_LENGTH);
	message *write_msg = (message *)malloc(MSG_LENGTH);

	set_usrinfo(info_buf);
	set_usrinfo(null_buf);

	memset(read_msg, 0x00, MSG_LENGTH);
	memset(write_msg, 0x00, MSG_LENGTH);

	write_msg->is_now_logined = 0;

	// set client id
	pthread_mutex_lock(&mutex);
	clnt_id = clnt_num;
	pthread_mutex_unlock(&mutex);

	// read socket
	while ((str_len = read(clnt_sock, (message *)read_msg, MSG_LENGTH)))
	{
		// initialize write_msg with read_msg
		memcpy(write_msg, read_msg, MSG_LENGTH);

		// file IO
		fd = open(FILE_NAME, O_RDWR | O_CREAT, 0666);
		lseek(fd, 0, SEEK_SET);

		if(read_msg->fname == PROC_KEY)
		{
			switch(read_msg->win)
			{
			
			}
		}
		else if (read_msg->fname == ID)
		{
			switch (read_msg->win)
			{
			case LOGIN:
				// if id exist
				if (!file_search(fd, INFO_LENGTH, read_msg->user.id) && strlen(read_msg->user.id) != 0)
					write_msg->exist_id = 1;
				else
					write_msg->exist_id = 0;
				break;
			case SIGN_UP:
				// if id doesn't exist
				if (file_search(fd, INFO_LENGTH, read_msg->user.id) && strlen(read_msg->user.id) != 0)
				{
					write_msg->exist_id = 0;
					write(fd, (usrinfo *)&(read_msg->user), INFO_LENGTH);
				}
				else
					write_msg->exist_id = 1;
				break;
			}
			close(fd);
		}
		else if (read_msg->fname == PASSWORD)
		{
			switch (read_msg->win)
			{
			case LOGIN:
				if ((search_now_logins(&(read_msg->user))) != 2)
				{ // if id is already logged in
					write_msg->is_now_logined = 1;
				}
				else if (!file_search(fd, INFO_LENGTH, read_msg->user.id) && strlen(read_msg->user.id) != 0)
				{ // if id exist
					lseek(fd, -(INFO_LENGTH), SEEK_CUR);
					read(fd, (usrinfo *)info_buf, INFO_LENGTH);
					if (!strcmp(info_buf->password, read_msg->user.password))
					{
						write_msg->is_password_matched = 1;
					}
					else
					{
						write_msg->is_password_matched = 0;
					}
				}
				break;
			case SIGN_UP:
				// if id doesn't exist
				if (file_search(fd, INFO_LENGTH, read_msg->user.id))
				{
					write_msg->exist_id = 0;
				}
				else
				{
					lseek(fd, -(INFO_LENGTH), SEEK_CUR);
					write(fd, (usrinfo *)&(read_msg->user), INFO_LENGTH);
				}
				break;
			case WITHDRAWAL:
				if (!file_search(fd, INFO_LENGTH, read_msg->user.id))
				{
					lseek(fd, -(INFO_LENGTH), SEEK_CUR);
					read(fd, (usrinfo *)info_buf, INFO_LENGTH);

					if (!strcmp(info_buf->password, read_msg->user.password))
					{
						write_msg->is_password_matched = 1;

						// delete user information(move last data to delete position)
						lseek(fd, -(INFO_LENGTH), SEEK_CUR);
						for (cur_pos = 0; nbytes > 0; cur_pos++)
						{
							set_usrinfo(info_buf);
							nbytes = read(fd, (usrinfo *)info_buf, INFO_LENGTH);
						}
						// dump 0x00 on last info
						lseek(fd, -(INFO_LENGTH), SEEK_END);
						write(fd, (usrinfo *)null_buf, INFO_LENGTH);

						if (cur_pos == 1)
						{
							free(info_buf);
							free(null_buf);
							close(fd);
							return 0;
						}
						// write last data on cur_pos
						lseek(fd, -(cur_pos * INFO_LENGTH), SEEK_END);
						write(fd, (usrinfo *)info_buf, INFO_LENGTH);
					}
					else
						write_msg->is_password_matched = 0;
				}
				break;
			}
			free(info_buf);
			free(null_buf);
			close(fd);
		}
		else if (read_msg->fname == W_CONTROL)
		{
			switch (read_msg->win)
			{
			case SIGN_UP:
				printf("SIGN UP: %s\n", read_msg->user.id);
				break;
			case LOGIN:
				printf("SIGN IN: %s\n", read_msg->user.id);

				pthread_mutex_lock(&mutex);
				memcpy(&(now_logins[clnt_num-1]), &(read_msg->user), INFO_LENGTH);
				pthread_mutex_unlock(&mutex);
				break;
			case SIGN_IN+10:	// when click play button
				if(clnt_id == 1)
					memcpy(&(write_msg->user), &(now_logins[1]), INFO_LENGTH);
				else
					memcpy(&(write_msg->user), &(now_logins[0]), INFO_LENGTH);
				write_msg->playing_num = ++playing_num;
				break;
			case SIGN_IN:
				printf("SIGN OUT: %s\n", read_msg->user.id);
				delete_now_logins(&(read_msg->user));
				break;
			case WITHDRAWAL:
				printf("WITHDRAWAL: %s\n", read_msg->user.id);
				delete_now_logins(&(read_msg->user));
				break;
			case STATISTICS_WAITING:
				memcpy(&(write_msg->user), &(now_logins[0]), INFO_LENGTH);
				write_msg->playing_num = playing_num;
				break;
			case STATISTICS_OK:
				write_msg->is_ok_clicked = 1;
				break;
			}
		}

		// send msg to client
		write(clnt_sock, (message *)write_msg, MSG_LENGTH);
	}
	close(clnt_sock);
}

// for soceket error hanldling
void error_handling(char *err_msg)
{
	fputs(err_msg, stderr);
	fputc('\n', stderr);
	exit(1);
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

// initialize usrinfo structure
void set_usrinfo(usrinfo *user)
{
	memset(user->id, 0x00, MAX_ID_LENGTH);
	memset(user->password, 0x00, MAX_PASSWORD_LENGTH);
	user->win = 0;
	user->lose = 0;
}

// if find target, return index. if not, return 2
int search_now_logins(usrinfo *target)
{
	int i;

	for (i = 0; i < 2; i++)
	{
		if (!strcmp(now_logins[i].id, target->id))
		{
			return i;
		}
	}

	return i;
}

// delete target from now_login[] and sort
void delete_now_logins(usrinfo *target)
{
	int idx = search_now_logins(target);

	if (idx == 2)
		return;
	else if(idx == 0)
	{
		pthread_mutex_lock(&mutex);
		clnt_num--;
		pthread_mutex_unlock(&mutex);
	}
	else
	{
		pthread_mutex_lock(&mutex);
		memcpy(&(now_logins[0]), &(now_logins[1]), INFO_LENGTH);
		clnt_num--;
		pthread_mutex_unlock(&mutex);
	}
}
