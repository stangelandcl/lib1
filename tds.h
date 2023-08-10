#ifndef TDS_H
#define TDS_H

/* see example and license (public domain) at end of file */

#if defined(TDS_STATIC) || defined(TDS_EXAMPLE)
#define TDS_API static
#define TDS_IMPLEMENTATION
#else
#define TDS_API extern
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define TDS_MAXCOLS 64

typedef struct TdsBuf {
	char *data;
	size_t n, cap;
} TdsBuf;
typedef struct TdsResponse {
	char *data;
	size_t n;
} TdsResponse;
typedef struct TdsCol {
	char name[32];
	unsigned len;
	unsigned char type;
	unsigned char precision;
	unsigned char scale;
} TdsCol;
typedef struct TdsSlice {
	unsigned char *p;
	size_t n;
} TdsSlice;
typedef struct TdsParser {
	TdsCol cols[TDS_MAXCOLS];
	char error[512];
	TdsSlice slice; /* current location of parse. changing */
	unsigned npacket : 14;
	unsigned logged_in : 1;
	unsigned pad : 1;
	unsigned ncols : 6;
	unsigned pad2 : 2;
	unsigned i : 6; /* column index */
	char state;
} TdsParser;
typedef struct TdsConn {
	char error[512];
	int fd;
	TdsBuf buf;
	short npacket;
	char logged_in;
} TdsConn;
typedef enum TdsType {
	tds_type_none, /* never used. getting this is a bug */
	tds_type_float,
	tds_type_double,
	tds_type_bool,
	tds_type_i8,
	tds_type_i16,
	tds_type_i32,
	tds_type_i64,
	tds_type_u8,
	tds_type_u16,
	tds_type_u32,
	tds_type_u64,
	tds_type_date, /* int (32 bits) days since unix epoch */
	tds_type_time, /* nanoseconds since unix epoch i64 */
	tds_type_bytes, /* interpret string as bytes */
	tds_type_string /* interpret string as text */
} TdsType;
typedef struct TdsString {
	const char *text;
	size_t n;
} TdsString;
/* NaN, INT64_MIN/UINT_MAX, 0 length are null values */
typedef union TdsValueData {
	float f;
	double d;
	uint8_t boolean;
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int32_t date; /* days since unix epoch */
	int64_t time; /* nanoseconds since unix epoch */
	TdsSlice bytes;
	TdsString s; /* not null terminated */
} TdsValueData;
typedef struct TdsValue {
	TdsType type;
	const char *name; /* column name */
	TdsValueData data;
} TdsValue;

TDS_API void tds_parser_init(TdsParser *p, const void *data, size_t n);
/* return 1 on has row of data. return 0 on done or error */
TDS_API int tds_row(TdsParser *p);
/* return 1 on has column value. return 0 on done or error */
TDS_API int tds_col(TdsParser *p, TdsValue *v);
/* returns error if there is one */
TDS_API const char* tds_parser_error(TdsParser *p);
TDS_API void tds_print_columns(TdsParser *parser);
/* free buffer after use */
TDS_API int tds_to_json(TdsBuf *buf, const uint8_t *data, size_t ndata);
TDS_API int tds_to_csv(TdsBuf *buf, const uint8_t *data, size_t ndata);
TDS_API void tds_buf_destroy(TdsBuf*);

TDS_API int tds_bool(TdsValue *v, uint8_t *i);
/* return 1 if have value. 0 if null */
TDS_API int tds_i8(TdsValue *v, int8_t *i);
TDS_API int tds_i16(TdsValue *v, int16_t *i);
TDS_API int tds_i32(TdsValue *v, int32_t *i);
TDS_API int tds_i64(TdsValue *v, int64_t *i);
TDS_API int tds_u8(TdsValue *v, uint8_t *i);
TDS_API int tds_u16(TdsValue *v, uint16_t *i);
TDS_API int tds_u32(TdsValue *v, uint32_t *i);
TDS_API int tds_u64(TdsValue *v, uint64_t *i);
TDS_API int tds_float(TdsValue *v, float *f);
TDS_API int tds_double(TdsValue *v, double *f);
TDS_API void tds_str(TdsValue *v, const char **c, size_t *n);
TDS_API void tds_bytes(TdsValue *v, uint8_t **c, size_t *n);
/* days since unix epoch */
TDS_API int tds_date(TdsValue *v, int32_t*);
/* nanoseconds since unix epoch */
TDS_API int tds_time(TdsValue *v, int64_t *);

TDS_API void tds_connect1(TdsConn *conn, int socket);
/* returns 0 on success. < 0 on error. */
TDS_API int tds_connect(TdsConn *conn, const char *host, int port, int timeout_sec);
/* returns 0 on success. < 0 on error. */
TDS_API int tds_login(TdsConn *conn, const char *host, const char *app, const char *user, const char *password);
TDS_API void tds_timeout(TdsConn *conn, int seconds);
/* returns 0 on success. < 0 on error. */
TDS_API int tds_query(TdsConn *conn, TdsResponse *result, const char *format, ...);
/* returns 0 on success. < 0 on error. */
TDS_API int tds_vquery(TdsConn *conn, TdsResponse *result, const char *format, va_list args);
/* returns 0 on success. < 0 on error. */
TDS_API int tds_command(TdsConn *conn, const char *format, ...);
/* returns 0 on success. < 0 on error. */
TDS_API int tds_vcommand(TdsConn *conn, const char *format, va_list args);
/* return error string if there is an error (valid until tds_destroy()) or 0 of no error */
TDS_API const char* tds_error(TdsConn *conn);
TDS_API void tds_response_destroy(TdsResponse*);
TDS_API void tds_destroy(TdsConn *conn);

#ifdef __cplusplus
}
#endif

#endif

#ifdef TDS_IMPLEMENTATION

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	define SHUT_RDWR SD_BOTH
#	define close closesocket
#else
#	include <fcntl.h>
#	include <netdb.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <sys/socket.h>
#	include <sys/time.h>
#	include <sys/types.h>
#	include <unistd.h>
#endif


#define TDS_PACKET_SIZE 512
#define TDS_MAX_PACKET_SIZE 8192
#define TDS_STRING1(x) #x
#define TDS_STRING(x) TDS_STRING1(x)

enum {
	TDS_MAXNAME=30,
	TDS_PROGNLEN=10,
	TDS_RPLEN = (15*16) + 12 + 1,
	TDS_PKTLEN=6,

	TDS_INT2_LSB_HI=2, /* big endian */
	TDS_INT2_LSB_LO=3, /* little endian */

	TDS_INT4_LSB_HI=0, /* big endian */
	TDS_INT4_LSB_LO=1, /* little endian */

	TDS_CHAR_ASCII=6,

	TDS_FLT_IEEE_HI=4,
	TDS_FLT_IEEE_LO=10,

	/* 8 byte date format */
	TDS_TWO_I4_LSB_HI=8,
	TDS_TWO_I4_LSB_LO=9,

	TDS_OPT_FALSE=0,
	TDS_OPT_TRUE=1,

	TDS_LDEFSQL=0,

	TDS_NOCVT_SHORT=0, /* don't extend */
	TDS_CVT_SHORT=1, /* extend 4 byte datetime and floats to 8 bytes */

	TDS_FLT4_IEEE_HI=12,
	TDS_FLT4_IEEE_LO=13,

	TDS_TWO_I2_LSB_HI=16,
	TDS_TWO_I2_LSB_LO=17,

	/* password encryption. maybe RSA key */
	TDS_SEC_LOG_ENCRYPT3=0x80,

	/* notify on language changes */
	TDS_NO_NOTIFY=0,
	TDS_NOTIFY=1,

	TDS_CAP_REQUEST=1,
	TDS_CAP_RESPONSE=2,


	TDS_REQ_DYNAMIC_SUPPRESS_PARAMFMT=1,

	TDS_REQ_LOGPARAMS=1<<7,
	TDS_REQ_ROWCOUNT_FOR_SELECT=1<<6,
	TDS_DATA_LOBLOCATOR=1<<5,
	TDS_REQ_RPC_BATCH=1<<4,
	TDS_REQ_LANG_BATCH=1<<3,
	TDS_REQ_DYN_BATCH=1<<2,
	TDS_REQ_GRID=1<<1,
	TDS_REQ_INSTID=1,

	TDS_RPCPARAM_LOB=1<<7,
	TDS_DATA_USECS=1<<6,
	TDS_DATA_BIGDATETIME=1<<5,
	TDS_UNUSED4=1<<4,
	TDS_UNUSED5=1<<3,
	TDS_MULTI_REQUESTS=1<<2,
	TDS_REQ_MIGRATE=1<<1,
	TDS_UNUSED8=1,

	TDS_REQ_DBRPC2=1<<7,
	TDS_REQ_CURINFO3=1<<6,
	TDS_DATA_XML=1<<5,
	TDS_REQ_BLOB_NCHAR_16=1<<4,
	TDS_REQ_LARGEIDENT=1<<3,
	TDS_DATA_SINT1=1<<2,
	TDS_CAP_CLUSTERFAILOVER=1<<1,
	TDS_DATA_UNITEXT=1,

	TDS_REQ_SRVPKTSIZE=1<<7,
	TDS_CSR_KEYSETDRIVEN=1<<6,
	TDS_CSR_SEMISENSITIVE=1<<5,
	TDS_CSR_INSENSITIVE=1<<4,
	TDS_CSR_SENSITIVE=1<<3,
	TDS_CSR_SCROLL=1<<2,
	TDS_DATA_INTERVAL=1<<1,
	TDS_DATA_TIME=1,

	TDS_DATA_DATE=1<<7,
	TDS_BLOB_NCHAR_SCSU=1<<6,
	TDS_BLOB_NCHAR_8=1<<5,
	TDS_BLOB_NCHAR_16=1<<4,
	TDS_IMAGE_NCHAR=1<<3,
	TDS_DATA_NLBIN=1<<2,
	TDS_CUR_IMPLICIT=1<<1,
	TDS_DATA_UINTN=1,

	TDS_DATA_UINT8=1<<7,
	TDS_DATA_UINT4=1<<6,
	TDS_DATA_UINT2=1<<5,
	TDS_REQ_RESERVED2=1<<4,
	TDS_WIDETABLE=1<<3,
	TDS_REQ_RESERVED1=1<<2,
	TDS_OBJECT_BINARY=1<<1,
	TDS_DATA_COLUMNSTATUS=1,

	TDS_OBJECT_CHAR=1<<7,
	TDS_OBJECT_JAVA1=1<<6,
	TDS_DOL_BULK=1<<5,
	TDS_DATA_VOID=1<<4,
	TDS_DATA_INT8=1<<3,
	TDS_DATA_BITN=1<<2,
	TDS_DATA_FLTN=1<<1,
	TDS_PROTO_DYNPROC=1,

	TDS_PROTO_DYNAMIC=1<<7,
	TDS_DATA_BOUNDARY=1<<6,
	TDS_DATA_SENSITIVITY=1<<5,
	TDS_REQ_URGEVT=1<<4,
	TDS_PROTO_BULK=1<<3,
	TDS_PROTO_TEXT=1<<2,
	TDS_CON_LOGICAL=1<<1,
	TDS_CON_INBAND=1,

	TDS_CON_OOB=1<<7,
	TDS_CSR_MULTI=1<<6,
	TDS_CSR_REL=1<<5,
	TDS_CSR_ABS=1<<4,
	TDS_CSR_LAST=1<<3,
	TDS_CSR_FIRST=1<<2,
	TDS_CSR_PREV=1<<1,
	TDS_DATA_MONEYN=1,

	TDS_DATA_DATETIMEN=1<<7,
	TDS_DATA_INTN=1<<6,
	TDS_DATA_LBIN=1<<5,
	TDS_DATA_LCHAR=1<<4,
	TDS_DATA_DEC=1<<3,
	TDS_DATA_IMAGE=1<<2,
	TDS_DATA_TEXT=1<<1,
	TDS_DATA_NUM=1,

	TDS_DATA_FLT8=1<<7,
	TDS_DATA_FLT4=1<<6,
	TDS_DATA_DATE4=1<<5,
	TDS_DATA_DATE8=1<<4,
	TDS_DATA_MNY4=1<<3,
	TDS_DATA_MNY8=1<<2,
	TDS_DATA_VBIN=1<<1,
	TDS_DATA_BIN=1,

	TDS_DATA_VCHAR=1<<7,
	TDS_DATA_CHAR=1<<6,
	TDS_DATA_BIT=1<<5,
	TDS_DATA_INT4=1<<4,
	TDS_DATA_INT2=1<<3,
	TDS_DATA_INT1=1<<2,
	TDS_REQ_PARAM=1<<1,
	TDS_REQ_MSG=1,

	TDS_REQ_DYNF=1<<7,
	TDS_REQ_CURSOR=1<<6,
	TDS_REQ_BCP=1<<5,
	TDS_REQ_MSTMT=1<<4,
	TDS_REQ_EVT=1<<3,
	TDS_REQ_RPC=1<<2,
	TDS_REQ_LANG=1<<1,
	TDS_REQ_NONE=0,

	TDS_RES_NO_TDSCONTROL=1<<3,

	TDS_RES_SUPPRESS_FMT=1<<6,

	TDS_DATA_NOINTERVAL=1<<5,

	TDS_RES_RESERVED1=1,

	TDS_RES_NOTDSDEBUG=1<<1,
	TDS_DATA_NOBOUNDARY=1,

	TDS_DATA_NOSENSITIVITY=1<<7,
	TDS_PROTO_NOBULK=1<<6,
	TDS_CON_NOOOB=1<<3,

	TDS_CAPABILITY=0xe2,

	TDS_BUF_LOGIN=2,
	TDS_BUF_RESPONSE=4,
	TDS_BUF_NORMAL=15,

	TDS_BUFSTAT_NONE=0,
	/* last buffer in a request/resp */
	TDS_BUFSTAT_EOM=1,

	TDS_ENVCHANGE=0xe3,
	TDS_ENV_DB=1,
	TDS_ENV_LANG=2,
	TDS_ENV_CHARSET=3,
	TDS_ENV_PACKSIZE=4,

	TDS_MSG=0x65,
	TDS_MSG_HASARGS=1,

	TDS_LOGINACK=0xad,
	TDS_LOG_SUCCEED=5,
	TDS_LOG_FAIL=6,
	TDS_LOG_NEGOTIATE=7,

	TDS_EED=0xe5,
	TDS_ORDERBY=0xa9,

	TDS_DONE=0xfd,
	TDS_DONE_COUNT=1<<1,

	TDS_LANGUAGE=0x21,
	TDS_ROWFMT=0xee,
	TDS_ROWFMT2=0x61,
	TDS_ROW=0xd1,

	TDS_BINARY=0x2d,
	TDS_INT=0x2d,
	TDS_BIT=0x32,
	TDS_BLOB=0x24,
	TDS_BOUNDARY=0x68,
	TDS_CHAR=0x2f,
	TDS_DATE=0x31,
	TDS_DATEN=0x7b,
	TDS_DATETIME=0x3d,
	TDS_DATETIMEN=0x6f,
	TDS_DECN=0x6a,
	TDS_FLT4=0x3b,
	TDS_FLT8=0x3e,
	TDS_FLTN=0x6d,
	TDS_IMAGE=0x22,
	TDS_INT1=0x30,
	TDS_INT2=0x34,
	TDS_INT4=0x38,
	TDS_INT8=0xbf,
	TDS_INTERVAL=0x2e,
	TDS_INTN=0x26,
	TDS_LONGBINARY=0xe1,
	TDS_LONGCHAR=0xaf,
	TDS_MONEY=0x3c,
	TDS_MONEYN=0x6e,
	TDS_NUMN=0x6c,
	TDS_SENSITIVITY=0x67,
	TDS_SHORTDATE=0x3a,
	TDS_SHORTMONEY=0x7a,
	TDS_SINT1=0xb0,
	TDS_TEXT=0x23,
	TDS_TIME=0x33,
	TDS_TIMEN=0x93,
	TDS_UINT2=0x41,
	TDS_UINT4=0x42,
	TDS_UINT8=0x43,
	TDS_UINTN=0x44,
	TDS_UNITEXT=0xae,
	TDS_VARBINARY=0x25,
	TDS_VARCHAR=0x27,
	TDS_VOID=0x1f,
	TDS_XML=0xa3,
	TDS_BIGDATETIMEN=0xbb,
};

typedef struct TdsHeader { unsigned char type, status; } TdsHeader;

static void tds_debug(const char *format, ...) {
	static int debug;
	va_list arg;
	if(!debug) debug = getenv("TDS_DEBUG") != 0 ? 1 : 2;
	if(debug != 1) return;
	printf("tds: ");
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);
	printf("\n");
}

static int
tds_parser_make_error(TdsParser *p, const char *format, ...) {
	va_list arg;
	if(p->state == 'e') return -1;
	p->state = 'e';
	va_start(arg, format);
	vsnprintf(p->error, sizeof p->error, format, arg);
	va_end(arg);
	return -1;
}

static void
tds_make_error(TdsConn *conn, const char *format, ...) {
	va_list arg;
	if(conn->error[0]) return;
	va_start(arg, format);
	vsnprintf(conn->error, sizeof conn->error, format, arg);
	va_end(arg);
}

TDS_API const char*
tds_error(TdsConn *conn) {
	return conn->error[0] ? conn->error : 0;
}

TDS_API void
tds_destroy(TdsConn *conn) {
	shutdown(conn->fd, SHUT_RDWR);
	close(conn->fd);
	free(conn->buf.data);
}
TDS_API void tds_buf_destroy(TdsBuf *buf) {
	free(buf->data);
}

static int
tds_slice(TdsSlice *slice, int n, unsigned char **p) {
	if(n > (int)slice->n || n < 0) return -1;
	*p = (unsigned char*)slice->p;
	slice->p += n;
	slice->n -= n;
	return 0;
}

static int
tds_slice2(TdsSlice *slice, int n, TdsSlice *out) {
	if(n > (int)slice->n || n < 0) return -1;
	out->p = slice->p;
	out->n = n;
	slice->p += n;
	slice->n -= n;
	return 0;
}

static int
tds_peek(TdsSlice *slice, int n, unsigned char **p) {
	if(n > (int)slice->n || n < 0) return -1;
	*p = (unsigned char*)slice->p;
	return 0;
}

static int
tds_writebytes(TdsBuf *buf, const void *data, size_t n) {
	if(buf->n + n > buf->cap) {
		size_t cap = buf->cap * 2;
		char *p;
		if(cap < 512) cap = 512;
		if(cap < buf->n + n) cap = buf->n + n;
		p = (char*)realloc(buf->data, cap);
		if(!p) return -1;
		buf->data = p;
		buf->cap = cap;
	}
	memcpy(buf->data + buf->n, data, n);
	buf->n += n;
	return 0;
}

static int
tds_writebyte(TdsBuf *buf, unsigned char c) {
	return tds_writebytes(buf, &c, sizeof c);
}

TDS_API int
tds_to_json(TdsBuf *buf, const uint8_t *data, size_t ndata) {
	TdsParser p;
	int row = 0, n, col;
	TdsValue v;
	char str[128];
	int64_t i64;
	uint64_t u64;

	memset(buf, 0, sizeof *buf);
	tds_parser_init(&p, data, ndata);
	tds_writebytes(buf, "[", 1);
	while(tds_row(&p)) {
		if(row++) tds_writebytes(buf, ",", 1);
		tds_writebytes(buf, "{", 1);
		col = 0;
		while(tds_col(&p, &v)) {
			if(col++) tds_writebytes(buf, ",", 1);

			/* write name */
			tds_writebytes(buf, "\"", 1);
			for(const char *c = v.name;*c;c++) {
				if(*c == '"') tds_writebytes(buf, "\\", 1);
				tds_writebytes(buf, c, 1);
			}
			tds_writebytes(buf, "\"", 1);
			tds_writebytes(buf, ":", 1);

			switch(v.type) {
			case tds_type_none:
null:
				tds_writebytes(buf, "null", 4); break;
			case tds_type_bool:
				if(tds_i64(&v, &i64)) {
					if(i64) tds_writebytes(buf, "true", 4);
					else tds_writebytes(buf, "false", 5);
				} else goto null;
				break;
			case tds_type_i8:
			case tds_type_i16:
			case tds_type_date:
			case tds_type_i32:
			case tds_type_time:
			case tds_type_i64:
				if(tds_i64(&v, &i64)) {
					n = snprintf(str, sizeof str, "%lld", (long long)i64);
					if(n < (int)sizeof str) tds_writebytes(buf, str, n);
					else tds_writebytes(buf, "0", 1);
				} else goto null;
				break;
			case tds_type_u8:
			case tds_type_u16:
			case tds_type_u32:
			case tds_type_u64:
				if(tds_u64(&v, &u64)) {
					n = snprintf(str, sizeof str, "%llu", (unsigned long long)u64);
					if(n < (int)sizeof str) tds_writebytes(buf, str, n);
					else tds_writebytes(buf, "0", 1);
				} else goto null;
				break;
			case tds_type_float:
			case tds_type_double: {
				double d;
				if(tds_double(&v, &d)) {
					n = snprintf(str, sizeof str, "%f", d);
					if(n < (int)sizeof str) tds_writebytes(buf, str, n);
					else tds_writebytes(buf, "0", 1);
				} else goto null;
			} break;
			case tds_type_string: {
				size_t i;
				tds_writebytes(buf, "\"", 1);
				for(i=0;i<v.data.s.n;i++) {
					if(v.data.s.text[i] == '"') tds_writebytes(buf, "\\", 1);
					tds_writebytes(buf, &v.data.s.text[i], 1);
				}
				tds_writebytes(buf, "\"", 1);
			} break;
			case tds_type_bytes: {
				size_t i;
				const char hex[] = "0123456789abcdef";
				for(i=0;i<v.data.bytes.n;i++) {
					uint8_t b = v.data.bytes.p[i];
					tds_writebytes(buf, &hex[b>>4], 1);
					tds_writebytes(buf, &hex[b&0xF], 1);
				}
			} break;
			}
		}
		tds_writebytes(buf, "}", 1);
	}
	tds_writebytes(buf, "]", 1);
	return tds_parser_error(&p) ? -1 : 0;
}

static void
tds_csv_text(TdsBuf *buf, const char *text, size_t n) {
	size_t i, quote = 0;

	for(i=0;i<n;i++) {
		if(text[i] == '"' || text[i] == ',' || text[i] == ' ' || text[i] == '\r' || text[i] == '\t' || text[i] == '\n') {
			quote = 1;
			break;
		}
	}

	if(quote) {
		tds_writebytes(buf, "\"", 1);
		for(i=0;i<n;i++) {
			if(text[i] == '"') tds_writebytes(buf, "\"", 1);
			tds_writebytes(buf, &text[i], 1);
		}
		tds_writebytes(buf, "\"", 1);
	} else tds_writebytes(buf, text, n);
}

TDS_API int
tds_to_csv(TdsBuf *buf, const uint8_t *data, size_t ndata) {
	TdsParser p;
	int row = 0, n, col,i;
	TdsValue v;
	char str[128];
	TdsCol *c;
	int64_t i64;
	uint64_t u64;

	memset(buf, 0, sizeof *buf);
	tds_parser_init(&p, data, ndata);
	while(tds_row(&p)) {
		if(!row++) {
			for(i=0;i<(int)p.ncols;i++) {
				c = &p.cols[i];
				if(i) tds_writebytes(buf, ",", 1);
				tds_csv_text(buf, c->name, strlen(c->name));
			}
			tds_writebytes(buf, "\n", 1);

			tds_print_columns(&p);
		}
		col = 0;
		while(tds_col(&p, &v)) {
			if(col++) tds_writebytes(buf, ",", 1);

			switch(v.type) {
			case tds_type_none: break;
			case tds_type_bool:
				if(tds_i64(&v, &i64)) {
					if(i64) tds_writebytes(buf, "true", 4);
					else tds_writebytes(buf, "false", 5);
				}
				break;
			case tds_type_i8:
			case tds_type_i16:
			case tds_type_date:
			case tds_type_i32:
			case tds_type_time:
			case tds_type_i64:
				if(tds_i64(&v, &i64)) {
					n = snprintf(str, sizeof str, "%lld", (long long)i64);
					if(n < (int)sizeof str) tds_writebytes(buf, str, n);
				}
				break;
			case tds_type_u8:
			case tds_type_u16:
			case tds_type_u32:
			case tds_type_u64:
				if(tds_u64(&v, &u64)) {
					n = snprintf(str, sizeof str, "%llu", (unsigned long long)u64);
					if(n < (int)sizeof str) tds_writebytes(buf, str, n);
				}
				break;
			case tds_type_float:
			case tds_type_double: {
				double d;
				if(tds_double(&v, &d)) {
					n = snprintf(str, sizeof str, "%f", d);
					if(n < (int)sizeof str) tds_writebytes(buf, str, n);
				}
			} break;
			case tds_type_string: tds_csv_text(buf, v.data.s.text, v.data.s.n); break;
			case tds_type_bytes: {
				size_t i;
				const char hex[] = "0123456789abcdef";
				for(i=0;i<v.data.bytes.n;i++) {
					uint8_t b = v.data.bytes.p[i];
					tds_writebytes(buf, &hex[b>>4], 1);
					tds_writebytes(buf, &hex[b&0xF], 1);
				}
			} break;
			}
		}
		tds_writebytes(buf, "\n", 1);
	}
	return tds_parser_error(&p) ? -1 : 0;
}

static int
tds_writestr(TdsBuf *buf, const char *text, size_t max_text) {
	size_t n = strlen(text);
	if(n > max_text) n = max_text;
	if(tds_writebytes(buf, text, n)) return -1;
	while(max_text-- > n) if(tds_writebyte(buf, 0)) return -1;
	if(tds_writebyte(buf, n)) return -1;
	return 0;
}
static int
tds_writeweirdstr(TdsBuf *buf, const char *text, size_t max_text) {
	size_t n = strlen(text);
	if(n > max_text) n = max_text;
	if(tds_writebyte(buf, 0)) return -1;
	if(tds_writebyte(buf, (unsigned char)n)) return -1;
	if(tds_writebytes(buf, text, n)) return -1;
	while(max_text-- > n) if(tds_writebyte(buf, 0)) return -1;
	if(tds_writebyte(buf, n + 2)) return -1;
	return 0;
}

static ptrdiff_t
tds_send(int fd, const void *buf, ptrdiff_t n) {
	char *p = (char*)buf;
	ptrdiff_t sent = 0;
	int rc;

	while(sent != n) {
		rc = send(fd, p + sent, (int)(n - sent), 0);
		if(rc <= 0) {
			if(rc == -1 && errno == EINTR) continue;
			if(rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
			if(rc == 0) return 0;
			printf("error writing %s\n", strerror(errno));
			return -1;
		}
		sent += rc;
	}
	return sent;
}

static ptrdiff_t
tds_recv(int fd, void *buf, ptrdiff_t n) {
	char *p = (char*)buf;
	ptrdiff_t sent = 0;
	int rc;

	while(sent != n) {
		rc = recv(fd, p + sent, (int)(n - sent), MSG_WAITALL);
		if(rc <= 0) {
			if(rc == -1 && errno == EINTR) continue;
			if(rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) break;
			if(rc == 0) return 0;
			printf("error writing %s\n", strerror(errno));
			return -1;
		}
		sent += rc;
	}
	return sent;
}

static void
tds_socket_timeout(int socket, int timeout_in_sec) {
#if _WIN32
	timeout_in_sec *= 1000;
	if(timeout_in_sec < 0) timeout_in_sec = 0;
	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout_in_sec, 4);
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout_in_sec, 4);
#else
	struct timeval v;
	if(timeout_in_sec < 0) timeout_in_sec = 2*1024*1024;
	v.tv_sec = timeout_in_sec;
	v.tv_usec = 0;

	setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &v, sizeof v);
	setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &v, sizeof v);
#endif
}

static int
tds_keepalive(int fd) {
	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&opt, sizeof opt);

#ifdef _WIN32
	DWORD cnt;
	struct tcp_keepalive alive;
	alive.keepalivetime = 15*60*1000;
	alive.keepaliveinterval = 60*1000;
	alive.onoff = TRUE;

	if(WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof alive, 0, 0, &cnt, 0, 0) == SOCKET_ERROR)
		printf("setting keepalive failed\n");
#else
	/* number of failed probes before dropping connection */
	opt = 5;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (void*)&opt, sizeof opt);

	/* seconds to wait before starting a new sent of keepalive checks */
	opt = 15*60;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&opt, sizeof opt);

	/* keep alive interval between unacknowledged probes */
	opt = 60;
	setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (void*)&opt, sizeof opt);
#endif
	return 0;
}

static void
tds_socket_init() {
#ifdef _WIN32
	static int init;
	if(!init) {
		WSADATA data;
		unsigned short version = (3 << 4) | 3;
		WSAStartup(version, &data);
		init = 1;
	}
#endif
}

static int
tds_socket_connect(const char* host, int port, int timeout) {
	int fd, rc;
	struct addrinfo hints = {0}, *res, *addr;
	char ports[16];

	tds_socket_init();

	snprintf(ports, sizeof ports, "%d", port);
	hints.ai_family = AF_INET; /* AF_UNSPEC */
	hints.ai_socktype = SOCK_STREAM;
	/* hints.ai_flags = AI_ADDRCONFIG; */
	rc = getaddrinfo(host, ports, &hints, &res);
	if(rc) {
		printf("getaddrinfo failed %d\n", rc);
		return -1;
	}

	for(addr=res;addr;addr=addr->ai_next) {
		fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(fd >= 0) break;
		else printf("socket creation error %s\n", strerror(errno));
	}

	if(!addr) {
		freeaddrinfo(res);
		return -2; /* connection failed */
	}


	tds_socket_timeout(fd, timeout);
	tds_keepalive(fd);

	rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
	freeaddrinfo(res);
	if(rc < 0) {
		close(fd);
		return -3;
	}
	return fd;
}


static int
tds_sendpacket(int fd, int type, const void *data, size_t n) {
	const char *b = (const char*)data;
	size_t n_header = 8, n_pkt;;
	size_t max = TDS_PACKET_SIZE - n_header;
	char buf[TDS_PACKET_SIZE];

	tds_debug("send pdu %zu", n);
	while(n) {
		size_t count = n > max ? max : n;
		n_pkt = count + n_header;
		buf[0] = type;
		buf[1] = n - count > 0 ? TDS_BUFSTAT_NONE : TDS_BUFSTAT_EOM;
		buf[2] = n_pkt >> 8;
		buf[3] = n_pkt;
		buf[4] = buf[5] = buf[6] = buf[7] = 0;
		/* printf("    chunk=%zu\n", n_pkt); */

		assert(n_header + count <= sizeof buf);
		memcpy(buf+n_header, b, count);
		b += count;
		n -= count;

		if(tds_send(fd, buf, n_pkt) != (ptrdiff_t)n_pkt) return -1;
	}
	return 0;
}

static void
tds_dumphex(const void *data, size_t n) {
	const unsigned char *p = (const unsigned char*)data;
	while(n--) printf("%02x", *p++);
	printf("\n");
}

static int
tds_recvpacket(int fd, TdsBuf *buf, TdsHeader *h) {
	unsigned char header[8], packet[TDS_MAX_PACKET_SIZE];
	int rc, n;

	buf->n = 0;
	do {
		if(tds_recv(fd, header, sizeof header) != sizeof header) return -1;
		h->type = header[0];
		h->status = header[1];
		n = (header[2] << 8) | header[3];
		n -= sizeof header;
		if(n > (int)sizeof packet) return -1;
		if(tds_recv(fd, packet, n) != n) return -1;
		if((rc = tds_writebytes(buf, packet, n))) return rc;
	} while(!(header[1] & TDS_BUFSTAT_EOM) && n);
	tds_debug("received pdu %zu", buf->n);
	return 0;
}


static void
tds_dump(const char *filename, const void *data, size_t n) {
	FILE *f = fopen(filename, "wb");
	fwrite(data, 1, n, f);
	fclose(f);
}

TDS_API void
tds_connect1(TdsConn *conn, int fd) {
	memset(conn, 0, sizeof *conn);
	conn->fd = fd;
	conn->npacket = TDS_PACKET_SIZE;
}

TDS_API int
tds_connect(TdsConn *conn, const char *host, int port, int timeout_sec) {
	int fd = tds_socket_connect(host, port, timeout_sec);
	if(fd < 0) return -1;
	tds_connect1(conn, fd);
	return 0;
}

TDS_API void
tds_timeout(TdsConn *conn, int seconds) {
	tds_socket_timeout(conn->fd, seconds);
}

static int
tds_parse_envchange(TdsParser *parser) {
	unsigned char *p;
	TdsSlice s2;
	char key[256], value[256];
	int n, size, type;

	tds_debug("TDS_ENVCHANGE");
	if(tds_slice(&parser->slice, 2, &p)) return -1;
	size = (p[1] << 8) | p[0];
	if(tds_slice2(&parser->slice, size, &s2)) return -1;
	while(s2.n) {
		if(tds_slice(&s2, 1, &p)) break; /* type */
		type = *p;
		if(tds_slice(&s2, 1, &p)) break;
		n = *p;
		if(tds_slice(&s2, n, &p)) break;
		memcpy(key, p, n); key[n] = 0;
		if(tds_slice(&s2, 1, &p)) break;
		n = *p;
		if(tds_slice(&s2, n, &p)) break;
		memcpy(value, p, n); value[n] = 0;
		if(type == TDS_ENV_PACKSIZE) {
			n = atoi(key);
			if(n > TDS_MAX_PACKET_SIZE || n < TDS_PACKET_SIZE)
				return tds_parser_make_error(parser, "Invalid packet size %d", n);
			parser->npacket = n;
		}
	}
	return 0;
}

static int
tds_parse_msg(TdsParser *parser) {
	unsigned char *p;
	TdsSlice s2;
	int n, args;

	tds_debug("TDS_MSG");
	if(tds_slice(&parser->slice, 1, &p)) return 0; /* length */
	n = *p;
	if(tds_slice2(&parser->slice, n, &s2)) return 0;
	n += 2;
	if(tds_slice(&s2, 1, &p)) return -1;
	args = *p == TDS_MSG_HASARGS;
	(void)args;
	if(tds_slice(&s2, 2, &p)) return -1;
	return 0;
}

static int
tds_parse_loginack(TdsParser *parser) {
	TdsSlice s2;
	unsigned char *p;
	int n;
	char version[4];

	tds_debug("TDS_LOGINACK");
	if(tds_slice(&parser->slice, 2, &p)) return -1;
	n = (p[1] << 8) | p[0];
	if(tds_slice2(&parser->slice, n, &s2)) return -1;
	if(tds_slice(&s2, 1, &p)) return -1;
	if(*p != TDS_LOG_SUCCEED) return -1; /* login failed */
	if(tds_slice(&s2, 4, &p)) return -1;
	memcpy(version, p, 4);
	parser->logged_in = 1;
	return 0;
}

static int
tds_parse_done(TdsParser *parser) {
	unsigned char *p;
	int status, txn;

	tds_debug("TDS_DONE");
	if(tds_slice(&parser->slice, 2, &p)) return -1;
	status = p[1] << 8 | p[0];
	if(tds_slice(&parser->slice, 2, &p)) return -1;
	txn = p[1] << 8 | p[0];
	(void)txn;
	if(status & TDS_DONE_COUNT) {
		if(tds_slice(&parser->slice, 4, &p)) return -1;
	}
	return 0;
}

static int
tds_parse_orderby(TdsParser *parser) {
	unsigned char *p;
	int count;

	tds_debug("TDS_ORDERBY");
	if(tds_slice(&parser->slice, 2, &p)) return -1;
	count = p[1] << 8 | p[0];
	if(tds_slice(&parser->slice, count, &p)) return -1;
	return 0;
}

static int
tds_parse_capability(TdsParser *parser) {
	unsigned char *p;
	int n;

	tds_debug("TDS_CAPABILITY");
	if(tds_slice(&parser->slice, 2, &p)) return -1;
	n = p[1] << 8 | p[0];
	if(tds_slice(&parser->slice, n, &p)) return -1;
	return 0;
}

static int
tds_parse_eed(TdsParser *parser) {
	TdsSlice s2;
	unsigned char *p;
	int severity, err = 0;
	unsigned n;

	tds_debug("TDS_EED");
	if(tds_slice(&parser->slice, 2, &p)) return 0;
	n = p[1] << 8 | p[0];
	if(tds_slice2(&parser->slice, n, &s2)) return -1;
	if(tds_slice(&s2, 4, &p)) return -1; /* msg number */
	if(tds_slice(&s2, 1, &p)) return -1; /* state */
	if(tds_slice(&s2, 1, &p)) return -1; /* class */
	severity = *p;
	if(tds_slice(&s2, 1, &p)) return -1; /* sql statelen */
	if(tds_slice(&s2, *p, &p)) return -1; /* sql state */
	if(tds_slice(&s2, 1, &p)) return -1; /* status */
	if(tds_slice(&s2, 2, &p)) return -1; /* transactionstatus */
	if(tds_slice(&s2, 2, &p)) return -1; /* msg len */
	n = p[1] << 8 | p[0];
	if(tds_slice(&s2, n, &p)) return -1; /* msg body */
	if(n > 0 && severity > 10) {
		tds_debug("eed error level %d", severity);
		err = tds_parser_make_error(parser, "%.*s", (int)n, (char*)p);
	}
	if(tds_slice(&s2, 1, &p)) return -1; /* server name length */
	if(tds_slice(&s2, *p, &p)) return -1; /* server name */
	if(tds_slice(&s2, 1, &p)) return -1; /* proc name length */
	if(tds_slice(&s2, *p, &p)) return -1; /* proc name */
	if(tds_slice(&s2, 2, &p)) return -1; /* line number. ushort */
	n = p[1] << 8 | p[0];
	return err;
}

static int
tds_parse_login(TdsConn *conn, TdsBuf *buf) {
	TdsParser p;

	tds_parser_init(&p, buf->data, buf->n);
	while(tds_row(&p)) {}
	if(p.logged_in) conn->logged_in = 1;
	if(p.npacket) conn->npacket = p.npacket;
	return tds_parser_error(&p) ? -1 : 0;
}

/* return: -1 error. 0 not rowfmt2. > 0 length of data parsed */
static int
tds_parse_rowfmt(TdsParser *parser, unsigned char type) {
	unsigned char *p;
	int len, cols,i, n, fmt2, have_label;
	TdsCol *col;

	fmt2 = type == TDS_ROWFMT2;
	tds_debug("TDS_ROWFMT%s", fmt2 ? "2" : "");
	if(tds_slice(&parser->slice, 4, &p)) goto error;
	memcpy(&len, p, 4);
	len += 5;
	if(tds_slice(&parser->slice, 2, &p)) goto error;
	cols = 0;
	memcpy(&cols, p, 2);
	if(cols > TDS_MAXCOLS)
		return tds_parser_make_error(parser,
			"Too many columns: %d > %d",
			cols, TDS_MAXCOLS);
	parser->ncols = cols;
	if(!parser->ncols) goto error;

	for(i=0;i<parser->ncols;i++) {
		col = &parser->cols[i];
		have_label = 0;
		if(fmt2) {
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* nlabel */
			n = *p;
			if(tds_slice(&parser->slice, n, &p)) goto error; /* label */
			if(n) {
				snprintf(col->name, sizeof col->name, "%.*s", n, (char*)p);
				have_label = 1;
			}
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* ncatalog*/
			if(tds_slice(&parser->slice, *p, &p)) goto error; /* catalog */
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* nschema */
			if(tds_slice(&parser->slice, *p, &p)) goto error; /* schema */
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* ntable */
			if(tds_slice(&parser->slice, *p, &p)) goto error; /* table */
		}
		if(tds_slice(&parser->slice, 1, &p)) goto error; /* ncolumn */
		n = *p;
		if(tds_slice(&parser->slice, n, &p)) goto error; /* column */
		if(!have_label) snprintf(col->name, sizeof col->name, "%.*s", n, (char*)p);
		if(fmt2) {
			if(tds_slice(&parser->slice, 4, &p)) goto error; /* status */
		} else {
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* status */
		}
		if(tds_slice(&parser->slice, 4, &p)) goto error; /* user type */
		if(tds_slice(&parser->slice, 1, &p)) goto error; /* data type */
		col->type = *p;
		/* printf("type=%02x\n", col->type); */
		switch(col->type) {
		case TDS_INTN:
		case TDS_UINTN:
		case TDS_CHAR:
		case TDS_VARCHAR:
		case TDS_BOUNDARY:
		case TDS_SENSITIVITY:
		case TDS_BINARY:
		case TDS_VARBINARY:
		case TDS_FLTN:
		case TDS_DATETIMEN:
		case TDS_DATEN:
		case TDS_TIMEN:
		case TDS_MONEYN:
			if(tds_slice(&parser->slice, 1, &p)) goto error;
			col->len = *p;
			break;
		case TDS_LONGCHAR:
		case TDS_LONGBINARY:
			if(tds_slice(&parser->slice, 4, &p)) goto error;
			col->len = p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
			break;
		case TDS_BLOB:
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* blob type */
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* nlocale */
			if(tds_slice(&parser->slice, *p, &p)) goto error; /* locale */
			if(tds_slice(&parser->slice, 2, &p)) goto error; /* nclassid */
			n = p[0] | p[1] << 8;
			if(tds_slice(&parser->slice, n, &p)) goto error; /* classid */
			continue; /* skip locale at bottom */
		case TDS_DECN:
		case TDS_NUMN:
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* len */
			col->len = *p;
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* precision */
			col->precision = *p;
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* scale */
			col->scale = *p;
			break;
		case TDS_TEXT:
		case TDS_XML:
		case TDS_IMAGE:
		case TDS_UNITEXT:
			if(tds_slice(&parser->slice, 4, &p)) goto error; /* len */
			col->len = p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
			if(tds_slice(&parser->slice, 2, &p)) goto error;
			n = p[0] | p[1] << 8;
			if(tds_slice(&parser->slice, n, &p)) goto error; /* varname */
			break;
		case TDS_BIGDATETIMEN:
			if(tds_slice(&parser->slice, 1, &p)) goto error;
			col->len = *p;
			if(tds_slice(&parser->slice, 1, &p)) goto error; /* possibly resolution */
			break;
		}

		if(tds_slice(&parser->slice, 1, &p)) goto error; /* nlocale */
		if(tds_slice(&parser->slice, *p, &p)) goto error; /* locale */
	}
	return 0;
error:
	return tds_parser_make_error(parser, "TDS_ROWFMT%s bad format", fmt2 ? "2" : "");
}

static int
tds_parse_column(TdsParser *parser, TdsValue *v) {
	unsigned char *p;
	TdsCol *col;
	const int64_t seconds_epoch = 2208988800LL; /* seconds since 1,1,1900 UTC */
	const int days_epoch = 25567; /* days since 1,1,1990 to 1,1,1970 UTC */
	unsigned n;

	memset(v, 0, sizeof *v);
	col = &parser->cols[parser->i++];
	v->name = col->name;
	switch(col->type) {
	case TDS_BIT:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		v->type = tds_type_bool;
		v->data.boolean = *p;
		break;
	case TDS_INT1:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		v->type = tds_type_u8;
		v->data.u8 = *p;
		break;
	case TDS_SINT1:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		v->type = tds_type_i8;
		v->data.i8 = (signed char)*p;
		break;
	case TDS_UINT2:
		if(tds_slice(&parser->slice, 2, &p)) return -1;
		memcpy(&v->data.u16, p, 2);
		v->type = tds_type_u16;
		break;
	case TDS_INT2:
		if(tds_slice(&parser->slice, 2, &p)) return -1;
		memcpy(&v->data.i16, p, 2);
		v->type = tds_type_i16;
		break;
	case TDS_UINT4:
		if(tds_slice(&parser->slice, 4, &p)) return -1;
		memcpy(&v->data.u32, p, 4);
		v->type = tds_type_u32;
		break;
	case TDS_INT4:
		if(tds_slice(&parser->slice, 4, &p)) return -1;
		memcpy(&v->data.i32, p, 4);
		v->type = tds_type_i32;
		break;
	case TDS_UINT8:
		if(tds_slice(&parser->slice, 8, &p)) return -1;
		memcpy(&v->data.u64, p, 8);
		v->type = tds_type_u64;
		break;
	case TDS_INT8:
		if(tds_slice(&parser->slice, 8, &p)) return -1;
		memcpy(&v->data.i64, p, 8);
		v->type = tds_type_i64;
	case TDS_FLT4:
		if(tds_slice(&parser->slice, 4, &p)) return -1;
		memcpy(&v->data.f, p, 4);
		v->type = tds_type_float;
		break;
	case TDS_FLT8:
		if(tds_slice(&parser->slice, 8, &p)) return -1;
		memcpy(&v->data.d, p, 8);
		v->type = tds_type_double;
		break;
	case TDS_INTN:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(tds_slice(&parser->slice, n, &p)) return -1;
		switch(n) {
		default:
		case 0: switch(col->len) {
			case 1: v->data.i8 = INT8_MIN;
				v->type = tds_type_i8;
				break;
			case 2: v->data.i16 = INT16_MIN;
				v->type = tds_type_i16;
				break;
			case 4: v->data.i32 = INT32_MIN;
				v->type = tds_type_i32;
				break;
			case 8: v->data.i64 = INT64_MIN;
				v->type = tds_type_i64;
				break;
			default: return -2;
			}
		case 1: memcpy(&v->data.i8, p, n);
			v->type = tds_type_i8; break;
		case 2: memcpy(&v->data.i16, p, n);
			v->type = tds_type_i16; break;
		case 4: memcpy(&v->data.i32, p, n);
			v->type = tds_type_i32; break;
		case 8: memcpy(&v->data.i64, p, n);
			v->type = tds_type_i64; break;
		}
		break;
	case TDS_UINTN:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(tds_slice(&parser->slice, n, &p)) return -1;
		switch(n) {
		default:
		case 0: switch(col->len) {
			case 1: v->data.u8 = UINT8_MAX;
				v->type = tds_type_u8;
				break;
			case 2: v->data.u16 = UINT16_MAX;
				v->type = tds_type_u16;
				break;
			case 4: v->data.u32 = UINT32_MAX;
				v->type = tds_type_u32;
				break;
			case 8: v->data.u64 = UINT64_MAX;
				v->type = tds_type_u64;
				break;
			default: return -2;
			}
		case 1: memcpy(&v->data.u8, p, n);
			v->type = tds_type_u8; break;
		case 2: memcpy(&v->data.u16, p, n);
			v->type = tds_type_u16; break;
		case 4: memcpy(&v->data.u32, p, n);
			v->type = tds_type_u32; break;
		case 8: memcpy(&v->data.u64, p, n);
			v->type = tds_type_u64; break;
		}
		break;
	case TDS_FLTN:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(tds_slice(&parser->slice, n, &p)) return -1;
		switch(n) {
		default:
		case 0: switch(col->len) {
			case 4: v->data.f = NAN;
				v->type = tds_type_float; break;
			case 8: v->data.d = NAN;
				v->type = tds_type_double; break;
			default: return -2;
			}
			break;
		case 4: memcpy(&v->data.f, p, n);
			v->type = tds_type_float; break;
		case 8: memcpy(&v->data.d, p, n);
			v->type = tds_type_double; break;
		}
		break;
	case TDS_CHAR:
	case TDS_VARCHAR:
	case TDS_BOUNDARY:
	case TDS_SENSITIVITY:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(tds_slice(&parser->slice, n, &p)) return -1;
		v->type = tds_type_string;
		v->data.s.text = (char*)p;
		v->data.s.n = n;
		break;
	case TDS_LONGCHAR:
		if(tds_slice(&parser->slice, 4, &p)) return -1;
		memcpy(&n, p, 4);
		if(tds_slice(&parser->slice, n, &p)) return -1;
		v->type = tds_type_string;
		v->data.s.text = (char*)p;
		v->data.s.n = n;
		break;
	case TDS_BINARY:
	case TDS_VARBINARY:
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(tds_slice(&parser->slice, n, &p)) return -1;
		v->type = tds_type_bytes;
		v->data.bytes.p = p;
		v->data.bytes.n = n;
		break;
	case TDS_LONGBINARY:
		/* col->user_type = 34 or 35 is utf-16 string */
		if(tds_slice(&parser->slice, 4, &p)) return -1;
		memcpy(&n, p, 4);
		if(tds_slice(&parser->slice, n, &p)) return -1;
		v->type = tds_type_bytes;
		v->data.bytes.p = p;
		v->data.bytes.n = n;
	case TDS_NUMN:
	case TDS_DECN: {
		double x = NAN;
		unsigned j;
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(n) {
			int positive;
			if(tds_slice(&parser->slice, 1, &p)) return -1;
			positive = *p == 0;
			x = 0;
			if(tds_slice(&parser->slice, --n, &p)) return -1;
			for(j=0;j<n;j++) {
				x *= 256;
				x += p[j];
			}
			if(col->scale) x /= pow(10, col->scale);
			if(!positive) x *= -1.0;
		}
		v->type = tds_type_double;
		v->data.d = x;
	} break;
	case TDS_MONEYN:
	case TDS_SHORTMONEY:
	case TDS_MONEY: {
		double x = NAN;
		unsigned j;
		switch(col->type) {
		case TDS_MONEYN:
			if(tds_slice(&parser->slice, 1, &p)) return -1;
			n = *p;
			break;
		case TDS_SHORTMONEY: n = 4; break;
		case TDS_MONEY: n = 8; break;
		default: return -2;
		}
		switch(n) {
		case 0: break;
		case 4:
		case 8: x = 0;
			if(tds_slice(&parser->slice, n, &p)) return -1;
			for(j=0;j<n;j++) {
				x *= 10.0;
				x += p[j];
			}
			x /= 10000;
			break;
		}
		v->type = tds_type_double;
		v->data.d = x;
	} break;
	case TDS_DATE:
	case TDS_DATEN:
		v->type = tds_type_date;
		switch(col->type) {
		case TDS_DATE: n = 4; break;
		case TDS_DATEN:
			if(tds_slice(&parser->slice, 1, &p)) return -1;
			n = *p;
			break;
		default: return -2;
		}
		switch(n) {
		case 0: v->data.date = INT32_MIN; break;
		case 4:
			if(tds_slice(&parser->slice, n, &p)) return -1;
			memcpy(&v->data.date, p, 4); /* days */
			v->data.date += days_epoch;
			break;
		default: return -2;
		}
		break;
	case TDS_SHORTDATE: {
		short a, b;
		if(tds_slice(&parser->slice, 2, &p)) return -1;
		memcpy(&a, p, 2); /* days */
		if(tds_slice(&parser->slice, 2, &p)) return -1;
		memcpy(&b, p, 2);
		v->type = tds_type_time;
		v->data.time = ((int64_t)a * 86400 + (int64_t)b * 60 - seconds_epoch) * 1000000000 /* seconds to nanoseconds */;
	} break;
	case TDS_TIME:
	case TDS_TIMEN: {
		uint32_t x;
		v->type = tds_type_time;
		switch(col->type) {
		case TDS_TIME: n = 4; break;
		case TDS_TIMEN:	if(tds_slice(&parser->slice, 1, &p)) return -1;
				n = *p;
				break;
		default: return -1;
		}
		switch(n) {
		case 0: v->data.time = INT64_MIN; break;
		case 4:
			if(tds_slice(&parser->slice, n, &p)) return -1;
			memcpy(&x, p, 4); /* ticks */
			x /= 300; /* ticks to seconds */
			x += seconds_epoch;
			v->data.time = x * 1000000000; /* second to nanoseconds */
			break;
		default: return -1;
		}
	} break;
	case TDS_DATETIME:
	case TDS_DATETIMEN: {
		v->type = tds_type_time;
		switch(col->type) {
		case TDS_DATETIME: n = 8; break;
		case TDS_DATETIMEN: if(tds_slice(&parser->slice, 1, &p)) return -1;
				n = *p;
				break;
		default: return -1;
		}
		switch(n) {
		default: return -2;
		case 0: v->data.time = INT64_MIN; break;
		case 4: {
			short a, b;
			if(tds_slice(&parser->slice, 2, &p)) return -1;
			memcpy(&a, p, 2); /* days since 1900 */
			if(tds_slice(&parser->slice, 2, &p)) return -1;
			memcpy(&b, p, 2); /* minutes */
			v->data.time = (int64_t)a * 86400 + b * 60 - seconds_epoch;
			v->data.time *= 1000000000; /* seconds to nanoseconds */
		} break;
		case 8: {
			int a, b;
			if(tds_slice(&parser->slice, 4, &p)) return -1;
			memcpy(&a, p, 4); /* days */
			if(tds_slice(&parser->slice, 4, &p)) return -1;
			memcpy(&b, p, 4); /* ticks: 300 / sec */
			v->data.time = (uint64_t)a * 86400 + b / 300 - seconds_epoch;
			v->data.time *= 1000000000; /* seconds to nanoseconds */
		} break;
		}
	} break;
	case TDS_XML:
	case TDS_TEXT:
		v->type = tds_type_string;
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(n) {
			if(tds_slice(&parser->slice, n, &p)) return -1; /* text */
			if(tds_slice(&parser->slice, 8, &p)) return -1; /* timestamp */
			if(tds_slice(&parser->slice, 4, &p)) return -1;
			memcpy(&n, p, 4); /* length of string */
			if(tds_slice(&parser->slice, n, &p)) return -1; /* actual string data */
			v->data.s.text = (char*)p;
			v->data.s.n = n;
		} else memset(&v->data.s, 0, sizeof v->data.s);
		break;
	case TDS_IMAGE:
		v->type = tds_type_bytes;
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(n) {
			if(tds_slice(&parser->slice, n, &p)) return -1; /* text */
			if(tds_slice(&parser->slice, 8, &p)) return -1; /* timestamp */
			if(tds_slice(&parser->slice, 4, &p)) return -1; /* data len */
			memcpy(&n, p, 4);
			if(tds_slice(&parser->slice, n, &p)) return -1; /* binary data */
			v->data.bytes.p = (uint8_t*)p;
			v->data.bytes.n = n;
		} else memset(&v->data.bytes, 0, sizeof v->data.bytes);
		break;
	case TDS_UNITEXT:
		v->type = tds_type_bytes; /* TODO: convert UTF-16 to UTF-8 */
		if(tds_slice(&parser->slice, 1, &p)) return -1;
		n = *p;
		if(n) {
			if(tds_slice(&parser->slice, n, &p)) return -1; /* text */
			if(tds_slice(&parser->slice, 8, &p)) return -1; /* timestamp */
			if(tds_slice(&parser->slice, 4, &p)) return -1; /* data len */
			memcpy(&n, p, 4);
			if(tds_slice(&parser->slice, n, &p)) return -1; /* utf-16 data */
			v->data.bytes.p = (uint8_t*)p;
			v->data.bytes.n = n;
		} else memset(&v->data.bytes, 0, sizeof v->data.bytes);
		break;
	default:
		tds_parser_make_error(parser, "TDS: unknown type %02x", col->type);
		return -3;
	}
	return 0;
}

TDS_API void
tds_parser_init(TdsParser *p, const void *data, size_t n) {
	p->slice.p = (unsigned char*)data;
	p->slice.n = n;
	p->state = 'p';
	p->error[0] = 0;
	p->ncols = 0;
	p->logged_in = 0;
	p->npacket = 0;
	p->i = 0;
}


static int
tds_parser_consume(TdsParser *p) {
	unsigned char *c;
	int rc;

	for(;;) {
		if(tds_slice(&p->slice, 1, &c)) return tds_parser_make_error(p, "Bad return format");
		switch(*c) {
		case TDS_CAPABILITY:
			if(tds_parse_orderby(p)) return tds_parser_make_error(p, "TDS_CAPABILITY bad format");
			break;
		case TDS_DONE:
			if(tds_parse_done(p) < 0) return tds_parser_make_error(p, "TDS_DONE bad format");
			p->state = 'd';
			return 2;
		case TDS_EED:
			if(tds_parse_eed(p)) return -1;
			break;
		case TDS_ENVCHANGE:
			if(tds_parse_envchange(p)) return tds_parser_make_error(p, "TDS_ENVCHANGE bad format");
			break;
		case TDS_LOGINACK:
			if(tds_parse_loginack(p)) return tds_parser_make_error(p, "TDS_LOGINACK bad format");
			break;
		case TDS_MSG:
			if(tds_parse_msg(p)) return tds_parser_make_error(p, "TDS_MSG bad format");
			break;
		case TDS_ORDERBY:
			if(tds_parse_orderby(p)) return tds_parser_make_error(p, "TDS_ORDERBY bad format");
			break;
		case TDS_ROW: p->state = 'p'; return 1;
		case TDS_ROWFMT:
		case TDS_ROWFMT2:
			if((rc = tds_parse_rowfmt(p, *c))) return rc;
			break;
		}
	}
	return 0;
}

/* return 1 on has row of data. return 0 on done or error */
TDS_API int
tds_row(TdsParser *p) {
	int rc;
	if(p->state != 'p') return 0;

	for(;;) {
		rc = tds_parser_consume(p);
		if(rc < 0 || rc == 2) return 0;
		if(!rc) continue;
		break; /* rc == 1 */
	}
	if(p->state != 'p') return 0;
	p->i = 0;
	return 1;
}
/* return 1 on has column value. return 0 on done or error */
TDS_API int
tds_col(TdsParser *p, TdsValue *v) {
	int rc;
	if(p->state != 'p') return 0;
	if(p->i >= p->ncols) return 0;
	rc = tds_parse_column(p, v);
	if(rc) return tds_parser_make_error(p, "Error parsing column %u", p->i);
	return 1;
}
/* returns error if there is one */
TDS_API const char*
tds_parser_error(TdsParser *p) {
	return p->state == 'e' ? p->error : 0;
}

TDS_API int tds_i8(TdsValue *v, int8_t *i) {
	int64_t x;
	int rc = tds_i64(v, &x);
	*i = (int8_t)x;
	return rc;
}
TDS_API int tds_i16(TdsValue *v, int16_t *i) {
	int64_t x;
	int rc = tds_i64(v, &x);
	*i = (int16_t)x;
	return rc;
}
TDS_API int tds_i32(TdsValue *v, int32_t *i) {
	int64_t x;
	int rc = tds_i64(v, &x);
	*i = (int32_t)x;
	return rc;
}
TDS_API int tds_i64(TdsValue *v, int64_t *i) {
	switch(v->type) {
	case tds_type_float:
		*i = (int64_t)v->data.f;
		return !isnan(v->data.f);
	case tds_type_double:
		*i = (int64_t)v->data.d;
		return !isnan(v->data.d);
	case tds_type_bool:
		*i = (int64_t)v->data.boolean;
		return v->data.boolean != UINT8_MAX;
	case tds_type_i8:
		*i = v->data.i8;
		return v->data.i8 != INT8_MIN;
	case tds_type_i16:
		*i = v->data.i16;
		return v->data.i16 != INT16_MIN;
	case tds_type_i32:
		*i = v->data.i32;
		return v->data.i32 != INT32_MIN;
	case tds_type_i64:
		*i = v->data.i64;
		return v->data.i64 != INT64_MIN;
	case tds_type_u8:
		*i = (int64_t)v->data.u8;
		return v->data.u8 != UINT8_MAX;
	case tds_type_u16:
		*i = (int64_t)v->data.u16;
		return v->data.u16 != UINT16_MAX;
	case tds_type_u32:
		*i = (int32_t)v->data.u32;
		return v->data.u32 != UINT32_MAX;
	case tds_type_u64:
		*i = (int64_t)v->data.u64;
		return v->data.u64 != UINT64_MAX;
	case tds_type_date:
		*i = v->data.date;
		return v->data.date != INT32_MIN;
	case tds_type_time:
		*i = v->data.time;
		return v->data.time != INT64_MIN;
	default: return 0;
	}
}
TDS_API int tds_bool(TdsValue *v, uint8_t *b) {
	return tds_u8(v, b);
}
TDS_API int tds_u8(TdsValue *v, uint8_t *i) {
	uint64_t x;
	int rc = tds_u64(v, &x);
	*i = (uint8_t)x;
	return rc;
}
TDS_API int tds_u16(TdsValue *v, uint16_t *i) {
	uint64_t x;
	int rc = tds_u64(v, &x);
	*i = (uint16_t)x;
	return rc;
}
TDS_API int tds_u32(TdsValue *v, uint32_t *i) {
	uint64_t x;
	int rc = tds_u64(v, &x);
	*i = (uint32_t)x;
	return rc;
}
TDS_API int tds_u64(TdsValue *v, uint64_t *i) {
	switch(v->type) {
	case tds_type_float:
		*i = (uint64_t)v->data.f;
		return !isnan(v->data.f);
	case tds_type_double:
		*i = (uint64_t)v->data.d;
		return !isnan(v->data.d);
	case tds_type_bool:
		*i = (uint64_t)v->data.boolean;
		return v->data.boolean != UINT8_MAX;
	case tds_type_i8:
		*i = (uint8_t)v->data.i8;
		return v->data.i8 != INT8_MIN;
	case tds_type_i16:
		*i = (uint16_t)v->data.i16;
		return v->data.i16 != INT16_MIN;
	case tds_type_i32:
		*i = (uint32_t)v->data.i32;
		return v->data.i32 != INT32_MIN;
	case tds_type_i64:
		*i = (uint64_t)v->data.i64;
		return v->data.i64 != INT64_MIN;
	case tds_type_u8:
		*i = (uint64_t)v->data.u8;
		return v->data.u8 != UINT8_MAX;
	case tds_type_u16:
		*i = (uint64_t)v->data.u16;
		return v->data.u16 != UINT16_MAX;
	case tds_type_u32:
		*i = (uint32_t)v->data.u32;
		return v->data.u32 != UINT32_MAX;
	case tds_type_u64:
		*i = (uint64_t)v->data.u64;
		return v->data.u64 != UINT64_MAX;
	case tds_type_date:
		*i = (uint32_t)v->data.date;
		return v->data.date != INT32_MIN;
	case tds_type_time:
		*i = (uint64_t)v->data.time;
		return v->data.time != INT64_MIN;
	default: return 0;
	}
}
TDS_API int tds_float(TdsValue *v, float *i) {
	double d;
	int rc = tds_double(v, &d);
	*i = (float)d;
	return rc;
}
TDS_API int tds_double(TdsValue *v, double *i) {
	switch(v->type) {
	case tds_type_float:
		*i = (double)v->data.f;
		return !isnan(v->data.f);
	case tds_type_double:
		*i = v->data.d;
		return !isnan(v->data.d);
	case tds_type_bool:
		*i = (double)v->data.boolean;
		return v->data.boolean != UINT8_MAX;
	case tds_type_i8:
		*i = (double)v->data.i8;
		return v->data.i8 != INT8_MIN;
	case tds_type_i16:
		*i = (double)v->data.i16;
		return v->data.i16 != INT16_MIN;
	case tds_type_i32:
		*i = (double)v->data.i32;
		return v->data.i32 != INT32_MIN;
	case tds_type_i64:
		*i = (double)v->data.i64;
		return v->data.i64 != INT64_MIN;
	case tds_type_u8:
		*i = (double)v->data.u8;
		return v->data.u8 != UINT8_MAX;
	case tds_type_u16:
		*i = (double)v->data.u16;
		return v->data.u16 != UINT16_MAX;
	case tds_type_u32:
		*i = (double)v->data.u32;
		return v->data.u32 != UINT32_MAX;
	case tds_type_u64:
		*i = (double)v->data.u64;
		return v->data.u64 != UINT64_MAX;
	case tds_type_date:
		*i = (double)v->data.date;
		return v->data.date != INT32_MIN;
	case tds_type_time:
		*i = (double)v->data.time;
		return v->data.time != INT64_MIN;
	default: return 0;
	}
}
TDS_API void tds_str(TdsValue *v, const char **c, size_t *n) {
	if(v->type == tds_type_string) {
		*c = v->data.s.text;
		*n = v->data.s.n;
		return;
	}
	*c = 0;
	*n = 0;
}
TDS_API void tds_bytes(TdsValue *v, uint8_t **c, size_t *n) {
	if(v->type == tds_type_bytes || v->type == tds_type_string) {
		*c = v->data.bytes.p;
		*n = v->data.bytes.n;
		return;
	}
	*c = 0;
	*n = 0;
}
TDS_API int tds_date(TdsValue *v, int32_t *i) {
	if(v->type == tds_type_i32 || v->type == tds_type_date)
		return tds_i32(v, i);
	if(v->type == tds_type_i64 || v->type == tds_type_time) {
		int64_t x;
		int rc = tds_i64(v, &x);
		*i = x / 1000000000LL / 86400;
		return rc;
	}
	return 0;
}
/* nanoseconds since unix epoch */
TDS_API int tds_time(TdsValue *v, int64_t *i) {
	if(v->type == tds_type_i32 || v->type == tds_type_date) {
		int32_t x;
		int rc = tds_i32(v, &x);
		*i = (int64_t)i * 1000000000LL * 86400;
		return rc;
	}
	if(v->type == tds_type_i64 || v->type == tds_type_time)
		return tds_i64(v, i);
	return 0;
}

TDS_API void tds_response_destroy(TdsResponse* r) {
	free(r->data);
}

TDS_API int
tds_query(TdsConn *conn, TdsResponse *resp, const char *sql, ...) {
	va_list args;
	int rc;
	va_start(args, sql);
	rc = tds_vquery(conn, resp, sql, args);
	va_end(args);
	return rc;
}

TDS_API int
tds_vquery(TdsConn *conn, TdsResponse *resp, const char *format, va_list args) {
	int n, rc;
	char *sql, *p;
	va_list arg0, arg1;
	TdsBuf buf = {0};
	TdsHeader h;

	memset(resp, 0, sizeof *resp);
	if(tds_error(conn)) return -1;

	va_copy(arg0, args);
	va_copy(arg1, args);

	n = vsnprintf(0, 0, format, arg0);
	if(n < 0) goto error;

	++n;
	sql = (char*)malloc(n + 7);
	if(!sql) goto error;
	vsnprintf(sql+6, n, format, arg1);

	sql[0] = TDS_LANGUAGE;
	sql[1] = n;
	sql[2] = n >> 8;
	sql[3] = n >> 16;
	sql[4] = n >> 24;
	sql[5] = 0; /* status. no params */
	n += 5;

	tds_debug("send TDS_LANGUAGE");
	rc = tds_sendpacket(conn->fd, TDS_BUF_NORMAL, sql, n);
	free(sql);
	if(rc) goto error;
	if(tds_recvpacket(conn->fd, &buf, &h)) goto error;
	assert(h.type == TDS_BUF_RESPONSE);
	p = (char*)realloc(buf.data, buf.n);
	resp->data = p ? p : buf.data;
	resp->n = buf.n;
	return 0;
error:
	tds_make_error(conn, "TDS query failed");
	return -1;
}

TDS_API int
tds_login(TdsConn *conn, const char *host, const char *app, const char *user, const char *password) {
	TdsBuf b = {0};
	const char lbufsize[] = {0,0,0,0};
	const char lspare[] = {0,0,0};
	const char version[] = {5,0,0,0};
	const char lprogvers[] = {0x0f,0x07,0x00,0x0d};
	const char loldsecure[] = {0,0};
	const char empty[] = {0,0,0,0};
	const char lsec[]={0,8,0,0,0,0,0,0,0,0};
	TdsHeader h;
	int rc;
	char localhost[256];

	if(tds_error(conn)) return -1;

	localhost[0] = 0;
	gethostname(localhost, sizeof localhost);
	/* client's hostname */
	if(tds_writestr(&b, localhost, TDS_MAXNAME)) goto error;
	/* client username */
	if(tds_writestr(&b, user, TDS_MAXNAME)) goto error;
	/* password. if empty then use encrypted password later */
	if(tds_writestr(&b, password,  TDS_MAXNAME)) goto error;
	/* app process name. process id? */
	if(tds_writestr(&b, app, TDS_MAXNAME)) goto error;
	if(tds_writebyte(&b, TDS_INT2_LSB_LO)) goto error;
	if(tds_writebyte(&b, TDS_INT4_LSB_LO)) goto error;
	if(tds_writebyte(&b, TDS_CHAR_ASCII)) goto error;
	if(tds_writebyte(&b, TDS_FLT_IEEE_LO)) goto error;
	if(tds_writebyte(&b, TDS_TWO_I4_LSB_LO)) goto error;
	if(tds_writebyte(&b, TDS_OPT_TRUE)) goto error; /* lusedb */
	if(tds_writebyte(&b, TDS_OPT_FALSE)) goto error; /* ldmpld */
	if(tds_writebyte(&b, TDS_LDEFSQL)) goto error; /* linterfacespare */
	if(tds_writebyte(&b, 0)) goto error; /* ltype = none */
	if(tds_writebytes(&b, lbufsize, sizeof lbufsize)) goto error; /* unused but needs sent */
	if(tds_writebytes(&b, lspare, sizeof lspare)) goto error; /* unused but needs sent */
	/* app name */
	if(tds_writestr(&b, app, TDS_MAXNAME)) goto error;
	/* server name */
	if(tds_writestr(&b, host, TDS_MAXNAME)) goto error;

	/* weird password. or empty string if encrypt password is false */
	if(tds_writeweirdstr(&b, password, TDS_RPLEN)) goto error;

	if(tds_writebytes(&b, version, sizeof version)) goto error;
	/* client library */
	if(tds_writestr(&b, "ADO.NET", TDS_PROGNLEN)) goto error;
	if(tds_writebytes(&b, lprogvers, sizeof lprogvers)) goto error;
	if(tds_writebyte(&b, TDS_NOCVT_SHORT)) goto error; /* lnoshort */
	if(tds_writebyte(&b, TDS_FLT4_IEEE_LO)) goto error; /* lflt4 */
	if(tds_writebyte(&b, TDS_TWO_I2_LSB_LO)) goto error; /* ldate4 */
	if(tds_writestr(&b, "us_english", TDS_MAXNAME)) goto error;
	if(tds_writebyte(&b, TDS_NOTIFY)) goto error;
	if(tds_writebytes(&b, loldsecure, sizeof loldsecure)) goto error;

	if(tds_writebyte(&b, 0)) goto error; /* lseclogin. Use TDS_SEC_LOG_ENCRYPT3 for secure password */
	if(tds_writebytes(&b, lsec, sizeof lsec)) goto error; /* 0,8,0,0,0,0,0,0,0,0//lsecbulk, lhalogin, lhasessionid, lsecspare */
	if(tds_writestr(&b, "utf8", TDS_MAXNAME)) goto error;
	if(tds_writebyte(&b, TDS_NOTIFY)) goto error; /* lsetcharset */
	if(tds_writestr(&b, TDS_STRING(TDS_MAX_PACKET_SIZE), TDS_PKTLEN)) goto error; /* lpacketsize */
	if(tds_writebytes(&b, empty, sizeof empty)) goto error; /* 4 empty bytes */

	/* capabilities */
	if(tds_writebyte(&b, TDS_CAPABILITY)) goto error;
	if(tds_writebyte(&b, 14*2+4)) goto error; /* low byte of little endian length */
	if(tds_writebyte(&b, 0)) goto error; /* high byte of little endian length */
	if(tds_writebyte(&b, TDS_CAP_REQUEST)) goto error;
	if(tds_writebyte(&b, 14)) goto error; /* # of capability bytes */
	if(tds_writebyte(&b, TDS_REQ_DYNAMIC_SUPPRESS_PARAMFMT)) goto error;
	if(tds_writebyte(&b, TDS_REQ_LOGPARAMS|TDS_REQ_ROWCOUNT_FOR_SELECT|TDS_DATA_LOBLOCATOR|TDS_REQ_LANG_BATCH|TDS_REQ_DYN_BATCH|TDS_REQ_GRID|TDS_REQ_INSTID)) goto error;
	if(tds_writebyte(&b, TDS_RPCPARAM_LOB|TDS_DATA_USECS|TDS_DATA_BIGDATETIME|TDS_UNUSED4|TDS_UNUSED5|TDS_MULTI_REQUESTS|TDS_REQ_MIGRATE|TDS_UNUSED8)) goto error;
	if(tds_writebyte(&b, TDS_REQ_CURINFO3|TDS_DATA_XML|TDS_REQ_LARGEIDENT|TDS_DATA_UNITEXT)) goto error;
	if(tds_writebyte(&b, TDS_REQ_SRVPKTSIZE|TDS_DATA_TIME)) goto error;
	if(tds_writebyte(&b, TDS_DATA_DATE|TDS_BLOB_NCHAR_SCSU|TDS_BLOB_NCHAR_8|TDS_BLOB_NCHAR_16|TDS_IMAGE_NCHAR|TDS_DATA_NLBIN|TDS_DATA_UINTN)) goto error;
	if(tds_writebyte(&b, TDS_DATA_UINT8|TDS_DATA_UINT4|TDS_DATA_UINT2|TDS_REQ_RESERVED2|TDS_WIDETABLE|TDS_REQ_RESERVED1|TDS_OBJECT_BINARY/*|TDS_DATA_COLUMNSTATUS*/)) goto error;
	if(tds_writebyte(&b, TDS_OBJECT_CHAR|TDS_DATA_INT8|TDS_DATA_BITN|TDS_DATA_FLTN)) goto error;
	if(tds_writebyte(&b, TDS_REQ_NONE)) goto error;
	if(tds_writebyte(&b, TDS_DATA_MONEYN)) goto error;
	if(tds_writebyte(&b, TDS_DATA_DATETIMEN|TDS_DATA_INTN|TDS_DATA_LBIN|TDS_DATA_LCHAR|TDS_DATA_DEC|TDS_DATA_IMAGE|TDS_DATA_TEXT|TDS_DATA_NUM)) goto error;
	if(tds_writebyte(&b, TDS_DATA_FLT8|TDS_DATA_FLT4|TDS_DATA_DATE4|TDS_DATA_DATE8|TDS_DATA_MNY4|TDS_DATA_MNY8|TDS_DATA_VBIN|TDS_DATA_BIN)) goto error;
	if(tds_writebyte(&b, TDS_DATA_VCHAR|TDS_DATA_CHAR|TDS_DATA_BIT|TDS_DATA_INT4|TDS_DATA_INT2|TDS_DATA_INT1|TDS_REQ_PARAM|TDS_REQ_MSG)) goto error;
	if(tds_writebyte(&b, TDS_REQ_DYNF|TDS_REQ_MSTMT|TDS_REQ_RPC|TDS_REQ_LANG)) goto error;

	if(tds_writebyte(&b, TDS_CAP_RESPONSE)) goto error;
	if(tds_writebyte(&b, 14)) goto error; /* # of bytes */
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, TDS_RES_NO_TDSCONTROL)) goto error;
	if(tds_writebyte(&b, TDS_RES_SUPPRESS_FMT)) goto error;
	if(tds_writebyte(&b, TDS_DATA_NOINTERVAL)) goto error;
	if(tds_writebyte(&b, TDS_RES_RESERVED1)) goto error;
	if(tds_writebyte(&b, TDS_RES_NOTDSDEBUG|TDS_DATA_NOBOUNDARY)) goto error;
	if(tds_writebyte(&b, TDS_DATA_NOSENSITIVITY|TDS_PROTO_NOBULK|TDS_CON_OOB)) goto error;
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, 0)) goto error;
	if(tds_writebyte(&b, 0)) goto error;

	if(tds_sendpacket(conn->fd, TDS_BUF_LOGIN, b.data, b.n)) {
		free(b.data);
		tds_make_error(conn, "TDS login send error");
		return -1;
	}
	if(tds_recvpacket(conn->fd, &b, &h)) {
		free(b.data);
		tds_make_error(conn, "TDS login recv error");
		return -1;
	}
	assert(h.type == TDS_BUF_RESPONSE);
	if((rc = tds_parse_login(conn, &b))) {
		free(b.data);
		tds_make_error(conn, "TDS login failed");
		return -2;
	}
	free(b.data);
	return 0;
error:
	free(b.data);
	tds_make_error(conn, "TDS login buffer error");
	return -1;
}

TDS_API int
tds_vcommand(TdsConn *conn, const char *format, va_list arg) {
	int rc = 0;
	TdsParser p;
	TdsResponse r;
	rc = tds_vquery(conn, &r, format, arg);
	tds_parser_init(&p, r.data, r.n);
	while(tds_row(&p)) {}
	if(tds_parser_error(&p)) {
		memcpy(conn->error, p.error, sizeof p.error);
		rc = -1;
	}
	tds_response_destroy(&r);
	return rc;
}

TDS_API int
tds_command(TdsConn *conn, const char *format, ...) {
	va_list arg;
	int rc;
	va_start(arg, format);
	rc = tds_vcommand(conn, format, arg);
	va_end(arg);
	return rc;
}
TDS_API void
tds_print_columns(TdsParser *p) {
	int i;
	char buf[5];
	TdsCol *c;
	const char *text;

	for(i=0;i<p->ncols;i++) {
		c = &p->cols[i];
		switch(c->type){
		case TDS_NUMN:case TDS_DECN:case TDS_FLTN:case TDS_FLT4:case TDS_FLT8: text = "float";break;
		case TDS_INTN:case TDS_INT1:case TDS_INT2:case TDS_INT4:case TDS_INT8:
                case TDS_SINT1:case TDS_UINT2:case TDS_UINT4: case TDS_UINT8: text = "int";break;
		case TDS_VARCHAR:case TDS_LONGCHAR:case TDS_CHAR: text="text"; break;
		case TDS_DATETIME:case TDS_SHORTDATE:case TDS_DATEN:
		case TDS_DATE: case TDS_TIME: case TDS_TIMEN: text="datetime"; break;
		case TDS_BINARY: case TDS_VARBINARY: text ="bytes"; break;
		default: snprintf(buf, sizeof buf, "%02x", c->type); break;
		}
		printf("%s %02x %s(%d) p=%d s=%d\n", c->name, c->type, text, c->len, c->precision, c->scale);
	}
}
#endif

#ifdef TDS_EXAMPLE
#define SOCKS5_STATIC
#include "socks5.h"
int main(int argc, char **argv) {
	TdsConn conn = {0};
	TdsResponse resp;
//	FILE *f;

	//int id, year, season;
	//float lat, lng;
	setbuf(stdout, 0);

	const char *sql =
		"select\n"
		"    id, name from table1\n";
	tds_connect(&conn, "host", 9121);
	assert(!tds_login(&conn, "host", "test", "user", "password"));
	assert(!tds_command(&conn, "USE database_name\n"));
	assert(!tds_query(&conn, &resp, sql));

	TdsBuf buf;
	tds_to_csv(&buf, (uint8_t*)resp.data, resp.n);
	printf("csv=%s\n", (char*)buf.data);
	free(buf.data);

	TdsParser p;
	tds_parser_init(&p, resp.data, resp.n);
	tds_row(&p);
	printf("Error %s\n", tds_parser_error(&p));
	printf("received %zu\n", resp.n);
	tds_response_destroy(&resp);
	if(tds_error(&conn)) printf("error: %s\n", tds_error(&conn));
	tds_destroy(&conn);
	printf("success\n");
	return 0;
}
#endif
/* Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

