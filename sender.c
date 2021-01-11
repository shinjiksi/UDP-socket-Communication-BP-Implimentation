  /*
		* * Class: CS425 Fall 2020
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

bool endTransmission = false;

struct BPHeader* createPacket(void);

uint8_t receiverWindowSize = 0;
uint16_t sequenceNumber = 0;

int endpoint;
struct sockaddr_in receiver, sender;       
struct itimerval timer;
struct in_addr receiverIP,senderIP;
void sendsPacket(void);
void resendsPackets(void);
void receivesPacket(void);
void insertsQueue(struct BPHeader* packet);
void deletesQueue(uint16_t acknowledgement);

typedef struct node {
    SLIST_ENTRY(node) pointers;
    struct BPHeader* packet;
} 

node;
SLIST_HEAD(slisthead, node);
struct slisthead listHead;
node* currentNode;
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

int main(int argc, char** argv){
    if (argc < 2){
        usage();
        return -1;
    }
    int receiverPort = atoi(argv[2]); 

    endpoint = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (endpoint == -1){
        fprintf(stderr, "ERROR: Faild to create a Socket Endpoint");
        return -1;
    }

    if (inet_aton(argv[1], &receiverIP) < 0)
        return -1;
    

    memset(&sender, 0, sizeof sender);         
    sender.sin_family = AF_INET;                
    sender.sin_port = htons(0);                 
    sender.sin_addr.s_addr = htonl(INADDR_ANY); 
   
    memset(&receiver, 0, sizeof receiver);   
    receiver.sin_family = AF_INET;       
    receiver.sin_port = htons(receiverPort); 
    receiver.sin_addr = receiverIP;          

    int connectToReceiver = bind(endpoint, (struct sockaddr *)&sender, sizeof(sender));

    if (connectToReceiver < 0)
        perror("ERROR: Sender Faild to Connect to Receiver");
    struct timeval timeout;
    timeout.tv_sec = receiverTimeout;
    timeout.tv_usec = 0;

    if (setsockopt(endpoint, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0)
        exit(1);

    currentNode = malloc(sizeof(node));

    while (!endTransmission){
        if (receiverWindowSize > 0)
            sendsPacket();
        
        else if (receiverWindowSize == 0){
            sendsPacket();
            receivesPacket();
        }
    }

    while (!SLIST_EMPTY(&listHead)){
        receivesPacket();
    }
    printf("\nAll Packets has Sent.\n");
    printf("Socket Closed.\n\n");
    return 0;
}

struct BPHeader* createPacket(void){// Bronco protocol header here
    struct BPHeader* packet = (struct BPHeader *)malloc(sizeof(struct BPHeader));
   
    if (receiverWindowSize == 0)
        packet->flags.bits.RWA = 1;
    
    else{
        int readByte = read(STDIN_FILENO, packet->data, 512);
        if (readByte > 0){
            packet->flags.bits.DAT = 1;
            packet->segment = sequenceNumber;
            sequenceNumber++;
            packet->size = strlen(packet->data);
            if (packet->size > 512)
                packet->size = 512;
            receiverWindowSize--;
        }
        else  { //emo
            packet->flags.bits.EOM = 1;
            endTransmission = true;
        }
    }
    return packet;
}

void sendsPacket(void){
    struct BPHeader* outPacket = createPacket();
    sendto(
        endpoint,
        outPacket,
        8 + outPacket->size,
        0,
        (const struct sockaddr *)&receiver,
        sizeof(receiver)
    );

    if (outPacket->flags.bits.RWA)
        printf("Waiting for Receiver to Response...\n");
    
    else if (outPacket->flags.bits.DAT){
        printf("Data packet was sent. \nSegment Number: %d \nPayload Size: %d\n\n", outPacket->segment, outPacket->size);
        insertsQueue(outPacket);
    }
    else if (outPacket->flags.bits.EOM)
        printf("EOM was Sent\n\n");
    
}

void receivesPacket(void) {
    struct BPHeader *receivesPacket = (struct BPHeader *)malloc(sizeof(struct BPHeader));
    int receiveCode = 0;
    int length;
    receiveCode = recvfrom(endpoint, (struct BPHeader *)receivesPacket, sizeof(struct BPHeader), 0, (struct sockaddr *)&sender, (socklen_t *)&length);
    if (receiveCode > 0){
        if (receivesPacket->window > 0 && receiverWindowSize == 0) {
            receiverWindowSize = receivesPacket->window;
            printf("\nThe window size is: %d\n\n", receivesPacket->window);
        }
        if (receivesPacket->flags.bits.ACK){
            printf("ACK_segment Number: %d\n", receivesPacket->acknowledgment);
            deletesQueue(receivesPacket->acknowledgment);
        }
    }
    else{
        if (receiverWindowSize != 0)  
            resendsPackets();
    }
}

void resendsPackets() {
    fprintf(stderr, "Resending Lost Packets...\n");
    node* nodePtr;
    SLIST_FOREACH(nodePtr, &listHead, pointers) {
        if (receiverWindowSize == 0) 
            break;
        sendto(endpoint, nodePtr->packet, sizeof(struct BPHeader), 0, (const struct sockaddr *) &receiver, sizeof(receiver));
        receiverWindowSize--;
    }
}
//-------------------QUEUE----------------------
void insertsQueue(struct BPHeader* packet){
    node* nodePtr = malloc(sizeof(node));
    nodePtr->packet = packet;

    if (SLIST_EMPTY(&listHead)) 
        SLIST_INSERT_HEAD(&listHead, nodePtr, pointers);
    
    else
        SLIST_INSERT_AFTER(currentNode, nodePtr, pointers);
    
    currentNode = nodePtr;
}

void deletesQueue(uint16_t acknowledgement) {
    node* nodePtr;
    while(!SLIST_EMPTY(&listHead)) {
        nodePtr = SLIST_FIRST(&listHead);
        if (nodePtr->packet->segment > acknowledgement)  
            break;

        else 
            SLIST_REMOVE_HEAD(&listHead, pointers);
            free(nodePtr->packet);
            free(nodePtr);
        
    }
}

