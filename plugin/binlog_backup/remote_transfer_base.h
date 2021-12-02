#ifndef _REMOTE_TRANSFER_BASE_H_
#define _REMOTE_TRANSFER_BASE_H_

#include <string>
#include "errorcup.h"
namespace kunlun {

class RemoteFileBase : public ErrorCup {
  typedef int fd_t;

public:
  RemoteFileBase();
  virtual ~RemoteFileBase();

  // must be reimplemented
  virtual int OpenFd() = 0;
  virtual size_t WriteByteImpl(fd_t, unsigned char *, size_t) = 0;
  virtual void TearDown() = 0;

  virtual void setRemoteFileName(const char *,const char *);
  size_t RemoteWriteByte(unsigned char *, size_t);

protected:
  fd_t m_fd_;
  std::string m_remote_fname_;
};
} // namespace kunlun

#endif /* _REMOTE_TRANSFER_BASE_H_ */
