// TODO: perror
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
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

pthread_rwlock_t *rwlock;

static void *botMain(void *botptr){
	M_Bot *bot = (M_Bot*)botptr;
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	cJSON *retMsg, *newMsg;
	bool running = true;
	while(running){
		pthread_testcancel();

		retMsg = M_fetchMsg(bot);
		newMsg = cJSON_GetArrayItem(cJSON_GetObjectItem(retMsg, "data"), 0);
		do{

		if(newMsg == NULL){
			sleep(1);
//			usleep(50 *1000);
			break;
		}
		if(bot->onGrpMsg != NULL &&
				strcmp(cJSON_GetObjectItem(newMsg, "type")->valuestring, "GroupMessage") == 0){
			bot->onGrpMsg(bot, newMsg);
		} else if(bot->onFriendMsg != NULL &&
				strcmp(cJSON_GetObjectItem(newMsg, "type")->valuestring, "FriendMessage") == 0){
			bot->onFriendMsg(bot, newMsg);
		}


		}while(0);
		cJSON_Delete(retMsg);

/*
		pthread_mutex_lock(&mutex);
		if(mail[0] == 1 && mail[1] == 1){
			running = false;
		}
		pthread_mutex_unlock(&mutex);
*/

	}
	return NULL;
}

int M_botStart(M_Bot *bot){
	if(bot->thrd != NULL){
		return 1;	// bot has been start
	}
	bot->thrd = malloc(sizeof(pthread_t));
	if(pthread_create(bot->thrd, NULL, botMain, bot) != 0){
		free(bot->thrd);
		bot->thrd = NULL;
		M_logError("error creating thread");
		return -1;
	}
	return 0;
}

inline int M_botStop(M_Bot *bot){
	return pthread_cancel(*bot->thrd);
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
	cJSON_Delete(pdata);
	if(rdata == NULL){
		M_logError("Http Post return NULL");
		free(bot->serverAddr);
		free(bot);
		return NULL;
	}
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in verify: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
		cJSON_Delete(rdata);
		free(bot->serverAddr);
		free(bot);
		return NULL;
	}
	bot->session = (char*)malloc(sizeof(char)*(strlen(cJSON_GetObjectItem(rdata, "session")->valuestring)+1));
	strcpy(bot->session, cJSON_GetObjectItem(rdata, "session")->valuestring);
	cJSON_Delete(rdata);

	pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "qq", (double)getQQ(bot->qqnum));
	strcpy(bot->serverAddr + bot->addrlen, "/bind");
	rdata = HttpPost(bot->serverAddr, pdata);
	cJSON_Delete(pdata);
	if(rdata == NULL){
		M_logError("Http Post return NULL");
		free(bot->serverAddr);
		free(bot->session);
		free(bot);
		return NULL;
	}
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in bind: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
		cJSON_Delete(rdata);
		free(bot->serverAddr);
		free(bot->session);
		free(bot);
		return NULL;
	}
	cJSON_Delete(rdata);
	bot->thrd = NULL;
	bot->onGrpMsg = NULL;
	bot->onFriendMsg = NULL;

	return bot;
}

void M_deleteBot(M_Bot *bot){
	cJSON *rdata, *pdata = cJSON_CreateObject();
	cJSON_AddStringToObject(pdata, "sessionKey", bot->session);
	cJSON_AddNumberToObject(pdata, "qq", (double)getQQ(bot->qqnum));
	strcpy(bot->serverAddr + bot->addrlen, "/release");
	rdata = HttpPost(bot->serverAddr, pdata);
	if(rdata == NULL){
		M_logError("Http Post return NULL");
		cJSON_Delete(pdata);
		free(bot->serverAddr);
		free(bot->session);
		free(bot);
		return;
	} 
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in release: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
		cJSON_Delete(pdata);
		cJSON_Delete(rdata);
		free(bot->serverAddr);
		free(bot->session);
		free(bot);
		return;
	}
	cJSON_Delete(pdata);
	cJSON_Delete(rdata);
	free(bot->serverAddr);
	free(bot->session);
	free(bot->thrd);
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
	cJSON_Delete(pdata);
	if(rdata == NULL){
		M_logError("Http Post return NULL");
		return M_HTTP_ERROR_INT;
	}
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in sending friendmsg: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
		cJSON_Delete(rdata);
		return M_HTTP_ERROR_INT;
	}
	int ret = cJSON_GetObjectItem(rdata, "messageId")->valueint;
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
	cJSON_Delete(pdata);
	if(rdata == NULL){
		M_logError("Http Post return NULL");
		return M_HTTP_ERROR_INT;
	}
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in sending groupmsg: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
		cJSON_Delete(rdata);
		return M_HTTP_ERROR_INT;
	}
	int ret = cJSON_GetObjectItem(rdata, "messageId")->valueint;
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
	cJSON_Delete(pdata);
	if(rdata == NULL){
		M_logError("Http Post return NULL");
		return M_HTTP_ERROR_INT;
	}
	if(cJSON_GetObjectItem(rdata, "code")->valueint != 0){
		M_logError("Error post value in sending groupmsg: %d", cJSON_GetObjectItem(rdata, "code")->valueint);
		cJSON_Delete(rdata);
		return M_HTTP_ERROR_INT;
	}
	int ret = cJSON_GetObjectItem(rdata, "messageId")->valueint;
	cJSON_Delete(rdata);
	return ret;
}
