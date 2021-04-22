# ==== Purpose ====
#
# This include will insert some data into a table at the master varying the
# debug sync point at slave that will be used to stop the IO thread in the
# middle of an XA transaction event stream (trying to let partial transactions in
# the relay log), and see if SQL thread can skip the incomplete event group and
# reexecute the complete version of the same event group that follows.
#
# ==== Usage ====
#
# --source extra/rpl_tests/rpl_xa_trx_boundary_parser_all_steps.inc
#
#

--let $_storage_engine= INNODB

# Check if SQL thread is running
--source include/rpl_connection_slave.inc
--let $_is_sql_thread_running= query_get_value(SHOW SLAVE STATUS, Slave_SQL_Running, 1)

# If the SQL thread is stopped, we will assert GTIDs based on
# Retrieved_Gtid_Set
if ( $_is_sql_thread_running == No )
{
  --let $assert_on_retrieved_gtid_set= 1
  --let $gtid_step_assert_include=include/gtid_step_assert_on_retrieved.inc
  --let $gtid_step_reset_include=include/gtid_step_reset_on_retrieved.inc
}
if ( $_is_sql_thread_running == Yes )
{
  --let $assert_on_retrieved_gtid_set= 0
  --let $gtid_step_assert_include=include/gtid_step_assert.inc
  --let $gtid_step_reset_include=include/gtid_step_reset.inc
}

--source include/rpl_connection_master.inc
# GTID steps will be based on master's UUID
--let $gtid_step_uuid= `SELECT @@GLOBAL.SERVER_UUID`
--source include/rpl_connection_slave.inc
--source $gtid_step_reset_include

# Creating tables t1 and t2 using $_storage_engine
# Table t1 will log the testcase activity
# Table t2 will be used to insert data to be tested
--source include/rpl_connection_master.inc
--eval CREATE TABLE t1 (i INT NOT NULL AUTO_INCREMENT PRIMARY KEY, info VARCHAR(64)) ENGINE=$_storage_engine
--eval CREATE TABLE t2 (i INT) ENGINE=$_storage_engine

#
# First, we insert some data, restart the slave IO thread and
# sync slave SQL thread (if it is running) with master
# as a normal case just for control.
#

# Insert data without splitting transactions in the relay log
INSERT INTO t1 (info) VALUE ('Insert data without splitting transactions in the relay log');

XA START 'trx0';
INSERT INTO t2 (i) VALUES (-6);
INSERT INTO t2 (i) VALUES (-5);
INSERT INTO t2 (i) VALUES (-4);
XA END 'trx0';
XA COMMIT 'trx0' ONE PHASE;

# Check if SQL thread was running before to sync it
if ( $_is_sql_thread_running == Yes )
{
  # Sync SQL thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_sql_with_master.inc
  --let diff_tables= master:t1, slave:t1
  --source include/diff_tables.inc
}
# Else we sync only the IO thread
if ( $_is_sql_thread_running == No )
{
  # Sync IO thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_io_with_master.inc
}

# Restart the IO thread not in the middle of transaction
--source include/rpl_connection_slave.inc
--source include/stop_slave_io.inc
--source include/start_slave_io.inc

# Check if the IO thread retrieved the correct amount of GTIDs
--source include/rpl_connection_slave.inc
--let $gtid_step_count= 4

--source $gtid_step_assert_include

#
# Second, we make master rotate its binlog
#

# Insert data rotating master binlog between two transactions
--source include/rpl_connection_master.inc
INSERT INTO t1 (info) VALUE ('Insert data rotating master binlog between two transactions');

XA START 'trx1';
INSERT INTO t2 (i) VALUES (-3);
INSERT INTO t2 (i) VALUES (-2);
XA END 'trx1';
XA PREPARE 'trx1';
XA COMMIT 'trx1';

FLUSH LOGS;
INSERT INTO t1 (info) VALUE ('After FLUSH LOGS at master');
XA START 'trx2';
INSERT INTO t2 (i) VALUES (-1);
INSERT INTO t2 (i) VALUES (0);
XA END 'trx2';
XA COMMIT 'trx2' ONE PHASE;

# Check if SQL thread was running before to sync it
if ( $_is_sql_thread_running == Yes )
{
  # Sync SQL thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_sql_with_master.inc
  --let diff_tables= master:t1, slave:t1
  --source include/diff_tables.inc
}
# Else we sync only the IO thread
if ( $_is_sql_thread_running == No )
{
  # Sync IO thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_io_with_master.inc
}

# Restart the IO thread again, not in the middle of transaction
--source include/rpl_connection_slave.inc
--source include/stop_slave_io.inc
--source include/start_slave_io.inc

# Check if the IO thread retrieved the correct amount of GTIDs
--source include/rpl_connection_slave.inc
--let $gtid_step_count= 5
--source $gtid_step_assert_include

#
# Third, let's go with splitting transactions
#

--let $info_table= t1
--let $table= t2
--let $counter= 0

# Stop after GTID, just if GTIDs are enabled
--inc $counter
--let $debug_point= stop_io_after_reading_gtid_log_event
--let $gtids_after_stop= 1
--let $gtids_after_sync= 3

--source extra/rpl_tests/rpl_xa_trx_boundary_parser_one_step.inc

# Stop after BEGIN query
--inc $counter
--let $debug_point= stop_io_after_reading_query_log_event
--let $gtids_after_stop= 1
--let $gtids_after_sync= 3
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_one_step.inc


# Stop after TABLE_MAP, just for RBR
--inc $counter
--let $debug_point= stop_io_after_reading_table_map_event
--let $gtids_after_stop= 1
--let $gtids_after_sync= 3
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_one_step.inc

# Stop after row event, just for RBR
--inc $counter
--let $debug_point= stop_io_after_reading_write_rows_log_event
--let $gtids_after_stop= 1
--let $gtids_after_sync= 3
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_one_step.inc

# Stop after XA END
--inc $counter
--let $debug_point= stop_io_after_xa_end_event
--let $gtids_after_stop= 1
--let $gtids_after_sync= 3
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_one_step.inc

# Stop after XA PREPARE
--inc $counter
--let $debug_point= stop_io_after_reading_xa_prepare_log_event
--let $gtids_after_stop= 2
--let $gtids_after_sync= 2
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_one_step.inc


# Check if SQL thread was running before to sync it
if ( $_is_sql_thread_running == Yes )
{
  # Sync SQL thread
  --source include/rpl_connection_master.inc
  --source include/sync_slave_sql_with_master.inc
  --let diff_tables= master:t1, slave:t1
  --source include/diff_tables.inc
}

# Dropping tables t1 and t2
--source include/rpl_connection_master.inc
DROP TABLE t1,t2;

# Check if SQL thread was running before to sync it
if ( $_is_sql_thread_running == Yes )
{
  # Let the slave to sync with the master before exiting the include
  --source include/sync_slave_sql_with_master.inc
}


