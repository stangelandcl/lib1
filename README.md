## lib1
Public domain single file C libraries in the vein of https://github.com/nothings/stb.

Check the bottom of each file for an example and license (public domain)

- [base64.h](base64.h) - base64 encode/decode. decodes base64url too
- [dataframe.h](dataframe.h) - dataframe library
- [hash.h](hash.h) - simple grow-only open addressing hash table
- [hash2.h](hash2.h) - simple non-templated grow-only open addressing hash table
- [json.h](json.h) - tiny, iterative zero memory overhead JSON parser.
As fast as jsmn. Faster than cJSON. Uses much less memory than either.
- [json2.h](json2.h) - faster, less precise parser running at 60-70% of simdjson.
- [jwt.h](jwt.h) - self-contained JWT (JSON Web Token) parser and validator. Web downloads
  must be made externally. This lib only contains logic for hashing, RSA validation, and parsing
- [md5.h](md5.h) - small md5 hash function
- [noise.h](noise.h) - modified noise encryption protocol
- [rsa.h](rsa.h) - RSA sign and verify
- [pg.h](pg.h) - minimal postgres driver handling unencrypted text protocol queries and md5
  password authentication only
- [sha.h](sha.h) - SHA hashes
- [socks5.h](socks5.h) - small SOCKS5 client for establishing a TCP connection through a SOCKS5
  proxy
- [url.h](url.h) - parse URL

### json
Example from the bottom of [json.h](json.h). Search JSON\_EXAMPLE.
```c
#define JSON_STATIC
#include "json.h"
#include <stdio.h>
#include <string.h>
int main() {
	char json[] =
		"{\n"
		"\"a_key\":\"a value\",\n"
		"\"values\":\n"
		"  [\n"
		"      {\"stat\": -123.45e7,\n"
		"       \"flag\":false,\n"
		"       \"status\":\"on\\t-going\",\n"
		"       \"count\":49991 },\n"
		"      {\"stat\": null,\n"
		"       \"flag\":true,\n"
		"       \"status\":\"\\uD83D\\ude03 done\",\n"
		"       \"count\": -10 }\n"
		"  ]\n"
		"}";
	Json p;
	JsonTok k, v;
	struct Val { double stat; int flag; char *status; int count; };
	struct Val vals[100], val;
	int nvals = 0, i;

	printf("parsing:\n%s\n", json);

	json_init(&p, json, sizeof json - 1);
	while(json_next(&p, &v)) {
		/* always handle all possible object/array values.
		   primitives are automatically skipped but arrays and objects
		   must be manually skipped. The simplest rule is
		   always call json_skip() as part of every
		   if statement chain. It is safe to call whether
		   token is actually a composite type or not */
		if(v.type != JSON_OBJECT) json_skip(&p);
		else while(json_object(&p, &k, &v)) {
			if(!JSON_EQ(&k, "values") || v.type != JSON_ARRAY)
				json_skip(&p);
			else while(json_array(&p, &v)) {
				if(v.type != JSON_OBJECT) {
					json_skip(&p);
					continue;
				}

				memset(&val, 0, sizeof val);
				while(json_object(&p, &k, &v)) {
					if(JSON_EQ(&k, "stat"))
						val.stat = json_float(&v);
					else if(JSON_EQ(&k, "flag"))
						val.flag = json_bool(&v);
					else if(JSON_EQ(&k, "status"))
						val.status = json_strdup(&v);
					else if(JSON_EQ(&k, "count"))
						val.count = (int)json_int(&v);
					else json_skip(&p); /* always call this
					function in if chains */
				}
				vals[nvals++] = val;
			}
		}
	}

	if(json_error(&p)) {
		printf("Error parsing: %s\n", json_error(&p));
		return -1;
	}

	printf("parsed %d values\n", nvals);
	for(i=0;i<nvals;i++) {
		struct Val *v = &vals[i];
		printf("i=%d stat=%f flag=%s status=%s count=%d\n",
			i, v->stat, v->flag ? "true" : "false", v->status, v->count);
	}

	return 0;
}

/* Output:
parsed 2 values
i=0 stat=-1234500000.000000 flag=false status=on        going count=49991
i=1 stat=0.000000 flag=true status=😃 done count=-10
*/
```
