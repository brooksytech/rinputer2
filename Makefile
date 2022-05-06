default:
	$(CROSS_COMPILE)$(CC) main.c -o rinputer2 $(CFLAGS) -lpthread -Wall -Wextra
