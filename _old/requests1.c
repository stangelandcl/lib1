#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static char* request_geterror(DWORD err)
{
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

static int request_url_strnstr(const char* haystack, size_t nhaystack, const char* needle)
{
    size_t n = strlen(needle);
    for (int i = 0; i < (int)nhaystack && (size_t)i <= nhaystack - n; i++) {
        if (!strncmp(haystack + i, needle, n))
            return i;
    }
    return -1;
}

/* return number of parameters passed on success, < 0 on failure */
static int request_url_parse(RequestUrl* u, const char* url, size_t nurl)
{
    memset(u, 0, sizeof *u);
    if (!nurl) {
        if (url)
            nurl = strlen(url);
        else
            return -1;
    }

    /* scheme */
    int pos = request_url_strnstr(url, nurl, "://");
    if (!pos)
        return -2;
    if (pos > 0) {
        u->scheme = url;
        u->nscheme = (unsigned)pos;
        url += pos + 3;
        nurl -= pos + 3;
    }

    /* user and optional password */
    pos = request_url_strnstr(url, nurl, "@");
    if (!pos)
        return -3;
    if (pos > 0) {
        u->user = url;
        int pos2 = request_url_strnstr(url, (unsigned)pos, ":");
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
    pos = request_url_strnstr(url, nurl, "/");
    if (pos < 0)
        pos = nurl;
    if (pos >= 0) {
        int pos2 = request_url_strnstr(url, (unsigned)pos, ":");
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

static void request_callback(HINTERNET session, DWORD_PTR ctx, DWORD status, void* info, DWORD ninfo)
{
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

static void request_init()
{
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
    }
    ReleaseSRWLockExclusive(&request_lock);
}

static HINTERNET request_connect(const char* server, size_t nserver, int port)
{
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

static Request* request_new(const char* url)
{
    Request* req = calloc(1, sizeof *req);
    req->url = strdup(url);
    InitializeConditionVariable(&req->cond);
    return req;
}

static int request_status(Request* req)
{
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

static char* request_reason(Request* req)
{
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

static int request_run(Request* req)
{
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
            WINHTTP_NO_REQUEST_DATA, 0,
            0, (DWORD_PTR)req)) {
        printf("request failed: %d\n", GetLastError());
        return -4;
    }
    // printf("request sent\n");
    while (!req->complete) {
        // printf("waiting\n");
        AcquireSRWLockExclusive(&req->lock);
        SleepConditionVariableSRW(&req->cond, &req->lock, INFINITE, 0);
        ReleaseSRWLockExclusive(&req->lock);
    }
    // printf("request complete\n");
    return req->error ? -5 : 0;
}

static void request_addheader(Request* req, const char* part1, const char* part2)
{
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

static void request_data(Request* req, char** data, size_t* ndata)
{
    *data = req->output;
    *ndata = req->noutput;
}

static void request_destroy(Request* req)
{
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
    free(req);
}

int main(int argc, char** argv)
{
    Request* req = request_new("google.com");
    if (request_run(req)) {
        printf("failure: %s\n", req->error);
    } else {
        char* data;
        size_t ndata;
        request_data(req, &data, &ndata);
        printf("%.*s\n", (int)ndata, data);
    }
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