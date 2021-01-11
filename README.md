# UDP-socket-Communication-BP-Implimentation
CS425 P2/P3

/*
		* Class: CS425 Fall 2020
		* Author: Shinji Kasai
    */

To compile:
 
    make 
    or 
    1. gcc sender.c -o sender
    2. gcc receiver.c -o receiver

To run:
run from receiver, then run sender. Reciever will response you. 

    Example:
        ./receiver 13579
        ./sender 127.0.0.1 13579 < testin1

       // order does not matter 

Reflection:
This project took me while to complete. 
I misunderstood the p2 and I nearly had to remake my project. I was able to create a new project
socket. There are tons of examples in youtube. I was watching a lot of TCP videos that creates a chatt 
style messages transfer then i started to misunderstand what is TCP for. For some reason my VSCode shows errors
even though its a valid syntax or logic. 

Source:
Lecture videos
Lecture slides
https://www.youtube.com/
https://www.geeksforgeeks.org/
https://stackoverflow.com/
