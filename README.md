# Elevator-module-for-Linux
An elevator that runs in the background of Linux

This project adds an elevator module to your linux kernel.
The module uses custom system calls that must be added to your kernel first.
You must go through the steps of adding system calls for this project to work.
This includes recompiling the kernel.
The module must also be added by using "sudo insmod #name of module#"

The code and makefile for the system calls are included in SystemCalls
The code for the modules, the elevator code, and the makefile are included in SyscallModule


The elevator works by using system calls to add "people" to a floor. The elevator travels between floors , 1-10, picking up people and taking them to their destination floor.

This does not include a program to test the module.
To test it, create a simple program that starts the elevator with start_elevator() systemcall.
To add people use issue_request(). This adds people to the selected floors and gives them a destination
Use stop_elevator to end the module. The elevator will empty and the go offline.
