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
#include "esp8266.h"
#include "task.h"
#include "uart.h"
#include "ems.h"

#ifdef UART_DBG
#define DBG(format, ...) os_printf(format, ## __VA_ARGS__)
#else
#define DBG(format, ...) do { } while(0)
#endif

// UartDev is defined and initialized in rom code.
extern UartDevice    UartDev;
extern uint8_t *pEMSRingBuff;

#define MAX_CB 4
static UartRecv_cb uart_recv_cb[4];
static uint8_t uart_recvTaskNum;
static uint8_t *uart_data;

static void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * complete redesign of rx_intr_handler
 * we use a ringbuffer of 1-2kB size to store the incoming data.
 * This should avoid data loss in case of network latencies.
 *****************************************************************************/


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
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  } else {
    /* rcv_buff size is 0x100 */
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
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
    //      has been received for 1 character periods.
    //    trigger rx-brk
    // no hardware flow-control
    WRITE_PERI_REG(UART_CONF1(uart_no),
                   ((1 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                   (1 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
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
  #define UART_INT_MASK (UART_RXFIFO_FULL_INT_ST | UART_RXFIFO_TOUT_INT_ST|UART_BRK_DET_INT_ST)

  // we assume that uart1 has interrupts disabled (it uses the same interrupt vector)
  uint8 uart_no = UART0;

  // simply discard any IRQ as long as EMS init isn't done
  if (!(EMSBusStatus & EMSBUS_RDY)) {
    if ((READ_PERI_REG(UART_INT_ST(uart_no)) & (UART_INT_MASK))) {
      //clear rx and tx fifo,not ready
      SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
      CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    }
    WRITE_PERI_REG(UART_INT_CLR(UART0), (UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR|UART_BRK_DET_INT_CLR));
  } else {
    // init the EMS receive buffer
    if (!(EMSBusStatus & EMSBUS_RX)) {
      EMSBusStatus |= EMSBUS_RX;
      pEMSRxBuf->writePtr = 0;
      pEMSRxBuf->sys_timeStamp = *(volatile uint32 *)(0x3ff20c00);
      pEMSRxBuf->sntp_timeStamp = realtime_stamp;
      uart_data = (uint8_t *)pEMSRxBuf->buffer;
    }

  if ((READ_PERI_REG(UART_INT_ST(uart_no)) & (UART_INT_MASK)))
    {
      while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
        *uart_data++ = 0xFF & READ_PERI_REG(UART_FIFO(UART0));
        pEMSRxBuf->writePtr += 1;
      }
      WRITE_PERI_REG(UART_INT_CLR(UART0), (UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR));
    }

    // BREAK detection == End of EMS data block
    if ((READ_PERI_REG(UART_INT_ST(uart_no)) & (UART_BRK_DET_INT_ST)))
    {
      ETS_UART_INTR_DISABLE();

      // on BRK detection we disable LOOPBACK and TXD_BRK
      CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_LOOPBACK);       //disable uart loopback
      CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_TXD_BRK);        //CLEAR BRK BIT

      *uart_data++ = '\xe5';   // write trailer
      *uart_data++ = '\x1a';
      pEMSRxBuf->writePtr += 2;

      os_param_t rxBuf = (os_param_t)pEMSRxBuf;            // required later
      pEMSRxBuf = (uart_data - pEMSRingBuff) < EMSRingBuffMax ?
                  (_EMSRxBuf *)uart_data : (_EMSRxBuf *)pEMSRingBuff;

      EMSBusStatus &= ~EMSBUS_RX;         // mark EMS bus as free (implicitly release IRQ buffer)

      // CLR interrupt status, reenable UART interrupt
      WRITE_PERI_REG(UART_INT_CLR(UART0), UART_BRK_DET_INT_CLR);
      ETS_UART_INTR_ENABLE();

      post_usr_task(uart_recvTaskNum, rxBuf);
    }
  }
}

/******************************************************************************
 * FunctionName : uart_recvTask
 * Description  : system task triggered on receive/BRK interrupt, empties FIFO and calls callbacks
 ******************************************************************************/
static void ICACHE_FLASH_ATTR
uart_recvTask(os_event_t *events)
{
  DBG("%s: sig=%lx, par=%p\n", __FUNCTION__, events->sig, (void *)events->par);

  _EMSRxBuf *pCurrent = (_EMSRxBuf *)events->par;    // remember current EMSRxBuf
  // transmit EMS buffer including header
  for (int i=0; i<MAX_CB; i++) {
    if (uart_recv_cb[i] != NULL)
      (uart_recv_cb[i])((char *)pCurrent, pCurrent->writePtr + sizeof(_EMSRxBuf) - EMS_MAXBUFFERSIZE);
  }
}

// Turn UART interrupts off and poll for nchars or until timeout hits
uint16_t ICACHE_FLASH_ATTR
uart0_rx_poll(char *buff, uint16_t nchars, uint32_t timeout_us) {
  uint16_t got = 0;
#ifndef EMSBUS
  ETS_UART_INTR_DISABLE();
  uint32_t start = system_get_time(); // time in us
  while (system_get_time()-start < timeout_us) {
    while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
      buff[got++] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
      if (got == nchars) goto done;
    }
  }
done:
  ETS_UART_INTR_ENABLE();
#endif /* ! EMSBUS */
  return got;
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

  // install uart1 putc callback
  os_install_putc1((void *)uart1_write_char);

  uart_recvTaskNum = register_usr_task(uart_recvTask);
  ETS_UART_INTR_ENABLE();
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
}
