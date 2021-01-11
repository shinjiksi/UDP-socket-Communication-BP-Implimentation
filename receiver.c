    /*
		* Class: CS425 Fall 2020
		* Author: Shinji Kasai
    */

#include"receiver.h"

/* Compare function for qsorting data packets before printing any info to stdout */
void usage() { //error messages
    
    fprintf(stderr, "ERROR: receiver - Unexpect Input");
    fprintf(stderr, "usage: ./receiver < portnumber ex:8080>\n");
    exit(2);                
}

int compare_data_packets(const void * a, const void * b);

bool endTransmission = false;

int main(int argc, char** argv)
{
    // Check args and get the variables we need
    if (argc < 2)
    {
        usage();
       	exit(0);
    }

    // Construct a network endpoint socket
    int endpoint = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (endpoint == -1)
    {
        fprintf(stderr, "Cannot create socket endpoint");
        return -1;
    }

    // Build and initialize a socket address struct for receiver
    int recvPort = atoi(argv[1]);      // Destination’s port number
    struct sockaddr_in recv;           // Socket address struct
    memset(&recv, 0, sizeof recv);     // Initialize members to 0
    recv.sin_family = AF_INET;         // Request IPv4 network
    recv.sin_port = htons(recvPort);   // Port in network byte order
    recv.sin_addr.s_addr = INADDR_ANY; // Accept datagrams from any host
    bind(endpoint, (struct sockaddr *)&recv, sizeof(recv));

    // Initialize sender socket and create a buffer for received packets
    struct sockaddr_in source;                                                            // Sender's address information
    struct BPHeader* receivedPacket = (struct BPHeader *)malloc(sizeof(struct BPHeader)); // Buffer to receive sender’s data
    struct BPHeader* allDataPackets = (struct BPHeader *)malloc(100 * sizeof(struct BPHeader)); // Arbitrary value of 100
    int len;                                                                              // Length of sender’s address
    int recvCode = 1;
    int currentDataIndex = 0;

    // Set receive socket timeout
    struct timeval timeout;
    timeout.tv_sec = receiveTimeout + 1; // Needs to be longer so sender has time to re-send packets if necessary
    timeout.tv_usec = 0;
    if (setsockopt(endpoint, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(timeout)) < 0)
    {
        perror("Error: setsockopt\n");
        exit(1);
    }

    while (1)
    {
        recvCode = recvfrom(endpoint, (struct BPHeader *)receivedPacket, sizeof(struct BPHeader), 0, (struct sockaddr *)&source, (socklen_t *)&len);
        if (recvCode > 0)
        {
            if (receivedPacket->flags.bits.EOM)
            {
                endTransmission = true;
            }
            else 
            {
                // Send an ACK packet back
                struct BPHeader* ackPacket = (struct BPHeader *)malloc(sizeof(struct BPHeader));
                if (receivedPacket->flags.bits.DAT)
                {
                    ackPacket->flags.bits.ACK = 1;
                    ackPacket->acknowledgment = receivedPacket->segment;
                    if (receivedPacket->size > 0)
                    {
                        if ((currentDataIndex + 1) % 100 == 0) 
                        {
                            allDataPackets = (struct BPHeader *)realloc(allDataPackets, (101 + currentDataIndex) * sizeof(struct BPHeader));
                        }
                        allDataPackets[currentDataIndex] = *receivedPacket;
                        currentDataIndex++;
                    }
                }
                // Send window size if requested
                if (receivedPacket->flags.bits.RWA)
                {
                    ackPacket->window = windowsize;
                }
                sendto(
                    endpoint,
                    ackPacket,
                    8, 
                    0,
                    (const struct sockaddr *)&source,
                    sizeof(source)
                );
                free(ackPacket);
            }
        }
        else if (endTransmission)
        {
            // We're done receiving the message, so sort our data packets in segment order and print 'em out!
            qsort(allDataPackets, currentDataIndex, sizeof(struct BPHeader), compare_data_packets);
            int printIndex = 0;
            int previousSegment = -1; 
            for (; printIndex < currentDataIndex; printIndex++)
            {
                if (previousSegment != allDataPackets[printIndex].segment) // Avoid printing any duplicate data segments
                {
                    write(STDOUT_FILENO, allDataPackets[printIndex].data, allDataPackets[printIndex].size);
                    previousSegment = allDataPackets[printIndex].segment;
                }
            }
            return 0;
        }
    }

    return 0;
}

int compare_data_packets(const void* a, const void* b) 
{
    struct BPHeader* packetOne = (struct BPHeader*)a;
    struct BPHeader* packetTwo = (struct BPHeader*)b;

    return (packetOne->segment > packetTwo->segment);
}