
/* 
 *    PHP Sendmail for Windows.
 *
 *  This file is rewriten specificly for PHPFI.  Some functionality
 *  has been removed (MIME and file attachments).  This code was 
 *  modified from code based on code writen by Jarle Aase.
 *
 *  This class is based on the original code by Jarle Aase, see bellow:
 *  wSendmail.cpp  It has been striped of some functionality to match
 *  the requirements of phpfi.
 *
 *  Very simple SMTP Send-mail program for sending command-line level
 *  emails and CGI-BIN form response for the Windows platform.
 *
 *  The complete wSendmail package with source code can be located
 *  from http://www.jgaa.com
 *
 */

/* $Id$ */

#include "php.h"				/*php specific */
#include <stdio.h>
#include <stdlib.h>
#include <winsock.h>
#include "time.h"
#include <string.h>
#include <malloc.h>
#include <memory.h>
#include <winbase.h>
#include "sendmail.h"
#include "php_ini.h"

/*
   extern int _daylight;
   extern long _timezone;
 */
/*enum
   {
   DO_CONNECT = WM_USER +1
   };
 */

static char *days[] =
{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *months[] =
{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* '*error_message' has to be passed around from php_mail() */
#define SMTP_ERROR_RESPONSE_SPEC	"SMTP server response: %s"
/* Convinient way to handle error messages from the SMTP server.
   response is ecalloc()d in Ack() itself and efree()d here
   because the content is in *error_message now */
#define SMTP_ERROR_RESPONSE(response)	{ \
											if (NULL != (*error_message = ecalloc(1, sizeof(SMTP_ERROR_RESPONSE_SPEC) + strlen(response)))) { \
												snprintf(*error_message, sizeof(SMTP_ERROR_RESPONSE_SPEC) + strlen(response), SMTP_ERROR_RESPONSE_SPEC, response); \
											} \
											efree(response); \
										}

#ifndef THREAD_SAFE
char Buffer[MAIL_BUFFER_SIZE];

/* socket related data */
SOCKET sc;
WSADATA Data;
struct hostent *adr;
SOCKADDR_IN sock_in;
int WinsockStarted;
/* values set by the constructor */
char *AppName;
char MailHost[HOST_NAME_LEN];
char LocalHost[HOST_NAME_LEN];
#endif
char seps[] = " ,\t\n";
char *php_mailer = "PHP 4 WIN32";

char *get_header(char *h, char *headers);

/* Error messages */
static char *ErrorMessages[] =
{
	{"Success"}, /* 0 */
	{"Bad arguments from form"}, /* 1 */
	{"Unable to open temporary mailfile for read"},
	{"Failed to Start Sockets"},
	{"Failed to Resolve Host"},
	{"Failed to obtain socket handle"}, /* 5 */
	{"Failed to connect to mailserver, verify your \"SMTP\" setting in php.ini"},
	{"Failed to Send"},
	{"Failed to Receive"},
	{"Server Error"},
	{"Failed to resolve the host IP name"}, /* 10 */
	{"Out of memory"},
	{"Unknown error"},
	{"Bad Message Contents"},
	{"Bad Message Subject"},
	{"Bad Message destination"}, /* 15 */
	{"Bad Message Return Path"},
	{"Bad Mail Host"},
	{"Bad Message File"},
	{"\"sendmail_from\" not set in php.ini or custom \"From:\" header missing"},
	{"Mailserver rejected our \"sendmail_from\" setting"} /* 20 */
};



/*********************************************************************
// Name:  TSendMail
// Input:   1) host:    Name of the mail host where the SMTP server resides
//                      max accepted length of name = 256
//          2) appname: Name of the application to use in the X-mailer
//                      field of the message. if NULL is given the application
//                      name is used as given by the GetCommandLine() function
//                      max accespted length of name = 100
// Output:  1) error:   Returns the error code if something went wrong or
//                      SUCCESS otherwise.
//
//  See SendText() for additional args!
//********************************************************************/
int TSendMail(char *host, int *error, char **error_message,
			  char *headers, char *Subject, char *mailTo, char *data)
{
	int ret;
	char *RPath = NULL;

	WinsockStarted = FALSE;

	if (host == NULL) {
		*error = BAD_MAIL_HOST;
		return FAILURE;
	} else if (strlen(host) >= HOST_NAME_LEN) {
		*error = BAD_MAIL_HOST;
		return FAILURE;
	} else {
		strcpy(MailHost, host);
	}

	/* use from address as return path (if specified in headers) */
	if (headers) {
		char *pos = NULL;
		/* Try to match 'From:' only at start of the string or after following a \r\n */
		if (strstr(headers, "\r\nFrom:")) {
			pos = strstr(headers, "\r\nFrom:") + 7;
		} else if (!strncmp(headers, "From:", 5)) {
			pos = headers + 5;
		}
		if (pos) {
			char *pos_end;
			/* Ignore any whitespaces */
			while (pos && ((*pos == ' ' || *pos == '\t')))
				pos++;
			/* Match until \r\n or end of header string */
			if (pos_end = strstr(pos, "\r\n")) {
				RPath = estrndup(pos, pos_end - pos);
			} else {
				RPath = estrndup(pos, strlen(pos));
			}
		}
	}
 
	/* Fall back to sendmail_from php.ini setting */
 	if (!RPath) {
		if (INI_STR("sendmail_from")) {
			RPath = estrdup(INI_STR("sendmail_from"));
		} else {
			*error = W32_SM_SENDMAIL_FROM_NOT_SET;
			return FAILURE;
		}
	}

	/* attempt to connect with mail host */
	*error = MailConnect();
	if (*error != 0) {
		if (RPath) {
			efree(RPath);
		}
		if (NULL == (*error_message = ecalloc(1, HOST_NAME_LEN + 128))) {
			return FAILURE;
		}
		snprintf(*error_message, HOST_NAME_LEN + 128, "Failed to connect to mailserver at \"%s\", verify your \"SMTP\" setting in php.ini", MailHost);
		return FAILURE;
	} else {
		ret = SendText(RPath, Subject, mailTo, data, headers, error_message);
		TSMClose();
		if (RPath) {
			efree(RPath);
		}
		if (ret != SUCCESS) {
			*error = ret;
			return FAILURE;
		}
		return SUCCESS;
	}
}

//********************************************************************
// Name:  TSendMail::~TSendMail
// Input:
// Output:
// Description: DESTRUCTOR
// Author/Date:  jcar 20/9/96
// History:
//********************************************************************/
void TSMClose()
{
	Post("QUIT\r\n");
	Ack(NULL);
	/* to guarantee that the cleanup is not made twice and 
	   compomise the rest of the application if sockets are used
	   elesewhere 
	*/

	shutdown(sc, 0); 
	closesocket(sc);
}


/*********************************************************************
// Name:  char *GetSMErrorText
// Input:   Error index returned by the menber functions
// Output:  pointer to a string containing the error description
// Description:
// Author/Date:  jcar 20/9/96
// History:
//*******************************************************************/
char *GetSMErrorText(int index)
{
	if (MIN_ERROR_INDEX <= index && index < MAX_ERROR_INDEX) {
		return (ErrorMessages[index]);

	} else {
		return (ErrorMessages[UNKNOWN_ERROR]);

	}
}


/*********************************************************************
// Name:  TSendText
// Input:       1) RPath:   return path of the message
//                                  Is used to fill the "Return-Path" and the
//                                  "X-Sender" fields of the message.
//                  2) Subject: Subject field of the message. If NULL is given
//                                  the subject is set to "No Subject"
//                  3) mailTo:  Destination address
//                  4) data:        Null terminated string containing the data to be send.
// Output:      Error code or SUCCESS
// Description:
// Author/Date:  jcar 20/9/96
// History:
//*******************************************************************/
int SendText(char *RPath, char *Subject, char *mailTo, char *data, char *headers, char **error_message)
{
	int res, i;
	char *p;
	char *tempMailTo, *token, *pos1, *pos2;
	char *server_response = NULL;

	/* check for NULL parameters */
	if (data == NULL)
		return (BAD_MSG_CONTENTS);
	if (mailTo == NULL)
		return (BAD_MSG_DESTINATION);
	if (RPath == NULL)
		return (BAD_MSG_RPATH);

	/* simple checks for the mailto address */
	/* have ampersand ? */
	/* mfischer, 20020514: I commented this out because it really
	   seems bogus. Only a username for example may still be a
	   valid address at the destination system.
	if (strchr(mailTo, '@') == NULL)
		return (BAD_MSG_DESTINATION);
	*/

	sprintf(Buffer, "HELO %s\r\n", LocalHost);

	/* in the beggining of the dialog */
	/* attempt reconnect if the first Post fail */
	if ((res = Post(Buffer)) != SUCCESS) {
		MailConnect();
		if ((res = Post(Buffer)) != SUCCESS)
			return (res);
	}
	if ((res = Ack(&server_response)) != SUCCESS) {
		SMTP_ERROR_RESPONSE(server_response);
		return (res);
	}

	sprintf(Buffer, "MAIL FROM:<%s>\r\n", RPath);
	if ((res = Post(Buffer)) != SUCCESS)
		return (res);
	if ((res = Ack(&server_response)) != SUCCESS) {
		SMTP_ERROR_RESPONSE(server_response);
		return W32_SM_SENDMAIL_FROM_MALFORMED;
	}


	tempMailTo = estrdup(mailTo);
	
	/* Send mail to all rcpt's */
	token = strtok(tempMailTo, ",");
	while(token != NULL)
	{
		sprintf(Buffer, "RCPT TO:<%s>\r\n", token);
		if ((res = Post(Buffer)) != SUCCESS)
			return (res);
		if ((res = Ack(&server_response)) != SUCCESS) {
			SMTP_ERROR_RESPONSE(server_response);
			return (res);
		}
		token = strtok(NULL, ",");
	}

	/* Send mail to all Cc rcpt's */
	efree(tempMailTo);
	if (headers && (pos1 = strstr(headers, "Cc:"))) {
		pos1 += 3; /* Jump over Cc: */
		if (NULL == (pos2 = strstr(pos1, "\r\n"))) {

			tempMailTo = estrndup(pos1, strlen(pos1));

		} else {
			tempMailTo = estrndup(pos1, pos2-pos1);

		}

		token = strtok(tempMailTo, ",");
		while(token != NULL)
		{
			sprintf(Buffer, "RCPT TO:<%s>\r\n", token);
			if ((res = Post(Buffer)) != SUCCESS)
				return (res);
			if ((res = Ack(&server_response)) != SUCCESS) {
				SMTP_ERROR_RESPONSE(server_response);
				return (res);
			}
			token = strtok(NULL, ",");
		}
		efree(tempMailTo);
	}

	if ((res = Post("DATA\r\n")) != SUCCESS)
		return (res);
	if ((res = Ack(&server_response)) != SUCCESS) {
		SMTP_ERROR_RESPONSE(server_response);
		return (res);
	}


	/* send message header */
	if (Subject == NULL)
		res = PostHeader(RPath, "No Subject", mailTo, headers, NULL);
	else
		res = PostHeader(RPath, Subject, mailTo, headers, NULL);
	if (res != SUCCESS)
		return (res);


	/* send message contents in 1024 chunks */
	if (strlen(data) <= 1024) {
		if ((res = Post(data)) != SUCCESS)
			return (res);
	} else {
		p = data;
		while (1) {
			if (*p == '\0')
				break;
			if (strlen(p) >= 1024)
				i = 1024;
			else
				i = strlen(p);

			/* put next chunk in buffer */
			strncpy(Buffer, p, i);
			Buffer[i] = '\0';
			p += i;

			/* send chunk */
			if ((res = Post(Buffer)) != SUCCESS)
				return (res);
		}
	}

	/*send termination dot */
	if ((res = Post("\r\n.\r\n")) != SUCCESS)
		return (res);
	if ((res = Ack(&server_response)) != SUCCESS) {
		SMTP_ERROR_RESPONSE(server_response);
		return (res);
	}

	return (SUCCESS);
}



/*********************************************************************
// Name:  PostHeader
// Input:       1) return path
//              2) Subject
//              3) destination address
//              4) headers
//				5) cc destination address
// Output:      Error code or Success
// Description:
// Author/Date:  jcar 20/9/96
// History:
//********************************************************************/
int PostHeader(char *RPath, char *Subject, char *mailTo, char *xheaders, char *mailCc)
{

	/* Print message header according to RFC 822 */
	/* Return-path, Received, Date, From, Subject, Sender, To, cc */

	time_t tNow = time(NULL);
	struct tm *tm = localtime(&tNow);
	int zoneh = abs(_timezone);
	int zonem, res;
	char *p;

	p = Buffer;
	zoneh /= (60 * 60);
	zonem = (abs(_timezone) / 60) - (zoneh * 60);

	if(!xheaders || !strstr(xheaders, "Date:")){
		p += sprintf(p, "Date: %s, %02d %s %04d %02d:%02d:%02d %s%02d%02d\r\n",
					 days[tm->tm_wday],
					 tm->tm_mday,
					 months[tm->tm_mon],
					 tm->tm_year + 1900,
					 tm->tm_hour,
					 tm->tm_min,
					 tm->tm_sec,
					 (_timezone <= 0) ? "+" : (_timezone > 0) ? "-" : "",
					 zoneh,
					 zonem);
	}

	if(!xheaders || !strstr(xheaders, "From:")){
		p += sprintf(p, "From: %s\r\n", RPath);
	}
	p += sprintf(p, "Subject: %s\r\n", Subject);
	p += sprintf(p, "To: %s\r\n", mailTo);
	if (mailCc && *mailCc) {
		p += sprintf(p, "Cc: %s\r\n", mailCc);
	}
	if(xheaders){
		p += sprintf(p, "%s\r\n", xheaders);
	}

	if ((res = Post(Buffer)) != SUCCESS)
		return (res);

	if ((res = Post("\r\n")) != SUCCESS)
		return (res);

	return (SUCCESS);
}



/*********************************************************************
// Name:  MailConnect
// Input:   None
// Output:  None
// Description: Connect to the mail host and receive the welcome message.
// Author/Date:  jcar 20/9/96
// History:
//********************************************************************/
int MailConnect()
{

	int res;
	short portnum;

	/* Create Socket */
	if ((sc = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		return (FAILED_TO_OBTAIN_SOCKET_HANDLE);

	/* Get our own host name */
	if (gethostname(LocalHost, HOST_NAME_LEN))
		return (FAILED_TO_GET_HOSTNAME);

	/* Resolve the servers IP */
	/*
	if (!isdigit(MailHost[0])||!gethostbyname(MailHost))
	{
		return (FAILED_TO_RESOLVE_HOST);
	}
        */

	portnum = (short) INI_INT("sendmail_port");
	if (!portnum) {
		portnum = 25;
	}

	/* Connect to server */
	sock_in.sin_family = AF_INET;
	sock_in.sin_port = htons(portnum);
	sock_in.sin_addr.S_un.S_addr = GetAddr(MailHost);

	if (connect(sc, (LPSOCKADDR) & sock_in, sizeof(sock_in)))
		return (FAILED_TO_CONNECT);

	/* receive Server welcome message */
	res = Ack(NULL);
	return (res);
}






/*********************************************************************
// Name:  Post
// Input:
// Output:
// Description:
// Author/Date:  jcar 20/9/96
// History:
//********************************************************************/
int Post(LPCSTR msg)
{
	int len = strlen(msg);
	int slen;
	int index = 0;

	while (len > 0) {
		if ((slen = send(sc, msg + index, len, 0)) < 1)
			return (FAILED_TO_SEND);
		len -= slen;
		index += slen;
	}
	return (SUCCESS);
}



/*********************************************************************
// Name:  Ack
// Input:
// Output:
// Description:
// Get the response from the server. We only want to know if the
// last command was successful.
// Author/Date:  jcar 20/9/96
// History:
//********************************************************************/
int Ack(char **server_response)
{
	static char buf[MAIL_BUFFER_SIZE];
	int rlen;
	int Index = 0;
	int Received = 0;

  again:

	if ((rlen = recv(sc, buf + Index, ((MAIL_BUFFER_SIZE) - 1) - Received, 0)) < 1)
		return (FAILED_TO_RECEIVE);

	Received += rlen;
	buf[Received] = 0;
	/*err_msg   fprintf(stderr,"Received: (%d bytes) %s", rlen, buf + Index); */

	/* Check for newline */
	Index += rlen;
	
	if ((buf[Received - 4] == ' ' && buf[Received - 3] == '-') ||
	    (buf[Received - 2] != '\r') || (buf[Received - 1] != '\n'))
		/* err_msg          fprintf(stderr,"Incomplete server message. Awaiting CRLF\n"); */
		goto again;				/* Incomplete data. Line must be terminated by CRLF
		                           And not contain a space followed by a '-' */

	if (buf[0] > '3') {
		/* If we've a valid pointer, return the SMTP server response so the error messages give away more information */
		if (server_response) {
			int dec = 0;
			/* See if we have something like \r, \n, \r\n or \n\r at the end of the message and chop it off */
			if (Received > 2) {
				if (buf[Received-1] == '\n' || buf[Received-1] == '\r') {
					dec++;
					if (buf[Received-2] == '\r' || buf[Received-2] == '\n') {
						dec++;
					}
				}

			}
			*server_response = estrndup(buf, Received - dec);
		}
		return (SMTP_SERVER_ERROR);
	}

	return (SUCCESS);
}


/*********************************************************************
// Name:  unsigned long GetAddr (LPSTR szHost)
// Input:
// Output:
// Description: Given a string, it will return an IP address.
//   - first it tries to convert the string directly
//   - if that fails, it tries o resolve it as a hostname
//
// WARNING: gethostbyname() is a blocking function
// Author/Date:  jcar 20/9/96
// History:
//********************************************************************/
unsigned long GetAddr(LPSTR szHost)
{
	LPHOSTENT lpstHost;
	u_long lAddr = INADDR_ANY;

	/* check that we have a string */
	if (*szHost) {

		/* check for a dotted-IP address string */
		lAddr = inet_addr(szHost);

		/* If not an address, then try to resolve it as a hostname */
		if ((lAddr == INADDR_NONE) && (strcmp(szHost, "255.255.255.255"))) {

			lpstHost = gethostbyname(szHost);
			if (lpstHost) {		/* success */
				lAddr = *((u_long FAR *) (lpstHost->h_addr));
			} else {
				lAddr = INADDR_ANY;		/* failure */
			}
		}
	}
	return (lAddr);
}								/* end GetAddr() */
