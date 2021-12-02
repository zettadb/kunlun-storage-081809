/* Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.
 *

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <ctype.h>
#include <fcntl.h>
#include <fstream>
#include <mysql/components/my_service.h>
#include <mysql/plugin.h>
#include <mysql_version.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#include "hdfs_transfer.h"
#include "m_string.h" // strlen
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sys.h" // my_write, my_malloc
#include "my_thread.h"
#include "mysql/psi/mysql_memory.h"
#include "remote_transfer_base.h"
#include "sql/binlog.h"     // log_bin_basename, log_bin_index
#include "sql/mysqld.h"     // my_bind_addr_str, mysqld_port
#include "sql/sql_plugin.h" // st_plugin_int

using namespace kunlun;
// confirm `buffer` is declared in the invoking context.
#define ErrorPluginLog(logfile, format, args...)                               \
  {                                                                            \
    const char *prefix = "[ ERROR ] ";                                         \
    my_write((logfile), (const uchar *)prefix, sizeof(prefix), MYF(0));        \
    size_t msg_len = snprintf(buffer, sizeof(buffer), format, args);           \
    my_write((logfile), (uchar *)buffer, msg_len, MYF(0));                     \
    buffer[0] = '\0';                                                          \
  }

#define InfoPluginLog(logfile, format, args...)                                \
  {                                                                            \
    const char *prefix = "[ INFO ] ";                                          \
    my_write((logfile), (const uchar *)prefix, sizeof(prefix), MYF(0));        \
    size_t msg_len = snprintf(buffer, sizeof(buffer), format, args);           \
    my_write((logfile), (uchar *)buffer, msg_len, MYF(0));                     \
    buffer[0] = '\0';                                                          \
  }

#define DebugPluginLog(logfile, msg)                                           \
  {                                                                            \
    const char *prefix = "[ DBUG ] ";                                          \
    my_write((logfile), (const uchar *)prefix, sizeof(prefix), MYF(0));        \
    size_t msg_len = snprintf(buffer, sizeof(buffer), format, args);           \
    my_write((logfile), (uchar *)buffer, msg_len, MYF(0));                     \
    buffer[0] = '\0';                                                          \
  }

#define WriteBackupState(statefile, msg)                                       \
  {                                                                            \
    size_t msg_len = snprintf(buffer, sizeof(buffer), "%s\n", (msg));          \
    my_write((statefile), (uchar *)buffer, msg_len, MYF(0));                   \
    buffer[0] = '\0';                                                          \
  }

#define WAIT_CONTINUE(seconds)                                                 \
  {                                                                            \
    sleep((seconds));                                                          \
    continue;                                                                  \
  }

PSI_memory_key key_memory_mysql_binlog_backup_context;

#ifdef HAVE_PSI_INTERFACE

static PSI_memory_info all_binlog_backup_memory[] = {
    {&key_memory_mysql_binlog_backup_context, "mysql_binlog_backup_context", 0,
     0, PSI_DOCUMENT_ME}};

static void init_binlog_backup_psi_keys() {
  const char *category = "binlog_backup";
  int count;

  count = static_cast<int>(array_elements(all_binlog_backup_memory));
  mysql_memory_register(category, all_binlog_backup_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */

#define RUNTIME_STRING_BUFFER 2048

/* Declare the protocal_callbacks */
struct st_command_service_cbs protocol_callbacks;

static char plugin_log_filename[FN_REFLEN];
static char backup_state_filename[FN_REFLEN];
static char binlog_machine_info[FN_REFLEN]; // _192#168#0#135_8001_
static std::string log_place_holder = "";

enum REMOTE_STORAGE_TYPE { HDFS = 0, ANONYMOUS };

struct mysql_binlog_backup_context {
  my_thread_handle binlog_backup_thread;
  my_thread_handle binlog_flush_interval_thread;
  File plugin_log_file;
  File backup_state_file;
  RemoteFileBase *rfbpt;
  REMOTE_STORAGE_TYPE storage_type;
  void *p;
};
static std::string binlog_fn = "";

static RemoteFileBase *get_backup_instance(REMOTE_STORAGE_TYPE s_type) {
  if (s_type == HDFS) {
    return new HdfsFile();
  }
  return new HdfsFile();
}

// get last backuped file
static std::string get_last_backuped_file() {
  std::vector<std::string> filelist;
  std::ifstream infile(backup_state_filename);
  std::string line;
  while (std::getline(infile, line)) {
    if (line.length() > 0) {
      filelist.push_back(line);
    }
  }
  if (filelist.size() == 0) {
    return "";
  }
  return filelist[filelist.size() - 1];
}

static std::string get_next_binlog_to_backup() {
  // read the binlog_backup.state last line
  std::string lb_filename = get_last_backuped_file();
  std::vector<std::string> filelist;
  std::ifstream infile(mysql_bin_log.get_index_fname());
  std::string line;
  while (std::getline(infile, line)) {
    if (line.length() > 0) {
      filelist.push_back(line);
    }
  }
  auto iter = filelist.begin();
  if (lb_filename.length() == 0) {
    return *iter;
  }
  for (; iter != filelist.end(); iter++) {
    if (lb_filename == *iter) {
      break;
    }
  }
  if (++iter == filelist.end()) {
    return *(--iter);
  }
  return *iter;
}

static bool have_new_binlog() {
  if (binlog_fn == "") {
    return true;
  }
  std::vector<std::string> filelist;
  std::ifstream infile(mysql_bin_log.get_index_fname());
  std::string line;
  while (std::getline(infile, line)) {
    if (line.length() > 0) {
      filelist.push_back(line);
    }
  }
  auto iter = filelist.begin();
  for (; iter != filelist.end(); iter++) {
    if (binlog_fn == *iter) {
      break;
    }
  }
  return (++iter) != filelist.end();
}

static void set_query_in_com_data(union COM_DATA *cmd, const char *query) {
  cmd->com_query.query = query;
  cmd->com_query.length = strlen(query);
}

static void print_cmd(enum_server_command cmd, COM_DATA *data, void *p) {
  char buffer[RUNTIME_STRING_BUFFER];
  struct mysql_binlog_backup_context *con =
      (struct mysql_binlog_backup_context *)p;
  switch (cmd) {
  case COM_INIT_DB:
    InfoPluginLog(con->plugin_log_file, "COM_INIT_DB: db_name[%s]\n",
                  data->com_init_db.db_name);
    break;
  case COM_QUERY:
    InfoPluginLog(con->plugin_log_file, "COM_QUERY: query[%s]\n",
                  data->com_query.query);
    break;
  case COM_STMT_PREPARE:
    InfoPluginLog(con->plugin_log_file, "COM_STMT_PREPARE: query[%s]\n",
                  data->com_stmt_prepare.query);
    break;
  case COM_STMT_EXECUTE:
    InfoPluginLog(con->plugin_log_file, "COM_STMT_EXECUTE: stmt_id [%lu]\n",
                  data->com_stmt_execute.stmt_id);
    break;
  case COM_STMT_SEND_LONG_DATA:
    InfoPluginLog(con->plugin_log_file,
                  "COM_STMT_SEND_LONG_DATA: stmt_id [%lu]\n",
                  data->com_stmt_send_long_data.stmt_id);
    break;
  case COM_STMT_CLOSE:
    InfoPluginLog(con->plugin_log_file, "COM_STMT_CLOSE: stmt_id [%u]\n",
                  data->com_stmt_close.stmt_id);
    break;
  case COM_STMT_RESET:
    InfoPluginLog(con->plugin_log_file, "COM_STMT_RESET: stmt_id [%u]\n",
                  data->com_stmt_reset.stmt_id);
    break;
  case COM_STMT_FETCH:
    InfoPluginLog(con->plugin_log_file, "COM_STMT_FETCH: stmt_id [%lu]\n",
                  data->com_stmt_fetch.stmt_id);
    break;
  default:
    InfoPluginLog(con->plugin_log_file,
                  "NOT FOUND: add command to print_cmd%s\n",
                  log_place_holder.c_str());
  }
}

static void switch_user(MYSQL_SESSION session) {
  MYSQL_SECURITY_CONTEXT sc;
  thd_get_security_context(srv_session_info_get_thd(session), &sc);
  security_context_lookup(sc, "root", "127.0.0.1", "localhost", "mysql");
}

static void run_cmd(MYSQL_SESSION session, enum_server_command cmd,
                    COM_DATA *data, struct mysql_binlog_backup_context *ctx,
                    void *p MY_ATTRIBUTE((unused))) {
  char buffer[RUNTIME_STRING_BUFFER];
  enum cs_text_or_binary txt_or_bin = CS_TEXT_REPRESENTATION;
  struct mysql_binlog_backup_context *con =
      (struct mysql_binlog_backup_context *)p;
  print_cmd(cmd, data, p);
  int fail = command_service_run_command(session, cmd, data,
                                         &my_charset_utf8_general_ci,
                                         &protocol_callbacks, txt_or_bin, ctx);
  if (fail) {
    ErrorPluginLog(con->plugin_log_file,
                   "run_statement failed, error code: %d\n", fail);
    return;
  }
  InfoPluginLog(con->plugin_log_file, "run_statement ok!%s\n",
                log_place_holder.c_str());
}

static void binlog_flush_thread_clean_func(void *p) {
  MYSQL_SESSION session = (MYSQL_SESSION) p;
  /* Close session: Must pass */
  srv_session_close(session);
  srv_session_deinit_thread();
}

static bool timesup() {
  time_t t = time(NULL);
  struct tm tm;

  // fetch dawn time stamp
  localtime_r(&t, &tm);
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  unsigned int dawn_stamp = mktime(&tm);

  // renew current time stamp
  localtime_r(&t, &tm);
  unsigned int current_stamp = mktime(&tm);

  int sub = current_stamp - dawn_stamp;
  if (sub >= 0 && sub <= 5) {
    return true;
  }
  return false;
}

static void *mysql_binlog_flush_interval(void *p) {
  DBUG_TRACE;
  struct mysql_binlog_backup_context *con =
      (struct mysql_binlog_backup_context *)p;
  char buffer[RUNTIME_STRING_BUFFER];
  MYSQL_SESSION session;
  COM_DATA cmd;

  /* push the clean handler */
  pthread_cleanup_push(binlog_flush_thread_clean_func, (void *)session);

  if (srv_session_init_thread(con->p)) {
    ErrorPluginLog(con->plugin_log_file, "init srv session faildi%s.",
                   log_place_holder.c_str());
    return 0;
  }
  /* Open session: Must pass */
  InfoPluginLog(con->plugin_log_file, "[thd_srv_session_open]%s\n",
                log_place_holder.c_str());
  session = srv_session_open(NULL, NULL);
  if (!session) {
    ErrorPluginLog(con->plugin_log_file, "srv_session_open failed%s\n",
                   log_place_holder.c_str());
    return 0;
  }
  InfoPluginLog(con->plugin_log_file, "srv_session_open successfully%s\n",
                log_place_holder.c_str());
  switch_user(session);
  // init db
  cmd.com_init_db.db_name = "mysql";
  cmd.com_init_db.length = strlen("mysql");
  run_cmd(session, COM_INIT_DB, &cmd, nullptr, p);

  char stmt_buffer[512] = {0};

  while (1) {
    if (timesup()) {
      // flush logs
      sprintf(stmt_buffer, "FLUSH LOGS");
      set_query_in_com_data(&cmd, stmt_buffer);
      run_cmd(session, COM_QUERY, &cmd, nullptr, p);
      memset(stmt_buffer, 0, sizeof(stmt_buffer));
    }
    sleep(3);
  }
  pthread_cleanup_pop(0);
  return 0;
}

static void *mysql_binlog_backup(void *p) {
  DBUG_TRACE;
  struct mysql_binlog_backup_context *con =
      (struct mysql_binlog_backup_context *)p;
  char buffer[RUNTIME_STRING_BUFFER];

  /* init the file transfer handler */
  RemoteFileBase *rfbpt = get_backup_instance(con->storage_type);
  con->rfbpt = rfbpt;

  uchar raw_buffer[64 * 1024]; // 64K
  bool binlog_rotated = true;
  File fd;

  for (;;) {
    /* get binlog file name */
    if (binlog_rotated) {
      std::string binlog_fn_next = get_next_binlog_to_backup();
      if (binlog_fn_next != binlog_fn) {
        binlog_fn = binlog_fn_next;
        rfbpt->setRemoteFileName(binlog_fn.c_str(), binlog_machine_info);
      } else {
        WAIT_CONTINUE(1);
      }
      /* just renew the binlog to process */
      binlog_rotated = false;
    }

    if ((fd = my_open(binlog_fn.c_str(), O_RDONLY, MYF(0))) < 0) {
      ErrorPluginLog(con->plugin_log_file, "%s", strerror(errno));
      WAIT_CONTINUE(1);
    }

    if (rfbpt->OpenFd() < 0) {
      ErrorPluginLog(con->plugin_log_file, "%s", rfbpt->getErr());
      /* pclose() actrually, ZOMBBIE watch out ~ */
      rfbpt->TearDown();
      /* fd leak */
      my_close(fd, MYF(0));
      WAIT_CONTINUE(1);
    }

    size_t r_count;
    for (;;) {
      r_count = my_read(fd, raw_buffer, sizeof(raw_buffer), MYF(0));
      switch (r_count) {
      case 0:
        /*
         * There are two scene may happen:
         * 1. binlog rotate.
         * 2. no binlog event write.
         **/
        binlog_rotated = have_new_binlog();
        if (binlog_rotated == false) {
          sleep(1);
        }
        break;
      case -1:
        /* error occours */
        ErrorPluginLog(con->plugin_log_file, "%s", strerror(errno));
        break;
      default:;
        // ...
      }

      if (binlog_rotated) {
        break;
      }
      if (r_count > 0 &&
          (rfbpt->RemoteWriteByte(raw_buffer, r_count) != r_count)) {
        ErrorPluginLog(con->plugin_log_file, "%s", rfbpt->getErr());
        break;
      }
    }

    /* pclose() actrually, ZOMBBIE watch out ~ */
    rfbpt->TearDown();
    my_close(fd, MYF(0));

    /*
     * Successfully backup current binlog file.
     * write binlog_backup.state
     */
    if (binlog_rotated) {
      WriteBackupState(con->backup_state_file, binlog_fn.c_str());
    }
  }

  return 0;
}

/*
  Initialize the Binlog-backup at server start or plugin installation.

  SYNOPSIS
    binlog_backup_plugin_init()

  DESCRIPTION
    Starts up binlog_backupbeat thread

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int binlog_backup_plugin_init(void *p) {
  DBUG_TRACE;

#ifdef HAVE_PSI_INTERFACE
  init_binlog_backup_psi_keys();
#endif

  memset(&protocol_callbacks, 0, sizeof(protocol_callbacks));
  struct mysql_binlog_backup_context *con;
  my_thread_attr_t attr; /* Thread attributes */
  char buffer[RUNTIME_STRING_BUFFER];
  time_t result = time(NULL);
  struct tm tm_tmp;

  struct st_plugin_int *plugin = (struct st_plugin_int *)p;

  con = (struct mysql_binlog_backup_context *)my_malloc(
      key_memory_mysql_binlog_backup_context,
      sizeof(struct mysql_binlog_backup_context), MYF(0));

  // for srv_session_init_thd
  con->p = p;

  /* init log file */
  fn_format(plugin_log_filename, "mysql-binlog_backup_plugin", "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(plugin_log_filename);
  con->plugin_log_file = my_open(plugin_log_filename, O_CREAT | O_RDWR, MYF(0));

  /* init backup state file */
  /*
   * Author:
   * www.zettadb.com
   *
   * Note:
   * <binlog_backup.state> file writen by plugin itself is
   * nested in the binlog directory in order to indicate
   * the set of the binlog file which is already backuped
   * to the reliable storage.
   *
   * based on this information, the plugin itself can determain
   * which binlog file is the first one needed to be backuped
   * after a character shifting or the rebooting operations by
   * mysqld server.
   *
   * */

  // TODO: binlog_backup.state file need a cleanup mechanism. May be
  //       another thread to deal with is it ?
  std::string tmp_str = std::string(log_bin_basename);
  std::size_t found = tmp_str.find_last_of("/\\");
  std::string binlog_base_path = tmp_str.substr(0, found);
  fn_format(backup_state_filename, "binlog_backup", binlog_base_path.c_str(),
            ".state", MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  con->backup_state_file =
      my_open(backup_state_filename, O_CREAT | O_RDWR | O_APPEND, MYF(0));

  /* init storage type */
  con->storage_type = HDFS;

  /* init binlog machine info */
  std::string bind_address = std::string(my_bind_addr_str);
  std::replace(bind_address.begin(), bind_address.end(), '.', '#');
  snprintf(binlog_machine_info, sizeof(binlog_machine_info), "I%s_P%u",
           bind_address.c_str(), mysqld_port);

  /*
    No threads exist at this point in time, so this is thread safe.
  */
  localtime_r(&result, &tm_tmp);
  snprintf(buffer, sizeof(buffer),
           "Starting up at %02d%02d%02d %2d:%02d:%02d\n", tm_tmp.tm_year % 100,
           tm_tmp.tm_mon + 1, tm_tmp.tm_mday, tm_tmp.tm_hour, tm_tmp.tm_min,
           tm_tmp.tm_sec);
  my_write(con->plugin_log_file, (uchar *)buffer, strlen(buffer), MYF(0));

  my_thread_attr_init(&attr);
  my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  /* now create the backup thread */
  if (my_thread_create(&con->binlog_backup_thread, &attr, mysql_binlog_backup,
                       (void *)con) != 0) {
    fprintf(stderr, "Could not create binlog_backup thread!\n");
    exit(0);
  }

  /* now create the `flush logs` interval thread */
  if (my_thread_create(&con->binlog_flush_interval_thread, &attr,
                       mysql_binlog_flush_interval, (void *)con) != 0) {
    fprintf(stderr, "Could not create binlog_flush_interval thread!\n");
    exit(0);
  }
  plugin->data = (void *)con;

  return 0;
}

/*
  Terminate the daemon example at server shutdown or plugin deinstallation.

  SYNOPSIS
    binlog_backup_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int binlog_backup_plugin_deinit(void *p) {
  DBUG_TRACE;
  char buffer[RUNTIME_STRING_BUFFER];
  struct st_plugin_int *plugin = (struct st_plugin_int *)p;
  struct mysql_binlog_backup_context *con =
      (struct mysql_binlog_backup_context *)plugin->data;
  time_t result = time(NULL);
  struct tm tm_tmp;
  void *dummy_retval;

  my_thread_cancel(&con->binlog_backup_thread);
  my_thread_cancel(&con->binlog_flush_interval_thread);

  localtime_r(&result, &tm_tmp);
  snprintf(buffer, sizeof(buffer),
           "Shutting down at %02d%02d%02d %2d:%02d:%02d\n",
           tm_tmp.tm_year % 100, tm_tmp.tm_mon + 1, tm_tmp.tm_mday,
           tm_tmp.tm_hour, tm_tmp.tm_min, tm_tmp.tm_sec);
  my_write(con->plugin_log_file, (uchar *)buffer, strlen(buffer), MYF(0));

  my_thread_join(&con->binlog_backup_thread, &dummy_retval);
  my_thread_join(&con->binlog_flush_interval_thread, &dummy_retval);

  my_close(con->plugin_log_file, MYF(0));
  my_close(con->backup_state_file,MYF(0));

  delete con->rfbpt;

  my_free(con);

  return 0;
}

struct st_mysql_daemon binlog_backup_plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(binlog_backup){
    MYSQL_DAEMON_PLUGIN,
    &binlog_backup_plugin,
    "binlog_backup",
    "zettadb",
    "Binlog backup, creates a binlog_backup beat file in "
    "mysql-binlog_backup.log",
    PLUGIN_LICENSE_GPL,
    binlog_backup_plugin_init,   /* Plugin Init */
    NULL,                        /* Plugin Check uninstall */
    binlog_backup_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    NULL, /* status variables                */
    NULL, /* system variables                */
    NULL, /* config options                  */
    0,    /* flags                           */
} mysql_declare_plugin_end;
