
//
// occurrences.c
//
// Copyright (c) 2013 Stephen Mathieson
// MIT licensed
//

#include <stdlib.h>
#include <string.h>
#include "occurrences.h"

/*
 * Get the number of occurrences of `needle` in `haystack`
 */

int occurrences(const char *needle, const char *haystack) {
  if (NULL == needle || NULL == haystack) return -1;

  char *pos = (char *)haystack;
  int i = 0;
  int l = strlen(needle);

  while ((pos = strstr(pos, needle))) {
    pos += l;
    i++;
  }

  return i;
}
