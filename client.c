#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

//KVS 헤더 구조체
#pragma pack(1) // padding 방지
#define OP_READ 0
#define OP_READ_REPLY 1
#define OP_WRITE 2
#define OP_WRITE_REPLY 3

#define KEY_SIZE 16
#define VALUE_SIZE 64

struct kvs_hdr{
  uint32_t op; // Operation type. 
  char key[KEY_SIZE]; 
  char value[VALUE_SIZE]; 
  uint64_t latency; // latency 측정용
} __attribute__((packed)); // padding 방지

//ns 단위 시간 함수
uint64_t get_cur_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  return t;
}

int readline(int fd, char *ptr, int maxlen) {
   int n, rc;
   char c;
   for(n=1; n<maxlen; n++){
      if ( (rc = read(fd, &c, 1)) == 1 ){
         *ptr ++ = c;
         if ( c == '\n' ) break;
      } else if ( rc == 0 ){
         if ( n == 1 ) return 0;
         else break;
      }
   }

   *ptr = 0;
   return n;
}


int main(int argc, char *argv[]) {
	if ( argc < 2 ){
	 printf("Input : %s port number\n", argv[0]);
	 return 1;
	}

	int SERVER_PORT = atoi(argv[1]);
  /* localhost에서 통신할 것이므로 서버 ip주소도 그냥 localhost */
	const char* server_name = "localhost"; // 127.0.0.1
	struct sockaddr_in srv_addr; // Create socket structure
	memset(&srv_addr, 0, sizeof(srv_addr)); // Initialize memory space with zeros
	srv_addr.sin_family = AF_INET; // IPv4
	srv_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, server_name, &srv_addr.sin_addr);  // Convert IP addr. to binary


	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Could not create socket\n");
		exit(1);
	}

	int n = 0;
	int maxlen = sizeof(struct kvs_hdr);
	//문자열이 아닌 구조체가 전송. 구조체 선언
	struct kvs_hdr SendBuffer;
	struct kvs_hdr RecvBuffer;

 	struct sockaddr_in cli_addr;
  	int cli_addr_len = sizeof(cli_addr);

	while(1){
		char input[100];

		if ( readline(0, input, sizeof(input)) > 0 ){ //input에 저장
			char cmd[5], key[KEY_SIZE], value[VALUE_SIZE];

			//배열 초기화
			memset(cmd, 0, sizeof(cmd));
			memset(key, 0, sizeof(key));
			memset(value, 0, sizeof(value));

			sscanf(input,"%s %s %s", cmd, key, value); //입력받은 데이터 구분



			if (strcmp(cmd, "get") == 0 && strlen(key) > 0 && strlen(value)==0) {
				SendBuffer.op = OP_READ;
				strcpy(SendBuffer.key, key);
			} else if (strcmp(cmd, "put") == 0 && strlen(key) > 0 && strlen(value) > 0) {
				SendBuffer.op = OP_WRITE;
				strcpy(SendBuffer.key, key);
				strcpy(SendBuffer.value, value);
			} else {
				continue;
			}
			
			SendBuffer.latency = get_cur_ns();
			sendto(sock, &SendBuffer, sizeof(SendBuffer), 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr));

		}

   		n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, &cli_addr_len);

		if (n > 0) {

			if (RecvBuffer.op == OP_READ_REPLY) {
				printf("The key %s has value %s\n", RecvBuffer.key, RecvBuffer.value);
			} else if (RecvBuffer.op == OP_WRITE_REPLY) {
				printf("your write for %s is done\n", RecvBuffer.key);
			} else if (RecvBuffer.op == 4) {
				printf("%s\n", RecvBuffer.value);
			}

			printf("Latency : %zu microseconds\n", (get_cur_ns() - SendBuffer.latency)/1000);
		}
	}

	close(sock);
	return 0;
}
