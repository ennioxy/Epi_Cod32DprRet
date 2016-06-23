/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   codudp.h
 * Author: egvirt
 *
 * Created on June 12, 2016, 4:43 PM
 */

#ifndef CODUDP_H
#define CODUDP_H

#ifdef __cplusplus
extern "C" {
#endif


#define ESCAPE 27

typedef unsigned int DWORD;

// ----------------------------------------------------------
#define SERVER "127.0.0.1"
#define BUFLEN 1024 //Max length of buffer
#define PORTrx 5005 //The port on which to listen for incoming data
                    //The port to which send data will be PORTrx+1

// Offset a cui iniziano i dati scambiati con Codesys
#define CODESYSDATA_OFFSET  20

// Numero di Variabili Scambiate
#define NVARS_INP   32  // 0x20
#define NVARS_RW    32  // 0x20
#define NVARS_OUT   32  // 0x20
#define NVARS_RETENTIVE   NVARS_RW  // 0x20

#define OFFVARS_INP 1                                   //  1 - 32
#define OFFVARS_RW  OFFVARS_INP + NVARS_INP              // 33 - 64
#define OFFVARS_OUT OFFVARS_INP + NVARS_INP + NVARS_RW  // 65 - 97

// Dir e nome del fiel per le Memorie Ritentive
#define RETENTIVEMEM_DIR    "/etc/epielettronica/"
#define RETENTIVE_FILENAME  "CodeSys_Memo.ret"
#define RETMEMFILE_ID "CODESYS RETMEMFILE"

// Comentare la seguente se non viene adoperata la 
// gestione delle Memorie ritentive
#define RETENTIVEMEMENABLE        


#ifdef __cplusplus
}
#endif

#endif /* CODUDP_H */

