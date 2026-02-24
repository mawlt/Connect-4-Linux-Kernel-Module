## Project Overview

This project simulates the development of a kernel module using the game of Connect4. In this project, the program must handle reads and writes from user space to interact with the Connect4 game which resides in kernel space. A lot of it involved transfering user commands from user space and game state information from kernel space into buffers which could be safely copied to their respective opposite sides using the copy_from_user and copy_to_user functions. This is facilitated by /dev/fourinarow which enables commands from user space to trigger kernel space logic and allows kernel space to send back game data to the user when prompted. Once a user command is copied to kernel space, the kernel processes it so that the right logic is executed using a file_operations structure. 

### How To Run
Follow the command syntax of echo "COMMAND IN ALL CAPS" > /dev/fourinarow to submit commands and cat /dev/fourinarow to display expected output (what is currently in response_buffer)
