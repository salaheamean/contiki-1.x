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
 * $Id: cfs-init.c,v 1.1 2006/04/17 15:11:58 kthacker Exp $
 */
#include "contiki.h"

#include "cfs.h"
#include "cfs-service.h"

#include <cbm.h>
#include <string.h>

static int  s_open(const char *n, int f);
static void s_close(int f);
static int  s_read(int f, char *b, unsigned int l);
static int  s_write(int f, char *b, unsigned int l) {return -1;}
static int  s_opendir(struct cfs_dir *p, const char *n) {return -1;}
static int  s_readdir(struct cfs_dir *p, struct cfs_dirent *e) {return -1;}
static int  s_closedir(struct cfs_dir *p) {return -1;}

static const struct cfs_service_interface interface =
  {
    CFS_SERVICE_VERSION,
    s_open,
    s_close,
    s_read,
    s_write,
    s_opendir,
    s_readdir,
    s_closedir
  };

EK_EVENTHANDLER(eventhandler, ev, data);
EK_PROCESS(proc, CFS_SERVICE_NAME ": init", EK_PRIO_NORMAL,
           eventhandler, NULL, (void *)&interface);

/*---------------------------------------------------------------------------*/
EK_PROCESS_INIT(cfs_init_init, arg)
{
  arg_free(arg);
  ek_service_start(CFS_SERVICE_NAME, &proc);
}
/*---------------------------------------------------------------------------*/
EK_EVENTHANDLER(eventhandler, ev, data)
{
  switch(ev) {
  case EK_EVENT_INIT:
    break;
  case EK_EVENT_REQUEST_REPLACE:
    ek_replace((struct ek_proc *)data, &interface);
    break;
  case EK_EVENT_REQUEST_EXIT:
    ek_exit();
    break;
  }
}
/*---------------------------------------------------------------------------*/
static int
s_open(const char *n, int f)
{
  if(cbm_open(2, 8, CBM_READ, n) == 0) {
    return 2;
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static void
s_close(int f)
{
  cbm_close(f);
}
/*---------------------------------------------------------------------------*/
static int
s_read(int f, char *b, unsigned int l)
{
  return cbm_read(f, b, l);
}
/*---------------------------------------------------------------------------*/
