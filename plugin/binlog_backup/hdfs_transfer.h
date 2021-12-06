#ifndef _HDFS_TRANSFER_H_
#define _HDFS_TRANSFER_H_

#include "errorcup.h"
#include "remote_transfer_base.h"
#include <string>
namespace kunlun {
class HdfsFile : public RemoteFileBase {
  typedef int fd_t;

public:
  HdfsFile();
  ~HdfsFile() override;

  /**
   * use popen to invoke the `hadoop` client program
   * under the ${PATH}, then return file descriptor
   * which refers to the m_dst_file_name to application,
   * which support the write()-like operation.
   * */
  virtual int OpenFd() override final;
  virtual void TearDown() override final;
  virtual size_t WriteByteImpl(fd_t, unsigned char *, size_t) override final;
  virtual void setRemoteFileName(const char *,const char *) override;



private:
  std::string m_cluster_name_;
  std::string m_shard_name_;
  FILE *m_hdfs_file_ptr_;
};

} // namespace kunlun

#endif /*_HDFS_TRANSFER_H_*/
