#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>

#define ADDRESS "192.168.1.220"
#define PORT 9821
#define MAX_SEQ 1

bool networkLayerIsEnabled = false;
bool timeoutFlag = false;
packet networkLayerBuffer[8];

unsigned int client_socket;

using namespace std;

void start_slave()
{
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ADDRESS);
    address.sin_port = htons(PORT);

    connect(client_socket, (struct sockaddr *)&address, sizeof(address));
}

static bool between(seq_nr a, seq_nr b, seq_nr c)
{
    /*Return true if a <= b < c circularly; false otherwise.*/
    if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return true;
    else
        return false;
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer)
{
    /*Construct and send a data frame. */
    frame s;               /* scratch variable */
    s.info = buffer;       /* insert packet into frame */
    s.seq = frame_nr;      /* insert sequence number into frame */
    s.ack = frame_nr;      /* piggyback ack */
    to_physical_layer(&s); /* transmit the frame */
}

void wait_for_event(event_type *event)
{
    if (networkLayerIsEnabled == true)
    {
        *event = network_layer_ready;
    }
    else if (timeoutFlag == true)
    {
        *event = timeout;
    }
    else
    {
        *event = frame_arrival;
    }
}

void from_network_layer(packet *p)
{
    // TODO: remove j
    // j is to loop through
    char j = 0;
    for (char i = 0; i < 8; i++)
        p->data[i] = networkLayerBuffer[j].data[i];

    j = (j + 1) % 8;
}

int frameRecievedNum = 0;
void to_network_layer(packet *p)
{
    for (int j = 0; j < 8; j++)
        networkLayerBuffer[frameRecievedNum].data[j] = p->data[j];
    frameRecievedNum = (frameRecievedNum + 1) % 4;
}

bool discardPacket = true;

bool from_physical_layer(frame *r)
{
    read(client_socket, r, sizeof(frame));

    printf("Frame %d Recieved\n", r->seq);
    return 1;
}

void to_physical_layer(frame *s)
{
    write(client_socket, s, sizeof(frame));
    printf("ACK %d Sent\n", s->seq);
}

void enable_network_layer()
{
    networkLayerIsEnabled = true;
}

void disable_network_layer()
{
    networkLayerIsEnabled = false;
}

void protocol()
{
    seq_nr next_frame_to_send;  /* MAX SEQ > 1; used for outbound stream */
    seq_nr ack_expected;        /* oldest frame as yet unacknowledged */
    seq_nr frame_expected;      /* next frame expected on inbound stream */
    frame r;                    /* scratch variable */
    packet buffer[MAX_SEQ + 1]; /* buffers for the outbound stream */
    seq_nr nbuffered;           /* number of output buffers currently in use */
    seq_nr i;                   /* used to index into the buffer array */
    event_type event;
    ack_expected = 0;       /* next ack expected inbound */
    next_frame_to_send = 0; /* next frame going out */
    frame_expected = 0;     /* number of frame expected inbound */
    nbuffered = 0;          /* initially no packets are buffered */

    while (true)
    {
        // switch (event)
        // {
        // case network_layer_ready: /* the network layer has a packet to send */
        //     /*Accept, save, and transmit a new frame.*/
        //     from_network_layer(&buffer[next_frame_to_send]);                       /* fetch new packet */
        //     nbuffered = nbuffered + 1;                                             /* expand the sender's window */
        //     send_data(next_frame_to_send, frame_expected, buffer[frame_expected]); /* transmit the frame */
        //     inc(next_frame_to_send);                                               /* advance sender's upper window edge */
        //     disable_network_layer();
        //     break;
        // case frame_arrival:
        /* a data or control frame has arrived */
        from_physical_layer(&r); /* get incoming frame from physical layer */

        if (r.seq == frame_expected)
        {
            /*Frames are accepted only in order. */

            // to_network_layer(&r.info); /* pass packet to network layer */
            if (r.seq == 1 && discardPacket)
            {
                discardPacket = false;
            }
            else
            {
                send_data(frame_expected, frame_expected, networkLayerBuffer[frame_expected]); /* ack */
                frame_expected++;
            }
        }
        /*Ack n implies n − 1, n − 2, etc.Check for this.*/
        // while (between(ack_expected, r.ack, next_frame_to_send))
        // {
        //     /* Handle piggybacked ack. */
        //     nbuffered = nbuffered - 1; /* one frame fewer buffered */
        //     inc(ack_expected);         /* contract sender’s window */
        // }
        //}
    }
}

int main()
{

    start_slave();

    //TODO: create thread

    // add data to network layer buffer

    // run protocol 5
    protocol();

    return 0;
}