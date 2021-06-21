all: md5 postgres postgres_win

md5:
	$(CC) -x c -O0 -ggdb3 -DMD5_MAIN -DMD5_IMPLEMENTATION md5.h
	./a.out

postgres:
	$(CC) -x c -O0 -ggdb3 -DPG_MAIN -DPG_IMPLEMENTATION postgres.h
	./a.out

postgres_win:
	x86_64-w64-mingw32-gcc -x c -O0 -ggdb3 -DPG_MAIN -DPG_IMPLEMENTATION postgres.h -lws2_32
	./a.out

