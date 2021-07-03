all: json md5 postgres postgres_win

cloc:
	cloc .

json:
	$(CXX) -Wall -Werror -Wno-unused-function -x c++ -O0 -ggdb3 -DJSON_EXAMPLE json.h -lm
	$(CC) -Wall -Werror -Wno-unused-function -x c -O0 -ggdb3 -DJSON_EXAMPLE json.h -lm && ./a.out

json_fuzz:
	clang -x c -O3 -ggdb3 -fno-inline-functions -DJSON_FUZZ json2.h -fsanitize=address,undefined,fuzzer -lm
	./a.out -max_len=100000000

md5:
	$(CC) -x c -O0 -ggdb3 -DMD5_EXAMPLE md5.h && ./a.out

postgres:
	$(CC) -x c -O0 -ggdb3 -DPG_EXAMPLE postgres.h && ./a.out

postgres_win:
	x86_64-w64-mingw32-gcc -x c -O0 -ggdb3 -DPG_MAIN -DPG_IMPLEMENTATION postgres.h -lws2_32 && ./a.out

