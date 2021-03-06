/*
 * Copyright (c) 2004, Adam Dunkels.
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is part of the Contiki operating system.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: popc.c,v 1.3 2005/02/22 22:23:08 adamdunkels Exp $
 */

#include "contiki.h"
#include "popc.h"
#include "popc-strings.h"

#define SEND_STRING(s, str) PSOCK_SEND(s, str, strlen(str))

enum {
  COMMAND_NONE,
  COMMAND_RETR,
  COMMAND_TOP,  
  COMMAND_QUIT
};

/*---------------------------------------------------------------------------*/
static
PT_THREAD(init_connection(struct popc_state *s))
{
  PSOCK_BEGIN(&s->s);

  PSOCK_READTO(&s->s, '\n');
  if(s->inputbuf[0] != '+') {
    PSOCK_CLOSE_EXIT(&s->s);    
  }

  SEND_STRING(&s->s, popc_strings_user);
  SEND_STRING(&s->s, s->user);
  SEND_STRING(&s->s, popc_strings_crnl);
  
  PSOCK_READTO(&s->s, '\n');
  if(s->inputbuf[0] != '+') {
    PSOCK_CLOSE_EXIT(&s->s);    
  }

  SEND_STRING(&s->s, popc_strings_pass);
  SEND_STRING(&s->s, s->pass);
  SEND_STRING(&s->s, popc_strings_crnl);
  
  PSOCK_READTO(&s->s, '\n');
  if(s->inputbuf[0] != '+') {
    PSOCK_CLOSE_EXIT(&s->s);    
  }

  popc_connected(s);

  PSOCK_END(&s->s);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(stat(struct popc_state *s))
{
  unsigned short num;
  unsigned long size;
  char *ptr;
  
  PSOCK_BEGIN(&s->s);
  
  SEND_STRING(&s->s, popc_strings_stat);
  
  PSOCK_READTO(&s->s, '\n');
  if(s->inputbuf[0] != '+') {
    PSOCK_CLOSE_EXIT(&s->s);    
  }

  num = 0;
  for(ptr = &s->inputbuf[4]; *ptr >= '0' && *ptr <= '9'; ++ptr) {
    num *= 10;
    num += *ptr - '0';
  }

  size = 0;
  for(ptr = ptr + 1; *ptr >= '0' && *ptr <= '9'; ++ptr) {
    size *= 10;
    size += *ptr - '0';
  }

  popc_messages(s, num, size);

  PSOCK_END(&s->s);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(retr(struct popc_state *s))
{
  PSOCK_BEGIN(&s->s);

  SEND_STRING(&s->s, popc_strings_retr);
  snprintf(s->outputbuf, sizeof(s->outputbuf), "%d", s->num);
  SEND_STRING(&s->s, s->outputbuf);
  SEND_STRING(&s->s, popc_strings_crnl);
  
  PSOCK_READTO(&s->s, '\n');
  if(s->inputbuf[0] != '+') {
    PSOCK_CLOSE_EXIT(&s->s);    
  }

  popc_msgbegin(s);
  while(s->inputbuf[0] != '.') {
    PSOCK_READTO(&s->s, '\n');
    if(s->inputbuf[0] != '.') {
      s->inputbuf[PSOCK_DATALEN(&s->s)] = 0;
      popc_msgline(s, s->inputbuf, PSOCK_DATALEN(&s->s));
    }
  }
  popc_msgend(s);
  
  PSOCK_END(&s->s);

}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(handle_connection(struct popc_state *s))
{
  PT_BEGIN(&s->pt);

  PSOCK_INIT(&s->s, s->inputbuf, sizeof(s->inputbuf) - 1);

  PT_WAIT_UNTIL(&s->pt, init_connection(s));
  PT_WAIT_UNTIL(&s->pt, stat(s));  

  timer_set(&s->timer, CLOCK_SECOND * 30);
	
  while(1) {
    PT_WAIT_UNTIL(&s->pt, s->command != COMMAND_NONE ||
		  timer_expired(&s->timer));

    if(timer_expired(&s->timer)) {
      PT_WAIT_UNTIL(&s->pt, stat(s));
      timer_set(&s->timer, CLOCK_SECOND * 30);
    }

    switch(s->command) {
    case COMMAND_RETR:
      PT_WAIT_UNTIL(&s->pt, retr(s));
      break;
    case COMMAND_QUIT:
      tcp_markconn(uip_conn, NULL);
      PSOCK_CLOSE(&s->s);
      PT_EXIT(&s->pt);
      break;
    default:
      break;
    }
    s->command = COMMAND_NONE;
    
  }
  PT_END(&s->pt);
}
/*---------------------------------------------------------------------------*/
void
popc_appcall(void *state)
{
  struct popc_state *s = (struct popc_state *)state;
  
  if(uip_closed() || uip_aborted() || uip_timedout()) {
    popc_closed(s);
  } else if(uip_connected()) {
    PT_INIT(&s->pt);
    handle_connection(s);
  } else if(s != NULL) {
    handle_connection(s);
  }

}
/*---------------------------------------------------------------------------*/
void *
popc_connect(struct popc_state *s, u16_t *addr,
	     char *user, char *pass)
{
  strncpy(s->user, user, sizeof(s->user));
  strncpy(s->pass, pass, sizeof(s->pass));
  s->conn = tcp_connect(addr, HTONS(110), s);
  return s->conn;
}
/*---------------------------------------------------------------------------*/
void
popc_retr(struct popc_state *s, unsigned short num)
{
  s->command = COMMAND_RETR;
  s->num = num;
}
/*---------------------------------------------------------------------------*/
void
popc_top(struct popc_state *s, unsigned short num, unsigned short lines)
{
  s->command = COMMAND_TOP;
  s->num = num;
  s->lines = lines;
}
/*---------------------------------------------------------------------------*/
