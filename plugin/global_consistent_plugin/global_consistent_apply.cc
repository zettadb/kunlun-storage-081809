/* Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.

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

/*
  This plugin serves as an example for all those who which to use the new
  Hooks installed by Replication in order to capture:
  - Transaction progress
  - Server state
 */

#include <assert.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/group_replication_priv.h>
#include <mysql/plugin.h>
#include <mysql/service_rpl_transaction_ctx.h>
#include <mysqld_error.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysys_err.h" // EE_FILENOTFOUND
#include "sql/current_thd.h"
#include "sql/sql_class.h"

#include "consistent_view.h"
#include "sql/log_event.h"

static MYSQL_PLUGIN plugin_info_ptr;
static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;
static Consistent_view *consistent_view = nullptr;

static char *clog_path_value;
static bool apply_check_value;

/*
  Binlog relay IO events observers.
  only launch the applier_before_dispatch_event handler
*/

static int binlog_relay_applier_before_dispatch_event(void *param) {
  if (!apply_check_value) {
    return 0;
  }

  if (!consistent_view) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "consistent_view handler is not valid.");
    return 0;
  }

  Log_event *ev = static_cast<Log_event *>(param);
  if (ev->get_type_code() != binary_log::XA_PREPARE_LOG_EVENT) {
    /* only check the xa_prepare_log_event */
    return 0;
  }

  std::string xid_str = ((XA_prepare_log_event *)ev)->get_xid_str();
  Xid_info xid_info;
  if (!(xid_info.init_xid_info(xid_str.c_str()))) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Init xid_info failed, xid_str is %s", xid_str.c_str());
  }
  Retval_Status retval = consistent_view->consistent_check_global(xid_info);
  return retval == OK_STOP ? 1 : 0;
}

Binlog_relay_IO_observer relay_io_observer = {
    sizeof(Binlog_relay_IO_observer),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    binlog_relay_applier_before_dispatch_event};

static void dump_binlog_relay_calls() {
  if (relay_io_observer.applier_before_dispatch_event) {
    LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
                 "\nglobal_consistent_apply_plugin:binlog_relay_"
                 "applier_before_dispatch_event");
  }
}

/*
  Initialize the Global Consistent Applier at server start or plugin
  installation.

  SYNOPSIS
    global_consistent_apply_plugin_init()

  DESCRIPTION
    Apply relay log event under global consistent restrict

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int global_consistent_apply_plugin_init(MYSQL_PLUGIN plugin_info) {
  plugin_info_ptr = plugin_info;

  DBUG_TRACE;

  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs))
    return 1;

  // init the consistent view handler
  consistent_view = new Consistent_view();

  Retval_Status retval = consistent_view->init_and_parse(clog_path_value);
  if (retval != OK_STOP) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure to init the consistent_view before apply event");
    goto err;
  }

  if (register_binlog_relay_io_observer(&relay_io_observer,
                                        (void *)plugin_info_ptr)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in registering the relay io observer");
    goto err;
  }

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
               "global_consistent_apply_plugin: init finished");

  return 0;

err:
  delete consistent_view;
  consistent_view = nullptr;
  return 1;
}

/*
  Terminate the Replication Observer example at server shutdown or
  plugin deinstallation.

  SYNOPSIS
    global_consistent_apply_plugin_deinit()

  DESCRIPTION
    Unregisters Server state observer and Transaction Observer

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int global_consistent_apply_plugin_deinit(void *p) {
  DBUG_TRACE;

  dump_binlog_relay_calls();

  delete consistent_view;
  consistent_view = nullptr;

  if (unregister_binlog_relay_io_observer(&relay_io_observer, p)) {
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failure in unregistering the relay io observer");
    deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);
    return 1;
  }

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG,
               "global_consistent_apply_plugin: deinit finished");
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  return 0;
}

static int commitlog_path_validate(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                                   SYS_VAR *var MY_ATTRIBUTE((unused)),
                                   void *save, st_mysql_value *value) {

  char errbuf[4096] = {0};
  int fd = open(clog_path_value, O_RDONLY);
  if (fd == -1) {
    my_error(EE_FILENOTFOUND, MYF(0), clog_path_value, my_errno(),
             my_strerror(errbuf, sizeof(errbuf), my_errno()));
    // my_message(EE_FILENOTFOUND,"Open commit log Faild, Please check the
    // commit log file is readable",MYF(0));
    return 1;
  };

  close(fd);
  const char *new_val;
  char buf[80];
  int len = sizeof(buf);

  new_val = value->val_str(value, buf, &len);
  *(const char **)(save) = new_val;

  return 0;
}

static MYSQL_SYSVAR_BOOL(check,             /* name */
                         apply_check_value, /* var */
                         PLUGIN_VAR_BOOL,
                         "Whether do the global consistent check apply or not ",
                         NULL, /* check func*/
                         NULL, /* update func*/
                         0);   /* default*/

static MYSQL_SYSVAR_STR(
    commit_log_path, clog_path_value, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
    "Absolute path to the commit log for the global consistent apply",
    commitlog_path_validate, /* check func*/
    NULL,                    /* update func*/
    NULL);                   /* default*/

static SYS_VAR *system_var[] = {MYSQL_SYSVAR(check),
                                MYSQL_SYSVAR(commit_log_path), NULL};

/*
  Plugin library descriptor
*/
struct Mysql_replication global_consistent_apply_plugin = {
    MYSQL_REPLICATION_INTERFACE_VERSION};

mysql_declare_plugin(global_consistent_apply){
    MYSQL_REPLICATION_PLUGIN,
    &global_consistent_apply_plugin,
    "global_consistent_apply",
    "zettadb",
    "Global consistent apply relay log.",
    PLUGIN_LICENSE_GPL,
    global_consistent_apply_plugin_init,   /* Plugin Init */
    NULL,                                  /* Plugin Check uninstall */
    global_consistent_apply_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    NULL,       /* status variables                */
    system_var, /* system variables                */
    NULL,       /* config options                  */
    0,          /* flags                           */
} mysql_declare_plugin_end;
