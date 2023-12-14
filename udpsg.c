/****************************************************
* udpsg.c - UDP Server Multi Plattform
* Stand 14.12.2023 - (C)Joembedded.de
*
* Aus Performance-Gruenden wird nicht jedes UDP Paket einzeln verarbeitet,
* sondern es wird versucht bis zu MAX_CLIENTS Pakete auf einmal zu lesen.
* Dadurch kann ein nachfolgender libCurl-Aufruf das 'curl_multi_init'
* Feature genutzt werden ( https://curl.se/ -> libcurl ). Eine schnellere
* Loesung waere ein einzelner libCurl-Aufruf mit den Daten als Array.
* Das wuerde aber das aufgerufene Script komplizierter machen. Daher lieber
* die einfache Version mit 'curl_multi'.
* 
*
* Der Server kann unter Windows MS VS und Linux GCC uebersetzt werden.
*
* Windows: Fuer VS wird libcurl.dll benoetigt:
* Installieren via vcpk (siehe https://github.com/curl/curl/blob/master/docs/INSTALL.md#building-using-vcpkg ):
*   git clone https://github.com/Microsoft/vcpkg.git
*   cd vcpkg
*   bootstrap-vcpkg.sh
*   vcpkg integrate install
*   vcpkg install curl[tool]
* Dauert ca. 5 min, dann kann libcurl wie gewohnt verwendet werden (exakt identisch zu gcc! Keine weiteren include oder libs noetig!)
*
* Linux:
*   apt install libcurl4-openssl-dev -y
* Dann einfach per 'gcc -lcurl' dazulinken
*
* Dir: cd /var/www/vhosts/joembedded.eu/c
* Uebersetzen als: gcc udpsg.c -o g.out
* Test: nc  -u joembedded.eu 5288 -v -v
*
*
* Test with: nc -u localhost 5288 -v -v
*            nc  -u joembedded.eu 5288 -v -v
*
****************************************************/

#ifdef _MSC_VER	// MS VC
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib") //Winsock Library
#else // GCC
typedef int SOCKET;
#endif

#include <stdio.h> 
#include <stdint.h>
#include <stdlib.h> 
#include <string.h> 
#include <time.h>

#ifdef _MSC_VER	// MS VS
#include<winsock2.h>
#else // GCC
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <fcntl.h>
#endif

#define PORT     5288	// UDP Listen Port
#define CALLSCRIPT "http://localhost/wrk/udplog/payload_minimal.php?p=" // Hex-Payload will be added

#define RX_BUFLEN 1024 
#define TX_BUFLEN 1024 

#define IDLE_TIME 10 // Idle-Timeout 10 sec

char tx_replybuf[TX_BUFLEN + 32]; // Etwas mehr Platz fuer optional ZEIT-Infos am Ende

SOCKET sockfd = (SOCKET)0;	// Server, localer Port

#define MAX_CLIENTS	10	// Maximum Number to receive, siehe oeben
typedef struct {
	struct sockaddr_in client_socket;	// Source
	char rx_buffer[RX_BUFLEN + 1];  // OPt. String wit 0-Terminator
	int rcv_len; // Receives len
} CLIENT;

CLIENT clients[MAX_CLIENTS];

int client_sock_len = sizeof(struct sockaddr_in);  //len is value/result 

// Init Socket - Return 0 if OK, Errors: All != 0
int init_udp_server_socket(void) {
	// Filling server information - Common for MSC / GCC
	struct sockaddr_in server_socket;
	memset(&server_socket, 0, sizeof(server_socket));
	server_socket.sin_family = AF_INET;
	server_socket.sin_addr.s_addr = INADDR_ANY;
	server_socket.sin_port = htons(PORT); // host2network

#ifdef _MSC_VER	// MS VS only
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return WSAGetLastError();  // use V2.2
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) return WSAGetLastError();
	if (bind(sockfd, (struct sockaddr*)&server_socket, sizeof(server_socket)) == SOCKET_ERROR) return WSAGetLastError();

	// If iMode = 0, blocking is enabled; 
	// If iMode != 0, non-blocking mode is enabled.
	u_long iMode = 1;
	int res = ioctlsocket(sockfd, FIONBIO, &iMode);
	if (res != NO_ERROR) return WSAGetLastError();

#else // GCC
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return -1; // -1 Invalid (-2: Options)
	if (bind(sockfd, (const struct sockaddr*)&server_socket, sizeof(server_socket)) < 0) return -3; // -3 Bind

	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) return -4; // Make it non-Blocking
#endif
	return 0;
}

// Receivce Data from Socket OK: >0: Return LEN (0-Terminated), else Error
int receive_from_udp_socket(int idx) {
	CLIENT* pcli = &clients[idx];
	pcli->rcv_len = 0;
	memset(&pcli->client_socket, 0, client_sock_len);

#ifdef _MSC_VER	// MS VS
	int rcv_len = recvfrom(sockfd, pcli->rx_buffer, RX_BUFLEN, 0, (struct sockaddr*)&pcli->client_socket, &client_sock_len);
	if (rcv_len == SOCKET_ERROR) {
		int lastError = WSAGetLastError();
		if (lastError == WSAETIMEDOUT) {
			//printf("(Timeout)\n");
			return 0;
		}
		if (lastError == WSAEWOULDBLOCK) {
			//printf("(NoData)\n");
			return 0;
		}
		if (lastError == WSAECONNRESET) {
			//printf("((SocketReset(10054))\n"); // only TCP?
			return 0;
		}
		if (lastError > 0) lastError = -lastError;
		return lastError;
	}
#else // GCC
	int rcv_len = recvfrom(sockfd, pcli->rx_buffer, RX_BUFLEN, 0, (struct sockaddr*)&pcli->client_socket, &client_sock_len);
	if (rcv_len == -1) { // Timeout
		//printf("(Timeout)\n");
		return 0;
	}
	else if (rcv_len < 0) {
		return rcv_len;
	}
#endif
	pcli->rx_buffer[rcv_len] = 0; // Important for Strings!
	pcli->rcv_len = rcv_len;
	return rcv_len;
}


int wait_for_udp_data(void) {
	// Clearing and setting the recieve descriptor
	fd_set recvsds;
	FD_ZERO(&recvsds);
	FD_SET(sockfd, &recvsds);

	struct timeval timeout;
	timeout.tv_sec = IDLE_TIME; // Set Struct each call
	timeout.tv_usec = 0;

#ifdef _MSC_VER	// MS VS
	int selpar0 = 0; /*Parameter ignored on MS VC */
#else // GCC
	int selpar0 = sockfd + 1; /*Parameter only for GCC */
#endif
	int res = select(selpar0, &recvsds, NULL, NULL, &timeout); // See Pocket Guide to TCP/IP Sockets
#ifdef _MSC_VER	// MS VS
	if (res == SOCKET_ERROR) {
		int lastError = WSAGetLastError();
		if (lastError > 0) lastError = -lastError;
		return lastError;
	}
#endif
	return res;
}

// Send Data, if OK: 0, else Error
int send_reply_to_udp_client(int idx, int tx_len) {
	CLIENT* pcli = &clients[idx];
	int res;
#ifdef _MSC_VER	// MS VS
	if (sendto(sockfd, tx_replybuf, tx_len, 0, (struct sockaddr*)&pcli->client_socket, client_sock_len) == SOCKET_ERROR) res = WSAGetLastError();
	else res = 0;
#else // GCC
	res = sendto(sockfd, tx_replybuf, tx_len, 0, (const struct sockaddr*)&pcli->client_socket, client_sock_len);
	if (res == tx_len) res = 0;
#endif
	return res;
}

// Cleanup - No Return
void close_udp_server_socket(void) {
#ifdef _MSC_VER	// MS VS only
	if (sockfd) closesocket(sockfd);
	WSACleanup();
#else // GCC
	if (sockfd) close(sockfd);
#endif
}

// Server-Loop - Return only if Error
int udp_server_loop(void) {
	int wcnt = 0;
	while (1) {
		printf(" Wait(%d) ", wcnt++);
		fflush(stdout);

		int res = wait_for_udp_data();
		if (res < 0) {
			printf("ERROR: 'select()' failed (%d)\n", res);
			return res;
		}
		else if (!res) { // Timeout
			continue;
		}

		int cidx;
		for (cidx = 0; cidx < MAX_CLIENTS; cidx++) {
			CLIENT* pcli = &clients[cidx];
			res = receive_from_udp_socket(cidx);
			if (res < 0) {
				printf("ERROR: ReceiveFrom UDP Server Socket Index[%d] failed (%d)\n", cidx, res);
				return res;
			}
			else if (!res) { // Nothing
				//printf("WARNING: ReceiveFrom UDP Server Socket Index[%d] Nothing (%d)\n", cidx, res);
				break;
			}
			int rcv_len = res;
		}

		printf("Anz. Pakete emmpfangen: %d\n", cidx);

		int ridx;	// Reply-Index
		for (ridx = 0; ridx < cidx; ridx++) {
			CLIENT* pcli = &clients[ridx];

			//print details of the client/peer and the data received
			printf("Rec.Idx[%d] from %s:%d: ", ridx, inet_ntoa(pcli->client_socket.sin_addr), ntohs(pcli->client_socket.sin_port));
			printf("Data[%d]: ", pcli->rcv_len);
			for (int di = 0; di < pcli->rcv_len; di++) {
				uint8_t c = (pcli->rx_buffer[di]) & 255;
				printf("%02X",c);
			}
			printf("\n");

/*
			// Minimum Reply
			time_t ct = time(NULL);
			uint32_t unixsec = (uint32_t)ct;
			sprintf(tx_replybuf, "FF%08X", unixsec);	// FF: plus Time (UTC.u32)
			int tx_len = (int)strlen(tx_replybuf);

			//now reply the client
			res = send_reply_to_udp_client(ridx, tx_len);
			if (res) {
				printf("ERROR: SendTo UDP Client Socket Index[%d] failed (%d)\n", ridx, res);
				break;
			}
*/
			}
	}

}

// --- MAIN ---
int main() {

	printf("--- UDP Server General " __DATE__ " " __TIME__ "\n");

	int res = init_udp_server_socket();
	if (res) {
		printf("ERROR: Init UDP Server Socket failed (%d)\n", res);
		return -1;
	}
	else {

		printf("---Wait on port %d---\n", PORT);
		udp_server_loop();

	}
	close_udp_server_socket();
	return 0;
}
//
