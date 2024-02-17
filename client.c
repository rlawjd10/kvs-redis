//입력은 ./client.out 0 1 또는 ./client.out 1 1
//1부터 10배로 증가하는 코드 추가

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

//KVS 헤더 구조체
#pragma pack(1) // padding 방지
#define LB_SIZE 16
#define VALUE_SIZE 64

#define ROUND_ROBIN 0
#define RANDOM 1

#define REQUEST 6

double avg_latencty[REQUEST] = {0};
uint64_t avg_tail[REQUEST] = {0};

struct kvs_hdr{ 
  int key;
  char value[VALUE_SIZE];
  char LB[LB_SIZE];
  uint64_t latency; // latency 측정용
} __attribute__((packed)); // padding 방지

//ns 단위 시간 함수
uint64_t get_cur_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  return t;
}

int compare(const void *a, const void *b) {
    return (*(uint64_t *)a > *(uint64_t *)b) - (*(uint64_t *)a < *(uint64_t *)b);
}

void calculate(uint64_t total_latency, uint64_t *tail_latency, int N) {

	// Calculate statistics
    double avg_latency = (double)total_latency / N;
    printf("Average Latency: %.3f microseconds\n", avg_latency / 1000);
	avg_latency = avg_latency / 1000;

    // Calculate tail latency using quicksort
    qsort(tail_latency, N, sizeof(uint64_t), compare);
    uint64_t tail_latency_value = tail_latency[(N * 99) / 100];
    printf("Tail Latency (99th percentile): %.3f microseconds\n\n", (double)tail_latency_value / 1000);
	tail_latency_value = (double)tail_latency_value / 1000;

	if (N == 1 || N ==5) {
        avg_latencty[0] += avg_latency;
		avg_tail[0] += tail_latency_value;
    } else if (N == 10 || N ==50) {
        avg_latencty[1] += avg_latency;
		avg_tail[1] += tail_latency_value;
    } else if (N == 100 || N ==500) {
        avg_latencty[2] += avg_latency;
		avg_tail[2] += tail_latency_value;
    } else if (N == 1000 || N ==5000) {
        avg_latencty[3] += avg_latency;
		avg_tail[3] += tail_latency_value;
    } else if (N == 10000 || N ==50000) {
        avg_latencty[4] += avg_latency;
		avg_tail[4] += tail_latency_value;
    } else if (N == 100000) {
        avg_latencty[5] += avg_latency;
		avg_tail[5] += tail_latency_value;
    }
}

void total_calculate(double *avg_latencty, uint64_t *avg_tail) {

	for (int i = 0; i < REQUEST; i++) {
		printf("10 times [%d] Average Latency: %.3f microseconds\n", i,avg_latencty[i] / 10);
		printf("10 times [%d] Tail Latency (99th percentile): %.3f microseconds\n\n", i, (double)avg_tail[i] /10);
	}
}

int main(int argc, char *argv[]) {

	if ( argc < 3 ) {
	 printf("error 1\n");
	 return 1;
	}

	//parameter 1, 2
	int LB_index = atoi(argv[1]);
	int N = atoi(argv[2]);

	//addr setting
	int num_servers = 4; //1 or 2 or 4
	const char* server_name = "localhost"; // 127.0.0.1
	struct sockaddr_in srv_addr[num_servers]; //server address & port number

	//port number
	int SERVER_PORT[4] = {5001, 5002, 5003, 5004};

	for(int i = 0; i < num_servers; i++) {
		memset(&srv_addr[i], 0, sizeof(srv_addr[i])); 
		srv_addr[i].sin_family = AF_INET;
		srv_addr[i].sin_port = htons(SERVER_PORT[i]);
		inet_pton(AF_INET, server_name, &srv_addr[i].sin_addr);
	}

	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Could not create socket\n");
		exit(1);
	}
	

 	struct sockaddr_in cli_addr;
  	int cli_addr_len = sizeof(cli_addr);

	int n = 0;
	struct kvs_hdr SendBuffer, RecvBuffer;


	//seed change
	srand(time(0));

	//load balancing index
	if (LB_index == ROUND_ROBIN) {
		for (int i = 0; i < 10; i++) {
			printf("i = %d\n", i);
			N = atoi(argv[2]);

			while(N < 100001) {
			static int server_index = 0;
			uint64_t latency = 0;
			uint64_t total_latency = 0;
   			uint64_t tail_latency[N];
			printf("N = %d\n", N);

			//input key
			for (int i = 0; i < N; i++) {

				SendBuffer.key = rand() % 100001;
				strcpy(SendBuffer.LB, "ROUND_ROBIN");
				SendBuffer.latency = get_cur_ns();

				sendto(sock, &SendBuffer, sizeof(SendBuffer), 0, (struct sockaddr *)&srv_addr[server_index], sizeof(srv_addr[server_index]));
		
				n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, &cli_addr_len);
		
				if (n > 0) {
          	 		latency = get_cur_ns() - SendBuffer.latency;
					//printf("[Round Robin] key %d Latency : %zu microseconds\n", RecvBuffer.key, latency/1000);

					total_latency += latency;
           			tail_latency[i] = latency;
				}	
				//next server RR
				server_index = (server_index + 1) % num_servers; //4
			}
			calculate(total_latency, tail_latency, N);
			N = N * 10;
			}
		}

		total_calculate(avg_latencty, avg_tail);
	} else if (LB_index == RANDOM) {
		for (int i = 0; i < 10; i++) {
			printf("i = %d\n", i);
			N = atoi(argv[2]);

			while(N < 100001) {
			printf("N = %d\n", N);
			uint64_t latency = 0;
			uint64_t total_latency = 0;
   			uint64_t tail_latency[N];

			for(int i = 0; i < N; i++) {

			int random_server = rand() % num_servers; //4

			SendBuffer.key = rand() % 100001;
			SendBuffer.latency = get_cur_ns();
			strcpy(SendBuffer.LB, "RANDOM");

			sendto(sock, &SendBuffer, sizeof(SendBuffer), 0, (struct sockaddr *)&srv_addr[random_server], sizeof(srv_addr[random_server]));
			
			n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), 0, (struct sockaddr *)&cli_addr, &cli_addr_len);

			if (n > 0) {
           		uint64_t latency = get_cur_ns() - SendBuffer.latency;
				//printf("[RANDOM] key %d Latency : %zu microseconds\n", RecvBuffer.key, latency/1000);

				total_latency += latency;
           		tail_latency[i] = latency;
			}
			}
			calculate(total_latency, tail_latency, N);
			N = N * 10;
			}
		}
		total_calculate(avg_latencty, avg_tail);
	} else {
		printf("Invalid LB_index\n");
		return 1;
	}

	close(sock);
	return 0;
}