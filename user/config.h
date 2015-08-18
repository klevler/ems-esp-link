#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
  uint32_t version;                   // flash write sequence number
  uint16_t crc;
  int32_t  baud_rate;
  char     hostname[32];               // if using DHCP
  uint32_t staticip, netmask, gateway; // using DHCP if staticip==0
  uint8_t  log_mode;
  char     ntp_server[32];
  int16_t  timezone;                  // Timezone (-12 - + 12)
  char    collectord[32];             // IP/Hostname Collectord
  int16_t collectord_port;            // portnumber
  char     api_key[48];               // RSSI submission API key (Grovestreams for now)
} FlashConfig;
extern FlashConfig flashConfig;

bool configSave(void);
bool configRestore(void);
void configWipe(void);

#endif
