if [ $# -ne 2 ]
then
    echo "usage: ./post-install.sh <installation-dir-full-path> <build-type>"
    exit 0;
fi

#copy doc
cp -f INSTALL.kunlun.md $1

srcroot=`pwd`

#create needed dirs in binary dir
cd $1
mkdir -p dba_tools share_lib .run_so etc

cd $1/bin
#make sure the built binaries have exe modes.
ls |while read f ; do chmod 0755 $f ; done

#copy stuff to binary
cd $srcroot/dba_tools
cp -f libjemalloc.so.3.6.0 $1/.run_so
cp -f libjemalloc.so.3.6.0  $1/share_lib

# these files must also exist in dba_tools/ dir
cp -f bootmysql.sh imysql.sh mgr_config.json seq_reserve_vals.sql stopmysql.sh template-small.cnf  $1/dba_tools
cp -f install-mysql.py monitormysql.sh  startmysql.sh template.cnf  $1/dba_tools
cp -f add_shards.py bootstrap.py common.py create_cluster.py meta_inuse.sql $1/dba_tools

#make sure all scripts are executable
cd $1/dba_tools
ls *.sh |while read f ; do chmod -R 0755 $f ; done

rm -fr $1/mysql-test
mkdir -p $1/resources
cp -f $srcroot/resources/mysql-connector-python-2.1.3.tar.gz $1/resources

cd $1/lib
mkdir -p deps

#copy all dependent dynamic lib files into lib/deps
cd $1/bin
ls | while read f; do
    if test -L "$f"; then
        relpath="`readlink $f`"
        test -f "$relpath" && rm -f "$f" && cp -f "$relpath" $f && rm -f "$relpath"
    fi
done

cd $1/lib
ls | while read f; do
    if test -L "$f"; then
        relpath="`readlink $f`"
        test -e "$relpath" || rm -f "$f"
    fi
done


cd $1/bin
export LD_LIBRARY_PATH="$1/lib:$LD_LIBRARY_PATH"
rm -f ./prog-deps.txt
ls | xargs ldd >> ./prog-deps.txt 2>/dev/null
cat ./prog-deps.txt | sed -n '/^.* => .*$/p' | sed  's/^.* => \(.*\)(.*$/\1/g' | sort | uniq | sed /^.*percona.*$/d | sed '/^ *$/d' | while read f ; do
	echo "install $f to lib/deps"
	cp -f $f $1/lib/deps
done
rm ./prog-deps.txt

# strip binaries if necessary
# echo "build-type: $2"
if test "$2" = "Release"; then
        cd $1
        strip -gs bin/* 2>/dev/null
        strip -gs lib/* 2>/dev/null
fi

