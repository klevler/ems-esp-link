/*
 * File : uart.c
 * This file is part of Espressif's AT+ command set program.
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 * Heavily modified and enhanced by Thorsten von Eicken in 2015
 */
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "uart.h"
#include "ems.h"

#define recvTaskPrio        0
#define recvTaskQueueLen    64

// UartDev is defined and initialized in rom code.
extern UartDevice    UartDev;

os_event_t    recvTaskQueue[recvTaskQueueLen];

#define MAX_CB 4
static UartRecv_cb uart_recv_cb[4];

static void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
static void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
  if (uart_no == UART1) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    //PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO2_U);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  } else {
    /* rcv_buff size is 0x100 */
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
    PIN_PULLUP_DIS (PERIPHS_IO_MUX_U0TXD_U);
    //PIN_PULLDWN_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
    PIN_PULLUP_DIS (PERIPHS_IO_MUX_U0RXD_U);
    //PIN_PULLDWN_DIS(PERIPHS_IO_MUX_U0RXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 0); // FUNC_U0RXD==0
  }

  uart_div_modify(uart_no, UART_CLK_FREQ / UartDev.baut_rate);

  if (uart_no == UART1)  //UART 1 always 8 N 1
    WRITE_PERI_REG(UART_CONF0(uart_no),
        CALC_UARTMODE(EIGHT_BITS, NONE_BITS, ONE_STOP_BIT));
  else
    WRITE_PERI_REG(UART_CONF0(uart_no),
        CALC_UARTMODE(UartDev.data_bits, UartDev.parity, UartDev.stop_bits));

  //clear rx and tx fifo,not ready
  SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);

  if (uart_no == UART0) {
    // Configure RX interrupt conditions as follows:
    //    trigger rx-full when there are 32 characters in the buffer
    //    trigger rx-timeout when the fifo is non-empty and nothing further
    //      has been received for 2 character periods.
    //    trigger rx-brk
    // no hardware flow-control
    // We do not enable framing error interrupts 'cause they tend to cause an interrupt avalanche
    // and instead just poll for them when we get a std RX interrupt.
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((64 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                   (2 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
                   UART_RX_TOUT_EN);
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA | UART_BRK_DET_INT_ENA);
  } else {
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));
  }

  //clear all interrupt
  WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
}

/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description  : Internal used function
 *                Use uart1 interface to transfer one char
 * Parameters   : uint8 TxChar - character to tx
 * Returns      : OK
*******************************************************************************/
STATUS
uart_tx_one_char(uint8 uart, uint8 c)
{
  //Wait until there is room in the FIFO
  while (((READ_PERI_REG(UART_STATUS(uart))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT)>=100) ;
  //Send the character
  WRITE_PERI_REG(UART_FIFO(uart), c);
  return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
  uart_tx_one_char(UART1, c);
}

void ICACHE_FLASH_ATTR
uart0_write_char(char c)
{
  uart_tx_one_char(UART0, c);
}
/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(char *buf, uint16 len)
{
  uint16 i;

  for (i = 0; i < len; i++)
  {
    uart_tx_one_char(UART0, buf[i]);
  }
}

/******************************************************************************
 * FunctionName : uart0_sendStr
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_sendStr(const char *str)
{
  while(*str)
  {
    uart_tx_one_char(UART0, *str++);
  }
}

/******************************************************************************
 * FunctionName : uart0_sendBrk
 * Description  : send a BRK on uart0
 *                callee must ensure that Tx FIFO is already empty and that
 *                BRK detect interrupt is enabled!
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_sendBrk(void)
{
    SET_PERI_REG_MASK(UART_CONF0(UART0), UART_TXFIFO_RST);    // clear TxFIFO
    CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_TXFIFO_RST);

    // To create a 1 char break we enable Loopback.
    // As soon as we get notified in RxIntrHandler about BRK detection we
    // will disable the loopback and unset the TXD_BRK
    SET_PERI_REG_MASK(UART_CONF0(UART0), UART_LOOPBACK);        //enable uart loopback
    SET_PERI_REG_MASK(UART_CONF0(UART0), UART_TXD_BRK);         //SET BRK BIT
}


/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
*******************************************************************************/
static void // must not use ICACHE_FLASH_ATTR !
uart0_rx_intr_handler(void *para)
{
  // we assume that uart1 has interrupts disabled (it uses the same interrupt vector)
  uint8 uart_no = UART0;

  // on BRK detection we disable LOOPBACK and TXD_BRK
  if (UART_BRK_DET_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_BRK_DET_INT_ST)) {
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_LOOPBACK);       //disable uart loopback
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_TXD_BRK);        //CLEAR BRK BIT
  }

  if ((READ_PERI_REG(UART_INT_ST(uart_no)) & (UART_RXFIFO_FULL_INT_ST|UART_RXFIFO_TOUT_INT_ST|UART_BRK_DET_INT_ST)))
  {
    //os_printf("stat:%02X",*(uint8 *)UART_INT_ENA(uart_no));
    ETS_UART_INTR_DISABLE();
    system_os_post(recvTaskPrio, 0, 0);
  }
}

/******************************************************************************
 * FunctionName : uart_recvTask
 * Description  : system task triggered on receive interrupt, empties FIFO and calls callbacks
*******************************************************************************/
static void ICACHE_FLASH_ATTR
uart_recvTask(os_event_t *events)
{
  _EMSRxBuf *p = pEMSRxBuf[emsRxBufIdx];

  if (p->writePtr == 0)
    p->timeStamp = system_get_time();

  while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
    //WRITE_PERI_REG(0X60000914, 0x73); //WTD // commented out by TvE

    // read FIFO from UART
    uint16 length = 0;
    while ((READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) &&
           (length < EMS_MAXBUFFERSIZE)) {
      p->buffer[length++] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
    }
    p->writePtr = length;
  }

  // FIFO is empty - check for BRK condition
  if ((READ_PERI_REG(UART_INT_ST(UART0)) & (UART_BRK_DET_INT_ST))){
    // append marker to end of buffer
    p->buffer[p->writePtr++] = '[';
    p->buffer[p->writePtr++] = 'B';
    p->buffer[p->writePtr++] = ']';

    // get next free EMS Receive buffer
    emsRxBufIdx = (emsRxBufIdx + 1) % EMS_MAXBUFFERS;
    _EMSRxBuf *pTmp = pEMSRxBuf[emsRxBufIdx];
    pTmp->writePtr = pTmp->readPtr = 0;

    // CLR interrupt status, reenable UART interrupt
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR|UART_BRK_DET_INT_CLR);
    ETS_UART_INTR_ENABLE();

    // transmit EMS buffer including header
    for (int i=0; i<MAX_CB; i++) {
      if (uart_recv_cb[i] != NULL) (uart_recv_cb[i])((char *)p, p->writePtr + sizeof(_EMSRxBuf) - EMS_MAXBUFFERSIZE);
    }
  } else {
    // CLR interrupt status, reenable UART interrupt
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR|UART_BRK_DET_INT_CLR);
    ETS_UART_INTR_ENABLE();
  }
}

void ICACHE_FLASH_ATTR
uart0_baud(int rate) {
  os_printf("UART %d baud\n", rate);
  uart_div_modify(UART0, UART_CLK_FREQ / rate);
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
  // rom use 74880 baut_rate, here reinitialize
  UartDev.baut_rate = uart0_br;
  uart_config(UART0);
  UartDev.baut_rate = uart1_br;
  uart_config(UART1);
  for (int i=0; i<4; i++) uart_tx_one_char(UART1, '\n');

  ETS_UART_INTR_ENABLE();

  // install uart1 putc callback
  os_install_putc1((void *)uart1_write_char);

  system_os_task(uart_recvTask, recvTaskPrio, recvTaskQueue, recvTaskQueueLen);
}

void ICACHE_FLASH_ATTR
uart_add_recv_cb(UartRecv_cb cb) {
  for (int i=0; i<MAX_CB; i++) {
    if (uart_recv_cb[i] == NULL) {
      uart_recv_cb[i] = cb;
      return;
    }
  }
  os_printf("UART: max cb count exceeded\n");
}

void ICACHE_FLASH_ATTR
uart_reattach()
{
  uart_init(BIT_RATE_74880, BIT_RATE_74880);
//  ETS_UART_INTR_ATTACH(uart_rx_intr_handler_ssc,  &(UartDev.rcv_buff));
//  ETS_UART_INTR_ENABLE();
}
