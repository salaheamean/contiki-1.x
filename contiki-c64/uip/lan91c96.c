/*
 * uIP lan91c96 (smc9194) driver
 * Based on cs8900a driver, copyrighted (c) 2001, by Adam Dunkels
 * Copyright (c) 2003, Josef Soucek
 * All rights reserved.
 *
 * Ethernet card for Commodore 64, based on lan91c96 chip
 * is a device created by IDE64 Project team.
 * More information: http://ide64.come.to
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
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
 * $Id: lan91c96.c,v 1.6 2005/01/26 23:36:23 oliverschmidt Exp $
 *
 */

#define UIP_ETHADDR0 0x00
#define UIP_ETHADDR1 0x0d
#define UIP_ETHADDR2 0x60
#define UIP_ETHADDR3 0x80
#define UIP_ETHADDR4 0x3d
#define UIP_ETHADDR5 0xb9

#include "lan91c96.h"
#include "uip.h"
#include "uip_arp.h"

#include <stdio.h>

// #define DEBUG

#define ETHBASE 0xde10

#define ETHBSR     ETHBASE+0x0e  //Bank select register             R/W (2B)

/* Register bank 0 */

#define ETHTCR     ETHBASE       //Transmition control register     R/W (2B)
#define ETHEPHSR   ETHBASE+2     //EPH status register              R/O (2B)
#define ETHRCR     ETHBASE+4     //Receive control register         R/W (2B)
#define ETHECR     ETHBASE+6     //Counter register                 R/O (2B)
#define ETHMIR     (ETHBASE+8)     //Memory information register      R/O (2B)
#define ETHMCR     ETHBASE+0x0a  //Memory Config. reg.    +0 R/W +1 R/O (2B)

/* Register bank 1 */

#define ETHCR      ETHBASE       //Configuration register           R/W (2B)
#define ETHBAR     ETHBASE+2     //Base address register            R/W (2B)
#define ETHIAR     ETHBASE+4     //Individual address register      R/W (6B)
#define ETHGPR     ETHBASE+0x0a  //General address register         R/W (2B)
#define ETHCTR     ETHBASE+0x0c  //Control register                 R/W (2B)

/* Register bank 2 */

#define ETHMMUCR   ETHBASE       //MMU command register             W/O (1B)
#define ETHAUTOTX  ETHBASE+1     //AUTO TX start register           R/W (1B)
#define ETHPNR     ETHBASE+2     //Packet number register           R/W (1B)
#define ETHARR     ETHBASE+3     //Allocation result register       R/O (1B)
#define ETHFIFO    ETHBASE+4     //FIFO ports register              R/O (2B)
#define ETHPTR     ETHBASE+6     //Pointer register                 R/W (2B)
#define ETHDATA    ETHBASE+8     //Data register                    R/W (4B)
#define ETHIST     (ETHBASE+0x0c)  //Interrupt status register        R/O (1B)
#define ETHACK     ETHBASE+0x0c  //Interrupt acknowledge register   W/O (1B)
#define ETHMSK     ETHBASE+0x0d  //Interrupt mask register          R/W (1B)

/* Register bank 3 */

#define ETHMT      ETHBASE       //Multicast table                  R/W (8B)
#define ETHMGMT    ETHBASE+8     //Management interface             R/W (2B)
#define ETHREV     ETHBASE+0x0a  //Revision register                R/W (2B)
#define ETHERCV    ETHBASE+0x0c  //Early RCV register               R/W (2B)

#define BANK(num) asm("lda #%b",num); asm("sta %w",ETHBSR);

#ifdef DEBUG
static void print_packet(u8_t *, u16_t);
#endif

static u8_t packet_status;
static u16_t packet_length;

extern u16_t uip_len;


#pragma optimize(push, off)
void lan91c96_init(void)
{
  /* Check if high byte is 0x33 */
  asm("lda %w", ETHBSR+1);
  asm("cmp #$33");
  asm("beq @L1");

  asm("inc $d021");              // Error

  asm("@L1:");

  /* Reset ETH card */
  BANK(0);
  asm("lda #%%10000000");        //Software reset
  asm("sta %w", ETHRCR+1);

  asm("lda #0");
  asm("sta %w", ETHRCR);
  asm("sta %w", ETHRCR+1);

  /* delay */
  asm("ldx #0");
  asm("@L2:");
  asm("cmp ($ff,x)");            //6 cycles
  asm("cmp ($ff,x)");            //6 cycles
  asm("dex");                    //2 cycles
  asm("bne @L2");                //3 cycles
                                 //17*256=4352 => 4,4 ms

  /* Enable transmit and receive */
  asm("lda #%%10000001");        //Enable transmit TXENA, PAD_EN
  asm("sta %w", ETHTCR);
  asm("lda #%%00000011");        //Enable receive, strip CRC ???
  asm("sta %w", ETHRCR+1);

  BANK(1);
  asm("lda %w", ETHCR+1);
  asm("ora #%%00010000");        //No wait (IOCHRDY)
  asm("sta %w", ETHCR+1);

  asm("lda #%%00001001");        //Auto release
  asm("sta %w", ETHCTR+1);
  
  /* Set MAC address */
  asm("lda #%b", (unsigned)UIP_ETHADDR0);
  asm("sta %w", ETHIAR);
  asm("lda #%b", (unsigned)UIP_ETHADDR1);
  asm("sta %w", ETHIAR+1);
  asm("lda #%b", (unsigned)UIP_ETHADDR2);
  asm("sta %w", ETHIAR+2);
  asm("lda #%b", (unsigned)UIP_ETHADDR3);
  asm("sta %w", ETHIAR+3);
  asm("lda #%b", (unsigned)UIP_ETHADDR4);
  asm("sta %w", ETHIAR+4);
  asm("lda #%b", (unsigned)UIP_ETHADDR5);
  asm("sta %w", ETHIAR+5);

  BANK(2);
  asm("lda #%%00001111");               //RCV INT, ALLOC INT, TX INT, TX EMPTY 
  asm("sta %w", ETHMSK);
}
#pragma optimize(pop)


#pragma optimize(push, off)
u16_t lan91c96_poll(void)
{
  // #######
//  BANK(0);
//  printf("RAM: %d ", ((*(unsigned int *)ETHMIR) & 0xff00));
//  BANK(2);
  // #######

  asm("lda %w", ETHIST);
  asm("and #%%00000001");                //RCV INT
  asm("bne @L1");

  /* No packet available */
  return 0;

  asm("@L1:");

  #ifdef DEBUG
  printf("RCV: IRQ\n");
  #endif

  asm("lda #0");
  asm("sta %w", ETHPTR);
  asm("lda #%%11100000");               //RCV,AUTO INCR.,READ
  asm("sta %w", ETHPTR+1);

  asm("lda %w", ETHDATA);               //Status word
  asm("lda %w", ETHDATA);
  asm("sta _packet_status");            //High byte only

  asm("lda %w", ETHDATA);               //Total number of bytes
  asm("sta _packet_length");
  asm("lda %w", ETHDATA);
  asm("sta _packet_length+1");

  /* Last word contain 'last data byte' and 0x60 */
  /* or 'fill byte' and 0x40 */

  packet_length -= 6;            //The packet contains 3 extra words

  asm("lda _packet_status");
  asm("and #$10");
  asm("beq @L2");

  packet_length++;

  #ifdef DEBUG
  printf("RCV: odd number of bytes\n");
  #endif

  asm("@L2:");
                    
  #ifdef DEBUG
  printf("RCV: L:%d ST-HIGH:0x%02x ",packet_length,packet_status);
  #endif

  if (packet_length > UIP_BUFSIZE)
  {
    /* Remove and release RX packet from FIFO*/ 
    asm("lda #%%10000000");
    asm("sta %w", ETHMMUCR);

    #ifdef DEBUG
    printf("RCV: UIP_BUFSIZE exceeded - packet dropped!\n");
    #endif

    return 0;
  }

  asm("lda #<_uip_buf");
  asm("sta ptr1");
  asm("lda #>_uip_buf");
  asm("sta ptr1+1");

  asm("ldy #0");
  asm("ldx _packet_length+1");
  asm("beq @RE1");               //packet_length < 256

  asm("@RL1:");
  asm("lda %w", ETHDATA);
  asm("sta (ptr1),y");
  asm("iny");
  asm("bne @RL1");
  asm("inc ptr1+1");
  asm("dex");
  asm("bne @RL1");

  asm("@RE1:");
  asm("lda %w", ETHDATA);
  asm("sta (ptr1),y");
  asm("iny");
  asm("cpy _packet_length");
  asm("bne @RE1");

  /* Remove and release RX packet from FIFO*/ 
  asm("lda #%%10000000");
  asm("sta %w", ETHMMUCR);

  #ifdef DEBUG
//  print_packet(uip_buf, packet_length);
  #endif

  return packet_length;
}
#pragma optimize(pop)

/* First 40+14 (IP nad TCP header) is send from uip_buf */
/* than data from uip_appdata                           */

#pragma optimize(push, off)
void lan91c96_send(void)
{
  #ifdef DEBUG
  printf("SND: send packet\n");
  #endif

  asm("lda _uip_len+1");  
  asm("ora #%%00100000");        //Allocate memory for TX
  asm("sta %w", ETHMMUCR);

  asm("ldx #8");                 //Wait...
  asm("@L1:");                   //Wait for allocation ready
  asm("lda %w", ETHIST);
  asm("and #%%00001000");        //ALLOC INT
  asm("bne @X1");
  asm("dex");
  asm("bne @L1");

    #ifdef DEBUG
    printf("SND: ERR: memory alloc timeout\n");
    #endif

    return;

  asm("@X1:");
  #ifdef DEBUG
  printf("SND: packet memory allocated\n");
  #endif

  asm("lda #%%00001000");        //Acknowledge int, is it necessary ???
  asm("sta %w", ETHACK);

  asm("lda %w", ETHARR);
  asm("sta %w", ETHPNR);         //Set packet address

  asm("lda #0");
  asm("sta %w", ETHPTR);
  asm("lda #%%01000000");        //AUTO INCR.
  asm("sta %w", ETHPTR+1);

  #ifdef DEBUG
  printf("SND: L:%d ", uip_len);
  #endif

  asm("lda #0");                 //Status written by CSMA
  asm("sta %w", ETHDATA);
  asm("sta %w", ETHDATA);

  asm("lda _uip_len");
  asm("and #$01");
  asm("beq @SD1");

    packet_length=uip_len+5;
    asm("jmp @LC1");

  asm("@SD1:");

    packet_length=uip_len+6;       //+6 for status word, length and ctl byte

  asm("@LC1:");

//  printf("SND: L:%d ", packet_length);

  asm("lda _packet_length");
  asm("sta %w", ETHDATA);
  asm("lda _packet_length+1");
  asm("sta %w", ETHDATA);

  #ifdef DEBUG
//  print_packet(uip_buf, uip_len);
  #endif

  /* Send 40+14=54 bytes of header */

  if(uip_len <= 54) {

    #ifdef DEBUG
    printf("SND: short packet sent.\n");
    #endif

    asm("ldx _uip_len");
    asm("ldy #0");
    asm("@WL1:");
    asm("lda _uip_buf,y");
    asm("sta %w", ETHDATA);
    asm("iny");
    asm("dex");
    asm("bne @WL1");

  } else {

    asm("ldx #54");
    asm("ldy #0");
    asm("@WL2:");
    asm("lda _uip_buf,y");
    asm("sta %w", ETHDATA);
    asm("iny");
    asm("dex");
    asm("bne @WL2");

    uip_len -= 54;

    asm("lda _uip_appdata");       //uip_appdata is pointer
    asm("sta ptr1");
    asm("lda _uip_appdata+1");
    asm("sta ptr1+1");

    asm("ldy #0");
    asm("ldx _uip_len+1");
    asm("beq @RE1");               //packet_length < 256

    asm("@RL1:");
    asm("lda (ptr1),y");
    asm("sta %w", ETHDATA);
    asm("iny");
    asm("bne @RL1");
    asm("inc ptr1+1");
    asm("dex");
    asm("bne @RL1");

    asm("@RE1:");
    asm("lda (ptr1),y");
    asm("sta %w", ETHDATA);
    asm("iny");
    asm("cpy _uip_len");
    asm("bne @RE1");

  }

  asm("lda _uip_len");
  asm("and #$01");
  asm("beq @R3");

  asm("lda #%%00100000");
  asm("sta %w", ETHDATA);        //Control byte

  asm("lda #%%11000000");        //ENQUEUE PACKET - transmit packet
  asm("sta %w", ETHMMUCR);

//  printf("\n## %02x", *(unsigned char *)ETHIST);

  return;

  asm("@R3:");

  asm("lda #0");
  asm("sta %w", ETHDATA);        //Fill byte
  asm("sta %w", ETHDATA);        //Control byte

  asm("lda #%%11000000");        //ENQUEUE PACKET - transmit packet
  asm("sta %w", ETHMMUCR);

//  printf("\n## %02x\n", *(unsigned char *)ETHIST);
  return;
}
#pragma optimize(pop)

#ifdef DEBUG
static void print_packet(u8_t *buf, u16_t length)
{
  int i;
  int remainder;
  int lines;
  u8_t a;
  int cur;
  int address=0;

  printf("\nPacket of length %d \n", length );

  lines = length / 8;
  remainder = length % 8;

  for ( i = 0; i < lines ; i ++ ) {
    printf(":%04x ", address=i*8);

    for ( cur = 0; cur < 8; cur ++ ) {
      a = *(buf ++ );
      printf("%02x ", a);
    }
    printf("\n");
  }

  printf(":%04x ", address+8);

  for ( i = 0; i < remainder ; i++ ) {
    a = *(buf ++ );
    printf("%02x ", a);
  }
  printf("\n");
}
#endif /* DEBUG */

