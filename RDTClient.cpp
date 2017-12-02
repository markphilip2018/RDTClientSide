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
#include <sstream>
#include <map>
#define MAXBUFLEN 100
#define PACKET_SIZE 500

using namespace std;
set<uint32_t> rec_packet_pool ;
set<uint32_t> rec_selective_repeat ;
map<uint32_t,packet> buffer ;

string SERVER_PORT  = "4950";
string SERVER_IP_ADDR ="127.0.0.1";
string FILE_NAME = "simp.png";
int seed =5 ;
int algo_type = 1;

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
    time_t start  = time(NULL);
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
        uint16_t check_sum = 0;
        size = pack.len;
        for(int i = 0 ; i < size ; i++)
                check_sum+=pack.data[i];

        check_sum += pack.cksum;
        cout<< "checksum : "<<pack.cksum<<endl;
        cout<<"-------------------------------------------------------: "<< check_sum<<endl;
        if(check_sum != 65535){
                cout<<"**************************************************: "<< check_sum<<endl;
            continue;
        }

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
        cout<< "Duration"<<time(NULL) - start <<endl;
    }

    myfile.close();
    cout<< "Duration"<<time(NULL) - start <<endl;

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

    time_t start  = time(NULL);
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
        }
        else
        {
            break;
        }
    }
    myfile.close();
    cout<< "Duration"<<time(NULL) - start <<endl;

}

//*******************************************************
/**
    this function to receive a requested file by using go back n approach
    @param file_name the name of file will be received
    @param sockfd the socket number of the server
    @param *p the address information of the server
*/
void receive_file_go_back_n(string file_name, int sockfd,struct addrinfo *p)
{
    ofstream myfile;
    myfile.open (file_name);

    int min = 0;
    int size = PACKET_SIZE;
    int numbytes;
    bool first_time= true ;

    time_t start  = time(NULL);
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

            }
            else
            {
                cout<<"dup ack of packet"<<pack.seqno<<endl;
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


            cout << "acknowledge num : "<<min-1<<endl;


            struct ack_packet acknowledgement;
            acknowledgement.ackno = min-1;

            if ((numbytes = sendto(sockfd,(struct ack_packet*)&acknowledgement, sizeof(acknowledgement), 0,
                                   p->ai_addr, p->ai_addrlen)) == -1)
            {
                perror("talker: sendto");
                exit(1);
            }


        }
        else
        {
            break;
        }
    }
    myfile.close();
    cout<< "Duration"<<time(NULL) - start <<endl;

}

void parse_args()
{

    int init_size;
    string line;
    ifstream infile("client.in");

    //get IP address
    getline(infile, line);
    if(line.compare("null") != 0)
       SERVER_IP_ADDR = line;

    // get port number of server
    getline(infile, line);
    if(line.compare("null") != 0)
       SERVER_PORT = line;

    // get file name
    getline(infile, line);
    if(line.compare("null") != 0)
        FILE_NAME = line ;

    // get initial size
    getline(infile, line);
    if(line.compare("null") != 0)
        init_size = atoi(line.c_str()) ;

    // get probability seed
    getline(infile, line);
    if(line.compare("null") != 0)
        seed = (int)(atof(line.c_str())*100);

    // algo type
    getline(infile, line);
    if(line.compare("null") != 0)
        algo_type = atoi(line.c_str());


    cout<<"IP : "<<SERVER_IP_ADDR<<endl;
    cout<<"PORT : "<<SERVER_PORT<<endl;
    cout<<"FILE : "<<FILE_NAME<<endl;
    cout<<"Init window size : "<<init_size<<endl;
    cout<<"seed : "<<seed<<endl ;
    cout<<"Algo : "<<algo_type<<endl ;

}

int main()
{
    parse_args();
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(SERVER_IP_ADDR.c_str(), SERVER_PORT.c_str(), &hints, &servinfo)) != 0)
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
    if ((numbytes = sendto(sockfd, FILE_NAME.c_str(), strlen(FILE_NAME.c_str()), 0,
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



    if(algo_type == 1)
    {
        receive_file_send_and_wait(FILE_NAME,sockfd,p);
    }
    else if(algo_type == 2)
    {
        receive_file_selective_repeat(FILE_NAME,sockfd,p);
    }
    else
        receive_file_go_back_n(FILE_NAME,sockfd,p);

    freeaddrinfo(servinfo);

    close(sockfd);
    return 0;
}
