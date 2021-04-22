Kunlun-storage originated from percona-mysql-8.0.18-9, and it contains fixes to all known XA bugs in mysql-8.0.18-9. Without such fixes, Kunlun DDC will not be crash safe and may lose committed transactions or be harmed by other serious data consistency errors, in the event of various hardware/software/network failures.

Kunlun-storage also contains features required by the computing node program of Kunlun distributed DBMS, and thus Kunlun Distributed Database Cluster(DDC) requires the use of Kunlun-storage as meta data cluster and storage shards. Finally, we enhanced performance of XA transaction processing, and part of such enhancements are also in this open source edition.

To achieve all above, we modified percona-mysql extensively --- including innodb, binlog recovery, binlog format, etc. Consequently, kunlun-storage's innodb data file format and some binlog events format are different from community MySQL-8.0.x or percona-server-8.0.x, the data directory of kunlun-storage can not be used by community MySQL-8.0.x or percona-server-8.0.x, and vice versa. But percona xtrabackup can correctly backup a kunlun-storage data directory and restore it.

We also maintain an enterprise edition of Kunlun distributed DBMS, which contains exclusively all performance enhancements in kunlun-storage and kunlun computing node software. Kunlun enterprise edition has identical functionality as this open source version. And they share the same data file format, WAL(redo) log file format, binlog events format, general log format, slow query log format, mysqld log format, and config file content. They also share the same metadata table format in both kunlun-storage and kunlun computing node. Consequently, the data directory of kunlun open source edition and kunlun enterprise edition can be used interchangably.

To install Kunlun-storage database instances, see INSTALL.kunlun.md for instructions.


Percona Server 8.0
------------------

Percona Server is a branch of MySQL 8.0 bringing higher performance,
reliability and more features.

http://www.percona.com/software/percona-server/

Documentation: http://www.percona.com/doc/percona-server/8.0

Bug Tracker: https://jira.percona.com/projects/PS

Sources: https://github.com/percona/percona-server
