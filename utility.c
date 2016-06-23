#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

// -----------------
// Per kbhit() :
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
// -----------------
#include "codudp.h"


extern void Epi_WriteVar(DWORD Address, DWORD Data);

        
// ---------------------------------------
// Check se tasto premuto
// - Se si rende 1 else 0
//
int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);

  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }

  return 0;
}

// ---------------------------------------------
// Verifica se esiste una procedura
// Memorizza eventuali PID nel file psresult.tmp
//
pid_t proc_find(const char* name, int noverbose)
{
    char buf[512];
    FILE *fp;
    ssize_t read;

    char *pline = NULL;
    size_t len = 0;
    int retcode = -1;

    // Non accetta nomi troppo lunghi
    if  (strlen(name) > 412)
        return -1;

    // Cancella il file temporaneo se gia' esiste
    if  (access("psresult.tmp", F_OK) != -1)
    {
        if  (remove("psresult.tmp") != 0)
            return -1;
    }


    // prepara stringa per comando di sistema 'ps'
    sprintf(buf, "pgrep ");

    // vi aggiunge il nome passato
    strcat(buf, name);

    // indirizza l'out su un file temporaneo
    strcat(buf, " > psresult.tmp");

    // ---------------------------------
    // Esegue comando di shell
    system(buf);
    // ---------------------------------

    fp = fopen("psresult.tmp", "r");
    while   ((read = getline(&pline, &len, fp)) != -1)
    {
        if  (noverbose == 0)
            printf("%s at %s", name, pline);
        retcode = 0;
    }

    free(pline);
    fclose(fp);

    return retcode;
}

// Gestore che simula una Dual Port Ram condivisa fra Codesys ed EpiMem :
// - entrambi gli enti possono scrivere e leggere queste memorie.
// (nel seguito chiamiano questa zona di memoria "DPM" per brevita')
// CONSIDERAZIONI :
// I programmi su Raspberry scrivono e leggono la DPM mediante
// operazioni r/w dirette (tramite funzioni della libreria epi) 
// Codesys invece, ad ogni scansione, invia una propria immagine
// "completa" della DPM
// Il presente Server deve attuare le 'scritture' e reinviare a
// Codesys l'immagine completa della DPM contenente le modifiche
// richieste dal Plc e quelle effettuate da EpiMem
// Il Server desume che il Plc richieda una modifica semplicemente
// dal fatto che un valore sia cambiato o meno : solo i valori cambiati 
// verranno scritti nella DPM.
//
// IMPLEMENTAZIONE :
// Si abbiano
// - "A0" : copia della DPM inviata da Codesys   
// - "A1" : copia della A0 al tempo-1 (mantenuta dal Server)
// - "B" : puntatore alla DPM su Rasperry
// - Nsize : dimensione DPM espressa in DWORD
//
void SharedMem_Update(DWORD *pA0, DWORD *pA1, DWORD *pB, int Nsize)
{
    int i;
    //DWORD X, Fx, Rx;
    DWORD EpiMem_Add;
 
    for (i=0; i<Nsize; i++)
    {
        if  (*pA0 != *pA1)
        {
            *pB = *pA0;
            // SCRIVE Var DPM
            EpiMem_Add = OFFVARS_RW + i; 
            //Epi_WriteVar(EpiMem_Add, *pB);
            Epi_WriteVar(EpiMem_Add, *pA0);
        }
        
        // Aggiorna valori old di A
        *pA1 = *pA0;
        
        pA0++;
        pA1++;
        pB++;
    }
}

