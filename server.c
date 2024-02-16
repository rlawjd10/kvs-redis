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
#define OP_READ 0
#define OP_READ_REPLY 1
#define OP_WRITE 2
#define OP_WRITE_REPLY 3
#define KEY_SIZE 16
#define VALUE_SIZE 64

//KVS 헤더 구조체
struct kvs_hdr{
  uint32_t op; // Operation type
  char key[KEY_SIZE]; 
  char value[VALUE_SIZE]; 
  uint64_t latency; // latency 측정용
} __attribute__((packed)); // padding 방지

//타임스탬프
uint64_t get_cur_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  return t;
}

int put(redisContext *c, char* key, char *value) {
    redisReply *reply;
    char key_str[KEY_SIZE];

    snprintf(key_str, KEY_SIZE, "%s", key);
    reply = redisCommand(c, "SET %s %s", key_str, value);
    if (reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(reply);
        return -1;
    } else {
        freeReplyObject(reply);
        return 0;
    }
}

char* get(redisContext *c, char* key) {
    redisReply *reply;
    char key_str[KEY_SIZE];
    snprintf(key_str, KEY_SIZE, "%s", key);
    reply = redisCommand(c, "GET %s", key_str);

    if (reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
        return NULL;
    } else {
		char* get_key = strdup(reply->str); //문자열 복제
		freeReplyObject(reply);
        return get_key;
    }
}

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

	while (1) {
		
		n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, &cli_addr_len);

		if (n > 0) {

			if (RecvBuffer.op == OP_READ) {
				char* get_value = get(redis_context, RecvBuffer.key);
				if (get_value == NULL) {
					printf("Error: Failed to retrieve value for key: %s\n", RecvBuffer.key);
					snprintf(RecvBuffer.value, VALUE_SIZE, "Error: Failed to retrieve value for key: %s", RecvBuffer.key);
					RecvBuffer.op = 4;
				} else {
					strcpy(RecvBuffer.value, get_value);
					RecvBuffer.op = OP_READ_REPLY;
				}
			} 
			else if (RecvBuffer.op == OP_WRITE) {
				int put_check = put(redis_context, RecvBuffer.key, RecvBuffer.value);
				if (put_check == -1) {
					printf("Error: Failed to store key-value pair: %s\n", RecvBuffer.key);
					snprintf(RecvBuffer.value, VALUE_SIZE, "Error: Failed to store key-value pair: %s", RecvBuffer.key);
					RecvBuffer.op = 4;
				} else {
					RecvBuffer.op = OP_WRITE_REPLY;
				}
			} 
			

			sendto(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
		}	
	}
	
	close(sock);
	redisFree(redis_context);

	return 0;
}
