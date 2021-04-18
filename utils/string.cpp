#include <string.h>

#include "string.h"

bool strStartsWith(const char *s, const char *test) {
  int len = strlen(test);

  for (int i = 0; i < len; i++) {
    if (s[i] != test[i]) return false;
  }

  return true;
}

unsigned long strHash(const char *str) {
    unsigned long hash = 5381;
    int c;

    while (c = *str++) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}
