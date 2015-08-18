// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt
/* Configuration stored in flash */

#include <esp8266.h>
#include <osapi.h>
#include "config.h"
#include "espfs.h"

// hack: this from LwIP
extern uint16_t inet_chksum(void *dataptr, uint16_t len);

FlashConfig flashConfig;
FlashConfig flashDefault = {
  2107,                       // sequence
  0,                          // crc
  9600,                       // Baudrate
  "ems-link\0",               // hostname
  0, 0x00ffffff, 0,           // static ip, netmask, gateway
  0,                          // log mode
  "0.europe.pool.ntp.org\0",  // NTP Timeserver
  2,                          // Timezone
  "collectord\0",             // EMS Collector Daemon Host
  7950,                       // EMS Collector Daemon Port
  "\0",                       // api_key
};

typedef union {
  FlashConfig fc;
  uint8_t     block[128];
} FlashFull;

#define FLASH_MAGIC  (0xaa55)

#define FLASH_ADDR   (0x3E000)
#define FLASH_SECT   (4096)

#if 1
void ICACHE_FLASH_ATTR
memDump(void *addr, int len) {
  for (int i=0; i<len;) {
    os_printf("%02x", ((uint8_t *)addr)[i++]);
    if ((i % 32) == 0)
      os_printf("\n");
  }
  os_printf("\n");
}
#endif

bool ICACHE_FLASH_ATTR configSave(void) {
  FlashFull ff;

  // erase primary
  uint32_t addr = FLASH_ADDR;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK) goto fail;

  memset(&ff, 0xff, sizeof(ff));
  memcpy(&ff, &flashConfig, sizeof(FlashConfig));

  ff.fc.crc = inet_chksum(&ff, sizeof(FlashConfig));  // calculate CRC

  // write primary
  if (spi_flash_write(addr, (void *)&ff, sizeof(ff)) != SPI_FLASH_RESULT_OK) goto fail;

  // erase backup
  addr = FLASH_ADDR + FLASH_SECT;
  if (spi_flash_erase_sector(addr>>12) != SPI_FLASH_RESULT_OK) return true; // no backup but we're OK

  // write backup
  spi_flash_write(addr, (void *)&ff, sizeof(ff)); // no backup but we're OK
  return true;

  fail:
  os_printf("*** Failed to save config ***\n");
  return false;
}

void ICACHE_FLASH_ATTR configWipe(void) {
  spi_flash_erase_sector(FLASH_ADDR>>12);
  spi_flash_erase_sector((FLASH_ADDR+FLASH_SECT)>>12);
}

// decide which flash sector to use based on crc and version (0 or 1, or -1 for error)
static int ICACHE_FLASH_ATTR selectFlash(FlashFull *ff0, FlashFull *ff1) {
  uint16_t crc;
  uint16_t version = flashDefault.version;

  // check CRC of ff0
  crc = ff0->fc.crc;
  ff0->fc.crc = 0;
  if ((ff0->fc.version == version) && (inet_chksum(ff0, sizeof(FlashConfig)) == crc)) return 0;

  // check CRC of ff1
  crc = ff1->fc.crc;
  ff1->fc.crc = 0;
  if ((ff1->fc.version == version) && (inet_chksum(ff1, sizeof(FlashConfig)) == crc)) return 1;

  return -1;
}

bool ICACHE_FLASH_ATTR configRestore(void) {
  // read both flash sectors
  FlashFull ff0, ff1;
  if (spi_flash_read(FLASH_ADDR, (void *)&ff0, sizeof(ff0)) != SPI_FLASH_RESULT_OK)
    memset(&ff0, 0, sizeof(ff0)); // clear in case of error
  if (spi_flash_read(FLASH_ADDR+FLASH_SECT, (void *)&ff1, sizeof(ff1)) != SPI_FLASH_RESULT_OK)
    memset(&ff1, 0, sizeof(ff1)); // clear in case of error

  // figure out which one is good
  int flash = selectFlash(&ff0, &ff1);

  // if neither is OK, we revert to defaults
  if (flash < 0) {
    memcpy(&flashConfig, &flashDefault, sizeof(FlashConfig));
    os_printf("*** FAILED to restore config ***\n");
    return false;
  }

  // copy good one into global var and return
  memcpy(&flashConfig, flash == 0 ? &ff0.fc : &ff1.fc, sizeof(FlashConfig));
  return true;
}
