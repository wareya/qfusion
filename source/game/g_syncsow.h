typedef struct asCurl_Request_s {
	asCurl_Request_s *next;
	
	wswcurl_req *req;
	asIScriptFunction *doneCallback;
	void *custom;
	bool isJSON;
} asCurl_Request_t;

typedef struct asCURL_s {
	asCurl_Request_s *requests;
	char *url;
	unsigned int refCount;
} asCurl_t;

extern void Curl_AddRef( asCurl_t *self );
extern void Curl_Release( asCurl_t *self );
extern void Curl_ReleaseAll( asCurl_t *self );

extern void Curl_Shutdown();

void RegisterCURLAddon( asIScriptEngine *engine );