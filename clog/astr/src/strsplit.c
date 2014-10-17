
#include <stdlib.h>
#include <string.h>
#include "strsplit.h"
#include "strdup.h"

int
strsplit (const char *str, char *parts[], const char *delimiter) {
  char *pch;
  int i = 0;
  char *tmp = strdup(str);
  pch = strtok(tmp, delimiter);

  if (pch) {
    parts[i++] = strdup(pch);
  } else {
    parts[i++] = strdup(str);
  }

  while (pch) {
    pch = strtok(NULL, delimiter);
    if (NULL == pch) break;
    parts[i++] = strdup(pch);
  }

  free(tmp);
  free(pch);
  return i;
}
