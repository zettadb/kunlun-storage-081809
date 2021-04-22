# ==== Purpose ====
#
# This include will insert some data into a table at the master varying the
# debug sync point at slave that will be used to stop the IO thread in the
# middle of an XA transaction event stream (trying to let partial transactions in
# the relay log), and see if SQL thread can skip the incomplete event group and
# reexecute the complete version of the same event group that follows.
#
# It will do this task (insert some data) twice.
#
# The first time with the SQL thread stopped, letting the IO thread do its job
# until all data is replicated, starting the SQL only at the end of the test.
#
# The second time, the SQL thread will be running all the time, syncing on each
# step of the test.
#
# ==== Usage ====
#
# --source extra/rpl_tests/rpl_xa_trx_boundary_parser.inc
#
#
--source include/have_binlog_format_row.inc

--echo ## Running the test with the SQL thread stopped
--source include/rpl_connection_slave.inc
--source include/stop_slave_sql.inc
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_all_steps.inc

--echo ## Starting and syncing the SQL thread before next round
--source include/rpl_connection_slave.inc
--source include/start_slave_sql.inc
--source include/rpl_connection_master.inc
--source include/sync_slave_sql_with_master.inc

--echo ## Running the test with the SQL thread started
--source extra/rpl_tests/rpl_xa_trx_boundary_parser_all_steps.inc
