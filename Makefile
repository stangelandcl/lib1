OPT=-Wall -Werror -Wno-unused-function -O0 -ggdb3
all: base64 cstring dataframe json jwt md5 noise postgres postgres_win rsa sha
WINCC=x86_64-w64-mingw32-gcc

cloc:
	cloc .

base64:
	$(CXX) $(OPT) -x c++ -DBASE64_EXAMPLE base64.h && ./a.out
	$(CC) $(OPT) -x c -DBASE64_EXAMPLE base64.h && ./a.out

cstring:
	$(CXX) $(OPT) -x c++ -DCSTRING_EXAMPLE cstring.h && ./a.out
	$(CC) $(OPT) -x c -DCSTRING_EXAMPLE cstring.h && ./a.out

dataframe:
	$(CXX) $(OPT) -x c++ -DDATAFRAME_EXAMPLE dataframe.h && ./a.out
	$(CC) $(OPT) -x c -DDATAFRAME_EXAMPLE dataframe.h && ./a.out

file:
	$(CXX) $(OPT) -x c++ -DFILE_EXAMPLE file.h && ./a.out
	$(CC) $(OPT) -x c -DFILE_EXAMPLE file.h && ./a.out

gzip:
	$(CXX) $(OPT) -x c++ -DGZIP_EXAMPLE gzip.h -lz && ./a.out
	$(CC) $(OPT) -x c -DGZIP_EXAMPLE gzip.h -lz && ./a.out

hash:
	$(CXX) $(OPT) -x c++ -DHASH_EXAMPLE hash.h && ./a.out
	$(CC) $(OPT) -x c -DHASH_EXAMPLE hash.h && ./a.out
	$(CXX) -O3 hash.cpp && ./a.out

hashg:
	$(CXX) $(OPT) -x c++ -DHASH_EXAMPLE hash.h && ./a.out
	$(CC) $(OPT) -x c -DHASH_EXAMPLE hash.h && ./a.out

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
	x86_64-w64-mingw32-gcc $(OPT) -mconsole -x c -DPG_EXAMPLE pg.h -lws2_32 && ./a.exe

requests:
	$(CC) $(OPT) -x c -DREQUESTS_EXAMPLE requests.h -l curl -O0 -g -fsanitize=address,undefined && ./a.out
	$(CXX) $(OPT) -x c++ -DREQUESTS_EXAMPLE requests.h -l curl && ./a.out
	x86_64-w64-mingw32-gcc -mconsole -x c -DREQUESTS_EXAMPLE requests.h -lwinhttp && ./a.exe

rsa:
	$(CXX) $(OPT) -x c++ -DRSA_EXAMPLE rsa.h && ./a.out
	$(CC) $(OPT) -x c -DRSA_EXAMPLE rsa.h && ./a.out

sb:
	$(CXX) $(OPT) -x c++ -DSB_EXAMPLE sb.h && ./a.out
	$(CC) $(OPT) -x c -DSB_EXAMPLE sb.h && ./a.out

sha:
	$(CXX) $(OPT) -x c++ -DSHA_EXAMPLE sha.h && ./a.out
	$(CC) $(OPT) -x c -DSHA_EXAMPLE sha.h && ./a.out

socks5:
	$(CXX) $(OPT) -x c++ -DSOCKS5_EXAMPLE socks5.h && ./a.out
	$(CC) $(OPT) -x c -DSOCKS5_EXAMPLE socks5.h && ./a.out
	$(WINCC) $(OPT) -x c -DSOCKS5_EXAMPLE socks5.h -mconsole -lws2_32 && ./a.exe

stream:
	$(CXX) $(OPT) -x c++ -DSTREAM_EXAMPLE stream.h && ./a.out
	$(CC) $(OPT) -x c -DSTREAM_EXAMPLE stream.h && ./a.out

teab:
	$(CXX) $(OPT) -x c++ -DTEAB_EXAMPLE teab.h && ./a.out
	$(CC) $(OPT) -x c -DTEAB_EXAMPLE teab.h && ./a.out

tds:
	$(CXX) $(OPT) -x c++ -DTDS_EXAMPLE tds.h -lm && ./a.out
	$(CC) $(OPT) -x c -DTDS_EXAMPLE tds.h -lm && ./a.out

threadpool:
	$(CXX) $(OPT) -x c++ -DTHREADPOOL_EXAMPLE threadpool.h -pthread && ./a.out
	$(CC) $(OPT) -x c -DTHREADPOOL_EXAMPLE threadpool.h -pthread && ./a.out

url:
	$(CXX) $(OPT) -x c++ -DURL_EXAMPLE url.h && ./a.out
	$(CC) $(OPT) -x c -DURL_EXAMPLE url.h && ./a.out
	$(WINCC) $(OPT) -x c -DURL_EXAMPLE url.h -mconsole -lws2_32 && ./a.exe

varint:
	$(CXX) $(OPT) -x c++ -DVARINT_EXAMPLE varint.h -lm && ./a.out
	$(CC) $(OPT) -x c -DVARINT_EXAMPLE varint.h -lm && ./a.out

