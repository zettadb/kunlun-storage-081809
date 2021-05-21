#Welcome to Kunlun-storage database instance installation guide!

##Intro

Kunlun-storage originated from percona-mysql-8.0.18-9, and it contains fixes to all known XA bugs in mysql-8.0.18-9. Without such fixes, Kunlun DDC will not be crash safe and may lose committed transactions or be harmed by other serious data consistency errors, in the event of various hardware/software/network failures.

Kunlun-storage also contains features required by the computing node program of Kunlun distributed DBMS, and thus Kunlun Distributed Database Cluster(DDC) requires the use of Kunlun-storage as meta data cluster and storage shards. Finally, we enhanced performance of XA transaction processing, and part of such enhancements are also in this open source edition.

To achieve all above, we modified percona-mysql extensively --- including innodb, binlog recovery, binlog format, etc. Consequently, kunlun-storage's innodb data file format and some binlog events format are different from community MySQL-8.0.x or percona-server-8.0.x, the data directory of kunlun-storage can not be used by community MySQL-8.0.x or percona-server-8.0.x, and vice versa. But percona xtrabackup can correctly backup a kunlun-storage data directory and restore it.

We also maintain an enterprise edition of Kunlun distributed DBMS, which contains exclusively all performance enhancements in kunlun-storage and kunlun computing node software. Kunlun enterprise edition has identical functionality as this open source version. And they share the same data file format, WAL(redo) log file format, binlog events format, general log format, slow query log format, mysqld log format, and config file content. They also share the same metadata table format in both kunlun-storage and kunlun computing node. Consequently, the data directory of kunlun open source edition and kunlun enterprise edition can be used interchangably.

##Build from source

The same as community mysql or percona-server --- use cmake to configure it then do something like 'make install -j8'.

A typical cmake configuration we often use is as below. Note that we do use jemalloc, a shared object of prebuilt jemalloc is provided and is installed to each kunlun-storage db instance.

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<installation directory> -DWITH_SSL=system -DWITH_BOOST=<boost source root directory> -DWITH_KEYRING_VAULT=0 -DWITH_ZLIB=bundled -DWITH_TOKUDB=NO -DWITH_EDITLINE=bundled -DWITH_LTO=0  -DWITH_ROUTER=0 -DWITH_FEDERATED_STORAGE_ENGINE=1 -DWITH_ARCHIVE_STORAGE_ENGINE=1 -DWITH_NGRAM_PARSER=1 -DWITHOUT_ROCKSDB=1 -DWITH_ZSTD=bundled -DWITH_LZ4=bundled -DWITH_PROTOBUF=bundled -DWITH_LIBEVENT=bundled -DWITH_JEMALLOC=0

After 'make install' completed, in root source directory do 'bash post-install <installation directory>'

##Library dependencies

If you are using a kunlun-storage program built from source on the same Linux distribution and version as where it's being used, simply skip this step because there is no dependency issues.

Add Kunlun-storage/lib into LD_LIBRARY_PATH.

All dynamic shared objects (*.so files) that programs in Kunlun-storage/bin depend on, are provided in Kunlun-storage/lib/deps directory. Try startup mysqld (e.g. mysqld --version) and see if your local Linux distro needs any of the *.so files. If so, copy needed ones into Kunlun-storage/lib.

DO NOT copy everything in deps into lib at once, otherwise your linux OS or any software may not be able to work because of library version mismatches!

## Database Instance Installation

Use the ./dba_tools/install-mysql.py script to install an MGR node. To do so, one first needs to prepare an MGR cluster configuration file using the template in dba_tools/mgr_config.json. In this doc below the config file will be called 'my-shards.json'.

One should set the 'mgr_config' argument to the path of the MGR cluster configuration file.

One should set the 'target_node_index' argument of install-mysql.py to specify the target db instance to install. 'target_node_index' is the target db instance's array index in the 'nodes' array of my-shards.json. Note that one should always install the group replication primary node before installing any of its replicas, and make sure the primary node specified in config file is really currently the primary node of the MGR shard.

Then in Kunlun-storage/dba_tools directory, do below:
`python install-mysql.py mgr_config=./my-shards.json target_node_index=0`
There are other optional parameters to install-mysql.py, but they are not used for now or can be generated automatically except the 'dbcfg' argument, as detailed below.

To startup mysql server, use the script 'startmysql.sh' Kunlun-storage/dba_tools: ./startmysql.sh 3306
There are several other useful scripts in Kunlun-storage/dba_tools, use 'imysql.sh port' to connect to a db instance created by install-mysql.py; Use 'monitormysql.sh port' to see current QPS and other performance counters; Use 'stopmysql.sh port' to stop a mysqld server process.

### Alternative db instance config templates

There are multiple db instance config template files in ./dba_tools directory. Among them, ./dba_tools/template.cnf is the default if 'dbcfg' argument isn't specified in install-mysql.py execution, and should always be used for Kunlun-storage db instances. Settings in ./dba_tools/template.cnf are premium for best performance, suitable for high performance mysql servers.

For the rest template files, the ./dba_tools/mysql-template.cnf is suitable for original mysql-8.0.x (x >=18) instances, and ./dba_tools/percona-template.cnf is suitable for original percona-mysql-8.0.18-9. Both of them originated from ./dba_tools/template.cnf, with unsupported arguments commented out and all other settings are the same as ./dba_tools/template.cnf.

And ./dba_tools/template-small.cnf is suitable for Kunlun-storage db instances with trivial resource consumption, e.g. I use it for Kunlun DDC cluster installation testing. One could use another template file as necessary for other purposes.

If one needs other config templates than ./dba_tools/template.cnf, specifiy the 'dbcfg' argument. For example when you have an binary installation of original mysql-8.0.x or a percona-mysql-8.0.x-y, you can copy the Kunlun-storage/dba_tools directory into that installation's root directory, in order to make use of the installation scripts and config templates. And you can create another template file and use it to create db instances as below example.

e.g. `python install-mysql.py mgr_config=./mgr_config0.json target_node_index=0 dbcfg=./mysql-template-small.cnf`

### Shard Config File Explained

{
    "group_uuid":"e6578c3a-547e-11ea-9780-981fd1bd410d",	# mgr group uuid. should be unique in a Kunlun DDC cluster for best readability.
    "nodes":						#each node contains configs of a db instance.
    [
       {
          "is_primary":true,				# whether this db instance is a primary node. there must be one and only one primary node in the 'nodes' array.
          "ip": "127.0.0.1",				# ip address to listen on, bind_address and report_host are set to this ip.
          "port": 6001,					# regular listen port
          "xport": 60010,				# mysql X protocol listen port
          "mgr_port": 60011,				# port for mgr communication between an MGR group. not used by clients.
          "innodb_buffer_pool_size":"64MB",		# innodb buffer pool size
          "data_dir_path":"/data",			# innodb data files are stored here
          "log_dir_path":"/data/log",			# binlogs and innodb redo logs and mysqld.err are stored in this path.
          "user":"abc",					# A valid OS user name, the created db instance's all data/log dirs/files will be owned by this user.
          "election_weight":50				# mgr primary election weight. don't modify it without reading the relevant doc first.
       }
	, more shard node (db instance) config objects
    ]
}


## CMake configuration

This binary is build using below cmake configuration. Note that WITH_JEMALLOC is 0 because there would be error during cmake generation or build phase if it's 1 on some Linux distributions. But a prebuilt jemalloc shared object library file is used when starting up mysqld, see dba_tools/bootmysql.sh for details. Setting WITH_LTO = 1 could produce a little bit of performance gains but it only works for a narrow range of gcc versions.
 
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/home/abc/mysql_installs/kunlun-storage-bin-rel -DWITH_SSL=system -DWITH_BOOST=/home/abc/Downloads/boost_1_70_0 -DWITH_KEYRING_VAULT=0 -DWITH_ZLIB=bundled -DWITH_TOKUDB=NO -DWITH_EDITLINE=bundled -DWITH_LTO=0  -DWITH_ROUTER=0 -DWITH_FEDERATED_STORAGE_ENGINE=1 -DWITH_ARCHIVE_STORAGE_ENGINE=1 -DWITH_NGRAM_PARSER=1 -DWITHOUT_ROCKSDB=1 -DWITH_ZSTD=bundled -DWITH_LZ4=bundled -DWITH_PROTOBUF=bundled -DWITH_LIBEVENT=bundled -DWITH_JEMALLOC=0
