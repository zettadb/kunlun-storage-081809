#include "remote_transfer_base.h"
#include <sys/types.h>
namespace kunlun {

RemoteFileBase::RemoteFileBase() { m_fd_ = -1; }
RemoteFileBase::~RemoteFileBase() {}

void RemoteFileBase::setRemoteFileName(const char *remote_file_name,
                                       const char *extra_info
                                       __attribute__((unused))) {
  m_remote_fname_ = std::string(remote_file_name);
}

size_t RemoteFileBase::RemoteWriteByte(unsigned char *buffer, size_t count) {
  return WriteByteImpl(m_fd_, buffer, count);
}

} // namespace kunlun
