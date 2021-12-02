#include "hdfs_transfer.h"
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace kunlun;

HdfsFile::HdfsFile() {
  m_remote_fname_ = "";
  m_hdfs_file_ptr_ = NULL;
  m_shard_name_ = "shard1";
  m_cluster_name_ = "cluster1";
}

HdfsFile::~HdfsFile() {
  if (m_hdfs_file_ptr_ != NULL) {
    // watch out the zombie proc
    pclose(m_hdfs_file_ptr_);
    m_hdfs_file_ptr_ = NULL;
  }
}

void HdfsFile::TearDown() {
  if (m_hdfs_file_ptr_ != NULL) {
    // watch out the zombie proc
    pclose(m_hdfs_file_ptr_);
    m_hdfs_file_ptr_ = NULL;
  }
}

void HdfsFile::setRemoteFileName(const char *binlog_file_name,
                                 const char *extra_info) {
  // get last_modification time
  struct stat attr;
  if (stat(binlog_file_name, &attr) < 0) {
    setErr("%s", strerror(errno));
  }
  time_t seconds = attr.st_ctim.tv_sec;
  struct tm tm_tmp;
  localtime_r(&seconds, &tm_tmp);
  char date_str_buffer[128] = {0};
  strftime(date_str_buffer, sizeof(date_str_buffer), "D%Y#%m#%d", &tm_tmp);
  char time_str_buffer[128] = {0};
  strftime(time_str_buffer, sizeof(time_str_buffer), "T%H#%M#%S", &tm_tmp);

  std::string fn = std::string(binlog_file_name);
  std::size_t found = fn.find_last_of("/\\");
  char buffer[2048] = {0};
  snprintf(buffer, sizeof(buffer),
           "/kunlun/backup/binlog/%s/%s/%s/_%s_%s_%s_%s_.lz4",
           m_cluster_name_.c_str(), m_shard_name_.c_str(), date_str_buffer,
           fn.substr(found + 1).c_str(), extra_info, date_str_buffer,
           time_str_buffer);
  m_remote_fname_ = std::string(buffer);
}

int HdfsFile::OpenFd() {
  // TODO: dynamic string buffer needed.
  char buf[2048] = {'\0'};
  snprintf(buf, sizeof(buf) - 1, "lz4 -B4 | hadoop  fs -put -p -f - %s",
           m_remote_fname_.c_str());
  // TODO: biodirection popen-like API needed.
  m_hdfs_file_ptr_ = popen(buf, "w");
  if (m_hdfs_file_ptr_ == NULL) {
    setErr("popen() failed, errno: %d, errmsg: %s", errno, strerror(errno));
    return -1;
  }
  return m_fd_ = fileno(m_hdfs_file_ptr_);
}

size_t HdfsFile::WriteByteImpl(fd_t fd, unsigned char *buffer, size_t count) {
  ssize_t numWrite = 0;
  ssize_t numLeft = count;
  while (numLeft) {
    numWrite = write(fd, buffer, numLeft);
    if (numWrite < 0) {
      if (errno == EINTR || errno == EAGAIN) {
        // TODO maybe here can interfere time limit opts
        continue;
      } else {
        setErr("write() failed, info: %s", strerror(errno));
        return count - numLeft;
      }
    }
    numLeft -= numWrite;
    buffer += numWrite;
  }
  return count - numLeft;
}
