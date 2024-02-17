#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <hiredis/hiredis.h>

#pragma pack(1) // padding 방지
#define VALUE_SIZE 64

#define LB_SIZE 16
#define MAX_SIZE 100001

struct kvs_hdr{ 
  int key; 
  char value[VALUE_SIZE]; 
  char LB[LB_SIZE];
  uint64_t latency; // latency 측정용
} __attribute__((packed)); // padding 방지

//redis
int put(redisContext *c, int key, char *value) {
    redisReply *reply;

    reply = redisCommand(c, "SET %d %s", key, value);
    if (reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(reply);
        return -1;
    } else {
        freeReplyObject(reply);
        return 0;
    }
}

char* get(redisContext *c, int key) {
    redisReply *reply;
    
    reply = redisCommand(c, "GET %d", key);

    if (reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
        return NULL;
    } else {
		char* get_key = strdup(reply->str); //문자열 복제
		freeReplyObject(reply);
        return get_key;
    }
}


//main
int main (int argc, char *argv[]){
	if ( argc < 2 ){
	 printf("Input : %s port number\n", argv[0]);
	 return 1;
	}

	int SERVER_PORT = atoi(argv[1]);
	
	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(SERVER_PORT); 
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int sock;
	
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Could not create listen socket\n");
		exit(1);
	}
	
	if ((bind(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) < 0) {
		printf("Could not bind socket\n");
		exit(1);
	}

	// Connect to Redis server
	//redisContext로 redis서버와 연결 후, error 체크
  	redisContext *redis_context = redisConnect("127.0.0.1", 6379);
  	if (redis_context->err) {
   		printf("Failed to connect to Redis: %s\n", redis_context->errstr);
   		return 1;
  	}
	
	//client주소 정보를 담는 구조체
	struct sockaddr_in cli_addr;
  	int cli_addr_len = sizeof(cli_addr);

	int n = 0;
	struct kvs_hdr RecvBuffer;

	// 데이터 채우기
	char value[VALUE_SIZE];
	memset(value, 0, sizeof(value));
	for (int i = 0; i < VALUE_SIZE; i += 4) {
		memcpy(value + i, "ABCD", 4);
	}

	while (1) {
		n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, &cli_addr_len);
		if (n > 0) {
			//printf("%s sock key = %d\n", RecvBuffer.LB, RecvBuffer.key);
			strcpy(RecvBuffer.value, value);
		    sendto(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
        }
	}
	
	close(sock);
	redisFree(redis_context);

	return 0;
}