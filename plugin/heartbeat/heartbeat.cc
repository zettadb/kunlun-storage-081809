/* Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include <mysql/plugin.h>
#include <mysql_version.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LOG_COMPONENT_TAG "heartbeat_plugin"
#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysqld_error.h>

#include "m_string.h"  // strlen
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_psi_config.h"
#include "my_sys.h"  // my_write, my_malloc
#include "my_thread.h"
#include "mysql/psi/mysql_memory.h"
#include "sql/sql_plugin.h"  // st_plugin_int
#include "sql/mysqld.h" // mysqld_port, my_bind_addr_str

#define ZETTA_SYS_DBNAME "zetta_sysdb"

#define STRING_BUFFER_SIZE 1024
#define LARGE_STRING_BUFFER_SIZE 1024

#define WRITE_STR(format)                                                 \
  {                                                                       \
    const size_t blen = snprintf(buffer, sizeof(buffer), "%s", (format)); \
    my_write(outfile, (uchar *)buffer, blen, MYF(0));                     \
  }

#define WRITE_VAL(format, value)                                             \
  {                                                                          \
    const size_t blen = snprintf(buffer, sizeof(buffer), (format), (value)); \
    my_write(outfile, (uchar *)buffer, blen, MYF(0));                        \
  }

/* Logging service */
static const char *log_filename = "heartbeat";
static File outfile;

static void create_log_file(const char *log_name) {
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile = my_open(filename, O_CREAT | O_RDWR, MYF(0));
}

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

/* Declare the protocal_callbacks */
struct st_command_service_cbs protocol_callbacks ;

/* PSI related */
PSI_memory_key key_memory_mysql_heartbeat_context;

#ifdef HAVE_PSI_INTERFACE

static PSI_memory_info all_heartbeat_memory[] = {
    {&key_memory_mysql_heartbeat_context, "heartbeat_context", 0, 0,
     PSI_DOCUMENT_ME}};

static void init_heartbeat_psi_keys() {
  const char *category = "heartbeat";
  int count;

  count = static_cast<int>(array_elements(all_heartbeat_memory));
  mysql_memory_register(category, all_heartbeat_memory, count);
}
#endif /* HAVE_PSI_INTERFACE */

struct heartbeat_context {
  my_thread_handle heartbeat_thread;
  void * p;
  
};

static void switch_user(MYSQL_SESSION session) {
  MYSQL_SECURITY_CONTEXT sc;
  thd_get_security_context(srv_session_info_get_thd(session), &sc);
  security_context_lookup(sc, "root", "127.0.0.1", "localhost", ZETTA_SYS_DBNAME);
}

static void print_cmd(enum_server_command cmd, COM_DATA *data) {
  char buffer[STRING_BUFFER_SIZE];
  switch (cmd) {
    case COM_INIT_DB:
      WRITE_VAL("COM_INIT_DB: db_name[%s]\n", data->com_init_db.db_name);
      break;
    case COM_QUERY:
      WRITE_VAL("COM_QUERY: query[%s]\n", data->com_query.query);
      break;
    case COM_STMT_PREPARE:
      WRITE_VAL("COM_STMT_PREPARE: query[%s]\n", data->com_stmt_prepare.query);
      break;
    case COM_STMT_EXECUTE:
      WRITE_VAL("COM_STMT_EXECUTE: stmt_id [%lu]\n",
                data->com_stmt_execute.stmt_id);
      break;
    case COM_STMT_SEND_LONG_DATA:
      WRITE_VAL("COM_STMT_SEND_LONG_DATA: stmt_id [%lu]\n",
                data->com_stmt_send_long_data.stmt_id);
      break;
    case COM_STMT_CLOSE:
      WRITE_VAL("COM_STMT_CLOSE: stmt_id [%u]\n", data->com_stmt_close.stmt_id);
      break;
    case COM_STMT_RESET:
      WRITE_VAL("COM_STMT_RESET: stmt_id [%u]\n", data->com_stmt_reset.stmt_id);
      break;
    case COM_STMT_FETCH:
      WRITE_VAL("COM_STMT_FETCH: stmt_id [%lu]\n",
                data->com_stmt_fetch.stmt_id);
      break;
    default:
      WRITE_STR("NOT FOUND: add command to print_cmd\n");
  }
}

static void run_cmd(MYSQL_SESSION session, enum_server_command cmd,
                    COM_DATA *data,struct heartbeat_context *ctx,
                    void *p MY_ATTRIBUTE((unused))) {
  char buffer[STRING_BUFFER_SIZE];
  enum cs_text_or_binary txt_or_bin = CS_TEXT_REPRESENTATION;
  print_cmd(cmd, data);
  int fail = command_service_run_command(session, cmd, data,
                                         &my_charset_utf8_general_ci,
                                         &protocol_callbacks, txt_or_bin, ctx);
  if (fail){ 
    WRITE_VAL("run_statement failed, error code: %d\n",fail);
    return ;
  }
    WRITE_STR("run_statement ok!\n");
}

static void set_query_in_com_data(union COM_DATA *cmd, const char *query) {
  cmd->com_query.query = query;
  cmd->com_query.length = strlen(query);
}

static void *init_heartbeat_table(void *p MY_ATTRIBUTE((unused))) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];// WRIRTE_* Marco
  char stmt_buffer[STRING_BUFFER_SIZE];

  COM_DATA cmd;
  /* Open session: Must pass */
  WRITE_STR("[srv_session_open]\n");
  MYSQL_SESSION session = srv_session_open(NULL, NULL);
  if (!session) {
    WRITE_STR("srv_session_open failed\n");
    return 0;
  }
  WRITE_STR("srv_session_open successfully\n");
  switch_user(session);

  // init zetta sysdb
  WRITE_STR("CREATE DATABASE IF_NOT_EXISTS\n");
  sprintf(stmt_buffer,"CREATE DATABASE IF NOT EXISTS " ZETTA_SYS_DBNAME);
  set_query_in_com_data(&cmd, stmt_buffer);
  run_cmd(session, COM_QUERY, &cmd, nullptr, p);
  memset(stmt_buffer,0,sizeof(stmt_buffer));

  // change databases
  WRITE_STR("CHANGE DATABASE\n");
  cmd.com_init_db.db_name = ZETTA_SYS_DBNAME;
  cmd.com_init_db.length = strlen(ZETTA_SYS_DBNAME);
  run_cmd(session, COM_INIT_DB, &cmd, nullptr, p);

  // init zetta heartbeat table 
  WRITE_STR("CREATE TABLE IF_EXISTS\n");
  sprintf(stmt_buffer,
          "CREATE TABLE IF NOT EXISTS `heartbeat` ("
          "`host_port` varchar(20) NOT NULL,"
          "`beat_time` varchar(30) DEFAULT NULL,"
          "PRIMARY KEY (`host_port`)"
          ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  set_query_in_com_data(&cmd, stmt_buffer);
  run_cmd(session, COM_QUERY, &cmd, nullptr, p);
  memset(stmt_buffer,0,sizeof(stmt_buffer));

  // insert ignore the row
  WRITE_STR("INSERT IGNORE the first heartbeat records\n");
  char host_port[512];
  sprintf(host_port,"%s:%u",my_bind_addr_str,mysqld_port);
  sprintf(stmt_buffer,
          "INSERT IGNORE INTO `heartbeat` values('%s','timestr_place_holder')",
          host_port);
  set_query_in_com_data(&cmd, stmt_buffer);
  run_cmd(session, COM_QUERY, &cmd, nullptr, p);

  /* Close session: Must pass */
  WRITE_STR("[srv_session_close]\n");
  if (srv_session_close(session)){
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "srv_session_close failed.");
  }
  return 0;

}

void thread_clean_func(void* p){
  char buffer[STRING_BUFFER_SIZE];// WRIRTE_* Marco
  MYSQL_SESSION session = (MYSQL_SESSION) p;
  /* Close session: Must pass */
  WRITE_STR("[srv_session_close]\n");
  if (srv_session_close(session)){
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "srv_session_close failed.");
  }
  WRITE_STR("deinit thread session\n");
  srv_session_deinit_thread();
}

static void *mysql_heartbeat(void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];// WRIRTE_* Marco
  char host_port[512];
  sprintf(host_port,"%s:%u",my_bind_addr_str,mysqld_port);
  char time_str_buffer[STRING_BUFFER_SIZE];
  time_t result;
  struct tm tm_tmp;
  COM_DATA cmd;
  struct heartbeat_context * context = (struct heartbeat_context *)p;
  MYSQL_SESSION session;

  /* push the clean handler */
  pthread_cleanup_push(thread_clean_func,(void *)session);

  if (srv_session_init_thread(context->p))
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "srv_session_init_thread failed.");

  /* Open session: Must pass */
  WRITE_STR("[thd_srv_session_open]\n");
  session = srv_session_open(NULL, NULL);
  if (!session) {
    WRITE_STR("srv_session_open failed\n");
    return 0;
  }
  WRITE_STR("srv_session_open successfully\n");

  switch_user(session);

  // change databases
  WRITE_STR("CHANGE DATABASE\n");
  cmd.com_init_db.db_name = ZETTA_SYS_DBNAME;
  cmd.com_init_db.length = strlen(ZETTA_SYS_DBNAME);
  run_cmd(session, COM_INIT_DB, &cmd, nullptr, p);

  while (1) {
    result = time(NULL);
    localtime_r(&result, &tm_tmp);
    snprintf(time_str_buffer, sizeof(time_str_buffer),
             "%04d-%02d-%02d %2d:%02d:%02d", tm_tmp.tm_year + 1900,
             tm_tmp.tm_mon + 1, tm_tmp.tm_mday, tm_tmp.tm_hour, tm_tmp.tm_min,
             tm_tmp.tm_sec);
    char update_stmt[4096];
    sprintf(update_stmt,"UPDATE `heartbeat` SET `beat_time` = '%s' WHERE `host_port` = '%s'",
            time_str_buffer,host_port);

    set_query_in_com_data(&cmd, update_stmt);
    run_cmd(session, COM_QUERY, &cmd, nullptr, p);
    memset(update_stmt,0,sizeof(update_stmt));
    sleep(3);
  }

  pthread_cleanup_pop(0);
  return 0;
}

/*
  Initialize the daemon example at server start or plugin installation.

  SYNOPSIS
    heartbeat_plugin_init()

  DESCRIPTION
    Starts up heartbeat thread

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int heartbeat_plugin_init(void *p) {
  DBUG_TRACE;

#ifdef HAVE_PSI_INTERFACE
  init_heartbeat_psi_keys();
#endif

  struct heartbeat_context *con;
  my_thread_attr_t attr; /* Thread attributes */
  char buffer[STRING_BUFFER_SIZE];
  memset(&protocol_callbacks,0,sizeof(protocol_callbacks));

  /* Inintialize the logging service */
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Heartbeat Plugin Installation.");
  WRITE_STR("srv_session_open failed\n");
  create_log_file(log_filename);

  init_heartbeat_table(p);

  struct st_plugin_int *plugin = (struct st_plugin_int *)p;
  con = (struct heartbeat_context *)my_malloc(
      key_memory_mysql_heartbeat_context,
      sizeof(struct heartbeat_context), MYF(0));
  // for srv_session_init_thd
  con->p = p;


  /* now create the thread */
  my_thread_attr_init(&attr);
  my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  if (my_thread_create(&con->heartbeat_thread, &attr, mysql_heartbeat,
                       (void *)con) != 0) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, "Create Heartbeat thread faild.");
    exit(0);
  }
  plugin->data = (void *)con;

  return 0;
}

/*
  Terminate the daemon example at server shutdown or plugin deinstallation.

  SYNOPSIS
    heartbeat_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int heartbeat_plugin_deinit(void *p) {
  DBUG_TRACE;
  char buffer[STRING_BUFFER_SIZE];
  struct st_plugin_int *plugin = (struct st_plugin_int *)p;
  struct heartbeat_context *con =
      (struct heartbeat_context *)plugin->data;
  void *dummy_retval;

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, "Heartbeat Plugin Uninstallation.");
  WRITE_STR("Heartbeat Plugin Uninstallation.\n");
  my_thread_cancel(&con->heartbeat_thread);
  /*
    Need to wait for the heartbeat thread to terminate before closing
    the file it writes to and freeing the memory it uses
  */
  my_thread_join(&con->heartbeat_thread, &dummy_retval);
  my_free(con);
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
  return 0;
}

struct st_mysql_daemon heartbeat_plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(heartbeat){
    MYSQL_DAEMON_PLUGIN,
    &heartbeat_plugin,
    "heartbeat",
    "Snowao",
    "Heartbeat by timestamp,by zetta Inc.",
    PLUGIN_LICENSE_GPL,
    heartbeat_plugin_init,   /* Plugin Init */
    NULL,                         /* Plugin Check uninstall */
    heartbeat_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    NULL, /* status variables                */
    NULL, /* system variables                */
    NULL, /* config options                  */
    0,    /* flags                           */
} mysql_declare_plugin_end;
