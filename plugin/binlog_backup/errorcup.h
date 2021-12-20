#ifndef _ERR_CUP_H_
#define _ERR_CUP_H_

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
namespace kunlun {
class ErrorCup {
public:
  ErrorCup() { bzero(m_errbuf_, 4096); }
  int setErr(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list arg;
    va_start(arg, fmt);

    int retVal = vsnprintf(m_errbuf_, sizeof(m_errbuf_) - 1, fmt, arg);
    va_end(arg);
    return retVal;
  }

  char *getErr() { return m_errbuf_; }

  void removeErr() { m_errbuf_[0] = '\0'; }

private:
  char m_errbuf_[4096];
};
} // namespace kunlun

#endif /* _ERR_CUP_H_ */
