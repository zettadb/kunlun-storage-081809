#!/bin/sh

if [ $# -ne 1 ];then
   echo "usage: $0 port"
   exit
fi

port=$1
base_dir=`dirname "$PWD"`

conf_list_file=${base_dir}/etc/instances_list.txt
if [ ! -f  $conf_list_file ];then
	echo "Can not find instances list file:$conf_list_file, can not stop."
	exit -1
fi

etcfile=`grep "$port==>" $conf_list_file | head -1 | sed "s/^$port==>//g"`
if test "$etcfile" = ""; then
	echo "Can not find instance with port:$port, can not stop."
	exit -1
elif test ! -f $etcfile; then
	echo "Can not find config file:$etcfile, can not stop."
	exit -1
fi

programe="bin/mysqld"

ps -ef | grep my_${port}.cnf | grep ${programe} | grep -- "--defaults-file="| grep -v grep | awk '{print $2}'  | xargs kill -9
