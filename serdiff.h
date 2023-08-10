#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void serdiff_save_i8(void* data, size_t count) {
        size_t i;
        int8_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (int8_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_i8(void* data, size_t count) {
        size_t i;
        int8_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (int8_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_u8(void* data, size_t count) {
        size_t i;
        uint8_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (uint8_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_u8(void* data, size_t count) {
        size_t i;
        uint8_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (uint8_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_i16(void* data, size_t count) {
        size_t i;
        int16_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (int16_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_i16(void* data, size_t count) {
        size_t i;
        int16_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (int16_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_u16(void* data, size_t count) {
        size_t i;
        uint16_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (uint16_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_u16(void* data, size_t count) {
        size_t i;
        uint16_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (uint16_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_i32(void* data, size_t count) {
        size_t i;
        int32_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (int32_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_i32(void* data, size_t count) {
        size_t i;
        int32_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (int32_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_u32(void* data, size_t count) {
        size_t i;
        uint32_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (uint32_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_u32(void* data, size_t count) {
        size_t i;
        uint32_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (uint32_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_i64(void* data, size_t count) {
        size_t i;
        int64_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (int64_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_i64(void* data, size_t count) {
        size_t i;
        int64_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (int64_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
}
static void serdiff_save_u64(void* data, size_t count) {
        size_t i;
        uint64_t diff,last, cur;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;

        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(diff));
            diff = (uint64_t)(cur - last);
            memcpy(q, &diff, sizeof(diff));
            q += sizeof(diff);
            last = cur;
        }
}
static void serdiff_load_u64(void* data, size_t count) {
        size_t i;
        uint64_t y, cur,last;
        uint8_t *p, *q;

        if(count < 2)
            return;

        p = (uint8_t*)data;
        memcpy(&last, p, sizeof(last));
        q = p + sizeof(last);
        --count;
        for(i=0;i<count;i++)
        {
            memcpy(&cur, q, sizeof(y));
            y = (uint64_t)(cur + last);
            memcpy(q, &y, sizeof(y));
            q += sizeof(y);
            last = y;
        }
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
