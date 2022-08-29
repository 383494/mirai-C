#pragma once
#define M_logError(str,...) fprintf(stderr, "File %s Func %s Line %d\n" str,__FILE__, __func__, __LINE__, __VA_ARGS__)

typedef struct QQ_t{
	long long qq;
} QQ_t;

static inline long long getQQ(QQ_t qnu){
	return qnu.qq;
}

typedef struct GID_t{
	long long gid;
} GID_t;

static inline long long getGID(GID_t gnu){
	return gnu.gid;
}

typedef struct M_Bot{
	char *serverAddr;
	int addrlen;
	char *session;
	QQ_t qqnum;
} M_Bot;

typedef cJSON msgChain;

// https://docs.mirai.mamoe.net/mirai-api-http/api/MessageType.html#plain
/*
enum MSG_TYPE{
	MSG_Source,
	MSG_Quote,
	MSG_At,
	MSG_AtAll,
	MSG_Face,
	MSG_Plain,
	MSG_Image,
	MSG_FlashImage,
	MSG_Voice,
	MSG_Xml,
	MSG_Json,
	MSG_App,
	MSG_Poke,
	MSG_MarketFace,
	MSG_MusicShare,
	MSG_ForwardMessage,
	MSG_File,
	MSG_MiraiCode,
};

char *MSGTSTR[] = {
	[MSG_Source] = "Source",
	[MSG_Quote] = "Quote",
	[MSG_At] = "At",
	[MSG_AtAll] = "AtAll",
	[MSG_Face] = "Face",
	[MSG_Plain] = "Plain",
	[MSG_Image] = "Image",
	[MSG_FlashImage] = "FlashImage",
	[MSG_Voice] = "Voice",
	[MSG_Xml] = "Xml",
	[MSG_Json] = "Json",
	[MSG_App] = "App",
	[MSG_Poke] = "Poke",
	[MSG_MarketFace] = "MarketFace",
	[MSG_MusicShare] = "MusicShare",
	[MSG_ForwardMessage] = "ForwardMessage",
	[MSG_File] = "File",
	[MSG_MiraiCode] = "MiraiCode",
};
*/

M_Bot* M_createBot(QQ_t qqnum, const char *serverAddr, const char *verifyKey);
void M_deleteBot(M_Bot *bot);

msgChain *Msg_new();
msgChain *Msg_addAt(msgChain *msg, QQ_t targetId);
msgChain *Msg_addAtAll(msgChain *msg);
msgChain *Msg_addPlain(msgChain *msg, const char *plain);

cJSON *M_fetchMsg(M_Bot *bot);

// Example:
// M_sendFriendMsg(bot, (QQ_t){123456}, Msg_addPlain(Msg_new(), "test"));
// Msg_new()的内存会自动释放
int M_sendFriendMsg(M_Bot *, QQ_t , msgChain *);
int M_sendGroupMsg(M_Bot *, GID_t , msgChain *);
int M_replyGroupMsg(M_Bot *, GID_t , int , msgChain *);

