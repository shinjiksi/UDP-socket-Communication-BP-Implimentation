

#include "sender.h"

#include "bp.h"
#include "network.h"

bool endTransmission = false;
uint8_t receiverWindowSize = 0;
uint16_t sequenceNumber = 0;

// Receiver socket and timeout
int endpoint;
struct sockaddr_in dest;
struct sockaddr_in sender;
struct itimerval timer;

// Queue
typedef struct ListNode {
    SLIST_ENTRY(ListNode) pointers;
    struct BPHeader* packet;
} ListNode;
SLIST_HEAD(slisthead, ListNode);
struct slisthead listHead;
ListNode* currentQueueNode;

int main(int argc, char** argv)
{
    // Check args and get the variables we need
    if (argc < 2)
    {
        fprintf(stderr, "Usage: sender IPAddressString UDPPort\n");
        return -1;
    }
    int destPort = atoi(argv[2]); // Destination’s port number

    // Construct a network endpoint socket
    endpoint = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (endpoint == -1)
    {
        fprintf(stderr, "Cannot create socket endpoint");
        return -1;
    }

    // Convert IP address string to network byte order
    struct in_addr destIP;
    if (inet_aton(argv[1], &destIP) < 0)
    {
        fprintf(stderr, "Error -- destination IPv4 address");
        return -1;
    }

    // Build and initialize a socket address structure for the destination
    memset(&dest, 0, sizeof dest);   // Pre-init members of automatic struct dest
    dest.sin_family = AF_INET;       // IPv4
    dest.sin_port = htons(destPort); // Port number in network byte order
    dest.sin_addr = destIP;          // Destination host's IP Address

    // Build and initialize a socket address structure for the sender
    memset(&sender, 0, sizeof sender);          // Zero-out uninitialized struct fields
    sender.sin_family = AF_INET;                // IPv4
    sender.sin_port = htons(0);                 // Let OS choose the port (don't use destPort!!!)
    sender.sin_addr.s_addr = htonl(INADDR_ANY); // Let OS choose source IP address
    int err = bind(endpoint, (struct sockaddr *)&sender, sizeof(sender));
    if (err < 0)
        perror("sender socket bind failed.");
    
    // Set receive socket timeout
    struct timeval timeout;
    timeout.tv_sec = RECV_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    if (setsockopt(endpoint, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0)
    {
        perror("Error: setsockopt\n");
        exit(1);
    }

    // Start queue
    currentQueueNode = malloc(sizeof(ListNode));

    while (!endTransmission)
    {
        if (receiverWindowSize > 0)
        {
            sendPacket();
        }
        else if (receiverWindowSize == 0)
        {
            sendPacket();
            receivePacket();
        }
    }

    while (!SLIST_EMPTY(&listHead))
    {
        receivePacket();
    }

    printf("All packets acknowledged! Closing sender...\n");
    return 0;
}

struct BPHeader* createPacket(void)
{
    struct BPHeader* packet = (struct BPHeader *)malloc(sizeof(struct BPHeader));
    // RWA packet
    if (receiverWindowSize == 0)
    {
        packet->flags.bits.RWA = 1;
    }
    else
    {
        // Data packet
        int bytesRead = read(STDIN_FILENO, packet->data, 512);
        if (bytesRead > 0)
        {
            packet->flags.bits.DAT = 1;
            packet->segment = sequenceNumber;
            // Sequence number will wrap back to zero if it exceeds max unsigned 16 bit value since its uint16_t
            // Note that this means future acknowledgements will also wrap
            sequenceNumber++;
            packet->size = strlen(packet->data);
            if (packet->size > 512)
                packet->size = 512;
            receiverWindowSize--;
        }
        // EOM packet
        else
        {
            packet->flags.bits.EOM = 1;
            endTransmission = true;
        }
    }
    return packet;
}

void sendPacket(void)
{
    struct BPHeader* outPacket = createPacket();
    sendto(
        endpoint,
        outPacket,
        8 + outPacket->size,
        0,
        (const struct sockaddr *)&dest,
        sizeof(dest)
    );
    
    // Log sent packet and any important info about it
    if (outPacket->flags.bits.RWA)
    {
        printf("Window size request packet sent.\n\n");
    }
    else if (outPacket->flags.bits.DAT)
    {
        printf("Data packet sent, segment: %d , payload size (bytes): %d\n\n", outPacket->segment, outPacket->size);
        insertQueue(outPacket);
    }
    else if (outPacket->flags.bits.EOM)
    {
        printf("End of message packet sent! Waiting for any remaining acknowledgements...\n\n");
    }
}

void receivePacket(void) 
{
    // Receive acknowledgement and window packets
    struct BPHeader *receivePacket = (struct BPHeader *)malloc(sizeof(struct BPHeader));
    int recvCode = 0;
    int len;
    recvCode = recvfrom(endpoint, (struct BPHeader *)receivePacket, sizeof(struct BPHeader), 0, (struct sockaddr *)&sender, (socklen_t *)&len);
    if (recvCode > 0)
    {
        if (receivePacket->window > 0 && receiverWindowSize == 0)
        {
            receiverWindowSize = receivePacket->window;
            printf("Window packet acknowledged, new window size is: %d\n\n", receivePacket->window);
        }
        if (receivePacket->flags.bits.ACK)
        {
            printf("Data packet acknowledged, segment: %d\n\n", receivePacket->acknowledgment);
            clearQueue(receivePacket->acknowledgment);
        }
    }
    else
    {
        if (receiverWindowSize != 0)
        {
            resendLostPackets();
        }
    }
}

void resendLostPackets() 
{
    fprintf(stderr, "No acknowledgement received. Re-sending unacknowledged packets.\n");
    ListNode* np;
    SLIST_FOREACH(np, &listHead, pointers) 
    {
        if (receiverWindowSize == 0) 
            break;
        sendto(endpoint, np->packet, sizeof(struct BPHeader), 0, (const struct sockaddr *) &dest, sizeof(dest));
        fprintf(stderr,"Data packet re-sent, segment: %d , payload size (bytes): %d\n\n",np->packet->segment, np->packet->size);
        receiverWindowSize--;
    }
}

void insertQueue(struct BPHeader* packet)
{
    ListNode* newNode = malloc(sizeof(ListNode));
    newNode->packet = packet;

    if (SLIST_EMPTY(&listHead))
    {
        SLIST_INSERT_HEAD(&listHead, newNode, pointers);
    }
    else
    {
        SLIST_INSERT_AFTER(currentQueueNode, newNode, pointers);
    }
    currentQueueNode = newNode;
}

void clearQueue(uint16_t acknowledgement) {
    ListNode* currentNode;
    while(!SLIST_EMPTY(&listHead)) {
        currentNode = SLIST_FIRST(&listHead);
        if (currentNode->packet->segment > acknowledgement) 
        {
            break;
        }
        else
        {
            SLIST_REMOVE_HEAD(&listHead, pointers);
            free(currentNode->packet);
            free(currentNode);
        }
    }
}