/**
 * @file ipv4_parse.c
 * @brief ipv4 log parse module 
 * @author zhoujz, zhoujz@chinanetcenter.com
 * @version 1.0
 * @date 10/15/2014 15:53:27, 41th 星期三
 * @par Copyright:
 * Copyright (c) Chinanetcenter 2014. All rights reserved.
 */

#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/*For inet_addr*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipv4_parse.h"

/**
 * @brief parse iptype string to mem with uint32_t format
 *
 * @param mem [io] Pointer to memory
 * @param len [in] mem size
 * @param val [in] iptype string
 */
void ipv4_parse_ip(char *mem, int len, char *val)
{
  uint32_t  ip;

  assert(sizeof(ip) == len);
  if (mem == NULL || val == NULL) {
    return;
  }

  ip = inet_addr(val);
  memcpy(mem, &ip, len);

  return;
}

/**
 * @brief Parse string type: copy str to mem
 *
 * @param mem [io] Pointer to memory
 * @param len [in] mem size 
 * @param val [in] string
 */
void ipv4_parse_str(char *mem, int len, char *val)
{
  strncpy(mem, val, len);
  return;
}

void ipv4_parse_uint8(char *mem, int len, char *val)
{
  uint8_t  ui;

  assert(sizeof(ui) == len);
  ui = (uint8_t) strtol(val, NULL, 0);
  memcpy(mem, &ui, len);

  return;
}

void ipv4_parse_uint16(char *mem, int len, char *val)
{
  uint16_t  ui;

  assert(sizeof(ui) == len);
  ui = (uint16_t) strtol(val, NULL, 0);
  memcpy(mem, &ui, len);

  return;
}

void ipv4_parse_uint32(char *mem, int len, char *val)
{
  uint32_t  ui;

  assert(sizeof(ui) == len);
  ui = (uint32_t) strtol(val, NULL, 0);
  memcpy(mem, &ui, len);

  return;
}

void ipv4_parse_uint64(char *mem, int len, char *val)
{
  uint64_t  ui;

  assert(sizeof(ui) == len);
  ui = (uint64_t) strtoll(val, NULL, 0);
  memcpy(mem, &ui, len);

  return;
}

char *ipv4_parse_ip2str(char *mem)
{
  static char buf[20];

  sprintf(buf, "%s", inet_ntoa(*(struct in_addr *)mem));

  return buf;
}

char *ipv4_parse_str2str(char *mem)
{
  return mem;
}

char *ipv4_parse_uint64_str(char *mem)
{
  static char buf[24];

  sprintf(buf, "%lu", (*(uint64_t *)mem));

  return buf;
}

char *ipv4_parse_uint32_str(char *mem)
{
  static char buf[24];

  sprintf(buf, "%u", (*(uint32_t *)mem));

  return buf;
}

char *ipv4_parse_uint16_str(char *mem)
{
  static char buf[24];

  sprintf(buf, "%u", (*(uint16_t *)mem));

  return buf;
}

char *ipv4_parse_uint8_str(char *mem)
{
  static char buf[5];

  sprintf(buf, "%u", (*(uint8_t *)mem));

  return buf;
}

