/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

// ______________________________________________________________
// Thread use dto manage Retentive Memory
// --------------------------------------
//
// Gestione : 
// 0 - Legge i dati retentive memorizzati su file
// 1 - Attende Invio Dati Retentive da PLC
// 2 - Li scarta ed invia i dati letti dal file scrivendo nel primo 0xAA55 
// 3 - LATO PLC : Se il PLC riceve 0xAA55 nella prima locazione allora copia i dati
//                arrivati nei retentive e scrive nel primo 0x55AA
// 4 - Quando riceve i Dati dal PLC li salva sul file : loop su 4
//
// ------------------------------------------------------------------
#include <stdio.h>      //printf
#include <string.h>     //memset
#include <stdlib.h>     //exit(0);
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
//#include <time.h>
#include <pthread.h>

#include "codudp.h"

// -----------------------------------------------------------------------------
// NOTA BENE :
// Questo "ifdef" racchiude tutto il modulo
//
FILE *pRetMemFile;
DWORD RetentiveMem[NVARS_RETENTIVE+2];
int   RetMemLoadCnt;    // Conteggia quanti Load sono stati effettuati 
int   RetMemSaveCnt;    // Conteggia quanti Save sono stati effettuati 
#ifdef RETENTIVEMEMENABLE
// -----------------------------------------------------------------------------
/*
extern FILE    *RetentiveDW_Open(char *fmode);
extern int     RetentiveDW_Close();
extern int     RetentiveDW_Load(FILE *fp, DWORD *pdw, int Nmem);
extern int     RetentiveDW_Save(FILE *fp, DWORD *pdw, int Nmem);

extern int SetRetUdp(int Port, char *pCliIP);
extern void CloseRetUdp(int srvflag, int cliflag);
*/
// File e alloazione locale delle Memorie Ritentive
int RetMemThread_Flag;


void* RetMemCycle(void *arg)
{
    int i = 0;
    RetMemThread_Flag = 0;
    //pthread_t id = pthread_self();

    while   (RetMemThread_Flag == 0)
    {
        i++;
        //uleep(1000);
        printf("\n%d ::::", i);
        sleep(4);
    }

    return NULL;
}

// ---------------------------------------------------------------
// RETENTIVE MEMORIES File Functions
//----------------------------------
// Funzioni che salvano e prelevano da disco le Memorie
// del PLC Codesys che si vuole siano Ritentive.
//
// - Si assume che i valori da gestire siano sempre
//   del tipo DWORD (32 bit)
//
FILE *RetentiveDW_Open(char *fmode)
{
    //FILE fp;
    char Buf[1024];
    strcpy(Buf, RETENTIVEMEM_DIR);
    strcat(Buf, RETENTIVE_FILENAME);
    pRetMemFile = fopen(Buf, fmode);
    return pRetMemFile;
}

int RetentiveDW_Close()
{
    if  (pRetMemFile == NULL)
        return -1;
    fclose(pRetMemFile);
    return 0;
}

int RetentiveDW_Save()
{
    int i;
    char Buf[32];

    if  (RetentiveDW_Open("w") == NULL)
        return -1;
    
    // Scrive Header del File
    if  (fputs(RETMEMFILE_ID, pRetMemFile) < 0)
        return -1;
    if  (fputs("\r\n", pRetMemFile) < 0)
        return -1;

    // Scrive valori
    sprintf(Buf, "%d\r\n", 0x55AA);
    fputs(Buf, pRetMemFile);
    for (i=1; i < NVARS_RETENTIVE; i++)
    {
        sprintf(Buf, "%d\r\n", RetentiveMem[i]);
        if  (fputs(Buf, pRetMemFile) < 0)
            break;
    }
    
    RetentiveDW_Close();

    RetMemSaveCnt++;    // Conteggia quanti Save sono stati effettuati 
    return 0;
}

int RetentiveDW_Load()
{
    int i, x, n;
    char *p;
    char Buf[64];
    DWORD dw;
    
    // resetta word ritentive
    n = sizeof(RetentiveMem)/sizeof(DWORD);
    if  (n < NVARS_RETENTIVE) 
        return -1;
    memset(RetentiveMem, 0, n);
    
    if  (RetentiveDW_Open("r") == NULL)
    {
        // Se non esiste lo crea
        RetentiveDW_Save();
        return -1;
    }
    
    // Controlla Header del File
    if  (fgets(Buf, 64, pRetMemFile) == NULL)
        return -1;
    if  ((p = strchr(Buf, 0x0D)) != NULL)
            *p = (char) 0;
    if  ((p = strchr(Buf, 0x0A)) != NULL)
            *p = (char) 0;
    if  (strcmp(Buf, RETMEMFILE_ID) != 0)
        return -1;

    // Carica valori
    for (i=0; i<NVARS_RETENTIVE; i++)
    {
        if  (fgets(Buf, 64, pRetMemFile) == NULL)
            return -1;
        x = sscanf(Buf, "%d", &dw);
        if  (x != 1)
            return -1;
        RetentiveMem[i] = dw;
    }
    
    RetentiveDW_Close();
    
    RetMemLoadCnt++;    // Conteggia quanti Load sono stati effettuati 
    return 0;
}


void    RetMem_Manager()
{
    if  (RetentiveMem[0] != 0xAA55)
        RetentiveDW_Load();
}
// -----------------------------------------------------------------------------
#endif  // "RETENTIVEMEMENABLE"
// -----------------------------------------------------------------------------
