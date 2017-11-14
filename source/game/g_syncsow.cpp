#include "g_local.h"
#include "../qcommon/cjson.h"
#include "../qalgo/base64.h"
#include "g_as_local.h"

#include "g_syncsow.h"

#define SS_DEBUG 0

//----------------------------------
// JSON
//----------------------------------

typedef struct
{
	cJSON *json;
	bool isRoot;
} asJSON_t;

void JSON_Constructor( asJSON_t* self )
{
	self->json = cJSON_CreateNull();
	//self->json = NULL;
	self->isRoot = true;
	if ( SS_DEBUG ) Com_Printf("CREATE JSON\n");
}

void JSON_StringConstructor( asJSON_t* self, asstring_t *value )
{
	/* TODO FIX ME */
	self->json = cJSON_CreateString(strdup(value->buffer));
	self->isRoot = false;
	if ( SS_DEBUG ) Com_Printf("CREATE JSON STRING\n");
}

void JSON_Destructor( asJSON_t* self )
{
	if ( self->isRoot )
	{
		cJSON_Delete(self->json);
		if ( SS_DEBUG ) Com_Printf("DELETE JSON ROOT\n");
		return;
	}
	if ( SS_DEBUG ) Com_Printf("DELETE JSON\n");
}

void JSON_Parse(asJSON_t* self, asstring_t *value)
{
	self->json = cJSON_Parse(value->buffer);
}

const asstring_t* JSON_Print(asJSON_t* self)
{
	if ( !self->json )
		return angelExport->asStringFactoryBuffer( "undefined", strlen("undefined") );
	char* value = cJSON_Print(self->json);
	return angelExport->asStringFactoryBuffer( value, strlen(value) );
}

const asstring_t* JSON_PrintUnformatted(asJSON_t* self)
{
	char* value = cJSON_PrintUnformatted(self->json);
	return angelExport->asStringFactoryBuffer( value, strlen(value) );
}

asJSON_t* JSON_GetItemString(/*asJSON_t* ret, */asJSON_t* self, asstring_t *item)
{
	asJSON_t* ret = (asJSON_t*) malloc(sizeof(*ret));
	JSON_Constructor(ret);
	ret->json = cJSON_GetObjectItem(self->json, item->buffer);
	ret->isRoot = false;
	if ( !ret->json )
	{
		self->json->type = cJSON_Object;
		cJSON_AddNullToObject(self->json, item->buffer);
		free(ret);
		ret = JSON_GetItemString(/*ret, */self, item);
	}
	return ret;
}

asJSON_t*  JSON_GetItemUint(/*asJSON_t* ret, */asJSON_t* self, unsigned int item)
{
	if ( SS_DEBUG ) Com_Printf("JSON JSON_GetItemUint\n");
	asJSON_t* ret = (asJSON_t*) malloc(sizeof(*ret));
	JSON_Constructor(ret);
	ret->json = cJSON_GetArrayItem(self->json, item);
	ret->isRoot = false;
	if ( !ret->json )
	{
		self->json->type = cJSON_Array;
		cJSON_AddItemToArray(self->json, cJSON_CreateNull());
		free(ret);
		ret = JSON_GetItemUint(/*ret, */self, item);
	}
	return ret;
}

unsigned int JSON_GetArrayLength(asJSON_t* self)
{
	if ( SS_DEBUG ) Com_Printf("JSON GetArrayLength\n");
	return cJSON_GetArraySize(self->json);
}

asJSON_t* JSON_AssignItemString(asJSON_t* self, asstring_t *item)
{
	self->json->valuestring = strdup(item->buffer);
	self->json->type = cJSON_String;
	//self->json = cJSON_CreateString( item->buffer );
	return self;
}

asJSON_t* JSON_AssignItemInt(asJSON_t* self, int item)
{
	self->json->valueint = item;
	self->json->valuedouble = item;
	self->json->type = cJSON_Number;
	//self->json = cJSON_CreateString( item->buffer );
	return self;
}

asJSON_t* JSON_AssignItemFloat(asJSON_t* self, float item)
{
	self->json->valueint = item;
	self->json->valuedouble = item;
	self->json->type = cJSON_Number;
	//self->json = cJSON_CreateString( item->buffer );
	return self;
}

asJSON_t* JSON_AssignJSONItem(asJSON_t* self, asJSON_t *item)
{
	self->json->next = item->json->next;
	self->json->prev = item->json->prev;
	self->json->child = item->json->child;
	self->json->type = item->json->type;
	self->json->valuestring = item->json->valuestring;
	self->json->valueint = item->json->valueint;
	self->json->valuedouble = item->json->valuedouble;
	self->json->string = item->json->string;

	item->isRoot = false;
	//self->json->type = cJSON_String;
	//self->json = cJSON_CreateString( item->buffer );
	return self;
}

asJSON_t* JSON_AssignItemArray(asJSON_t* self, CScriptArrayInterface *items)
{
	/* TODO FIX ME */
	int arr_size = items->GetSize();

	self->json = cJSON_CreateArray();

	for ( int i = 0; i < arr_size; i++ )
	{
		asJSON_t* item = (asJSON_t*)items->At(i);
		cJSON_AddItemToArray(self->json, item->json);

	}
	return self;
}

asJSON_t* JSON_AssignItemStringArray(asJSON_t* self, CScriptArrayInterface *items)
{
	int arr_size = items->GetSize();

	self->json->type = cJSON_Array;

	for ( int i = 0; i < arr_size; i++ )
	{
		asstring_t* item = (asstring_t*)items->At(i);
		asJSON_t* newjson = (asJSON_t*) malloc(sizeof(*newjson));
		JSON_StringConstructor(newjson, item);
		newjson->isRoot = false;
		cJSON_AddItemToArray(self->json, newjson->json);
	}
	return self;
}

const int JSON_GetType(asJSON_t* self)
{
	if ( !self->json )
		return -1;
	return self->json->type;
}

const asstring_t* JSON_GetName(asJSON_t* self)
{
	if ( !self->json )
		return angelExport->asStringFactoryBuffer( "undefined", sizeof("undefined") );
	char* value = self->json->string;
	return angelExport->asStringFactoryBuffer( value, strlen(value) );
}

const asstring_t* JSON_GetValueString(asJSON_t* self)
{
	if ( !(self->json && self->json->valuestring) )
		return angelExport->asStringFactoryBuffer( "undefined", sizeof("undefined") );
	char* value = self->json->valuestring;
	return angelExport->asStringFactoryBuffer( value, strlen(value) );
}

const int JSON_GetValueInt(asJSON_t* self)
{
	if ( !self->json )
		return 0;
	return self->json->valueint;
}

const float JSON_GetValueFloat(asJSON_t* self)
{
	if ( !self->json )
		return 0;
	return self->json->valuedouble;
}

void RegisterJsonClass( asIScriptEngine *engine )
{
	engine->RegisterObjectType( "JSON", sizeof(asJSON_t), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_C | asOBJ_APP_CLASS_ALLINTS);
	engine->RegisterEnum( "JSONType" );
	
	engine->RegisterObjectBehaviour( "JSON", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION( JSON_Constructor ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectBehaviour( "JSON", asBEHAVE_CONSTRUCT, "void f(const String& value)", asFUNCTION( JSON_StringConstructor ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectBehaviour( "JSON", asBEHAVE_DESTRUCT, "void f()", asFUNCTION( JSON_Destructor ), asCALL_CDECL_OBJFIRST );
	
	engine->RegisterObjectMethod( "JSON", "void Parse(String& value)", asFUNCTION(JSON_Parse), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const String @ Print()", asFUNCTION(JSON_Print), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const String @ PrintUnformatted()", asFUNCTION(JSON_PrintUnformatted), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const uint length()", asFUNCTION(JSON_GetArrayLength), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON getItem(const String& item)", asFUNCTION(JSON_GetItemString), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opIndex(const String& item)", asFUNCTION(JSON_GetItemString), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opIndex(const uint item)", asFUNCTION(JSON_GetItemUint), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opAssign(const String& value)", asFUNCTION(JSON_AssignItemString), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opAssign(const int value)", asFUNCTION(JSON_AssignItemInt), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opAssign(const float value)", asFUNCTION(JSON_AssignItemFloat), asCALL_CDECL_OBJFIRST );
	//engine->RegisterObjectMethod( "JSON", "JSON &opAssign(JSON &in)", asFUNCTION(JSON_AssignJSONItem), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opAssign(const array<JSON>& values)", asFUNCTION(JSON_AssignItemArray), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "JSON &opAssign(const array<String>& values)", asFUNCTION(JSON_AssignItemStringArray), asCALL_CDECL_OBJFIRST );
	
	engine->RegisterObjectMethod( "JSON", "const int get_type() const", asFUNCTION(JSON_GetType), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const String @ get_name() const", asFUNCTION(JSON_GetName), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const String @ get_valuestring() const", asFUNCTION(JSON_GetValueString), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const int get_valueint() const", asFUNCTION(JSON_GetValueInt), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "JSON", "const float get_valuefloat() const", asFUNCTION(JSON_GetValueFloat), asCALL_CDECL_OBJFIRST );
	
	engine->RegisterEnumValue( "JSONType", "JSON_FALSE", cJSON_False );
	engine->RegisterEnumValue( "JSONType", "JSON_TRUE", cJSON_True );
	engine->RegisterEnumValue( "JSONType", "JSON_NULL", cJSON_NULL );
	engine->RegisterEnumValue( "JSONType", "JSON_NUMBER", cJSON_Number );
	engine->RegisterEnumValue( "JSONType", "JSON_STRING", cJSON_String );
	engine->RegisterEnumValue( "JSONType", "JSON_ARRAY", cJSON_Array );
	engine->RegisterEnumValue( "JSONType", "JSON_OBJECT", cJSON_Object );
	engine->RegisterEnumValue( "JSONType", "JSON_ISREF", cJSON_IsReference );
}

//----------------------------------
// Curl
//----------------------------------

#define SS_GET 0
#define SS_POST 1
#define SS_PLAIN 2
#define SS_JSON 4

/*typedef struct asCurl_Request_s {
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
} asCurl_t;*/

asCurl_Request_t *Curl_NewRequest(asCurl_t *self)
{
	asCurl_Request_t *newReq = (asCurl_Request_t*)malloc(sizeof(*newReq));
	newReq->next = NULL;
	newReq->req = NULL;
	newReq->custom = NULL;
	newReq->isJSON = false;

	if ( self->requests )
	{
		asCurl_Request_t *curReq = self->requests;
		while ( curReq->next )
		{
			curReq = curReq->next;
		}
		curReq->next = newReq;
	} else {
		self->requests = newReq;
	}

	return newReq;
}

asCurl_Request_t *Curl_GetRequest(asCurl_t *self, wswcurl_req *req)
{
	asCurl_Request_t *curReq = self->requests;
	while ( curReq->req != req )
	{
		curReq = curReq->next;
	}
	return curReq;
}

void Curl_DelRequest(asCurl_t *self, wswcurl_req *req)
{
	asCurl_Request_t *curReq = self->requests;
	asCurl_Request_t *parent = NULL;
	while ( curReq->req != req )
	{
		parent = curReq;
		curReq = curReq->next;
	}

	if ( parent )
		parent->next = curReq->next;
	else
		self->requests = curReq->next;

	trap_CURL_Delete( curReq->req );
	//curReq->doneCallback->Release();
	delete curReq;
}

static size_t Curl_Read_Cb(wswcurl_req *req, const void *buf, size_t numb, float percentage, void *customp)
{
	asCurl_t *self = (asCurl_t *) customp;
	
	// get request
	asCurl_Request_t *curReq = Curl_GetRequest(self, req);

	char *html = (char *) buf;

	int error;
	asIScriptContext *ctx = angelExport->asAcquireContext( GAME_AS_ENGINE() );
	error = ctx->Prepare( static_cast<asIScriptFunction *>(curReq->doneCallback) );

	if ( error < 0 )
		return numb;
	
	unsigned int argument = 0;

	ctx->SetArgObject( argument++, self );

	if ( curReq->isJSON )
	{
		asJSON_t* ret = (asJSON_t*) malloc(sizeof(*ret));
		ret->json = cJSON_Parse(html);
		if ( !ret->json )
			JSON_Constructor(ret);
		ctx->SetArgObject( argument++, ret );
	}
	else
		ctx->SetArgAddress( argument++, angelExport->asStringFactoryBuffer( html, strlen(html) ) );

	if ( curReq->custom )
		ctx->SetArgAddress( argument++, curReq->custom );

	error = ctx->Execute();
	if ( G_ExecutionErrorReport( error ) )
		GT_asShutdownScript();

	curReq->doneCallback->Release();
	//curReq->custom->Release();
	
	return numb;
}

static void Curl_Done_Cb(wswcurl_req *req, int status, void *customp)
{
	asCurl_t *self = (asCurl_t *) customp;
	Curl_DelRequest(self, req);
	Curl_Release(self);
	trap_CURL_Delete( req );
}

static void Curl_Header_Cb(wswcurl_req *req, const char *buf, void *customp)
{
	//Com_Printf("Header Callback\n");
}

extern void Curl_HandleRequest( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t *params, int method, CScriptAnyInterface* custom )
{
	/*int active = trap_CURL_Perform();
	Com_Printf( "current active: %i\n", active );*/

	// create new request
	asCurl_Request_t *newReq = Curl_NewRequest(self);
	newReq->doneCallback = doneCb;

	if (method & SS_JSON)
		newReq->isJSON = true;
	else
		newReq->isJSON = false;
	
	if ( custom )
		newReq->custom = custom;

	char *iface = (char *)"";
	wswcurl_req *req;
	
	/*if ( params )
	{
		if ( method & SS_POST )
		{
			req = trap_CURL_Create(iface, self->url);
			
			for ( int index = 0; index < cJSON_GetArraySize(params->json); index++ )
			{
				cJSON *curr = cJSON_GetArrayItem(params->json, index);
				char *curr_json = cJSON_PrintUnformatted(curr);
				trap_CURL_FormAdd(req, curr->string, curr_json);
				delete curr_json;
			}
		}
		else
		{
			char full_url[4*1024];
			strncpy(full_url,self->url, sizeof(full_url));

			//char* full_url = strdup(self->url);
			strcat(full_url, "?");
			for ( int index = 0; index < cJSON_GetArraySize(params->json); index++ )
			{
				cJSON *curr = cJSON_GetArrayItem(params->json, index);
				if ( params->json->type == cJSON_Array )
					strcat(full_url, "%d=");
				else
					strcat(full_url, "%s=");

				strcat(full_url, "%s");

				if ( curr->next )
					strcat(full_url, "&");
				
				char *curr_json = cJSON_PrintUnformatted(curr);
				Com_Printf("curr: %s\n", curr_json);

				if ( params->json->type == cJSON_Array )
					sprintf(full_url, strdup(full_url), index, curr_json);
				else
					sprintf(full_url, strdup(full_url), curr->string, curr_json);

				delete curr_json;
			}
			req = trap_CURL_Create(iface, full_url, cJSON_PrintUnformatted(params->json));
		}
	}
	else
	{
		req = trap_CURL_Create(iface, self->url);
	}*/

	if ( params )
	{
		if ( method & SS_POST )
		{
			req = trap_CURL_Create(iface, self->url);
			trap_CURL_FormAdd(req, "json", cJSON_PrintUnformatted(params->json));
		} else {
			char *full_url = strdup(self->url);
			strcat(full_url, "?json=");
			strcat(full_url, cJSON_PrintUnformatted(params->json));
			req = trap_CURL_Create(iface, full_url);
		}
	}
	else
	{
		req = trap_CURL_Create(iface, self->url);
	}

	trap_CURL_Stream_Callbacks(req, Curl_Read_Cb, Curl_Done_Cb, Curl_Header_Cb, self);
	trap_CURL_Start(req);
	newReq->req = req;
}

extern void Curl_Get( asCurl_t *self, asIScriptFunction* doneCb )
{
	Curl_HandleRequest(self, doneCb, NULL, SS_GET|SS_PLAIN, NULL);
}

extern void Curl_GetParams( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t params )
{
	Curl_HandleRequest(self, doneCb, &params, SS_GET|SS_PLAIN, NULL);
}

extern void Curl_GetCustom( asCurl_t *self, asIScriptFunction* doneCb, CScriptAnyInterface* custom )
{
	Curl_HandleRequest(self, doneCb, NULL, SS_GET|SS_PLAIN, custom);
}

extern void Curl_GetJSON( asCurl_t *self, asIScriptFunction* doneCb )
{
	Curl_HandleRequest(self, doneCb, NULL, SS_GET|SS_JSON, NULL);
}

extern void Curl_GetJSONParams( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t params )
{
	Curl_HandleRequest(self, doneCb, &params, SS_GET|SS_JSON, NULL);
}

extern void Curl_GetJSONParamsCustom( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t params, CScriptAnyInterface* custom )
{
	Curl_HandleRequest(self, doneCb, &params, SS_GET|SS_JSON, custom);
}

extern void Curl_PostParams( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t params )
{
	Curl_HandleRequest(self, doneCb, &params, SS_POST|SS_PLAIN, NULL);
}

extern void Curl_PostJSONParams( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t params )
{
	Curl_HandleRequest(self, doneCb, &params, SS_POST|SS_JSON, NULL);
}

extern void Curl_PostJSONParamsCustom( asCurl_t *self, asIScriptFunction* doneCb, asJSON_t params, CScriptAnyInterface* custom )
{
	Curl_HandleRequest(self, doneCb, &params, SS_POST|SS_JSON, custom);
}

extern void Curl_SetUrl( asCurl_t *self, asstring_t *url )
{
	self->url = (char *)malloc(url->len+1);
	strcpy(self->url, url->buffer);
}

extern asstring_t* Curl_GetUrl( asCurl_t *self )
{
	return angelExport->asStringFactoryBuffer( self->url, strlen(self->url) );
}

extern asCurl_t* Curl_Factory()
{
	asCurl_t *curl = (asCurl_t *)malloc(sizeof(*curl));
	curl->refCount = 1;
	curl->url = (char *)"";
	curl->requests = NULL;
	/*asIScriptEngine* engine = GAME_AS_ENGINE();
	asIObjectType *ot = engine->GetObjectTypeById(engine->GetTypeIdByDecl("Curl"));
	engine->NotifyGarbageCollectorOfNewObject(curl, ot);*/
	if ( SS_DEBUG ) Com_Printf("CREATE CURL\n");
	return curl;
}

extern asCurl_t* Curl_StringFactory( asstring_t *url )
{
	asCurl_t *self = Curl_Factory();
	Curl_SetUrl(self, url);
	return self;
}

extern void Curl_AddRef( asCurl_t *self )
{
	self->refCount++;
	if ( SS_DEBUG ) Com_Printf("CURL AddRef refCount: %i\n", self->refCount);
}

extern void Curl_Release( asCurl_t *self )
{
	if ( --self->refCount == 0 )
	{
		//Com_Printf("STATE: %i\n",angelExport->asAcquireContext( GAME_AS_ENGINE() )->GetState());
		/*if ( angelExport->asAcquireContext( GAME_AS_ENGINE() )->GetState() == asEXECUTION_UNINITIALIZED )
		{
			while( self->requests )
			{
				Curl_DelRequest(self, self->requests->req);
			}
		}*/

		// delete this;
		if ( !self->requests )
		{
			free(self->url);
			free(self);
			if ( SS_DEBUG ) Com_Printf("DELETE CURL\n");
			return;
		}
		self->refCount++;
	}
	if ( SS_DEBUG ) Com_Printf("CURL Release refCount: %i\n", self->refCount);
}

extern void Curl_ReleaseAll( asCurl_t *self )
{
	//self->refCount |= 0x80000000;
	while( self->requests )
	{
		Curl_DelRequest(self, self->requests->req);
	}
	Curl_Release(self);
}

extern void Curl_Shutdown()
{
	Com_Printf("shutdown\n");
}

void RegisterCURLAddon( asIScriptEngine *engine )
{
	RegisterJsonClass( engine );
	
	engine->RegisterObjectType( "Curl", sizeof(asCurl_t), asOBJ_REF );

	engine->RegisterInterface("Test");

	// funcdefs
	engine->RegisterFuncdef("void CurlDone(Curl@ curl, String& context)");
	engine->RegisterFuncdef("void CurlJSONDone(Curl@ curl, JSON json)");
	engine->RegisterFuncdef("void CurlCustomDone(Curl@ curl, String& context, any@ custom)");
	engine->RegisterFuncdef("void CurlJSONCustomDone(Curl@ curl, JSON json, any@ custom)");
	
	// behaviours
	engine->RegisterObjectBehaviour( "Curl", asBEHAVE_FACTORY, "Curl @f()", asFUNCTION( Curl_Factory ), asCALL_CDECL );
	engine->RegisterObjectBehaviour( "Curl", asBEHAVE_FACTORY, "Curl @f(String& url)", asFUNCTION( Curl_StringFactory ), asCALL_CDECL );

	engine->RegisterObjectBehaviour( "Curl", asBEHAVE_ADDREF, "void f()", asFUNCTION( Curl_AddRef ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectBehaviour( "Curl", asBEHAVE_RELEASE, "void f()", asFUNCTION( Curl_Release ), asCALL_CDECL_OBJFIRST );

	// functions
	engine->RegisterObjectMethod( "Curl", "void getUrl(CurlDone @callback)", asFUNCTION( Curl_Get ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void getUrl(CurlDone @callback, JSON params)", asFUNCTION( Curl_GetParams ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void getUrlCustom(CurlCustomDone @callback, any@ custom)", asFUNCTION( Curl_GetCustom ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void getJSON(CurlJSONDone @callback)", asFUNCTION( Curl_GetJSON ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void getJSON(CurlJSONDone @callback, JSON params)", asFUNCTION( Curl_GetJSONParams ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void getJSONCustom(CurlJSONCustomDone @callback, JSON params, any@ custom)", asFUNCTION( Curl_GetJSONParamsCustom ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void postUrl(CurlDone @callback, JSON params)", asFUNCTION( Curl_PostParams ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void postJSON(CurlJSONDone @callback, JSON params)", asFUNCTION( Curl_PostJSONParams ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "void postJSONCustom(CurlJSONCustomDone @callback, JSON params, any@ custom)", asFUNCTION( Curl_PostJSONParamsCustom ), asCALL_CDECL_OBJFIRST );
	
	engine->RegisterObjectMethod( "Curl", "void set_url(String& url)", asFUNCTION( Curl_SetUrl ), asCALL_CDECL_OBJFIRST );
	engine->RegisterObjectMethod( "Curl", "String& get_url()", asFUNCTION( Curl_GetUrl ), asCALL_CDECL_OBJFIRST );
}