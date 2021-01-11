  /*
		* Class: CS425 Fall 2020
		* Author: Shinji Kasai
    */
#include "stdbool.h"
#include <stdio.h> 
#include <unistd.h> 
#include <sys/time.h>   
#include <sys/queue.h>  
#include <stdlib.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <signal.h>    
#include <unistd.h>
#include <sys/types.h> 
#include "stdint.h"
#include <netinet/in.h> 

#define maxChar 512
#define windowSize 255
#define receiverTimeout 10
  
  
void usage() { //error messages
    fprintf(stderr, "\n\nUSAGE for receiver: ./receiver 'portnumber'\n\n");
    fprintf(stderr, "Example for receiver  : ./receiver '8080'\n\n");
    fprintf(stderr, "USAGE for seander     : ./sender 'IP Adresse' 'portnumber' < filename\n\n");
    fprintf(stderr, "Example for sender    : ./sender 127.0.0.1 8080 < testin1\n\n");
    exit(2);                
}
int checkDataPac(const void * a, const void * b);
bool endTransmission = false; 

// unsigned int DAT:1;
// uiunsigned int8_t ACK:1;
// unsigned int RWA:1;
// unsigned int EOM:1;

struct BPHeader{
    uint16_t segment,acknowledgment, size;
    uint8_t window;
    union{
        uint8_t byte;
        struct{ uint8_t DAT:1,ACK:1,RWA:1,EOM:1; } bits;
    } flags;
    char data[512];
};

int checkDataPac(const void* a, const void* b) {
    struct BPHeader* packetOne = (struct BPHeader*)a;
    struct BPHeader* packetTwo = (struct BPHeader*)b;
    return (packetOne->segment > packetTwo->segment);
}

int main(int argc, char** argv)
{
 
    int receiverPort = atoi(argv[1]);    
    struct sockaddr_in receiver, sender;        // sender and receiver addres
    if (argc < 2) {
        usage();
        return -1;
    }

    int endpoint = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (endpoint == -1){
        fprintf(stderr, "ERROR: Faild to Create s Socket Endpoint");
        return -1;
    }

    memset(&sender, 0, sizeof sender);         
    sender.sin_family = AF_INET;                
    sender.sin_port = htons(0);                 
    sender.sin_addr.s_addr = htonl(INADDR_ANY); 

    memset(&receiver, 0, sizeof receiver);    
    receiver.sin_family = AF_INET;        
    receiver.sin_port = htons(receiverPort);  
    receiver.sin_addr.s_addr = INADDR_ANY; 
    bind(endpoint, (struct sockaddr *)&receiver, sizeof(receiver));

    int senderAddrLength;
    int receiverC = 1;
    int dataIndex = 0;
    struct BPHeader* receivedPacket = (struct BPHeader *)malloc(sizeof(struct BPHeader));
    struct BPHeader* allDataPackets = (struct BPHeader *)malloc(100 * sizeof(struct BPHeader));

    struct timeval timeout;
    timeout.tv_sec = receiverTimeout + 1;
    timeout.tv_usec = 0;
    if (setsockopt(endpoint, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0)
        exit(1);

    while (1){
        receiverC = recvfrom(endpoint, (struct BPHeader *)receivedPacket, 
        sizeof(struct BPHeader), 0, (struct sockaddr *)&sender, (socklen_t *)&senderAddrLength);
        if (receiverC > 0) {
            if (receivedPacket->flags.bits.EOM)
                endTransmission = true;
            
            else {
                struct BPHeader* ackPacket = (struct BPHeader *)malloc(sizeof(struct BPHeader));
                if (receivedPacket->flags.bits.DAT){
                    ackPacket->flags.bits.ACK = 1;
                    ackPacket->acknowledgment = receivedPacket->segment;
                    if (receivedPacket->size > 0){
                        if ((dataIndex + 1) % 100 == 0) {
                            allDataPackets = (struct BPHeader *)realloc(allDataPackets, 
                                (101 + dataIndex) * sizeof(struct BPHeader));
                        }
                        allDataPackets[dataIndex] = *receivedPacket;
                        dataIndex++;
                    }
                }
                if (receivedPacket->flags.bits.RWA)
                    ackPacket->window = windowSize;
                
                sendto(
                    endpoint, ackPacket, 8, 0, (const struct sockaddr *)&sender, sizeof(sender)
                );
                free(ackPacket);
            }
        }
        else if (endTransmission){
            qsort(allDataPackets, dataIndex, sizeof(struct BPHeader), checkDataPac);
            int pIndex = 0;
            int lastSegment = -1; 
            for (; pIndex < dataIndex; pIndex++){
                if (lastSegment != allDataPackets[pIndex].segment) {
                    write(STDOUT_FILENO, allDataPackets[pIndex].data, 
                        allDataPackets[pIndex].size);
                    lastSegment = allDataPackets[pIndex].segment;
                }
            }
                 fprintf(stderr, "\nSuccessfly Received All Packets from the Sender\n");
            
               // printf("Socket Closed. \n\n");
            return 0;
        }
    }
    return 0;
}

