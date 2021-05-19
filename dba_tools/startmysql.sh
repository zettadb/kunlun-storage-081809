#!/bin/sh

if [ $# -ne 1 ];then
	echo "usage: $0 port"
	exit -1
fi

port=$1

base_dir=`pwd | awk -F"/dba_tools$" '{print $1}'`
etcfile=${base_dir}/etc/my_${port}.cnf

if [ ! -f  $etcfile ];then
	echo "have not find etc file:$etcfile,can't not start\n"
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

