/*
 * Copyright (c) 2002, Adam Dunkels.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Adam Dunkels.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.  
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: smtp.c,v 1.2 2003/04/10 09:04:50 adamdunkels Exp $
 *
 */

#include "uip.h"
#include "smtp.h"

#include "smtp-strings.h"

#include <string.h>

#define STATE_SEND_NONE         0
#define STATE_SEND_HELO         1
#define STATE_SEND_MAIL_FROM    2
#define STATE_SEND_RCPT_TO      3
#define STATE_SEND_DATA         4
#define STATE_SEND_DATA_HEADERS 5
#define STATE_SEND_DATA_MESSAGE 6
#define STATE_SEND_DATA_END     7
#define STATE_SEND_QUIT         8
#define STATE_SEND_DONE         9

static char *localhostname;
static u16_t smtpserver[2];



#define ISO_nl 0x0a
#define ISO_cr 0x0d

#define ISO_period 0x2e

#define ISO_2  0x32
#define ISO_3  0x33
#define ISO_4  0x34
#define ISO_5  0x35


struct smtp_state {
  u8_t state;
  char *to;
  char *from;
  char *subject;
  char *msg;
  u16_t msglen;
  
  u16_t sentlen, textlen;
  u16_t sendptr;

};


static struct smtp_state s;

/*-----------------------------------------------------------------------------------*/
static void
senddata(void)    
{
  char *textptr;

  if(s.textlen != 0 &&
     s.textlen == s.sendptr) {
    return;
  }

  textptr = (char *)uip_appdata;
  switch(s.state) {
  case STATE_SEND_HELO:
    /* Create HELO message. */
    strcpy(textptr, smtp_helo);
    textptr += sizeof(smtp_helo) - 1;
    strcpy(textptr, localhostname);
    textptr += strlen(localhostname);
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    /*    printf("s.sendptr %d\n", s.sendptr);*/
    if(s.sendptr == 0) {
      s.textlen = textptr - (char *)uip_appdata;
      /*      printf("s.textlen %d\n", s.textlen);*/
    }
    textptr = (char *)uip_appdata;
    break;
  case STATE_SEND_MAIL_FROM:
    /* Create MAIL FROM message. */
    strcpy(textptr, smtp_mail_from);
    textptr += sizeof(smtp_mail_from) - 1;
    strcpy(textptr, s.from);
    textptr += strlen(s.from);
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    if(s.sendptr == 0) {
      s.textlen = textptr - (char *)uip_appdata;
    }
    textptr = (char *)uip_appdata;
    break;
  case STATE_SEND_RCPT_TO:
    /* Create RCPT_TO message. */
    strcpy(textptr, smtp_rcpt_to);
    textptr += sizeof(smtp_rcpt_to) - 1;
    strcpy(textptr, s.to);
    textptr += strlen(s.to);
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    if(s.sendptr == 0) {
      s.textlen = textptr - (char *)uip_appdata;
    }
    textptr = (char *)uip_appdata;
    break;
  case STATE_SEND_DATA:
    strcpy(textptr, smtp_data);
    textptr += sizeof(smtp_data) - 1;
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    if(s.sendptr == 0) {
      s.textlen = textptr - (char *)uip_appdata;
    }
    textptr = (char *)uip_appdata;
    break;
  case STATE_SEND_DATA_HEADERS:
    /* Create mail headers. */
    strcpy(textptr, smtp_to);
    textptr += sizeof(smtp_to) - 1;
    strcpy(textptr, s.to);
    textptr += strlen(s.to);
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    
    strcpy(textptr, smtp_from);
    textptr += sizeof(smtp_from) - 1;
    strcpy(textptr, s.from);
    textptr += strlen(s.from);
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    
    strcpy(textptr, smtp_subject);
    textptr += sizeof(smtp_subject) - 1;
    strcpy(textptr, s.subject);
    textptr += strlen(s.subject);
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    
    if(s.sendptr == 0) {
      s.textlen = textptr - (char *)uip_appdata;
    }
    textptr = (char *)uip_appdata;
    break;
  case STATE_SEND_DATA_MESSAGE:
    textptr = s.msg;
    if(s.sendptr == 0) {
      s.textlen = s.msglen;
    } 
    break;
  case STATE_SEND_DATA_END:
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    *textptr = ISO_period;
    ++textptr;
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    if(s.sendptr == 0) {
      s.textlen = 5;
    }
    textptr = (char *)uip_appdata;
    break;
  case STATE_SEND_QUIT:
    strcpy(textptr, smtp_quit);
    textptr += sizeof(smtp_quit) - 1;
    *textptr = ISO_cr;
    ++textptr;
    *textptr = ISO_nl;
    ++textptr;
    if(s.sendptr == 0) {
      s.textlen = textptr - (char *)uip_appdata;
    }
    textptr = (char *)uip_appdata;
    break;
  default:
    return;
  }

  textptr += s.sendptr;

  /*  printf("Senidng '%s'\n", textptr);*/
  
  if(s.textlen - s.sendptr > uip_mss()) {
    s.sentlen = uip_mss();
  } else {
    s.sentlen = s.textlen - s.sendptr;
  }
  uip_send(textptr, s.sentlen);
}
/*-----------------------------------------------------------------------------------*/
static void
acked(void)    
{
  s.sendptr += s.sentlen;
  s.sentlen = 0;

  if(s.sendptr == s.textlen) {
    switch(s.state) {
    case STATE_SEND_DATA_HEADERS:
      s.state = STATE_SEND_DATA_MESSAGE;
      break;
    case STATE_SEND_DATA_MESSAGE:
      s.state = STATE_SEND_DATA_END;
      break;
    case STATE_SEND_DATA_END:
      s.state = STATE_SEND_QUIT;
      break;
    case STATE_SEND_QUIT:
      s.state = STATE_SEND_DONE;
      smtp_done(SMTP_ERR_OK);
      uip_close();
      break;
    }
    s.sendptr = s.textlen = 0;
  }
}
/*-----------------------------------------------------------------------------------*/
static void
newdata(void)
{
  if(*(char *)uip_appdata == ISO_5) {
    smtp_done(1);
    uip_abort();
    return;
  }
  /*  printf("Got %d bytes: '%s'\n", uip_datalen(),
      uip_appdata);*/
  switch(s.state) {
  case STATE_SEND_NONE:       
    if(strncmp((char *)uip_appdata, smtp_220, 3) == 0) {
      /*      printf("Got 220\n");*/
      s.state = STATE_SEND_HELO;
    }
    break;
  case STATE_SEND_HELO:
    if(*(char *)uip_appdata == ISO_2) {
      /*      printf("2\n");*/
      s.state = STATE_SEND_MAIL_FROM;
      s.sendptr = 0;
    }    
    break;
  case STATE_SEND_MAIL_FROM:
    if(*(char *)uip_appdata == ISO_2) {
      /*      printf("2\n");*/
      s.state = STATE_SEND_RCPT_TO;
      s.textlen = s.sendptr = 0;
    }
    break;
  case STATE_SEND_RCPT_TO:
    if(*(char *)uip_appdata == ISO_2) {
      /*      printf("2\n");*/
      s.state = STATE_SEND_DATA;
      s.textlen = s.sendptr = 0;
    }
    break;
  case STATE_SEND_DATA:
    if(*(char *)uip_appdata == ISO_3) {
      /*      printf("3\n");*/
      s.state = STATE_SEND_DATA_HEADERS;
      s.textlen = s.sendptr = 0;
    }
    break;
  case STATE_SEND_DATA_HEADERS:    
    if(*(char *)uip_appdata == ISO_3) {
      /*      printf("3\n");*/
      s.state = STATE_SEND_DATA_MESSAGE;
      s.textlen = s.sendptr = 0;
    }
    break;
  }
    
}
/*-----------------------------------------------------------------------------------*/
DISPATCHER_UIPCALL(smtp_appcall, state)
{
  if(uip_connected()) {
    /*    senddata();*/
    return;
  }
  if(uip_acked()) {
    acked();
  }
  if(uip_newdata()) {    
    newdata();
  }
  if(uip_rexmit() ||
     uip_newdata() ||
     uip_acked()) {
    senddata();
  } else if(uip_poll()) {    
    senddata();
  }
  /*  if(uip_closed()) {
    printf("Dnoe\n");
    }*/


}
/*-----------------------------------------------------------------------------------*/
unsigned char
smtp_send(char *to, char *from, char *subject,
	  char *msg, u16_t msglen)
{
  struct uip_conn *conn;

  conn = uip_connect(smtpserver, 25);
  if(conn == NULL) {
    return 0;
  }
  dispatcher_markconn(conn, NULL);
  
  s.state = STATE_SEND_NONE;
  s.sentlen = s.sendptr = s.textlen = 0;
  s.to = to;
  s.from = from;
  s.subject = subject;
  s.msg = msg;
  s.msglen = msglen;

  return 1;
}
/*-----------------------------------------------------------------------------------*/
void
smtp_configure(char *lhostname, u16_t *server)
{
  localhostname = lhostname;
  smtpserver[0] = server[0];
  smtpserver[1] = server[1];
}
/*-----------------------------------------------------------------------------------*/
