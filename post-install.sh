if [ $# -ne 1 ]
then
    echo "usage: ./post-install.sh <installation-dir-full-path>"
    exit 0;
fi

#copy doc
cp INSTALL.kunlun.md $1

srcroot=`pwd`

#create needed dirs in binary dir
cd $1
mkdir -p dba_tools share_lib .run_so etc

cd $1/bin
#make sure the built binaries have exe modes.
ls |while read f ; do chmod 0755 $f ; done

#copy stuff to binary
cd $srcroot/dba_tools
cp libjemalloc.so.3.6.0 $1/.run_so
cp libjemalloc.so.3.6.0  $1/share_lib

# these files must also exist in dba_tools/ dir
cp bootmysql.sh  imysql.sh mgr_config.json  seq_reserve_vals.sql stopmysql.sh  template-small.cnf  $1/dba_tools
cp install-mysql.py  monitormysql.sh  startmysql.sh    template.cnf  $1/dba_tools

#make sure all scripts are executable
cd $1/dba_tools
ls *.sh |while read f ; do chmod -R 0755 $f ; done


cd $1/lib
mkdir -p deps

#copy all dependent dynamic lib files into lib/deps
cd $1/bin
ls | xargs ldd >> ./prog-deps.txt
cat ./prog-deps.txt | sed -n '/^.* => .*$/p' | sed  's/^.* => \(.*\)(.*$/\1/g' | sort | uniq | sed /^.*percona.*$/d | while read f ; do  cp $f $1/lib/deps ; done
rm ./prog-deps.txt

# strip binaries
cd $1
strip -gs bin/*
strip -gs lib/*

