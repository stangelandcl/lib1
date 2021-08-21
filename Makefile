OPT=-Wall -Werror -Wno-unused-function -O0 -ggdb3
all: base64 json jwt md5 noise postgres postgres_win rsa sha
WINCC=x86_64-w64-mingw32-gcc

cloc:
	cloc .

base64:
	$(CXX) $(OPT) -x c++ -DBASE64_EXAMPLE base64.h && ./a.out
	$(CC) $(OPT) -x c -DBASE64_EXAMPLE base64.h && ./a.out

dataframe:
	$(CXX) $(OPT) -x c++ -DDATAFRAME_EXAMPLE dataframe.h && ./a.out
	$(CC) $(OPT) -x c -DDATAFRAME_EXAMPLE dataframe.h && ./a.out

hash:
	$(CXX) $(OPT) -x c++ -DHASH_EXAMPLE hash.h && ./a.out
	$(CC) $(OPT) -x c -DHASH_EXAMPLE hash.h && ./a.out
	$(CXX) -O3 hash.cpp && ./a.out

json:
	$(CXX) $(OPT) -x c++ -DJSON_EXAMPLE json.h -lm
	$(CC) $(OPT)  -x c -DJSON_EXAMPLE json.h -lm && ./a.out

json2:
	$(CXX) $(OPT) -x c++ -DJSON_EXAMPLE json2.h -lm
	$(CC) $(OPT)  -x c -DJSON_EXAMPLE json2.h -lm && ./a.out


json_fuzz:
	clang -x c -O3 -ggdb3 -fno-inline-functions -DJSON_FUZZ json2.h -fsanitize=address,undefined,fuzzer -lm
	./a.out -max_len=100000000

jwt:
	#openssl rsa -pubin -inform PEM -text -noout < public_key
	$(CXX) $(OPT) -x c++ -DJWT_EXAMPLE jwt.h && ./a.out
	$(CC) $(OPT) -x c  -DJWT_EXAMPLE jwt.h && ./a.out

md5:
	$(CXX) $(OPT) -x c++ -DMD5_EXAMPLE md5.h && ./a.out
	$(CC) $(OPT) -x c -DMD5_EXAMPLE md5.h && ./a.out

noise:
	$(CC) -std=c11 -maes $(OPT) -x c -DNOISE_EXAMPLE noise.h && ./a.out msg
	$(CXX) -std=c++11 -maes $(OPT) -x c++ -DNOISE_EXAMPLE noise.h && ./a.out msg

pg:
	$(CXX) $(OPT) -x c++ -DPG_EXAMPLE pg.h -lm && ./a.out
	$(CC) $(OPT) -x c -DPG_EXAMPLE pg.h -lm && ./a.out

pg_win:
	x86_64-w64-mingw32-gcc $(OPT) -mconsole -x c -DPG_EXAMPLE pg.h -lws2_32 && ./a.out

rsa:
	$(CXX) $(OPT) -x c++ -DRSA_EXAMPLE rsa.h && ./a.out
	$(CC) $(OPT) -x c -DRSA_EXAMPLE rsa.h && ./a.out

sha:
	$(CXX) $(OPT) -x c++ -DSHA_EXAMPLE sha.h && ./a.out
	$(CC) $(OPT) -x c -DSHA_EXAMPLE sha.h && ./a.out

socks5:
	$(CXX) $(OPT) -x c++ -DSOCKS5_EXAMPLE socks5.h && ./a.out
	$(CC) $(OPT) -x c -DSOCKS5_EXAMPLE socks5.h && ./a.out
	$(WINCC) $(OPT) -x c -DSOCKS5_EXAMPLE socks5.h -mconsole -lws2_32 && ./a.exe

tds:
	$(CXX) $(OPT) -x c++ -DTDS_EXAMPLE tds.h -lm && ./a.out
	$(CC) $(OPT) -x c -DTDS_EXAMPLE tds.h -lm && ./a.out

url:
	$(CXX) $(OPT) -x c++ -DURL_EXAMPLE url.h && ./a.out
	$(CC) $(OPT) -x c -DURL_EXAMPLE url.h && ./a.out
	$(WINCC) $(OPT) -x c -DURL_EXAMPLE url.h -mconsole -lws2_32 && ./a.exe

