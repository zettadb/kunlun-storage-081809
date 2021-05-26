#!/bin/sh

if [ $# -ne 1 ];then
	echo "usage: $0 port"
	exit -1
fi

port=$1

base_dir=`pwd | awk -F"/dba_tools$" '{print $1}'`

conf_list_file=${base_dir}/etc/instances_list.txt
if [ ! -f  $conf_list_file ];then
	echo "have not found instances list file:$conf_list_file, can not start\n"
	exit -1
fi

etcfile=`grep "$port==>" $conf_list_file | head -1 | sed "s/^$port==>//g"` 
if test "$etcfile" = ""; then
	echo "have not found instance with port:$port, can not start\n"
	exit -1
elif test ! -f $etcfile; then
	echo "have not found config file:$etcfile, can not start\n"
	exit -1
fi

sockfile=` grep "^socket *=" ${etcfile}  | tail -n1| awk -F"=" '{print $2}'`
shelluser=`whoami`
mysqluser=`grep "user=" ${etcfile} | tail -n1 | awk -F"=" '{print $2}' `


programe="bin/mysqld"
cmd="ps -ef | grep ${programe} | grep -v vim | grep -v grep | grep -v defunct | grep -- 'my_${port}.cnf' | awk '{print \$2}' | head -n 1"
#echo $cmd
pid=$(eval ${cmd})
#echo $pid
if [ $pid"e" != "e" ];then
	echo "mysqld is running,can't start"
	exit -1
fi



if [ $shelluser != $mysqluser ];then
	su $mysqluser -c "./bootmysql.sh $base_dir ${etcfile} ${mysqluser}"
else
	./bootmysql.sh $base_dir ${etcfile} ${mysqluser}
fi

