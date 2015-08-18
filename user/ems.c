/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <esp8266.h>
#include "espmissingincludes.h"
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "uart.h"
#include "sntp.h"
#include "ems.h"
#include "config.h"

uint8_t	EMSBusBusy  = false;
uint8_t	EMSInitDone = false;

int	emsRxBufIdx = 0;

_EMSRxBuf *pEMSRxBuf;
_EMSRxBuf *paEMSRxBuf[EMS_MAXBUFFERS];

void ICACHE_FLASH_ATTR emsSNTPReInit(void) {
    sntp_stop();
    if (flashConfig.ntp_server[0]) {
        sntp_setservername(0, flashConfig.ntp_server);
        sntp_set_timezone(flashConfig.timezone);
        sntp_init();
    }
}

void ICACHE_FLASH_ATTR emsInit(void) {
    EMSBusBusy = false;			// EMS not ready

    // allocate and preset EMS Receive buffers
    for (int i=0; i< EMS_MAXBUFFERS; i++) {
	_EMSRxBuf *p = (_EMSRxBuf *)os_malloc(sizeof(_EMSRxBuf));
	paEMSRxBuf[i] = p;
    }
    pEMSRxBuf = paEMSRxBuf[0];  // preset EMS Rx Buffer

    emsSNTPReInit();            // (re)init SNTP system
}

// simple interface for EMS
void ICACHE_FLASH_ATTR emsRxHandler(_EMSRxBuf *rxBuf) {
//    void *emsData = (void *)&rxBuf->buffer;

    // pick most interesting data for EMS overview

}
