#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "packet_struct.h"
#include <iostream>
#include <fstream>
#include <set>

#define SERVERPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 100
#define PACKET_SIZE 500

using namespace std;
set<uint32_t> rec_packet_pool ;
int seed = 5 ;

bool probability_recieve(){
    int p= (rand() % 100) + 1;
    cout<<"probability of receive "<<p<<endl;
    return p > seed ;
}
void receive_file(string file_name, int sockfd,struct addrinfo *p)
{

    ofstream myfile;

    myfile.open (file_name);

    int size = PACKET_SIZE;
    int numbytes;
    bool first_time= true ;
    while(size == PACKET_SIZE)
    {

            struct packet pack;
            if ((numbytes = recvfrom(sockfd, (struct packet*)&pack, sizeof(pack), 0,
                                     p->ai_addr, &p->ai_addrlen)) == -1)
            {
                perror("recvfrom");
                exit(1);
            }
        if(!probability_recieve())
        {
            continue ;
        }
        size = pack.len;
        const bool is_in = rec_packet_pool.find(pack.seqno) != rec_packet_pool.end();
        if(!is_in)
        {
            for(int i = 0 ; i < size ; i++)
                myfile<<pack.data[i];

            rec_packet_pool.insert(pack.seqno);
        }
        else
        {
            cout<<"dup ack of packet"<<pack.seqno<<endl;
        }




        cout << "pack num : "<<pack.seqno<<" with length : "<<pack.len<<endl;


        struct ack_packet acknowledgement;
        acknowledgement.ackno = pack.seqno;

        if ((numbytes = sendto(sockfd,(struct ack_packet*)&acknowledgement, sizeof(acknowledgement), 0,
                               p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
    }

    myfile.close();

}
int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    /* if (argc != 3)
     {
         fprintf(stderr,"usage: talker hostname message\n");
         exit(1);
     }*/
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo("127.0.0.1", SERVERPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
// loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    if ((numbytes = sendto(sockfd, "simp.png", strlen(argv[2]), 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }
    char buf[MAXBUFLEN];
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,
                             p->ai_addr, &p->ai_addrlen)) == -1)
    {
        perror("recvfrom");
        exit(1);
    }

    printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    printf("talker: rec %s \n",buf);

    receive_file("simp.png",sockfd,p);

    freeaddrinfo(servinfo);

    //printf("talker: rec22222 %s \n",pack.data);
    close(sockfd);
    return 0;
}
