CC=gcc
FLAG=-g
LIB=-lm -lhmdp -lhdb -lhfs

default:
	$(CC) $(FLAG)  hftp.c common.c client.c $(LIB) -o client
	$(CC) $(FLAG)  hftp.c common.c server.c $(LIB) -o hftpd
