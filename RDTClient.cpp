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
#include <map>
#define SERVERPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 100
#define PACKET_SIZE 500

using namespace std;
set<uint32_t> rec_packet_pool ;
set<uint32_t> rec_selective_repeat ;
map<uint32_t,packet> buffer ;
int seed = 5 ;

/**
    this function to calculate the probability of receiving an acknowledgment
*/
bool probability_recieve()
{
    int p= (rand() % 100) + 1;
    cout<<"probability of receive "<<p<<endl;
    return p > seed ;
}

/**
    this function to receive a requested file by using send and wait approach
    @param file_name the name of file will be received
    @param sockfd the socket number of the server
    @param *p the address information of the server
*/
void receive_file_send_and_wait(string file_name, int sockfd,struct addrinfo *p)
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


int white_file_selective_repeat(ofstream myfile,string file_name,int min)
{


    return min;
}
/**
    this function to receive a requested file by using selective approach
    @param file_name the name of file will be received
    @param sockfd the socket number of the server
    @param *p the address information of the server
*/
void receive_file_selective_repeat(string file_name, int sockfd,struct addrinfo *p)
{
    ofstream myfile;
    myfile.open (file_name);

    int min = 0;
    int size = PACKET_SIZE;
    int numbytes;
    bool first_time= true ;

    while(1)
    {
        first_time = false;
        struct packet pack;

        timeval timeout = { 5, 0 };

        fd_set in_set;

        FD_ZERO(&in_set);
        FD_SET(sockfd, &in_set);

        // select the set
        int cnt = select(sockfd + 1, &in_set, NULL, NULL, &timeout);

        if (FD_ISSET(sockfd, &in_set))
        {
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
                rec_packet_pool.insert(pack.seqno);
                rec_selective_repeat.insert(pack.seqno);
                buffer.insert(std::pair<uint32_t,packet> (pack.seqno,pack));

                // need to put in function

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

            while(1)
            {
                const bool found = rec_selective_repeat.find(min) != rec_selective_repeat.end();

                if(found)
                {
                    struct packet pk = buffer[min++];
                    cout<<"packNUm*************: "<<pk.seqno<<"min : "<<min-1<<endl;
                    buffer.erase(pk.seqno);
                    for(int i = 0 ; i < pk.len ; i++)
                        myfile<<pk.data[i];
                }
                else
                    break;
            }
        }else{
            break;
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
    if ((numbytes = sendto(sockfd, "mark2.jpeg", strlen("mark2.jpeg"), 0,
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

    printf("talker: rec %s \n",buf);

    receive_file_selective_repeat("mark2.jpeg",sockfd,p);

    freeaddrinfo(servinfo);

    close(sockfd);
    return 0;
}
