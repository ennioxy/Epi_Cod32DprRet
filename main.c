/*
 *  === codepi_16rw ===
 *
    Legge/Scrive le variabili dichiarate in Codesys come Globals
    su UDP verso la memoria Condivisa EPI

    utilizza la libreria "libmemory.so"
 * 
 * 
 * MODIFICHE
 * E.G., 14/06/16:
 * - La Porta Udp puo' essere assegnata all'avvio
 *   digitandola come primo parametro  (valore {1001-6554})
 * 
*/
#include <linux/kernel.h>
#include <stdio.h>      //printf
#include <string.h>     //memset
#include <stdlib.h>     //exit(0);
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include "codudp.h"

//#include <cstdlib>

//using namespace std;
// -------------------------------------------------------------------
// Set e close dei sockets per la comunicazione Udp
//
extern char *GetUdpErrors();
extern int  SetCodUdp(int Port, char *pCliIP);
extern void CloseUdpComm(int srvflag, int cliflag);
extern int  UdpReceive(int BufLen, char *pRcvBuf);
extern int  UdpSend(int BufLen, char *pSndBuf);

extern int   kbhit(void);
extern pid_t proc_find(const char* name, int noverbose);

void SharedMem_Update(DWORD *pA0, DWORD *pA1, DWORD *pB, int Nsize);

// ----------------------------------------------------------
// Funzioni di 'epimemory.so'
//
extern char* ReadVersion();
extern DWORD ReadMem(DWORD Address);
extern void  WriteMem(DWORD Address, DWORD Data);

extern DWORD RetentiveMem[NVARS_RETENTIVE+2];

#ifdef RETENTIVEMEMENABLE 
extern int RetentiveDW_Save();
extern int RetentiveDW_Load();
/*
extern int SetRetUdp(int Port, char *pCliIP);
extern void* RetMemCycle(void *arg);
pthread_t retmemthrd_id;
pthread_attr_t thr_attrb;
extern int RetMemThread_Flag;
*/
#endif        

// ----------------------------------------------------------
//

// DataR : dati letti da EpiMem e trasmessi al PLC
DWORD DataR[NVARS_INP + 2];
// DataW : dati da scrivere in EpiMem e ricevuti dal PLC
DWORD DataW[NVARS_OUT + 2];

// Dati letti da Epimem in sumlazione DPM
DWORD DataDPM[NVARS_RW+1];
// Dati da scrivere in Epimem in simlazione DPM ricevuti dal PLC
DWORD DataDPM_W[NVARS_RW+1];
// Dati DataDPM_W al tempo - 1
DWORD DataDPM_W1[NVARS_RW+1];

// Buffer per comunicazione UDP con Codesys
char UDP_buf[BUFLEN];
int  UDP_len;

// Porta di Ricezione. N.B. Porta di Tx = N.PortaRx + 1
int UDP_Port;

// Flag che comanda la copia della prima Dword ricevuta
// sulla prima dword inviata
int EchoFlag;
// Flag che blocca l' I/O sulla consolle
int BlindFlag;

// NB: seguente introdotto per gestione di shared anche come retentive
// Flag che se == 1 allora abilita gestione Shared 
int SharedGoFlag;

// ----------------------------------------------------------
//
int Udp_VarXchg(int Nr, int Nw, DWORD *pDataR, DWORD *pDataW);
int Udp_VarRx(DWORD *pDataW, DWORD *pDataDPM_W);
int Udp_VarTx(int txlen, DWORD *pDataR, DWORD *pDPM);

//
// ======================================================================
//

// -----------------------------------------
// Visualizza Versione del Programma
void Show_EpiVersion()
{
    char *p = ReadVersion();
    printf("libmemory : %s\n", p);
}


// -----------------------------------------
// Legge una Variabile Epi (DWORD)
//
DWORD Epi_ReadVar(DWORD Address)
{

    DWORD dw;
    dw = ReadMem(Address);
    return dw;
}

// -----------------------------------------
// Scrive una Variabile Epi (DWORD)
//
void Epi_WriteVar(DWORD Address, DWORD Data)
{
    WriteMem(Address, Data);
}


// --------------------------------------------------
// Imposta i parametri impostabili da riga di comando :
// "echo" e "blind"
// Rende -1 se errore nella riga di comando
//
int SetParameters(int argc, char* argv[])
{
    int i, x;
    EchoFlag = 1;
    BlindFlag = 1;
    if  (argc < 2)
        return 0;

    if  (sscanf(argv[1], "%d", &x) == 1)
    {
        if  ((x > 1000) && (x < 65535))
            UDP_Port = x;
    }
            
    for (i=1; i<argc; i++)
    {
        if  (strcmp(argv[i], "test") == 0)
            BlindFlag = 0;
        else
            return -1;
    }
    return 0;
}


// ---------------------------------------------------
// *** ==== ***
// *** MAIN ***
// *** ==== ***
//int main(void)
int main(int argc, char* argv[])
{
    //char a, *p;
    char *p;
    int i, x, comm_cnt;
    //int Nr, Nw;
    DWORD EpiMem_Add;
    pid_t plc_pid;

    // --------------------------------------------------
    // Set parametri BlindFlag ed EcoFlag
    //
    x = SetParameters(argc, argv);
    if  (x < 0)
    {
        printf("\nParametro sconosciuto.\n");
        printf("Sintassi: ncodepi_16rw <echo> <blind>\n");
        exit(1);
    }

    if  (BlindFlag == 0) printf("CODEPI : connessione I/O Epi e PLC\n");

    // -------------------------------------------------
    // Test presenza del PLC
    //
    plc_pid = proc_find("codesyscontrol", BlindFlag);
    if  (plc_pid < 0)
    {
        printf("PLC NON TROVATO\nIl Programma Termina\n");
        exit(1);
    }


    if  ((BlindFlag == 0) && (EchoFlag > 0)) printf("ECHO ON: Copia 1.DW ricevuta da PLC in 1.DW inviata al PLC\n");

    if  (BlindFlag == 0) printf("Attivazione Comunicazione UDP : ");

    SharedGoFlag = 1;
    
#ifdef RETENTIVEMEMENABLE        
    SharedGoFlag = 0;
    //  --------------------------------------------------
    // Preleva valori in ritenzione
    x = RetentiveDW_Load();
#endif
    
    //  --------------------------------------------------
    // Imposta Comunicazione Udp
    UDP_Port = PORTrx;  //default
    //a = SERVER[0];
    p = (char *) &SERVER[0];
    if  (SetCodUdp(UDP_Port, p) != 0)
    {
        if  (BlindFlag == 0) printf("\nErrore : %s\n Il Programma Termina\n", GetUdpErrors());
        exit(1);
    }
    else
        if  (BlindFlag == 0) printf("Ok\n");

    if  (BlindFlag == 0)
    {
        // Visualizza Versione del Programma
        Show_EpiVersion();

        printf("------------------------\n");
        printf("Premere ESC per terminare\n");
        printf("------------------------\n");
    }

/*    
#ifdef RETENTIVEMEMENABLE        
    // ------------------------------------------------------
    // Imposta comunicazione Udp per gestione retentive memory
    // Porta impostata a +2 rispetto alla comunicazione Udp precedente
    UDP_Port = PORTrx + 2;  //default + 2
    //a = SERVER[0];
    p = (char *) &SERVER[0];
    if  (SetRetUdp(UDP_Port, p) != 0)
    {
        x = 1;
        pthread_attr_init(&thr_attrb);
        x = pthread_create(&retmemthrd_id, &thr_attrb, &RetMemCycle, NULL);
    }
        x = 1;
        x = 2;
        x = 31;
        x = 41;
        x = 51;
        
#endif        
*/
    

    // ===============================================================================
    // CICLO DI SCAMBIO :
    // -----------------
    // - Legge Memorie scritte da Plc per Zona Memorie Condivise di OUTPUT ("Y_nn")
    // - Legge Zona DPM con variabili modificate da Epimem
    // - Aggiorna Zona DPM con variabili modificate dal PLC ("Z_nn")
    // - Legge Zona Memorie Condivise di INPUT da inviare al Plc ("X_nn")
    //
    comm_cnt = 1;
    while   (1)
    {
        // -----------------------------------------------------------
        // Nota Bene: la Memoria EpiMem[0] non deve essere utilizzata
        // -----------------------------------------------------------
        
        // Gestisce memoria in simulazione DPM (vedi commenti in 'utility.c)
        
        // RICEZIONE dati via UDP : immagine OUTPUTS e DPM ("Y_nn" e "Z_nn")
        UDP_len = Udp_VarRx(&DataW[0], &DataDPM_W[0]);
        if  ( UDP_len < 0)
        {
            if  (BlindFlag == 0)
                printf("\nUdp Communication Error : stop program\n");
            break;
        }
        
        // SCRIVE Memorie Condivise associate agli OUTPUTS
        EpiMem_Add = OFFVARS_OUT; 
        for (i=0; i < NVARS_OUT; i++)
            Epi_WriteVar(EpiMem_Add++, DataW[i]);
        
        // Se richiesto :
        // - Copia 1.Dato ricevuto da PLC in 1.Dato reso al PLC
        // (Solitamente un counter aggiornato dal PLC : consente semplice verifica sw ok)
        if  (EchoFlag == 1)
        {
            DataR[0] = DataW[0];
            Epi_WriteVar(1, DataW[0]);
        }

#ifdef RETENTIVEMEMENABLE
        // attende almeno due cicli di comunicazione prima di abilitare la
        // modifica da Plc delle memorie ritentive
        if  (comm_cnt > 2)
            SharedGoFlag = 1;
#endif
        if  (SharedGoFlag == 1)
        {
            // LEGGE Memorie Condivise usate come DPM  ("zxnn" su Plc)
            EpiMem_Add = OFFVARS_RW;   //1;
            for (i=0; i < NVARS_RW; i++)
                DataDPM[i] = Epi_ReadVar(EpiMem_Add++);

            // -------------------------------------------------------
            // GESTISCE eventuali scritture ricevute da PLC ("Z_nn")
            SharedMem_Update(&DataDPM_W[0], &DataDPM_W1[0], &DataDPM[0], NVARS_RW);


            // LEGGE Memorie Condivise associate agli INPUTS ("X_nn")
            EpiMem_Add = OFFVARS_INP;   //1;
            for (i=0; i < NVARS_INP; i++)
                DataR[i] = Epi_ReadVar(EpiMem_Add++);

        }
        else
        {
            // Copia memorie ritentive ! (solo rer i primi due cicli)a 
            memcpy(&DataDPM[0], &RetentiveMem[0], NVARS_RETENTIVE * sizeof(DWORD)); 
        }

#ifdef RETENTIVEMEMENABLE
        // MEMORIZZAZIONE MEM RITENTIVE (tipo 'Z')
        // - Comando da PLC : DataDPM_W[0] = 0x5A5A 
        if  (DataDPM_W[0] == 0x5A5A)
        {
            //resetta comando
            DataDPM[0] = 0x55AA;
            // Copia memorie dpram in buffer ritentive 
            memcpy(&RetentiveMem[0], &DataDPM[0], NVARS_RETENTIVE * sizeof(DWORD)); 
            // Le salva su file
            RetentiveDW_Save();
        }
#endif
        
        // INVIO dati al Plc : immagine INPUTS e DPM ("X_nn" e "zx_nn") 
        x = Udp_VarTx(UDP_len, &DataR[0], &DataDPM[0]);
        if  ( x < 0)
        {
            if  (BlindFlag == 0)
                printf("\nUdp Communication Error : stop program\n");
            break;
        }
        
        comm_cnt++;

        // 
        // Se Blind attivato : Torna i Ciclo
        if  (BlindFlag == 1)
            continue;
        //
        // =======================================================

        // -------------------------------------------------------
        // Testa eventuali tasti premuti
        // Se ESCAPE termina
        if (kbhit())
        {
            x = getchar();
            printf("Eseguite %d comunicazioni\n", comm_cnt);
            if  (x == ESCAPE)
            {
                printf("Programma Terminato dall'utente\n\n");
                break;
            }
            printf("Premi Escape per terminare\n");
        }
    }
    // ===============================================================================

    // Chiude sia Server che Client Udp
    CloseUdpComm(1, 1);

/*    
#ifdef RETENTIVEMEMENABLE
    // Attende che il thread gestione mem ritentive termini
    if  (retmemthrd_id != 0)
    {
        RetMemThread_Flag = -1;             // forza termine del thread
        pthread_join(retmemthrd_id, NULL);    
    }
        
#endif        
 */
    return 0;
}

// --------------------------------------------------
// Legge le variabili da inviate dal PLC 
// 
// pDataW : variabili da scrivere in OUTPUTS
// pDataDPM_W : immagine variabili DPM da scrivere
//
int Udp_VarRx(DWORD *pDataW, DWORD *pDataDPM_W)
{

    int i;
    int recv_len;
    DWORD *pdword;

    // Azzera buffer di ricezione
    memset(UDP_buf, 0, BUFLEN);

    // -------------------------------------------------------------
    //keep listening for data
    //
    //printf("Waiting for Udp Message...");
    fflush(stdout);

    //--------------------------------------------------
    // try to receive some data, this is a blocking call
    //
    if  ((recv_len = UdpReceive(BUFLEN, UDP_buf)) == -1)
    {
        if  (BlindFlag == 0)
            printf("Recv Error : %s\n", GetUdpErrors());
        return -1;
    }


    // Read data (sent by PLC) to write to EpiMem
    pdword = (DWORD *) &UDP_buf[CODESYSDATA_OFFSET];
    for (i=0; i<NVARS_OUT; i++)
        *pDataW++ = *pdword++;
    
    for (i=0; i<NVARS_RW; i++)
        *pDataDPM_W++ = *pdword++;

    return recv_len;
}

// --------------------------------------------------
// Legge/Scrive le variabili da scambiare con Codesys
// - vuole N. dword da leggere e da scrivere
//
// pDataR : variabili INPUTS da rendere al PLC
// pDPM : immagine variabili DPM da rendere al PLC
//
int Udp_VarTx(int txlen, DWORD *pDataR, DWORD *pDPM)
{
    int i;
    //int send_len;
    DWORD *pdword;

    // Write data (from EpiMem) to be sent to Plc
    pdword = (DWORD *) &UDP_buf[CODESYSDATA_OFFSET];
    for (i=0; i<NVARS_INP; i++)
        *pdword++ = *pDataR++;
    for (i=0; i<NVARS_RW; i++)
        *pdword++ = *pDPM++;

    //send_len = CODESYSDATA_OFFSET + NVARS_RW + NVARS_INP;
    if  (UdpSend(txlen, UDP_buf) == -1)
    {
        if  (BlindFlag == 0)
            printf("Send Error : %s\n", GetUdpErrors());
        return -1;
    }

    return 0;
}


// --------------------------------------------------
// Legge/Scrive le variabili da scambiare con Codesys
// - vuole N. dword da leggere e da scrivere
// Nota:
// Read e' "read from EpiMem"
// Write e' "write to EpiMem"
//
int Udp_VarXchg(int Nr, int Nw, DWORD *pDataR, DWORD *pDataW)
{

    int i;
    //int slen = sizeof(sock_sender) , recv_len;
    //int recv_len;
    //char buf[BUFLEN];
    DWORD *pdword;


    // Azzera buffer di ricezione
    memset(UDP_buf, 0, BUFLEN);

    // -------------------------------------------------------------
    //keep listening for data
    //
    //printf("Waiting for Udp Message...");
    //fflush(stdout);

    //--------------------------------------------------
    // try to receive some data, this is a blocking call
    //
    if  ((UDP_len = UdpReceive(BUFLEN, UDP_buf)) == -1)
    {
        if  (BlindFlag == 0)
            printf("Recv Error : %s\n", GetUdpErrors());
        return -1;
    }


    // Read data (sent by PLC) to write to EpiMem
    pdword = (DWORD *) &UDP_buf[CODESYSDATA_OFFSET];
    for (i=0; i<Nr; i++)
        *pDataW++ = *pdword++;

    // Write data (from EpiMem) to be sent to Plc
    pdword = (DWORD *) &UDP_buf[CODESYSDATA_OFFSET];
    for (i=0; i<Nw; i++)
        *pdword++ = *pDataR++;

    if  (UdpSend(UDP_len, UDP_buf) == -1)
    {
        if  (BlindFlag == 0)
            printf("Send Error : %s\n", GetUdpErrors());
        return -1;
    }

    return 0;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


/*
 * Dichiarazioni della libreria "libmemory.so"
 *
unit so_interface;

{$mode objfpc}{$H+}

interface

Type Ctrls  = array[0..127] of char;

Type Device         = record
       Name      : string[40];
       Dev_Type  : Integer;
       Address   : Longint;
       Status    : Integer;
     end;

Type Debug_Commands = record
       Set_Active    : Integer;
       Set_Line_Stop : Longint;
       Set_Status    : Integer;
     end;

Type Debug_Infos    = record
       Prgm_in_use   : Ctrls;
       Task_in_use   : Ctrls;
       Task_num      : Integer;
       Active        : Integer;
       Line_Stop     : Longint;
       Address       : Longint;
       Status        : Integer;
     end;

//------------------------------------------------------------------------------
Const HandleMemoryAddress   = $3FFF;
      Mt_Net_Handle         = $1;
      DBase_Handle          = $2;
      EpiSoft_IO_Handle     = $4;
      Transponder_Handle    = $8;
      Laser_Handle          = $10;
      Camera_Handle         = $20;
      Client_Handle         = $40;
      Generic_Handle        = $80;
      Omron_Handle          = $100;       //Added 04_06_07
      DBbrowser_Handle      = $200;       //
      DataLogger_Handle     = $400;       //Added 20_09_07
      SMS_Handle            = $800;       //Added 19_12_07
      WELL_Handle           = $1000;      //Added 08_01_08
      S7_Handle             = $2000;      //Added 20_02_08
      utf_server_Handle     = $4000;      //Added 11_06_09
      lite_client_Handle    = $8000;      //Added 27_10_10
      strumenti_Handle      = $10000;
      MPROG_Handle          = $40000000;
      EPI_ENG_SERVER_Handle = $80000000;
//------------------------------------------------------------------------------
      Tick_MemoryAddress    = $3FFE;
      CloseMessages         = $3FFD;
      Progs_inUse           = $3FFC;
      Main_Handle           = $3FFB;
      Close_All_Command     = $3FFA;
      Steps_per_sec         = $3FF9;      //Modified 28_10_07
      I2C_errors            = $3FF8;
      Mem_Battery_on        = $3FF7;      //Aggiunto 30_08_08
      Math_error            = $3FF6;
//------------------------------------------------------------------------------
Const
//  WM_EPI_EXECUTOR         = WM_USER + 1;
      EPI_Main_lock   = 1;
      EPI_Main_Unlock = 2;
      Database_in_use = 1023;
//------------------------------------------------------------------------------
Const MaxSharedLondWords    = $3FFF; // 16383
      MaxSharedPchars       = $03FF; //  1023
      Battery_Mem_Lenght32  = $1FFF; //  8192  LongWord (32 bit)
      Battery_Mem_Lenght16  = $3FFF; // 16384  Word     (16 bit)
      Max_Devices_Num       = $1FFF; //  8191      Device
      Def_Steps_per_sec     = 5000;  // Added 28_10_07
      Max_Steps_per_sec     = 50000; // Added 28_10_07
      Min_Steps_per_sec     = 100;   // Added 28_10_07
      ext_Inp = 111;
      ext_Inp1 = 222;
      ext_Out = 333;
      ext_Out1 = 444;
const
  gtklib = 'libmemory.so';

  (*
  Function  ReadVersion : Pchar; cdecl;
  Function  ReadBatteryStatus : longWord;  cdecl;
  Procedure SetBatteryStatus(Status : longWord);  cdecl;
  Function  ReadMem(Address : Word): longWord;  cdecl;
  Procedure WriteMem(Address : Word; Data : longWord); cdecl;
  //Function  ReadMemVect(Address : Longint; Buffer : PByteArray; BufferLen : Cardinal): longInt;  cdecl; external gtklib;;
  //Function  WriteMemVect(Address : Longint; Buffer : PByteArray; BufferLen : Cardinal): longInt; cdecl; external gtklib;;
  Function  clear_Mems : Pchar; cdecl;
  Function  ReadStrings(Address  : Word): Ctrls;  cdecl;
  Procedure WriteStrings(Address : Word; Data : Pchar); cdecl;
  Function  ReadStdStr(Address : Word): ShortString;  cdecl;
  Procedure WriteStdStr(Address : Word; Data : String); cdecl;
  Function  clear_Strings : Pchar;  cdecl;
  Function  clear_Battery_Mem : Pchar;  cdecl;
  Function  Battery_Mem_is_Changed : longWord;  cdecl;
  Function  GetDivice(Index : Integer) : Device;  cdecl;
  Procedure SetDivice(Index : Integer; Dev : Device);  cdecl;
  Procedure SetDebug_Commands(D : Debug_Commands);  cdecl;
  Function  GetDebug_Commands : Debug_Commands;  cdecl;
  Procedure SetDebug_Infos(D : Debug_Infos);     cdecl;
  Function  GetDebug_Infos   : Debug_Infos;     cdecl;
implementation
*)
  Function  ReadVersion : Pchar;  cdecl; external gtklib;
  Function  ReadBatteryStatus : longWord;  cdecl; external gtklib;
  Procedure SetBatteryStatus(Status : longWord);  cdecl; external gtklib;
  Function  ReadMem(Address : Word): longWord;  cdecl; external gtklib;
  Procedure WriteMem(Address : Word; Data : longWord); cdecl; external gtklib;
//Function  ReadMemVect(Address : Longint; Buffer : PByteArray; BufferLen : Cardinal): longInt;  cdecl; external gtklib;;
//Function  WriteMemVect(Address : Longint; Buffer : PByteArray; BufferLen : Cardinal): longInt; cdecl; external gtklib;;
  Function  clear_Mems : Pchar; cdecl; external gtklib;
  Function  ReadStrings(Address  : Word): Ctrls;  cdecl; external gtklib;
  Procedure WriteStrings(Address : Word; Data : Pchar); cdecl; external gtklib;
  Function  ReadStdStr(Address : Word): ShortString;  cdecl; external gtklib;
  Procedure WriteStdStr(Address : Word; Data : String); cdecl; external gtklib;
  Function  clear_Strings : Pchar;  cdecl; external gtklib;
  Function  clear_Battery_Mem : Pchar;  cdecl; external gtklib;
  Function  Battery_Mem_is_Changed : longWord;  cdecl; external gtklib;
  Function  GetDivice(Index : Integer) : Device;  cdecl; external gtklib;
  Procedure SetDivice(Index : Integer; Dev : Device);  cdecl; external gtklib;
  Procedure SetDebug_Commands(D : Debug_Commands);  cdecl; external gtklib;
  Function  GetDebug_Commands : Debug_Commands;  cdecl; external gtklib;
  Procedure SetDebug_Infos(D : Debug_Infos);     cdecl; external gtklib;
  Function  GetDebug_Infos   : Debug_Infos;     cdecl; external gtklib;
implementation
end.
*/


 
