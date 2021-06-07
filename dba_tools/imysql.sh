#!/bin/sh

if [ $# -lt 1 ];then
   echo "usage: $0 port {sql}"
   echo "arg num: $#"
   exit
fi

cmd="e"
if [ $# -ge 2 ];then
    cmd=$2
fi

echo "cmd: ${cmd}"

port=$1
base_dir=`dirname "$PWD"`

conf_list_file=${base_dir}/etc/instances_list.txt
if [ ! -f  $conf_list_file ];then
	echo "Can not find instances list file:$conf_list_file\n"
	exit -1
fi

etcfile=`grep "$port==>" $conf_list_file | head -1 | sed "s/^$port==>//g"`
if test "$etcfile" = ""; then
	echo "Can not find instance with port:$port\n"
	exit -1
elif test ! -f $etcfile; then
	echo "Can not find config file:$etcfile\n"
	exit -1
fi

sockfile=` grep "^socket *=" ${etcfile}  | tail -n1| awk -F"=" '{print $2}'`

echo ${sockfile}
echo ${base_dir}

if [ "$cmd" != "e" ];then
    cd ${base_dir} ; ./bin/mysql -uroot -proot -S${sockfile} -A -e "$cmd"
else
    cd ${base_dir} ; ./bin/mysql -uroot -proot -S${sockfile} -A
fi

