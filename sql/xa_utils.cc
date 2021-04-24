#include "sql/xa_aux.h"
#include "sql/xa.h"

extern int ddc_mode;
char *serialize_xid(char *buf, long /*fmt*/, long gln, long bln,
                           const char *dat) {
  buf[0] = '\'';
  memcpy(buf + 1, dat, gln + bln);
  buf[gln + bln + 1] = '\'';
  buf[gln + bln + 2] = '\0';
  return buf;
}
