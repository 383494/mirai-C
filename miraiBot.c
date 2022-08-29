#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "miraiBot.h"

// write to *(cJSON**)buf
size_t write_cb(void *stream, size_t size, size_t nmemb, void *buf){
	*(cJSON**)buf = cJSON_Parse((const char*)stream);
	return size*nmemb;
}

cJSON *HttpPost(const char *url, cJSON *sendData){
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if(!curl){ return NULL; }
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-type:application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0L); //不返回头信息
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	cJSON *ret = NULL;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret);

	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	char *sdata = cJSON_Print(sendData);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, sdata); 

	res = curl_easy_perform(curl);
	if(res != CURLE_OK){
		M_logError("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
//		return NULL;
	}
	free(sdata);
	curl_slist_free_all(headers); //防止内存泄漏
	curl_easy_cleanup(curl);
	return ret;
}

cJSON *HttpGet(const char *url){
	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if(!curl){ return NULL; }
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-type:application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0L); //不返回头信息
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	cJSON *ret = NULL;
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret);

//	curl_easy_setopt(curl, CURLOPT_POST, 0L);	//默认get

	res = curl_easy_perform(curl);
	if(res != CURLE_OK){
		M_logError("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
//		return NULL;
	}
	curl_slist_free_all(headers); //防止内存泄漏
	curl_easy_cleanup(curl);
	return ret;
}

M_Bot* M_createBot(QQ_t qqnum, const char *serverAddr, const char *verifyKey){
	M_Bot *bot = (M_Bot*)malloc(sizeof(M_Bot));
	bot->addrlen = strlen(serverAddr);
	bot->serverAddr = (char*)malloc(sizeof(char)*(bot->addrlen+50));
	strcpy(bot->serverAddr, serverAddr);
	bot->qqnum.qq = getQQ(qqnum);
	cJSON *rdata, *pdata = cJSON_CreateObject();	//rdata: receiveData
	cJSON_AddStringToObject(pdata, "verifyKey", verifyKey);
	strcpy(bot->serverAddr + bot->addrlen, "/verify");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in verify: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
	}
	cJSON_Delete(pdata);
	bot->session = (char*)malloc(sizeof(char)*(strlen(cJSON_GetObjectItem(rdata, "session")->valuestring)+1));
	strcpy(bot->session, cJSON_GetObjectItem(rdata, "session")->valuestring);
	cJSON_Delete(rdata);

	pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "qq", (double)getQQ(bot->qqnum));
	strcpy(bot->serverAddr + bot->addrlen, "/bind");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in bind: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
	}
	cJSON_Delete(pdata);
	cJSON_Delete(rdata);

	return bot;
}

void M_deleteBot(M_Bot *bot){
	cJSON *rdata, *pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "qq", (double)getQQ(bot->qqnum));
	strcpy(bot->serverAddr + bot->addrlen, "/release");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in release: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
	}
	cJSON_Delete(pdata);
	cJSON_Delete(rdata);
	free(bot->serverAddr);
	free(bot->session);
	free(bot);
}

inline msgChain *Msg_new(){ return cJSON_CreateArray(); }

msgChain *Msg_addAt(msgChain *msg, QQ_t targetId){
	msgChain *add = cJSON_CreateObject();
	cJSON_AddStringToObject(add, "type", "At");
	cJSON_AddNumberToObject(add, "target", getQQ(targetId));
//	cJSON_AddStringToObject(add, "display", "");
	
	cJSON_AddItemToArray(msg, add);
	return msg;
}

msgChain *Msg_addAtAll(msgChain *msg){
	msgChain *add = cJSON_CreateObject();
	cJSON_AddStringToObject(add, "type", "AtAll");
	
	cJSON_AddItemToArray(msg, add);
	return msg;
}

msgChain *Msg_addPlain(msgChain *msg, const char *plain){
	msgChain *add = cJSON_CreateObject();
	cJSON_AddStringToObject(add, "type", "Plain");
	cJSON_AddStringToObject(add, "text", plain);
	
	cJSON_AddItemToArray(msg, add);
	return msg;
}

cJSON *M_fetchMsg(M_Bot *bot){
	strcpy(bot->serverAddr + bot->addrlen, "/fetchMessage?sessionKey=");
	strcat(bot->serverAddr, bot->session);
	strcat(bot->serverAddr, "&count=1");
	cJSON *ret = HttpGet(bot->serverAddr);
	if(cJSON_GetObjectItem(ret, "code")->valueint != 0){
		M_logError("Error at fetchmsg: %d", cJSON_GetObjectItem(ret, "code")->valueint);
	}
	return ret;
}

// 返回消息标识
int M_sendFriendMsg(M_Bot *bot, QQ_t frd, msgChain *msg){
	cJSON *rdata, *pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "target", getQQ(frd));
	cJSON_AddItemToObject(pdata, "messageChain", msg);

	strcpy(bot->serverAddr + bot->addrlen, "/sendFriendMessage");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in sending friendmsg: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
	}
	int ret = cJSON_GetObjectItem(rdata, "messageId")->valueint;
	cJSON_Delete(pdata);
	cJSON_Delete(rdata);
	return ret;
}

int M_sendGroupMsg(M_Bot *bot, GID_t grp, msgChain *msg){
	cJSON *rdata, *pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "target", getGID(grp));
	cJSON_AddItemToObject(pdata, "messageChain", msg);

	strcpy(bot->serverAddr + bot->addrlen, "/sendGroupMessage");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in sending groupmsg: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
	}
	int ret = cJSON_GetObjectItem(rdata, "messageId")->valueint;
	cJSON_Delete(pdata);
	cJSON_Delete(rdata);
	return ret;
}

int M_replyGroupMsg(M_Bot *bot, GID_t grp, int quoteMsgId, msgChain *msg){
	cJSON *rdata, *pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "target", getGID(grp));
	cJSON_AddNumberToObject(pdata, "quote", quoteMsgId);
	cJSON_AddItemToObject(pdata, "messageChain", msg);

	strcpy(bot->serverAddr + bot->addrlen, "/sendGroupMessage");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in sending groupmsg: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
	}
	int ret = cJSON_GetObjectItem(rdata, "messageId")->valueint;
	cJSON_Delete(pdata);
	cJSON_Delete(rdata);
	return ret;
}
