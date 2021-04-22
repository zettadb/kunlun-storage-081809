use test;
create table t1(a serial primary key, b int);

insert into t1(b) values(1);

begin;
insert into t1(b) values(2);
commit;

--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc

#1
xa start 'a1';
insert into t1(b) values(3);
xa end 'a1';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a1' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a1
xa recover;
show global status like 'innodb_num_aborted_txns_no_gtid';
show global status like 'innodb_num_recovered_txns';
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

#2
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a11';
update t1 set b=b+1 where a=1;
xa end 'a11';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a11' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a11
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

#3
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a2';
insert into t1(b) values(3);
update t1 set b=b+1 where a=2;
xa end 'a2';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a2' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a2
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
#4
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a3';
insert into t11(b) values(1);
insert into t1(b) values(11);
xa end 'a3';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a3' $xa_cmd_extra;
--source include/start_mysqld.inc
--echo expect no a3
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#5
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a31';
delete from t11 where a=2;
update t11 set b=b+1 where a=1;
update t1 set b=b+1 where a=1;
xa end 'a31';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a31' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a31
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#6
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a32';
insert into t11(b) values(4);
update t11 set b=b+1 where a=2;
update t1 set b=b+1 where a=1;
xa end 'a32';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a32' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a32
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
#7
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a33';
insert into t11(b) values(1);
insert into t1(b) values(11);
xa end 'a33';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a33' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a33
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
#8
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a331';
insert into t11(b) values(1);
update t1 set b=b+1 where a=1;
xa end 'a331';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a331' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a331
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#9
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a332';
insert into t11(b) values(4);
insert into t1(b) values(11);
update t1 set b=b+1 where a=1;
delete from t1 where a=2;
xa end 'a332';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a332' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a332
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#10
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a34';
delete from t11 where a=2;
update t11 set b=b+1 where a=1;
update t1 set b=b+1 where a=1;
delete from t1 where a=2;
xa end 'a34';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a34' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a34
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#11
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a333';
update t11 set b=b+1 where a=1;
insert into t1(b) values(11);
xa end 'a333';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a333' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a333
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#12
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a334';
update t11 set b=b+1 where a=1;
insert into t1(b) values(11);
update t1 set b=b+1 where a=1;
delete from t1 where a=2;
xa end 'a334';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a334' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a334
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#13
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a35';
insert into t11(b) values(4);
update t11 set b=b+1 where a=2;
insert into t1(b) values(11);
xa end 'a35';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a35' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a35
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#14
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a351';
insert into t11(b) values(4);
update t11 set b=b+1 where a=2;
update t1 set b=b+1 where a=1;
delete from t1 where a=2;
xa end 'a351';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a351' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a351
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc

create temporary table t11(a serial primary key, b int);
insert into t11(b) values(1),(2),(3);
#15
--let $rpl_gtid_mode= ON
--let $rpl_set_enforce_gtid_consistency= 1
--source include/rpl_set_gtid_mode.inc
--source include/add_debug_point.inc
xa start 'a352';
insert into t11(b) values(4);
update t11 set b=b+1 where a=2;
insert into t1(b) values(11);
update t1 set b=b+1 where a=1;
delete from t1 where a=2;
xa end 'a352';

--source include/expect_crash.inc

--error 2013
eval xa $xa_cmd 'a352' $xa_cmd_extra;

--source include/start_mysqld.inc

--echo expect no a352
xa recover;
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_recovered_txns\', Value, 1]" = $innodb_recovered_txns_expected
--source include/assert.inc
--let $assert_text= innodb txn recovery checks
--let $assert_cond= "[show global status like \'innodb_num_aborted_txns_no_gtid\', Value, 1]" = $innodb_aborted_txns_no_gtid_expected
--source include/assert.inc


drop table t1;

