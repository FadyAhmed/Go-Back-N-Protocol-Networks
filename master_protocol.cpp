#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "protocol.h"
#include <iostream>
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
#include <vector>
#include <chrono>
#include <ctime>

#define ADDRESS "192.168.1.220"
#define PORT 9821
#define MAX_SEQ 8

unsigned int server_socket;
unsigned int client_socket;

using namespace std;

void start_master()
{
    cout << "Master started\n";
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ADDRESS);
    address.sin_port = htons(PORT);

    bind(server_socket, (struct sockaddr *)&address, sizeof(address));

    listen(server_socket, 1);

    client_socket = accept(server_socket, NULL, NULL);

    cout << "Coonnection accepted\n";
}

bool networkLayerIsEnabled = false;
bool timeoutFlag = false;
bool discardPacket = true;
packet networkLayerBuffer[8];
vector<timer> timers;

static bool between(seq_nr a, seq_nr b, seq_nr c)
{
    /*Return true if a <= b < c circularly; false otherwise.*/
    if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return true;
    else
        return false;
}

void start_timer(seq_nr frame_no, int add)
{
    timer timer;
    timer.frame_nr = frame_no;
    timer.endTime = std::time(NULL) + 3 + add;

    timers.push_back(timer);
}

void stop_timer(seq_nr frame_no)
{
    for (int i = 0; i < timers.size(); i++)
    {
        if (timers[i].frame_nr == frame_no)
        {
            timers.erase(timers.begin() + i);
            return;
        }
    }
}

static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet info)
{
    /*Construct and send a data frame. */
    frame s;          /* scratch variable */
    s.seq = frame_nr; /* insert sequence number into frame */
    s.info = info;    /* insert packet into frame */
    //s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1); /* piggyback ack */
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

void start_time_out()
{
    timeoutFlag = true;
}

void stop_time_out()
{
    timeoutFlag = false;
}

int nextFrameToBeSend = 0;
void from_network_layer(packet *p)
{
    for (char i = 0; i < 8; i++)
    {
        p->data[i] = networkLayerBuffer[nextFrameToBeSend].data[i];
    }
    nextFrameToBeSend = (nextFrameToBeSend + 1) % MAX_SEQ;
}

void to_network_layer(packet *p)
{
    static int m = 0;
    for (char j = 0; j < 8; j++)
        networkLayerBuffer[m].data[j] = p->data[j];
    m = (m + 1) % 8;
}

bool from_physical_layer(frame *r)
{
    if (read(client_socket, r, sizeof(frame)))
    {
        printf("ACK %d Recieved \n", r->seq);
        return true;
    }
    return false;
}

void to_physical_layer(frame *s)
{
    sleep(1);

    write(client_socket, s, sizeof(frame));
    printf("Frame %d Sent\n", s->seq);
}

void enable_network_layer()
{
    networkLayerIsEnabled = true;
}

void disable_network_layer()
{
    networkLayerIsEnabled = false;
}

void check()
{
    //check if any timeout
    for (int i = 0; i < timers.size(); i++)
    {
        time_t now = std::time(NULL);

        if (timers[i].endTime <= now)
        {
            cout << "frame " << timers[i].frame_nr << " time out" << endl;
            //ack_expected = timers[i].frame_nr;
            start_time_out();
            break;
        }
    }
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
    enable_network_layer(); /* allow network layer ready events */
    ack_expected = 0;       /* next ack expected inbound */
    next_frame_to_send = 0; /* next frame going out */
    frame_expected = 0;     /* number of frame expected inbound */
    nbuffered = 0;          /* initially no packets are buffered */

    while (true)
    {
        check();

        wait_for_event(&event); /* four possibilities: see event type above */
        switch (event)
        {
        case network_layer_ready: /* the network layer has a packet to send */
            /*Accept, save, and transmit a new frame.*/
            from_network_layer(&buffer[next_frame_to_send]);                           /* fetch new packet */
            nbuffered++;                                                               /* expand the sender's window */
            send_data(next_frame_to_send, frame_expected, buffer[next_frame_to_send]); /* transmit the frame */
            start_timer(next_frame_to_send, frame_expected * 3);                       /* start the timer running */
            inc(next_frame_to_send);                                                   /* advance sender's upper window edge */
            disable_network_layer();
            break;
        case frame_arrival: /* a data or control frame has arrived */
            sleep(2);

            if (from_physical_layer(&r))
            {
                /* get incoming frame from physical layer */
                if (r.seq == frame_expected)
                {
                    /*Frames are accepted only in order. */
                    to_network_layer(&r.info); /* pass packet to network layer */
                    inc(frame_expected);       /* advance lower edge of receiver's window */
                }
                /*Ack n implies n − 1, n − 2, etc.Check for this.*/
                while (between(ack_expected, r.ack, next_frame_to_send))
                {
                    /*Handle piggybacked ack. */
                    nbuffered = nbuffered - 1; /* one frame fewer buffered */
                    stop_timer(ack_expected);  /* frame arrived intact; stop timer */
                    inc(ack_expected);         /* contract sender's window */
                }
                if (timeoutFlag != 1)
                    enable_network_layer();
            }
            break;

        case cksum_err:
            break;                             /* just ignore bad frames */
        case timeout:                          /* trouble; retransmit all outstanding frames */
            next_frame_to_send = ack_expected; /* start retransmitting here */
            for (i = 1; i <= nbuffered; i++)
            {
                stop_timer(next_frame_to_send);
                send_data(next_frame_to_send, frame_expected, buffer[next_frame_to_send]); /* resend frame */
                start_timer(next_frame_to_send, frame_expected * 3);                       /* start the timer running */
                inc(next_frame_to_send);                                                   /* prepare to send the next one */
            }
            stop_time_out();
        }

        if (nbuffered < 2)
            enable_network_layer();
        else
        {
            disable_network_layer();
        }
    }
}

int main()
{
    start_master();

    // add data to network layer buffer
    networkLayerBuffer[0] = {1, 2, 3, 4, 5, 6, 7, 8};
    networkLayerBuffer[1] = {2, 10, 11, 12, 13, 14, 15, 16};
    networkLayerBuffer[2] = {3, 18, 19, 20, 21, 22, 23, 24};
    networkLayerBuffer[3] = {4, 18, 19, 20, 21, 22, 23, 24};
    networkLayerBuffer[4] = {5, 18, 19, 20, 21, 22, 23, 24};
    networkLayerBuffer[5] = {6, 18, 19, 20, 21, 22, 23, 24};
    networkLayerBuffer[5] = {7, 18, 19, 20, 21, 22, 23, 24};
    networkLayerBuffer[7] = {8, 18, 19, 20, 21, 22, 23, 24};

    // run protocol 5
    protocol();

    return 0;
}