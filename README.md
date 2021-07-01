## lib1
Public domain single file C libraries in the vein of https://github.com/nothings/stb.

- [json.h](json.h) - tiny iterative zero memory overhead JSON parser. 
Faster than jsmn and cJSON using much less memory.
- [md5.h](md5.h) - small md5 hash function
- [postgres.h](postgres.h) - minimal postgres driver handling text protocol queries and md5
  password authentication only


### json
Example from the bottom of [json.h](json.h). Search JSON\_EXAMPLE.
```c
#define JSON_STATIC
#include "json.h"
#include <stdio.h>
int main() {
	const char json[] = 
		"{\n"
		"\"a_key\":\"a value\",\n"
		"\"values\":\n"
		"  [\n"
		"      {\"stat\": -123.45e7,\n"
		"       \"flag\":false,\n"
		"       \"status\":\"on-going\",\n"
		"       \"count\":49991 },\n"
		"      {\"stat\": null,\n"
		"       \"flag\":true,\n"
		"       \"status\":\"done\",\n"
		"       \"count\": -10 }\n"
		"  ],\n"
		"}";
	Json p;
	JsonTok t, k, v;
	struct Val { double stat; int flag; char *status; int count; };
	struct Val vals[100];
	int nvals = 0, i;

	printf("parsing:\n%s\n", json);

	json_init(&p, json, sizeof json - 1);
	while(json_next(&p, &t)) {

		/* always handle all possible object/array values. 
		   primitives are automatically skipped but arrays and objects
		   must be manually skipped. The simplest rule is 
		   always call json_skip_composite() as part of every
		   if statement chain. It is safe to call whether 
		   token is actually a composite type or not */
		if(t.type != JSON_OBJECT) json_skip_composite(&p, &t);
		else while(json_object(&p, &k, &v)) {
			if(!JSON_STR(&k, "values") || v.type != JSON_ARRAY)
				json_skip_composite(&p, &v);
			else while(json_array(&p, &t)) {
				if(t.type != JSON_OBJECT) json_skip_composite(&p, &t);
				else {
					struct Val val = {0};
					while(json_object(&p, &k, &v)) {
						if(JSON_STR(&k, "stat")) 
							val.stat = json_float(&v);									
						else if(JSON_STR(&k, "flag")) 
							val.flag = json_bool(&v);									
						else if(JSON_STR(&k, "status")) 
							val.status = json_strdup(&v);
						else if(JSON_STR(&k, "count")) 
							val.count = (int)json_int(&v);
						else json_skip_composite(&p, &v); /* always call this
						function in if chains */		
					}
					vals[nvals++] = val;
				}
			}
		}
	}

	printf("parsed %d values\n", nvals);
	for(i=0;i<nvals;i++) {
		struct Val *v = &vals[i];
		printf("i=%d stat=%f flag=%s status=%s count=%d\n", 
			i, v->stat, v->flag ? "true" : "false", v->status, v->count);
	}

	return 0;
}

```
