CC = gcc
CFLAGS = -Wall	-g	-MMD
#added
all: sender receiver

#This builds a shared library.
receiver: receiver.o
	$(CC) $(CFLAGS)  $^ -o $@

sender: sender.o
	$(CC) $(CFLAGS)  $^ -o $@

.PHONY: clean
clean:
	$(RM) *.o *.d *.gcno *.gcda *.gcov receiver sender