#ifndef REQUESTS_H
#define REQUESTS_H

#include <stddef.h>

#if defined(REQUESTS_STATIC) || defined(REQUESTS_EXAMPLE)
#define REQUESTS_API static
#define REQUESTS_IMPLEMENTATION
#else
#define REQUESTS_API extern;
#endif

struct Request;

REQUESTS_API struct Request* request_new(const char* url);
REQUESTS_API void request_addheader(struct Request* req, const char *part1, const char *part2);
REQUESTS_API void request_data(struct Request* req, char **data, size_t *ndata);
REQUESTS_API char* request_error(struct Request *req);
REQUESTS_API int request_status(struct Request* req);
REQUESTS_API char* request_reason(struct Request* req);
REQUESTS_API char* request_header(struct Request *req, const char *header);
REQUESTS_API void request_post(struct Request *req, const char *content_type,  const char *content_encoding,  const char *data, size_t ndata, int copy);
REQUESTS_API int request_run(struct Request* req);
REQUESTS_API void request_destroy(struct Request* req);

#endif

#ifdef REQUESTS_IMPLEMENTATION
#include <string.h>

static int request_strnstr(const char* haystack, size_t nhaystack, const char* needle) {
    size_t n = strlen(needle);
    for (int i = 0; i < (int)nhaystack && (size_t)i <= nhaystack - n; i++) {
        if (!strncmp(haystack + i, needle, n))
            return i;
    }
    return -1;
}

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, winhttp.lib)

typedef struct Conn {
    char server[128];
    HINTERNET handle;
    struct Conn* next;
    int port;
    int nserver;
} Conn;

typedef struct RequestHeader {
    wchar_t* header;
    struct RequestHeader* next;
} RequestHeader;

typedef struct RequestOutputHeader {
    char *key;
    char *value;
    struct RequestOutputHeader *next;
} RequestOutputHeader;

typedef struct Request {
    HINTERNET handle;
    char* url;
    char* verb;
    char* reason;
    char* input;
    size_t ninput;
    char* output;
    size_t noutput;
    size_t output_capacity;
    CONDITION_VARIABLE cond;
    SRWLOCK lock;
    char* error;
    RequestHeader* headers;
    RequestOutputHeader *output_headers;
    unsigned owns_input : 1;
    unsigned complete : 1;
} Request;

typedef struct RequestUrl {
    const char* scheme;
    size_t nscheme;
    const char* user;
    size_t nuser;
    const char* password;
    size_t npassword;
    const char* host;
    size_t nhost;
    const char* port;
    size_t nport;
    const char* path; /* excludes leading / */
    size_t npath;
} RequestUrl;

static HINTERNET request_session;
static Conn* request_conns;
static SRWLOCK request_lock;

static char* request_geterror(DWORD err) {
    LPSTR buf = 0;
    DWORD n = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, 0);
    // printf("n error=%u\n", n);
    char* e;
    if (n) {
        e = malloc(n + 1);
        memcpy(e, buf, n);
        e[n] = 0;
    } else {
        char b[256];
        snprintf(b, sizeof b, "Unknown winhttp error: %u", err);
        e = strdup(b);
    }
    if (buf)
        LocalFree(buf);
    return e;
}


/* return number of parameters passed on success, < 0 on failure */
static int request_url_parse(RequestUrl* u, const char* url, size_t nurl) {
    memset(u, 0, sizeof *u);
    if (!nurl) {
        if (url)
            nurl = strlen(url);
        else
            return -1;
    }

    /* scheme */
    int pos = request_strnstr(url, nurl, "://");
    if (!pos)
        return -2;
    if (pos > 0) {
        u->scheme = url;
        u->nscheme = (unsigned)pos;
        url += pos + 3;
        nurl -= pos + 3;
    }

    /* user and optional password */
    pos = request_strnstr(url, nurl, "@");
    if (!pos)
        return -3;
    if (pos > 0) {
        u->user = url;
        int pos2 = request_strnstr(url, (unsigned)pos, ":");
        if (pos2 >= 0) {
            u->nuser = pos2;
            u->password = url + pos2 + 1;
            u->npassword = pos - (pos2 + 1);
        } else {
            u->nuser = pos;
        }
        url += pos + 1;
        nurl -= pos + 1;
    }

    u->host = url;
    pos = request_strnstr(url, nurl, "/");
    if (pos < 0)
        pos = nurl;
    if (pos >= 0) {
        int pos2 = request_strnstr(url, (unsigned)pos, ":");
        if (pos2 >= 0) {
            u->nhost = pos2;
            u->port = url + pos2 + 1;
            u->nport = pos - (pos2 + 1);
        } else {
            u->nhost = pos;
        }
        url += pos;
        nurl -= pos;
        if (nurl) {
            u->path = url;
            u->npath = nurl;
        }
    }

    return 0;
}

static void request_callback(HINTERNET session, DWORD_PTR ctx, DWORD status, void* info, DWORD ninfo) {
    Request* req = (Request*)ctx;
    // printf("callback: status=%u ninfo=%u\n", status, ninfo);
    switch (status) {
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
        ninfo = *(DWORD*)info;
        // printf("request have data: %zu + %u\n", req->noutput, ninfo);
        if (!ninfo) {
            req->complete = 1;
            WakeConditionVariable(&req->cond);
            break;
        }
        if (ninfo > req->output_capacity - req->noutput) {
            size_t cap = req->output_capacity * 2;
            if (cap < ninfo)
                cap = 1024 * 1024;
            if (cap < ninfo)
                cap = ninfo;
            req->output = realloc(req->output, cap);
            req->output_capacity = cap;
        }
        WinHttpReadData(req->handle, req->output + req->noutput, ninfo, 0);
        break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
        // printf("request read complete: %zu + %u\n", req->noutput, ninfo);
        req->noutput += ninfo;
        /* fallthrough */
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
        // printf("request headers available\n");
        WinHttpQueryDataAvailable(req->handle, 0);
        break;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
        // printf("request send request\n");
        if (req->owns_input) {
            free(req->input);
            req->input = 0;
            req->ninput = 0;
        }
        WinHttpReceiveResponse(req->handle, 0);
        break;
    case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
        // printf("request write complete\n");
        break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        // printf("request callback error\n");
        WINHTTP_ASYNC_RESULT* r = info;
        // printf("error code=%u\n", r->dwError);
        req->error = request_geterror(r->dwError);
        // printf("error: %s\n", req->error);
        req->complete = 1;
        WakeConditionVariable(&req->cond);
        break;
    default:
        // printf("request unhandled callback\n");
        break;
    }
}

static void request_init() {
    wchar_t proxy[256];

    if (request_session)
        return;
    AcquireSRWLockExclusive(&request_lock);
    if (!request_session) {
        // printf("initializing request\n");
        char proxybuf[256];
        DWORD nproxybuf = GetEnvironmentVariableA("HTTPS_PROXY", proxybuf, sizeof proxybuf);
        if (nproxybuf) {
            RequestUrl u;
            int rc = request_url_parse(&u, proxybuf, nproxybuf);
            if (rc) {
                rc = MultiByteToWideChar(CP_UTF8, 0, proxybuf, nproxybuf, proxy, sizeof proxy / sizeof proxy[0]);
                assert(rc > 0);
                proxy[rc] = 0;
            } else {
                char tmp[128];
                snprintf(tmp, sizeof tmp, "%.*s:%.*s", (int)u.nhost, u.host, (int)u.nport, u.port);
                // printf("setting proxy=%s\n", tmp);
                rc = MultiByteToWideChar(CP_UTF8, 0, tmp, -1, proxy, sizeof proxy / sizeof proxy[0]);
                proxy[rc] = 0;
            }
        } else
            proxy[0] = 0;
        // printf("proxy=%ls\n", proxy);

        request_session = WinHttpOpen(
            L"winhttp",
            proxy[0] ? WINHTTP_ACCESS_TYPE_NAMED_PROXY : WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            proxy[0] ? proxy : WINHTTP_NO_PROXY_NAME,
            proxy[0] ? L"localhost:127.0.0.1" : WINHTTP_NO_PROXY_BYPASS,
            WINHTTP_FLAG_ASYNC);
        if (!request_session) {
            printf("Error creating winhttp session: %d\n", GetLastError());
            abort();
        }

        assert(request_session);

        if (WinHttpSetStatusCallback(request_session, request_callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, 0) == WINHTTP_INVALID_STATUS_CALLBACK) {
            printf("Error installing http request callback: %d\n", GetLastError());
            abort();
        }

        if (!WinHttpSetTimeouts(request_session, 8 * 1000, 10 * 1000, 15 * 60 * 1000, 60 * 60 * 1000)) {
            printf("Error setting winhttp timeouts: %d\n", GetLastError());
            abort();
        }

#ifndef WINHTTP_OPTION_TCP_KEEPALIVE
#define WINHTTP_OPTION_TCP_KEEPALIVE ((unsigned)152)
#endif
        struct tcp_keepalive {
            unsigned long onoff;
            unsigned long keepalivetime;
            unsigned long keepaliveinterval;
        } alive;
        alive.onoff = 1;
        alive.keepalivetime = 15*60*1000;
        alive.keepaliveinterval = 60*1000;
        /* set if OS supports it. if not than continue on */
        if(!WinHttpSetOption(request_session, WINHTTP_OPTION_TCP_KEEPALIVE, &alive, sizeof alive)
           && GetLastError() != ERROR_WINHTTP_INVALID_OPTION) {
            printf("Error setting winhttp options: %d\n", GetLastError());
        }
    }
    ReleaseSRWLockExclusive(&request_lock);
}

static HINTERNET request_connect(const char* server, size_t nserver, int port) {
    request_init(); /* call outside lock to avoid recursive locks */

    // printf("request_connect: %.*s:%d\n", (int)nserver, server, port);
    AcquireSRWLockExclusive(&request_lock);
    for (Conn* conn = request_conns; conn; conn = conn->next) {
        if (conn->nserver == (int)nserver && conn->port == port && !memcmp(server, conn->server, nserver)) {
            // printf("Found existing server: %.*s\n", (int)nserver, server);
            ReleaseSRWLockExclusive(&request_lock);
            return conn->handle;
        }
    }
    ReleaseSRWLockExclusive(&request_lock);

    wchar_t buf[128];
    int rc = MultiByteToWideChar(CP_UTF8, 0, server, nserver, buf, sizeof buf / sizeof buf[0]);
    assert(rc > 0);
    buf[rc] = 0;
    HINTERNET hConnect = WinHttpConnect(request_session, buf, port, 0);
    if (!hConnect) {

        printf("Error connecting to %s:%d: %d\n", server, port, GetLastError());
        return 0;
    }

    // printf("connected\n");
    AcquireSRWLockExclusive(&request_lock);
    Conn* conn = calloc(1, sizeof *conn);
    snprintf(conn->server, sizeof conn->server, "%s", server);
    conn->nserver = (int)nserver;
    conn->port = port;
    conn->handle = hConnect;
    conn->next = request_conns;
    request_conns = conn->next;
    ReleaseSRWLockExclusive(&request_lock);
    return hConnect;
}

REQUESTS_API Request* request_new(const char* url) {
    Request* req = calloc(1, sizeof *req);
    req->url = strdup(url);
    InitializeConditionVariable(&req->cond);
    return req;
}

REQUESTS_API int request_status(Request* req) {
    if (req->error)
        return -1;
    DWORD code = 0;
    DWORD size = sizeof code;
    WinHttpQueryHeaders(req->handle,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &code, &size, WINHTTP_NO_HEADER_INDEX);
    return (int)code;
}

REQUESTS_API char* request_reason(Request* req) {
    if (req->error)
        return "request_run failed";
    if (!req->reason) {
        wchar_t reason[1024];
        DWORD nr = (DWORD)sizeof reason;
        // printf("getting reason\n");
        if (!WinHttpQueryHeaders(req->handle,
                WINHTTP_QUERY_STATUS_TEXT,
                WINHTTP_HEADER_NAME_BY_INDEX,
                reason, &nr, WINHTTP_NO_HEADER_INDEX)) {
            // printf("get reason failed: %d\n", GetLastError());
            nr = 0;
        }
        // printf("got reason nr=%u\n", nr);

        char* m = malloc(nr + 1);
        for (size_t i = 0; i < nr; i++)
            m[i] = (char)reason[i];
        m[nr] = 0;
        req->reason = m;
    }
    return req->reason;
}

REQUESTS_API void request_post(
    Request *req,
    const char *content_type,
    const char *content_encoding,
    const char *data,
    size_t ndata,
    int copy)
{
    free(req->verb);
    req->verb = strdup("POST");
    if(req->owns_input) free(req->input);
    if(copy) {
        req->input = malloc(ndata + 1); /* in case ndata is zero */
        memcpy(req->input, data, ndata);
        req->owns_input = 1;
    } else {
        req->input = (char*)data;
        req->owns_input = 0;
    }
    req->ninput = ndata;

    if(content_type) request_addheader(req, "Content-Type: ", content_type);
    if(content_encoding) request_addheader(req, "Content-Encoding: ", content_encoding);

    char buf[64];
    snprintf(buf, sizeof buf, "%zu", ndata);
    request_addheader(req, "Content-Length: ", buf);
}


static int request_run(Request* req) {
    RequestUrl u;
    if (request_url_parse(&u, req->url, 0))
        return -1;
    int port;
    if (u.nport)
        port = 0;
    else if (!strncmp(u.scheme, "http", u.nscheme))
        port = 80;
    else if (!strncmp(u.scheme, "https", u.nscheme))
        port = 443;
    for (size_t i = 0; i < u.nport; i++) {
        port *= 10;
        port += u.port[i] - '0';
    }
    // printf("port=%d from '%.*s'\n", port, (int)u.nport, u.port);
    HINTERNET conn = request_connect(u.host, u.nhost, port);
    if (!conn)
        return -2;
    const char* verb = req->verb ? req->verb : "GET";
    wchar_t vbuf[16];
    size_t i = 0;
    for (const char* p = verb; *p; ++p, ++i)
        vbuf[i] = toupper(*p);
    vbuf[i] = 0;
    // printf("cverb='%s'\n", verb);
    if (request_url_parse(&u, req->url, 0))
        return -1;
    int n = MultiByteToWideChar(CP_UTF8, 0, u.path, req->url - u.path, 0, 0);
    assert(n >= 0);
    wchar_t* pbuf = malloc((n + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, u.path, req->url - u.path, pbuf, n);
    pbuf[n] = 0;
    // printf("verb='%ls'\n", vbuf);
    // printf("pbuf=%ls\n", pbuf);
    // printf("scheme=%.*s\n", (int)u.nscheme, u.scheme);
    DWORD flag = (u.nscheme == 5 && !memcmp(u.scheme, "https", 5)) ? WINHTTP_FLAG_SECURE : 0;
    // printf("flag=%u\n", flag);
    req->handle = WinHttpOpenRequest(
        conn,
        vbuf,
        n ? pbuf : 0,
        L"HTTP/1.1",
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flag);
    free(pbuf);
    if (!req->handle)
        return -3;

    for (RequestHeader* h = req->headers; h; h = h->next) {
        // printf("adding header='%ls'\n", h->header);
        if (!WinHttpAddRequestHeaders(
                req->handle,
                h->header,
                -1,
                WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD)) {
            printf("adding headers failed: %d\n", GetLastError());
        }
    }

    // printf("request opened\n");
    if (!WinHttpSendRequest(req->handle,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            req->ninput ? req->input : WINHTTP_NO_REQUEST_DATA, req->ninput,
            0, (DWORD_PTR)req)) {
        printf("request failed: %d\n", GetLastError());
        return -4;
    }
    // printf("request sent\n");
    int done = 0;
    do {
        // printf("waiting\n");
        AcquireSRWLockExclusive(&req->lock);
        SleepConditionVariableSRW(&req->cond, &req->lock, INFINITE, 0);
        done = req->complete;
        ReleaseSRWLockExclusive(&req->lock);
    } while(!done);
    // printf("request complete\n");
    return req->error ? -5 : 0;
}

REQUESTS_API void request_addheader(Request* req, const char* part1, const char* part2) {
    char buf[8192];
    snprintf(buf, sizeof buf, "%s%s", part1, part2);
    // printf("adding header: %s\n", buf);
    int n = MultiByteToWideChar(CP_UTF8, 0, buf, -1, 0, 0);
    assert(n >= 0);
    wchar_t* pbuf = malloc(n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, pbuf, n);
    // printf("adding '%.*ls'\n", n, pbuf);

    RequestHeader* h = calloc(1, sizeof *h);
    h->header = pbuf;
    h->next = req->headers;
    req->headers = h;
}

REQUESTS_API char* request_header(Request *req, const char *header) {
    /* check cache */
    size_t n = strlen(header);
    for(RequestOutputHeader *h = req->output_headers;h;h=h->next) {
        int match = 1;
        for(size_t i=0;i<n && match;i++) {
            if(tolower(header[i]) != tolower(h->key[i]))
                match = 0;
        }
        if(match) return h->value;
    }

    wchar_t buf[65536], name[1024];
    DWORD idx = 0, nbuf = sizeof buf - sizeof buf[0];
    int nname = MultiByteToWideChar(CP_UTF8, 0, header, -1, name, sizeof name / sizeof name[0] - 1);
    if(nname <= 0) return 0;
    name[nname] = 0;

#if 0
    /* dump all headers */
    wchar_t nn[100000];
    DWORD nnsz = sizeof nn - 2;
    WinHttpQueryHeaders(req->handle, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nn, &nnsz, WINHTTP_NO_HEADER_INDEX);
    nn[nnsz] = 0;
    printf("nn=%ls\n", nn);
#endif

    if(!WinHttpQueryHeaders(req->handle, WINHTTP_QUERY_CUSTOM, name, buf, &nbuf, WINHTTP_NO_HEADER_INDEX)) {
        if(GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND) return 0;
        //printf("query failed: %d\n", GetLastError());
        return 0;
    }

    char *value;
    int required = WideCharToMultiByte(CP_UTF8, 0, buf, nbuf / sizeof buf[0], 0, 0, 0, 0);
    if(required < 0) return 0;
    value = malloc(required + 1);
    WideCharToMultiByte(CP_UTF8, 0, buf, nbuf / sizeof buf[0], value, required, 0, 0);
    value[required] = 0;

    /* add to cache */
    RequestOutputHeader *h = malloc(sizeof *h);
    h->key = strdup(header);
    h->value = value;
    h->next = req->output_headers;
    req->output_headers = h;

    return value;
}

REQUESTS_API void request_destroy(Request* req) {
    if(req->owns_input) free(req->input);
    free(req->url);
    free(req->verb);
    free(req->reason);
    if (req->handle)
        WinHttpCloseHandle(req->handle);
    free(req->input);
    free(req->output);
    for (RequestHeader* h = req->headers; h;) {
        free(h->header);
        RequestHeader* tmp = h;
        h = h->next;
        free(tmp);
    }
    for (RequestOutputHeader* h = req->output_headers; h;) {
        free(h->key);
        free(h->value);
        RequestOutputHeader* tmp = h;
        h = h->next;
        free(tmp);
    }
    free(req);
}

REQUESTS_API void request_data(Request* req, char** data, size_t* ndata) {
    *data = req->output;
    *ndata = req->noutput;
}

REQUESTS_API char* request_error(Request *req) {
  return req->error;
}

#elif defined(__linux__)

#include <curl/curl.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct RequestHeader {
    char *key;
    char *value;
    struct RequestHeader *next;
} RequestHeader;

typedef struct Request {
    CURL* curl;
    struct curl_slist *headers;
    pthread_mutex_t mtx;
    pthread_cond_t cnd;
    struct Request *next;
    char *reason;
    char *error;
    char *output;
    size_t noutput;
    size_t output_capacity;
    RequestHeader *output_headers;
    unsigned complete : 1;
} Request;

static pthread_mutex_t requests_incoming_mtx = PTHREAD_MUTEX_INITIALIZER;
static Request *requests_incoming;
static CURLM *requests_multi;

static void* requests_threadrun(void *ctx) {
    requests_multi = curl_multi_init();
    if(!requests_multi) {
        printf("creating multi handle failed\n");
        abort();
    }
    long max_concurrent = 1024;
    curl_multi_setopt(requests_multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long)max_concurrent);
    curl_multi_setopt(requests_multi, CURLMOPT_MAX_HOST_CONNECTIONS, (long)4);
    curl_multi_setopt(requests_multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

    for(;;) {
        pthread_mutex_lock(&requests_incoming_mtx);
        for(Request *req = requests_incoming;req;req=req->next) {
            //printf("adding new handle\n");
            curl_multi_add_handle(requests_multi, req->curl);
        }
        requests_incoming = 0;
        pthread_mutex_unlock(&requests_incoming_mtx);

        int running_count;
        CURLMcode mc = curl_multi_perform(requests_multi, &running_count);
        assert(!mc);
        CURLMsg *msg;
        do {
            int msgs;
            msg = curl_multi_info_read(requests_multi, &msgs);
            if(msg && msg->msg == CURLMSG_DONE) {
                //printf("request complete\n");
                CURL *curl = msg->easy_handle;
                curl_multi_remove_handle(requests_multi, curl);
                Request *req;
                curl_easy_getinfo(curl, CURLINFO_PRIVATE, &req);
                if(!req->noutput && msg->data.result != CURLE_OK)
                    req->error = strdup(curl_easy_strerror(msg->data.result));
                req->complete = 1;
                pthread_cond_signal(&req->cnd);
            }
        } while(msg);

        int ready;
        curl_multi_poll(requests_multi, 0, 0, INT_MAX, &ready);
    }
    curl_multi_cleanup(requests_multi);
}

static void requests_init1() {
    curl_global_init(CURL_GLOBAL_ALL);
    pthread_t t;
    if(pthread_create(&t, 0, requests_threadrun, 0)) {
        printf("requests multi thread create failed\n");
        abort();
    }
}

static void requests_init() {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, requests_init1);
}

REQUESTS_API void request_addheader(Request *req, const char *a, const char *b) {
    size_t n = snprintf(0, 0, "%s%s", a, b);
    char *c = (char*)malloc(n + 1);
    snprintf(c, n + 1, "%s%s", a, b);
    req->headers = curl_slist_append(req->headers, c);
    free(c);
}

static size_t request_writecb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    Request *req = (Request*)userdata;
    size *= nmemb;
    size_t required = req->noutput + size;
    if(required > req->output_capacity) {
        size_t cap = req->output_capacity * 2;
        if(cap < 1024*1024) cap = 1024*1024;
        if(cap < required) cap = required;
        req->output = (char*)realloc(req->output, cap);
        req->output_capacity = cap;
    }

    memcpy(req->output + req->noutput, ptr, size);
    req->noutput += size;
    return size;
}

static void request_destroyheaders(Request *req) {
    for(RequestHeader *h = req->output_headers;h;) {
        free(h->key);
        free(h->value);
        RequestHeader *next = h->next;
        free(h);
        h = next;
    }
    req->output_headers = 0;
}

static size_t request_headercb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t result = size * nmemb;
    //printf("header: '%.*s'\n", (int)nmemb, ptr);
    Request *req = (Request*)userdata;
    int httppos = request_strnstr(ptr, nmemb, "HTTP/");
    if(httppos == 0) {
        /* curl includes headers from intermediate queries.
           keep the ones from the final query only.
           each new HTTP status indicates a new response */
        request_destroyheaders(req);

        /* status line */
        int space = request_strnstr(ptr, nmemb, " ");
        if(space > 0) {
            ptr += space + 1;
            nmemb -= space + 1;
            space = request_strnstr(ptr, nmemb, " ");
            if(space > 0) {
                ptr += space + 1;
                nmemb -= space + 1;
                int end = request_strnstr(ptr, nmemb, "\r\n");
                if(end >= 0) {
                    free(req->reason);
                    req->reason = (char*)malloc(end + 1);
                    memcpy(req->reason, ptr, end);
                    req->reason[end] = 0;
                }
            }
        }
    } else {
        int colon = request_strnstr(ptr, nmemb, ":");
        if(colon > 1) {
            int value = colon + 1;
            while(value < (int)nmemb && ptr[value] == ' ') ++value;

            char *keystart = ptr;
            size_t nkey = colon;

            ptr += value;
            nmemb -= value;

            int end = request_strnstr(ptr, nmemb, "\r\n");
            if(end >= 0) {
                RequestHeader *h = (RequestHeader*)malloc(sizeof *h);
                h->key = (char*)malloc(nkey + 1);
                memcpy(h->key, keystart, nkey);
                h->key[nkey] = 0;
                h->value = (char*)malloc(end + 1);
                memcpy(h->value, ptr, end);
                h->value[end] = 0;
                h->next = req->output_headers;
                req->output_headers = h;
            }
        }
    }
    /* todo parse status lines and headers */

    return result;
}

REQUESTS_API Request *request_new(const char *url) {
    requests_init();
    Request *req = (Request*)calloc(1, sizeof *req);
    assert(req);
    req->curl = curl_easy_init();
    curl_easy_setopt(req->curl, CURLOPT_URL, url);
    curl_easy_setopt(req->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(req->curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(req->curl, CURLOPT_WRITEFUNCTION, request_writecb);
    curl_easy_setopt(req->curl, CURLOPT_WRITEDATA, req);
    curl_easy_setopt(req->curl, CURLOPT_HEADERFUNCTION, request_headercb);
    curl_easy_setopt(req->curl, CURLOPT_HEADERDATA, req);
    curl_easy_setopt(req->curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(req->curl, CURLOPT_PRIVATE, req);
    curl_easy_setopt(req->curl, CURLOPT_TCP_KEEPALIVE, 1L);  /* enable TCP keep-alive for this transfer */
    curl_easy_setopt(req->curl, CURLOPT_TCP_KEEPIDLE, 15L*60); /* keep-alive idle time in seconds*/
    curl_easy_setopt(req->curl, CURLOPT_TCP_KEEPINTVL, 60L);   /* interval time between keep-alive probes in seconds.     once an OS specific number of these have failed the OS will close the connection */
    curl_easy_setopt(req->curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(req->curl, CURLOPT_LOW_SPEED_LIMIT, 0L);
    curl_easy_setopt(req->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    /* disable Expect: 100-continue and 1 sec delay which curl library introduces */
    req->headers = curl_slist_append(req->headers, "Expect:");
    //req->headers = curl_slist_append(req->headers, "Transfer-Encoding:"); /* disable chunked encoding */
    pthread_mutex_init(&req->mtx, 0);
    pthread_cond_init(&req->cnd, 0);
    return req;
}

REQUESTS_API void request_destroy(Request *req) {
    free(req->error);
    free(req->output);
    free(req->reason);
    if(req->headers) curl_slist_free_all(req->headers);
    if(req->curl) curl_easy_cleanup(req->curl);
    pthread_mutex_destroy(&req->mtx);
    pthread_cond_destroy(&req->cnd);
    request_destroyheaders(req);
    free(req);
}

REQUESTS_API int request_status(Request *req) {
    long code = 0;
    curl_easy_getinfo(req->curl, CURLINFO_RESPONSE_CODE, &code);
    return (int)code;
}

REQUESTS_API void request_data(Request *req, char **data, size_t *size) {
    *data = req->output;
    *size = req->noutput;
}

REQUESTS_API char* request_error(Request *req) {
    return req->error;
}

REQUESTS_API char* request_reason(Request *req) {
    return req->reason;
}
REQUESTS_API char* request_header(struct Request *req, const char *header) {
    size_t n = strlen(header);
    for(RequestHeader *h = req->output_headers;h;h=h->next) {
        int match = 1;
        for(size_t i=0;i<n && match;i++) {
            if(tolower(header[i]) != tolower(h->key[i]))
                match = 0;
        }
        if(match) return h->value;
    }
    return 0;
}

REQUESTS_API int request_run(Request *req) {
    pthread_mutex_lock(&requests_incoming_mtx);
    req->next = requests_incoming;
    requests_incoming = req;
    pthread_mutex_unlock(&requests_incoming_mtx);
    curl_multi_wakeup(requests_multi);

    int done = 0;
    do {
        pthread_mutex_lock(&req->mtx);
        pthread_cond_wait(&req->cnd, &req->mtx);
        done = req->complete;
        pthread_mutex_unlock(&req->mtx);
    } while(!done);

    return req->error != 0;
}

static void request_print_headers(Request *req) {
    for(RequestHeader *h = req->output_headers;h;h=h->next) {
        printf("%s: %s\n", h->key, h->value);
    }
}

REQUESTS_API void request_post(
    Request *req,
    const char *content_type,
    const char *content_encoding,
    const char *data,
    size_t ndata,
    int copy)
{
    /* length then data or curl runs strlen() */
    curl_easy_setopt(req->curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)ndata);
    curl_easy_setopt(req->curl, copy ? CURLOPT_COPYPOSTFIELDS : CURLOPT_POSTFIELDS, data);

    if(content_type) request_addheader(req, "Content-Type: ", content_type);
    if(content_encoding) request_addheader(req, "Content-Encoding: ", content_encoding);

    char buf[64];
    snprintf(buf, sizeof buf, "%zu", ndata);
    request_addheader(req, "Content-Length: ", buf);
}



//#elif defined(__APPLE__)

#endif


#endif

#ifdef REQUESTS_EXAMPLE
#include <stdio.h>
int main(int argc, char** argv) {
    Request* req = request_new("google.com");
    if (request_run(req)) {
        printf("failure: %s\n", request_error(req));
    } else {
        char* data;
        size_t ndata;
        request_data(req, &data, &ndata);
        //printf("%.*s\n", (int)ndata, data);
    }
    printf("status=%d reason=%s\n", request_status(req), request_reason(req));
    //request_print_headers(req);
    char *len = request_header(req, "Content-TYPE");
    printf("content-type=%s\n", len);
    request_destroy(req);
}
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
#endif