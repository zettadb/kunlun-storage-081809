#!/bin/sh

if [ $# -ne 1 ];then
   echo "usage: $0 port"
   exit
fi

port=$1
base_dir=`dirname "$PWD"`

conf_list_file=${base_dir}/etc/instances_list.txt
if [ ! -f  $conf_list_file ];then
	echo "have not found instances list file:$conf_list_file, can not start"
	exit -1
fi

etcfile=`grep "$port==>" $conf_list_file | head -1 | sed "s/^$port==>//g"`
if test "$etcfile" = ""; then
	echo "have not found instance with port:$port, can not start"
	exit -1
elif test ! -f $etcfile; then
	echo "have not found config file:$etcfile, can not start"
	exit -1
fi

sockfile=` grep "^socket *=" ${etcfile}  | tail -n1| awk -F"=" '{print $2}'`

$base_dir/bin/mysqladmin shutdown -S ${sockfile} -uroot -proot

programe="bin/mysqld"
cmd="ps -ef | grep ${programe} | grep -v vim | grep -v grep | grep -v defunct | grep -- 'my_${port}.cnf' | awk '{print \$2}' | head -n 1"

stopfail=1
for i in `seq 0 30`
#for i in `seq 0 15`
do
	pid=$(eval ${cmd})
	#echo $pid
	if [ $pid"e" != "e" ];then
       	 	echo "mysqld is running,count:${i}."
	else
		echo "mysqld is stoped"
		stopfail=0
		break
	fi

   	sleep 1
done

#force stop
if [ $stopfail -ne 0 ];then
   ps -ef | grep my_${port}.cnf | grep ${programe} | grep -- "--defaults-file="| grep -v grep | awk '{print $2}'  | xargs kill -9
fi

exit $stopfail
