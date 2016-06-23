/*
 *  === cod_udp.c ===
 *
    Legge/Scrive le variabili dichiarate in Codesys come Globals
    su UDP verso la memoria Condivisa EPI

    utilizza la libreria "libmemory.so"
*/

#include <stdio.h>      //printf
#include <string.h>     //memset
#include <stdlib.h>     //exit(0);
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "codudp.h"

//typedef unsigned int DWORD;


// ----------------------------------------------------------
// Socket e ID del Server e Socket del Client Codesys da cui riceviamo i dati
struct sockaddr_in sock_srv, sock_sender;
int udp_server = 0;
// Socket e ID del Client usato per inviare i Dati a Codesys
struct sockaddr_in sock_cli;
int udp_client = 0;
// 
int ServerPort;
int ClientPort;

char UdpErrors[512];

#ifdef RETENTIVEMEMENABLE        
struct sockaddr_in retsock_srv, retsock_sender, retsock_cli;
int retudp_server = 0;
int retudp_client = 0;
int retServerPort;
int retClientPort;
#endif        

//
// ======================================================================
//

// ------------------------------------
// Rende Buffer con eventuale Errore
//
char *GetUdpErrors()
{return UdpErrors;}

// ----------------------------------------
// Set della comunicazione Udp :
// - Utilizza 'Port' come porta del Server (ricezione)
// - il valore 'Port+1' come porta del Client
// - 'pCliIP' e' l'indirizzo del destinatario (trasmissione)
//
int SetCodUdp(int Port, char *pCliIP)
{
    int err = 0;
    // Clear segnalazione errori
    UdpErrors[0] = '\0';

    // -----------------------------------------------------------------
    // Crea un Server UDP
    //
    ServerPort = Port;
    if ((udp_server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror(UdpErrors);
        return -1;
    }

    // zero out the structure
    memset((char *) &sock_srv, 0, sizeof(sock_srv));

    // set the structure
    sock_srv.sin_family = AF_INET;
    sock_srv.sin_port = htons(ServerPort);
    sock_srv.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if( bind(udp_server , (struct sockaddr*)&sock_srv, sizeof(sock_srv) ) == -1)
    {
        if  (errno == EADDRINUSE);
            err |= 0x100;
        perror(UdpErrors);
        err = 1;
    }

    // -----------------------------------------------------------------
    // Crea un Client UDP
    //
    ClientPort = Port + 1;
    if ( (udp_client=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror(UdpErrors);
        err |= 2;
    }

    memset((char *) &sock_cli, 0, sizeof(sock_cli));
    sock_cli.sin_family = AF_INET;
    sock_cli.sin_port = htons(ClientPort);

    if (inet_aton(pCliIP , &sock_cli.sin_addr) == 0)
    {
        perror(UdpErrors);
        err |= 2;
    }

    return err;
}


// --------------------------------------
// Chiude socket Server o Client
// se 'srvflag' o 'cliflag' > 0
//
void CloseUdpComm(int srvflag, int cliflag)
{
    if  ((srvflag > 0) && (udp_server > 0))
        close(udp_server);
    if  ((cliflag > 0) && (udp_client > 0))
        close(udp_client);
}

// --------------------------------------------------------------------
// MEMORIA RITENTIVA :
#ifdef RETENTIVEMEMENABLE

int SetRetUdp(int Port, char *pCliIP)
{
    int err = 0;
    // Clear segnalazione errori
    UdpErrors[0] = '\0';

    // -----------------------------------------------------------------
    // Crea un Server UDP
    //
    retServerPort = Port;
    if ((retudp_server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror(UdpErrors);
        return -1;
    }

    // zero out the structure
    memset((char *) &retsock_srv, 0, sizeof(retsock_srv));

    // set the structure
    retsock_srv.sin_family = AF_INET;
    retsock_srv.sin_port = htons(retServerPort);
    retsock_srv.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if( bind(retudp_server , (struct sockaddr*)&retsock_srv, sizeof(retsock_srv) ) == -1)
    {
        perror(UdpErrors);
        err = 1;
    }

    // -----------------------------------------------------------------
    // Crea un Client UDP
    //
    retClientPort = Port + 1;
    if ( (retudp_client=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        perror(UdpErrors);
        err |= 2;
    }

    memset((char *) &retsock_cli, 0, sizeof(retsock_cli));
    retsock_cli.sin_family = AF_INET;
    retsock_cli.sin_port = htons(retClientPort);

    if (inet_aton(pCliIP , &retsock_cli.sin_addr) == 0)
    {
        perror(UdpErrors);
        err |= 2;
    }

    return err;
}

// --------------------------------------
// Chiude socket Server o Client
// se 'srvflag' o 'cliflag' > 0
//
void CloseRetUdp(int srvflag, int cliflag)
{
    if  ((srvflag > 0) && (udp_server > 0))
        close(retudp_server);
    if  ((cliflag > 0) && (udp_client > 0))
        close(retudp_client);
}
// --------------------------------------------------------------------
#endif

//--------------------------------------------------
// try to receive some data, this is a blocking call
//
int UdpReceive(int BufLen, char *pRcvBuf)
{
    //int slen = sizeof(sock_sender);
    int slen = sizeof(struct sockaddr_in);
    int recv_len;

    /* Dal manuale :
    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen);
    */
    if ((recv_len = recvfrom(udp_server, pRcvBuf, BufLen, 0,
                             (struct sockaddr *) &sock_sender, (socklen_t *)&slen)) == -1)
    {
        perror(UdpErrors);
        return -1;
    }
    return recv_len;
}

//--------------------------------------------------
// try to send some data
//
int UdpSend(int BufLen, char *pSndBuf)
{
    int x;
    //int slen = sizeof(sock_cli);
    int slen = sizeof(struct sockaddr_in);
    
    x = sendto(udp_client, pSndBuf, BufLen, 0, (struct sockaddr*) &sock_cli, slen);
    if (x < 0)
    {
        perror(UdpErrors);
        return -1;
    }
    return x;
}
