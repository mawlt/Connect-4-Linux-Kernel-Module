#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/random.h>

//function prototypes
static int dev_open(struct inode *inode, struct file *file);
static int dev_release(struct inode *inode, struct file *file);
static ssize_t dev_read(struct file *file, char __user *user_buff, size_t len, loff_t *offset);
static ssize_t dev_write(struct file *file, const char __user *user_buff, size_t len, loff_t *offset);
static char* fourinarow_devnode(struct device *dev, umode_t *mode);

static int hor_win(char color);
static int vert_win(char color);
static int diag_win(char color);


#define ROWS 9
#define COLS 9
#define TOTAL_BOARD 64	//always (ROWS - 1) * (COLS - 1)
#define BUFF_LEN 256	//supports boards as large as 15x15 since the board will be the largest thing being put in response_buffer

static char board[ROWS][COLS];		//game state variables
static char user_color = 'Y';
static int turn = 0;			//keeps track of who's turn it is, evens are for player turns and odds are for computer turns
static int available_space = TOTAL_BOARD;	//keeps track if the board fills up and a tie needs to be declared
static int win_flag = 0;
static int comp_win_flag = 0;
static int valid_comp_moves[COLS - 1];		//each element corresponds to a column on board and signals if they fill up so that the computer
static int game_in_progress = 0;		//doesn't keep on generating invalid moves

static char response_buffer[BUFF_LEN];	//what will be seen by the user if the user decides to look inside of fourinarow
static int response_len = 0;

static int major;
static struct class *fourinarow_class;		//dev variables
static struct device *fourinarow_device;

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
	.write = dev_write,
};

static int dev_open(struct inode *inode, struct file *file){
	if(imajor(inode) != major){
		printk(KERN_ALERT "fourinarow: device open attempt failed: wrong major number found\n");
		return -ENODEV;
	}
	printk(KERN_INFO "fourinarow: file opened\n");
	return 0;
}

static int dev_release(struct inode *inode, struct file *file){
	printk(KERN_INFO "fourinarow: file closed\n");
	return 0;
}

static ssize_t dev_read(struct file *file, char __user *user_buff, size_t len, loff_t *offset){
	ssize_t return_data_size;

	if(*offset >= response_len){	//checks if everything that needs to be sent has already been sent
		return 0;
	}

	if((response_len - *offset) < len){	//checks if there is anything left over from the last read that needs to be sent back to the user
		return_data_size = response_len - *offset;
	}
	else{					//otherwise sets the size of the return data to len
		return_data_size = len;
	}
	if(copy_to_user(user_buff, response_buffer + *offset, return_data_size) != 0){
		return -EFAULT;		//verifies that data was successfully copied to user_buff
	}
	*offset += return_data_size;
	return return_data_size;
}

static ssize_t dev_write(struct file *file, const char __user *user_buff, size_t len, loff_t *offset){
	char input[64] = {0};	//internal input buffer that holds the copied user input

	int i = 0;		//general purpose variables used throughout dev_write
	int k = 0;
	int row_indicator = ROWS - 1;
	char col_indicator = 'A';
	int flag = 0;           //general purpose flag

	//used in strncmp(input, "DROPC ", 6) == 0 case
	int user_col = 0;	//holds the index of the column user wants to drop token in

	//used in strncmp(input, "BOARD", 5) == 0
	int buff_ind = 0;	//keeps track of index in response_buffer being copied from board

	unsigned int random = 0;	//used for computer movement
	char comp_color = 'R';		//default color for computer is red

	if(len > sizeof(input)){
		return -EINVAL;
	}

	if(copy_from_user(input, user_buff, len) != 0){
		return -EFAULT;
	}

	input[len] = '\0';
	printk(KERN_INFO "fourinarow: user command successfully copied");

	if(strncmp(input, "RESET Y", 7) == 0 || strncmp(input, "RESET R", 7) == 0){
		col_indicator = 'A';
		row_indicator = ROWS - 1;
		memset(response_buffer, '\0', BUFF_LEN);
		for(i = 0; i < ROWS; i++){
			for(k = 0; k < COLS; k++){
				if(i == 0 && k == 0){
					board[i][k] = ' ';
				}
				if(i == 0){
					if(k > 0){				//the header row/column are generated exactly the same as before
						board[i][k] = col_indicator;	//provided board has already been generated
						col_indicator++;
					}
				}
				if(k == 0){
					if(i > 0){
						board[i][k] = '0' + row_indicator;
						row_indicator--;
					}
				}
				if(i > 0 && k > 0){		//resets everything except for the head row/column to 0
					board[i][k] = '0';
				}
			}
		}
		if(input[6] == 'Y'){		//resets game state variables
			user_color = 'Y';
		}
		else{
			user_color = 'R';
		}
		win_flag = 0;
		comp_win_flag = 0;
		turn = 0;
		available_space = TOTAL_BOARD;
		game_in_progress = 1;

		strcpy(response_buffer, "OK\n");
		response_len = strlen(response_buffer);
	}
	else if(strncmp(input, "BOARD", 5) == 0){
		memset(response_buffer, '\0', BUFF_LEN);
		if(game_in_progress == 0){
			strcpy(response_buffer, "NOGAME\n");
			response_len = strlen(response_buffer);
		}
		else{
			buff_ind = 0;
			for(i = 0; i < ROWS; i++){
				response_buffer[buff_ind++] = '\n';	//starts each row with a newline
				for(k = 0; k < COLS; k++){
					response_buffer[buff_ind++] = board[i][k];	//manually sets each element of response_buffer
				}							//its corresponding board element
			}
			response_buffer[buff_ind++] = '\n';
			response_buffer[buff_ind] = '\0';
			response_len = strlen(response_buffer);
			printk(KERN_INFO "fourinarow: response_buffer = %s\n", response_buffer);
			printk(KERN_INFO "fourinarow: response_len = %d\n", response_len);
		}
	}
	else if(strncmp(input, "DROPC ", 6) == 0){
		memset(response_buffer, '\0', BUFF_LEN);
		for(k = 1; k < COLS; k++){
			if(input[6] == col_indicator){
				flag = 1;
				user_col = k;
				break;
			}
			col_indicator++;
		}
		if(game_in_progress == 0){			//case: game is not in progress
                        strcpy(response_buffer, "NOGAME\n");
                        response_len = strlen(response_buffer);
                }
		else if(flag == 0){				//case: user put in an invalid column to drop token in
			snprintf(response_buffer, BUFF_LEN, "Invalid column\n");
			response_len = strlen(response_buffer);
		}
		else if(win_flag == 1){				//case: the game has already been won and needs to be reset
			strcpy(response_buffer, "WIN\n");
			response_len = strlen(response_buffer);
		}
		else if(comp_win_flag == 1){			//case: computer already won and game needs to be reset
			strcpy(response_buffer, "You let the computer win . . . \n");
			response_len = strlen(response_buffer);
		}
		else if(available_space == 0){			//case: the win flag is not set and there is no space to drop on the board so a tie
			strcpy(response_buffer, "TIE\n");
			response_len = strlen(response_buffer);
		}
		else if(turn % 2 != 0){				//case: it is not the user's turn
			strcpy(response_buffer, "OOT\n");
			response_len = strlen(response_buffer);
		}
		else{						//case: the user can try to drop token
			flag = 0;				//flag signals if a token has been dropped
			for(i = row_indicator; i > 0; i--){
				if(flag != 1){
					if(board[i][user_col] == '0'){
						board[i][user_col] = user_color;
						flag = 1;
						available_space--;		//updates game state variables
						turn++;
						break;
					}
				}
			}
			if(hor_win(user_color) == 1 || vert_win(user_color) == 1 || diag_win(user_color) == 1){
				strcpy(response_buffer, "WIN\n");		//case: user has won after their move, sets win flag
				response_len = strlen(response_buffer);
				win_flag = 1;
			}
			else if(flag == 0){					//case: column user is trying to drop in has no space left
				strcpy(response_buffer, "No space in column left, try a different column\n");
				response_len = strlen(response_buffer);
			}
			else if(available_space == 0){				//case: after user dropped token, there is no space left and no one has won
				strcpy(response_buffer, "TIE\n");		//tie
				response_len = strlen(response_buffer);
			}
			else{							//case: game on!
				strcpy(response_buffer, "OK\n");
				response_len = strlen(response_buffer);
			}
		}
	}
	else if(strncmp(input, "CTURN", 5) == 0){		//generally the same logic as DROPC but for the computer
		flag = 1;
		get_random_bytes(&random, sizeof(random));	//generates random number between 1 and COLS - 1
		random = (random % (COLS - 1)) + 1;
		if(valid_comp_moves[random] == 1){		//gives computer multiple chances to generate a valid move
			while(flag < COLS - 1){			//the flag < COLS - 1 is set just in case there are no valid moves
				get_random_bytes(&random, sizeof(random));
                		random = (random % (COLS - 1)) + 1;
				if(valid_comp_moves[random] == 0){
					flag = COLS;
				}
				flag++;				//flag will be reset later
			}
		}
		if(game_in_progress == 0){                      //case: game is not in progress
                        strcpy(response_buffer, "NOGAME\n");
                        response_len = strlen(response_buffer);
                }
		else if(win_flag == 1){                         //case: the game has already been won and needs to be reset
                        strcpy(response_buffer, "WIN\n");
                        response_len = strlen(response_buffer);
                }
		else if(comp_win_flag == 1){			//case: computer won
			strcpy(response_buffer, "Computer already won\n");
			response_len = strlen(response_buffer);
		}
                else if(available_space == 0){                  //case: the win flag is not set and there is no space to drop on the board so a tie
                        strcpy(response_buffer, "TIE\n");
                        response_len = strlen(response_buffer);
                }
                else if(turn % 2 != 1){                         //case: it is not the computer's turn
                        strcpy(response_buffer, "OOT\n");
                        response_len = strlen(response_buffer);
		}
		else{
			if(user_color == 'R'){
				comp_color = 'Y';
			}
			else{
				comp_color = 'R';
			}

			flag = 0;                               //flag signals if a token has been dropped
                        for(i = row_indicator; i > 0; i--){
                               	if(flag != 1){
                                       	if(board[i][random] == '0'){
                                               	board[i][random] = comp_color;
                                               	flag = 1;
                                               	available_space--;              //updates game state variables
                                               	turn++;
                                               	break;
                                       	}
                               	}
                       	}
			if(hor_win(comp_color) == 1 || vert_win(comp_color) == 1 || diag_win(comp_color) == 1){
                                strcpy(response_buffer, "WIN\n");               //case: comp has won after their move, sets computer win flag
                                response_len = strlen(response_buffer);
                                comp_win_flag = 1;
                        }
                        else if(flag == 0){                                     //case: computer did not generate valid move
                                strcpy(response_buffer, "Computer action invalid\n");
                                response_len = strlen(response_buffer);
				valid_comp_moves[random] = 1;			//indicates that that column is invalid for computer
                        }
                        else if(available_space == 0){                          //case: after comp move, there is no space left and no one has won
                                strcpy(response_buffer, "TIE\n");               //tie
                                response_len = strlen(response_buffer);
                        }
                        else{                                                   //case: game on!
                                strcpy(response_buffer, "OK\n");
                                response_len = strlen(response_buffer);
                        }
		}
	}
	else{
		strcpy(response_buffer, "Invalid command\n");
		response_len = strlen(response_buffer);
	}
	return len;
}

static int hor_win(char color){
	int win = 0;
	int i;
	int k;
	for(i = 0; i < ROWS; i++){
		win = 0;
		for(k = 0; k < COLS; k++){
			if(board[i][k] == color){
				win++;			//iterates if there is a streak of the same color on the same row
			}
			else{
				win = 0;		//otherwise signal broken streak
			}
		}
		if(win >= 4){
			return 1;
		}
	}
	return 0;
}

static int vert_win(char color){
	int win[COLS] = {0};	//each element in the array corresponds to column in board
	int i;
	int k;
	for(i = 0; i < ROWS; i++){
		for(k = 0; k < COLS; k++){
			if(board[i][k] == color){	//same logic as hor_win except horizontally
				win[k]++;
			}
			else{
				win[k] = 0;
			}
		}
	}
	for(k = 0; k < COLS; k++){
		if(win[k] >= 4){
			return 1;
		}
	}
	return 0;
}

static int diag_win(char color){
	int win[2] = {0};
	int break_flag = 0;
	int i;
	int k;
	for(i = 0; i < ROWS - 3; i++){		//checks upper-right diagonal from where index is
		for(k = 0; k < COLS - 3; k++){
			if(board[i][k] == color){
				if(board[i + 1][k + 1] == color && board[i + 2][k + 2] == color && board[i + 3][k + 3] == color){
					win[0] = 1;
					break_flag = 1;
					break;
				}
			}
		}
		if(break_flag == 1){
			break;
		}
	}
	break_flag = 0;
	for(i = 3; i < ROWS; i++){		//checks lower-right diagonal from where index is
		for(k = 0; k < COLS - 3; k++){
			if(board[i][k] == color){
				if(board[i - 1][k + 1] == color && board[i - 2][k + 2] == color && board[i - 3][k + 3] == color){
                                        win[1] = 1;
					break_flag = 1;
					break;
                                }
			}
		}
		if(break_flag == 1){
			break;
		}
	}
	if(win[0] == 1 || win[1] == 1){
		return 1;
	}
	return 0;
}

static char* fourinarow_devnode(struct device *dev, umode_t *mode){
	if(mode){
		*mode = 0666;
	}
	return NULL;
}

static int __init fourinarow_init(void){
	major = register_chrdev(0, "fourinarow", &fops);	//gets major number for module
	if(major < 0){
		printk(KERN_ALERT "fourinarow: failed to register a major number\n");
		return major;
	}

	fourinarow_class = class_create(THIS_MODULE, "fourinarow_class");	//creates a category? for the module to sit in
	if(IS_ERR(fourinarow_class)){
		unregister_chrdev(major, "fourinarow");
		printk(KERN_ALERT "fourinarow: failed to register device\n");
		return PTR_ERR(fourinarow_class);
	}

	fourinarow_class->devnode = fourinarow_devnode;		//updates permissions

	fourinarow_device = device_create(fourinarow_class, NULL, MKDEV(major, 0), NULL, "fourinarow");
	if(IS_ERR(fourinarow_device)){				//actually creates and puts fourinarow under /dev/
		class_destroy(fourinarow_class);
		unregister_chrdev(major, "fourinarow");
		printk(KERN_ALERT "fourinarow: failed to create device\n");
		return PTR_ERR(fourinarow_device);
	}

	printk(KERN_INFO "fourinarow: module loaded\n");
	return 0;
}

static void __exit fourinarow_exit(void){			//detaches module from kernel
	device_destroy(fourinarow_class, MKDEV(major, 0));
	class_destroy(fourinarow_class);
	unregister_chrdev(major, "fourinarow");
	printk(KERN_INFO "fourinarow: module unloaded\n");
}

MODULE_LICENSE("GPL");

module_init(fourinarow_init);
module_exit(fourinarow_exit);
