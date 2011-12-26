/***************************************************************************
                          smi2txt_v0.1  -  SMI2txt
                             -------------------
    Easily convert your smi files into txt files with complete number,
    date and time

    begin                : irgendwann Oktober 2001 ;-)
    copyright            : (C) 2002 by Michael Simons
    email                : misi@planet-punk.de
    url                  : http://www.planet-punk.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *   For more information visit: http://www.gnu.org                        *
 *                                                                         *
 *   I am not responsible for any damage done by this programm. I even not *
 *   guarantee that it works.                                              *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "7bit.h"
typedef enum{
	false, true}
bool;

/* Fehlerbehandlung Anfang */
#define ANZ_ERR 10
#define IO_ERROR_FNF 0
#define IO_ERROR_CNW 1
#define IO_ERROR_FAE 2
#define NO_VALID_SMI 3
#define NO_VALID_CHARSET 4
#define TO_LESS_PARMS 5
#define MEM_ERR 6
bool errors[ANZ_ERR];
/* Fehlerbehandlung Ende */

#define SMS_LAENGE 181
#define LBIT1 6
#define LAENGE_ZEIT 9
#define LAENGE_DATUM 9

typedef struct {
	char *fname;
	short NumberLength;
	char *number;                  /* Format: Entweder numerisch oder ASCII */
	char date[LAENGE_ZEIT];        /* Format: DD-MM-YY\0" */
	char time[LAENGE_DATUM];       /* Format: HH:MM:SS\0"   */
	char message[181];
}
SMS;


static int counter;
char *DatErrName;

void TauscheFelder(char *feld, char *neufeld, short n) {
	int i;
	for(i=0; i<n-1; i+=2) {
		neufeld[i]=feld[i+1];
		neufeld[i+1]=feld[i];
	}
}

void FormatiereDatum(char *Datum, char *temp) {
	/* Datum kommt im Format YYMMDD an */
	Datum[0]='\0';
	sprintf(Datum,"%c%c.%c%c.%c%c",temp[4],temp[5],temp[2],temp[3],temp[0],temp[1]);
}

void FormatiereZeit(char *Zeit, char *temp) {
	/* Zeit kommt im Format HHMMSS an */
	Zeit[0]='\0';
	sprintf(Zeit,"%c%c:%c%c:%c%c",temp[0],temp[1],temp[2],temp[3],temp[4],temp[5]);
}

void BastelText(short *binary, char *message, short n) {
	int i,j,k;
	int potenz;

	j=7;
	for(i=k=0; i<n; ++i, ++k) {
		if(j==-1) {
			j=7;
			potenz=(int)pow(2.0,j)-1;
			i ? --i : 0;
			message[k]=binary[i] & potenz;
		}
		else
		    potenz=(int)pow(2.0,j)-1;
		message[k]=(binary[i-1]>>(j+1) ) |  ((binary[i] & potenz)<<(7-j));
		--j;
	}
	for(i=0; i<n; ++i)
		message[i]=bit7[message[i]];
	message[n]='\0';
}


/*------------------------------------------------------------------------
 Procedure:     ParseMessage ID:1
 Purpose:       Bildet aus eine SMI character String und SMS *sms
 Input:         char *fname - Dateiname der ursprünglichen Datei

 Output:        short binary[] - binärer Code der SMI
                SMS *sms       - fertige SMS
 Errors:
------------------------------------------------------------------------*/
bool ParseMessage(char *fname, short binary[SMS_LAENGE], SMS *sms) {
	int i;
	short OffSet,diff;
	char RawSMS[SMS_LAENGE*2+1], temp[6];
	bool rc;

	rc = true;
	diff=0;
	/* Auf string kopieren */
	for(i=0; i<SMS_LAENGE; ++i)
		sprintf(RawSMS+i*2,"%02x",binary[i]);
/* strncmp("0b0b00000001",RawSMS,12) ||*/
/* strncmp("0b0b010b0001", RawSMS, 12)  */
	if(  strncmp("0b0b", RawSMS, 4) ) {
		rc = false;
		errors[NO_VALID_SMI]=true;
	}
	else {
		sms->fname=(char *)calloc(strlen(fname)+1,sizeof(char));
		strcpy(sms->fname,fname);
		OffSet = binary[LBIT1]+LBIT1+3;       /* SMS Center Nummer ueberlesen  */
		sms->NumberLength = binary[OffSet-1]; /* Laenge der Telefonnummer      */

		/* Nummer */
		switch (binary[OffSet]) {             /* Art der Telefonnummer         */
		case 0x85:
		case 0x89:                            /* Nationale Nummer              */
		case 0x81:                            /* Betreiberintern               */
		case 0x91:                            /* Normale internationale Nummer */
			if(sms->NumberLength%2) {         /* Ungerade Nummern              */
				sms->NumberLength+=1;
				++diff;
			}
			sms->number = (char *)malloc((sms->NumberLength+1)*sizeof(char));
			TauscheFelder(RawSMS + (OffSet+1)*2,sms->number,sms->NumberLength);
			sms->number[sms->NumberLength-diff]='\0';
			break;
		case 0xd0:                           /* ASCII Name                     */
			sms->number = (char *)malloc((sms->NumberLength+10)*sizeof(char));
			BastelText(binary+OffSet+1,sms->number,sms->NumberLength);
			sms->number[sms->NumberLength/2+1]='\0';
			break;
		default:
			break;
		}

		/* Datum & Zeit */
		OffSet+=sms->NumberLength/2 + 3;

		TauscheFelder(RawSMS + OffSet*2,temp,6);
		FormatiereDatum(sms->date,temp);
		OffSet+=3;

		TauscheFelder(RawSMS + OffSet*2,temp,6);
		FormatiereZeit(sms->time,temp);
		OffSet+=4;

		/* Text */
		BastelText(binary+OffSet+1,sms->message,binary[OffSet]);
	}

	return rc;
}


bool ReadMessage(char *DatName, short binary[SMS_LAENGE]) {
	bool rc = true;
	int i;
	FILE *smi;
	i=0;

	if( !(smi=fopen(DatName,"rb")) ) {
		rc = false;
		errors[IO_ERROR_FNF]=true;
	}
	else {
		while(  i<SMS_LAENGE && !feof(smi))
			binary[i++] = fgetc(smi);

		if(i!=SMS_LAENGE) {
			rc = false;
			errors[NO_VALID_SMI]=true;
		}
		fclose(smi);
	}
	return rc;
}

void ShowMessage(SMS *sms) {
	int i;
	printf("%d\n",counter);
	for(i=0;i<80;++i)
		printf("-");
	printf("\n%s\n",sms->fname);
	printf("Absendernummer : %s\n"
		    "Datum          : %s\n"
		    "Uhrzeit        : %s\n",
		sms->number,sms->date,sms->time);
	printf("Nachrichtentext:\n"
		    "%s\n",sms->message);
	for(i=0;i<80;++i)
		printf("-");
	printf("\n\n");
}

char *DateiNamenOhneNummer(char *Datum, char *Zeit){
	char *rc;
	char ZeitHelp[LAENGE_ZEIT];

	rc = (char *)calloc(23,sizeof(char));
	if(rc){
		strcpy(ZeitHelp,Zeit);
		ZeitHelp[2]=ZeitHelp[5]='-';
		sprintf(rc,"%s_%s.txt",Datum,ZeitHelp);

	}
	return rc;
}

bool SchreibeDatei(SMS *sms, char *(*Name)(char*, char*)) {
	char *DateiName;
	FILE *TextDatei;
	bool rc = true;

	if(!(DateiName=Name(sms->date,sms->time))) {
		rc = false;
		errors[MEM_ERR]=true;
	}
	else if( (TextDatei=fopen(DateiName,"r")) ) {
		rc = false;
		errors[IO_ERROR_FAE]=true;
		strcpy(DatErrName,DateiName);
		fclose(TextDatei);
	}
	else if(!(TextDatei=fopen(DateiName,"w")) ) {
		rc = false;
		errors[IO_ERROR_CNW]=true;
		strcpy(DatErrName,DateiName);
		printf("%s\n",DatErrName);
	}
	else {
		fprintf(TextDatei,
			"%s\n"
			    "%s, %s\n---\n"
			    "%s\n",
			sms->number,sms->date,sms->time,sms->message);
		fclose(TextDatei);
	}
	free(DateiName);
	return rc;
}


void FehlerAusgabe(char* progname, char *SmiName) {
	int i;
	FILE *ErrorLog;
	time_t now;
	char *timestr=calloc(19,sizeof(char));

	time(&now);
	strftime(timestr,19,"%d-%m-%y, %H-%M-%S",localtime(&now));

	if(!(ErrorLog=fopen("smi2txt_error.log","a")))
		fprintf(stderr,"%s\n%s Error: Konnte Error Log Datei nicht zum Schreiben öffnen.\n",timestr,progname);
	else {
		for(i=0; i<ANZ_ERR; ++i) {
			if(errors[i]) {
				if(i==IO_ERROR_FNF)
					fprintf(ErrorLog,"%s\n%s: SMS Datei \"%s\" konnte nicht geöffnet werden\n",
						timestr,progname,SmiName);
				else if(i==IO_ERROR_CNW)
					fprintf(ErrorLog,"%s\n%s: Ausgabedatei \"%s\" konnte nicht zum Schreiben geöffnet werden\n",
						timestr,progname,DatErrName);
				else if(i==IO_ERROR_FAE)
					fprintf(ErrorLog,"%s\n%s: Ausgabedatei \"%s\" existiert bereits.\n",
						timestr,progname,DatErrName);
				else if(i==NO_VALID_SMI)
					fprintf(ErrorLog,"%s\n%s: \"%s\" ist keine gültige SMS Datei\n",
						timestr,progname,SmiName);
				else if(i==NO_VALID_CHARSET)
					fprintf(ErrorLog,"%s\n%s: Dieser Fehler sollte eigentlich nicht auftreten\n",
						timestr,progname);
				else if(i==TO_LESS_PARMS)
					fprintf(stderr,"%s: Ungültiger Aufruf.\nSyntax: %s <Dateiname>.\n",
						progname,progname);
				else if(i==MEM_ERR) {
					fprintf(ErrorLog,"%s\n%s: Fehler beim Anlegen von Speicher, Programm wird abgebrochen\n",
						timestr,progname);
					exit(EXIT_FAILURE);
				}
			}
		}
		fclose(ErrorLog);
	}
	for(i=0; i<ANZ_ERR; ++i)
		errors[i]=false;
	free(timestr);
}



int main(int argc, char *argv[]) {
	SMS a_sms;
	short binary[SMS_LAENGE];
	bool error;
	int i,parms;

	DatErrName=calloc(256+1,sizeof(char));

	error=false;
	for(i=0; i<ANZ_ERR; ++i)
		errors[i]=false;

	if(argc<=1) {
		error=true;
		errors[TO_LESS_PARMS]=true;
		FehlerAusgabe(argv[0],NULL);
	}
	else {
		for(parms=1; parms< argc; ++parms) {
			if(!ReadMessage(argv[parms],binary))
				error=true;
			else if(!ParseMessage(argv[parms],binary,&a_sms))
				error = true;
			else if(!SchreibeDatei(&a_sms,DateiNamenOhneNummer))
				error = true;

			if(error) {
				FehlerAusgabe(argv[0],argv[parms]);
				DatErrName[0]='\0';
				error=false;
			}
			else {
				++counter;
				//ShowMessage(&a_sms);
				free(a_sms.number);
				;
				free(a_sms.fname);
			}
		}
		printf("Von %d Dateien wurden %d erfolgreich konvertiert.\n"
			    "Mögliche Fehler sind in der Datei \"smi2txt_error.log\" dokumentiert.\n",argc-1,counter);
		printf("\nsmi2txt done by Michael Simons, http://www.planet-punk.de\n");
	}
	free(DatErrName);
	exit(EXIT_SUCCESS);
}
