# Project Overview

This project simulates the development of a kernel module using the game of Connect4. In this project, the program must handle reads and writes from user space to interact with the Connect4 game which resides in kernel space. A lot of it involved transfering user commands from user space and game state information from kernel space into buffers which could be safely copied to their respective opposite sides using the copy_from_user and copy_to_user functions. This is facilitated by /dev/fourinarow which enables commands from user space to trigger kernel space logic and allows kernel space to send back game data to the user when prompted. Once a user command is copied to kernel space, the kernel processes it so that the right logic is executed using a file_operations structure. 

## Requirements
- Linux system (native or VM)
- make, gcc, and build tools
  
Tested on: Debian (6.x kernel)

## How to run
1. In project directory run make which should produce
> fourinarow.ko
2. Insert kernel module and verify it is loaded
> sudo insmod fourinarow.ko

> lsmod | grep fourinarow

3. /dev/fourinarow is created which can be verified by
> ls -l /dev/fourinarow

## How the interface works
Commands follow a simple pattern of WRITE command -> READ response
> echo -n "{COMMAND}" > /dev/fourinarow && cat /dev/fourinarow

For example:
> echo -n "RESET Y" > /dev/fourinarow && cat /dev/fourinarow

Restarts the game board with you being the yellow with the first move
### Valid commands
- RESET Y -> starts/resets game as Yellow
- RESET R -> same as above but as Red
- BOARD -> displays the board
- DROPC X -> drops token in column X (A-H) (will not execute if it is computer's turn)
- CTURN -> executes computer's move (will not execute if it is player's turn)

## Educational Scope
This project prioritizes kernel-space device interaction and systems programming concepts over UI or application-layer abstraction. It is intended as a demonstration of Linux kernel module development and character device design rather than a production game implementation.
