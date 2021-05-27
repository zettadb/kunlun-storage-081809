#!/bin/python

import os
import os.path
import sys
import re
import time
import random
import fcntl
import struct
import socket
import subprocess
import json
import shlex


def param_replace(string, rep_dict):
    pattern = re.compile("|".join([re.escape(k) for k in rep_dict.keys()]), re.M)
    return pattern.sub(lambda x: rep_dict[x.group(0)], string)

def get_root_init_pwd(logdir):
    install_inf = open(logdir + '/mysqld.err', 'r')
    lines = install_inf.readlines()
    for line in lines:
        if 'A temporary password is generated for root@localhost' in line:
            ret = line.split('root@localhost: ')[1][:-1]
            return ret


def make_mgr_args(mgr_config_path, replace_items, target_node_index):
    jsconf = open(mgr_config_path)
    jstr = jsconf.read()
    jscfg = json.loads(jstr)

    nodeidx = target_node_index
    group_uuid = jscfg['group_uuid']
    idx = 0
    white_list = ''
    local_addr = ''
    local_ip = ''
    seeds = ''
    is_master_node = False
    weight = 50
    mgr_port = 0
    server_xport = 0
    server_port  = 0
    innodb_buffer_pool_size = 0
    server_data_prefix = ''
    server_log_prefix = ''
    db_inst_user = ''
    log_arch = ''

    for val in jscfg['nodes']:
        if val['is_primary'] == True and idx == nodeidx:
            is_master_node = True
        if white_list != '':
            white_list = white_list + ','
            seeds = seeds + ','

        white_list = white_list + val['ip']
        seeds = seeds + val['ip'] + ':' + str(val['mgr_port'])

        if idx == nodeidx:
            local_ip = val['ip']
            local_addr = val['ip'] + ':' + str(val['mgr_port'])
            mgr_port = val['mgr_port']
            weight = val['election_weight']
            server_port = val['port']
            server_xport = val['xport']
            innodb_buffer_pool_size = val['innodb_buffer_pool_size']
            server_data_prefix = val['data_dir_path']
            server_log_prefix = val['log_dir_path']
            db_inst_user = val['user']
            if val.has_key('innodb_log_dir_path'):
                log_arch = val['innodb_log_dir_path']

        idx = idx + 1

    mgr_num_nodes = idx + 1
#    if mgr_num_nodes < 2:
#        raise ValueError("Config error, need at least two nodes in the shard.")
    if local_addr == '' or nodeidx < 0 or nodeidx >= mgr_num_nodes:
        raise ValueError("Config error, target_node_index must be in [0, {}).".format(mgr_num_nodes))
    if seeds == '':
        raise RuntimeError("Config error, no primary node specified.")

    if server_xport == mgr_port or server_port == mgr_port or server_port == server_xport :
        raise ValueError("Config error, MGR port(" + str(mgr_port) + "), client regular port(" + str(server_port) + ") and X protocol port(" + str(server_xport) + ") must be different.")
    data_path = server_data_prefix + "/" + str(server_port)
    prod_dir = data_path + "/prod"
    data_dir = data_path + "/dbdata_raw/data"
    innodb_dir = data_path + "/dbdata_raw/dbdata"
    tmp_dir = data_path + "/tmp"
    
    log_path = server_log_prefix + "/" + str(server_port)
    log_dir = log_path + "/dblogs"
    log_relay = log_path + "/dblogs/relay"
    log_bin_arg = log_path + "/dblogs/bin"

	# seperate binlog/relaylogs from innodb redo logs, store them in two dirs so possibly two disks to do parallel IO.
    if log_arch == '':
        log_arch = data_path + "/dblogs/arch"

    replace_items["place_holder_ip"] = local_ip
    replace_items["place_holder_mgr_recovery_retry_count"] = str(mgr_num_nodes*100)
    replace_items["place_holder_mgr_local_address"] = local_addr
    replace_items["place_holder_mgr_seeds"] = seeds
    replace_items["place_holder_mgr_whitelist"] = white_list
    replace_items["place_holder_mgr_member_weight"] = str(weight)
    replace_items["place_holder_mgr_group_name"] = group_uuid
    replace_items["prod_dir"] = prod_dir
    replace_items["data_dir"] = data_dir
    replace_items["innodb_dir"] = innodb_dir
    replace_items["log_dir"] = log_dir
    replace_items["tmp_dir"] = tmp_dir
    replace_items["log_relay"] = log_relay
    replace_items["log_bin_arg"] = log_bin_arg
    replace_items["log_arch"] = log_arch
    replace_items["place_holder_innodb_buffer_pool_size"] = innodb_buffer_pool_size
    replace_items["place_holder_x_port"] =str(server_xport)
    replace_items["place_holder_port"] = str(server_port)
    replace_items["place_holder_user"] = db_inst_user

    
    os.makedirs(prod_dir)
    os.makedirs(data_dir)
    os.makedirs(innodb_dir)
    os.makedirs(log_dir)
    os.makedirs(tmp_dir)
    os.makedirs(log_relay)
    os.makedirs(log_bin_arg)
    os.makedirs(log_arch)
    jsconf.close()
    return is_master_node, server_port, data_path, log_path, log_dir, db_inst_user



class MysqlConfig:

    def __init__(self, config_template_file, install_path,  server_id, cluster_id, shard_id, mgr_config_path, target_node_index):


        replace_items = {
                #"place_holder_extra_port": str(int(server_port)+10000), mysql8.0 doesn't have 'extra_port' var.
                "base_dir": install_path,
                "place_holder_server_id": str(server_id),
                "place_holder_shard_id": str(shard_id),
                "place_holder_cluster_id": str(cluster_id),
                }

        is_master, server_port, data_path, log_path, log_dir, user = make_mgr_args(mgr_config_path, replace_items, target_node_index)
        config_template = open(config_template_file, 'r').read()
        conf = param_replace(config_template, replace_items)

        subprocess.call(["chown", user+":users", log_path, "-R"])
        subprocess.call(["chown", user+":users", data_path, "-R"])

        cnf_file_path = data_path+"/my_"+ str(server_port) +".cnf"
        cnf_file = open(cnf_file_path, 'w')
        cnf_file.write(conf)
        cnf_file.close()
        
        if is_master:
            start_mgr_sql = 'SET GLOBAL group_replication_bootstrap_group=ON; START GROUP_REPLICATION; SET GLOBAL group_replication_bootstrap_group=OFF; '
            #select group_replication_set_as_primary(@@server_uuid);  this can't be done now, it requires a quorum.
        else:
            start_mgr_sql = 'START GROUP_REPLICATION;'

        cmdstr = " ".join(["su", user, "-c",  "\""+install_path+"/bin/mysqld", "--defaults-file="+cnf_file_path, "--user="+user, "--initialize \""]) #, stdout=install_inf)
        os.system(cmdstr)
        root_init_password = get_root_init_pwd(log_dir)
        assert(root_init_password != None)

        # Enable the mgr options, which have to be commented at initialization because plugins are not loaded when mysqld is started with --initialize.
        os.system("sed -e 's/^#group_replication_/group_replication_/' -i " + cnf_file_path)
        os.system("sed -e 's/^#clone_/clone_/' -i " + cnf_file_path)

        os.system(" ".join(["su", user, "-c", "\"./bootmysql.sh", install_path, cnf_file_path, user+"\""]))

        os.system("sed -e 's/#skip_name_resolve=on/skip_name_resolve=on/' -i " + cnf_file_path)

        change_pwd_sql = "set sql_log_bin=0;ALTER USER 'root'@'localhost' IDENTIFIED BY 'root';"
        #TODO: use md5 signature for each below password, don't use plain text password; AND delete 'root' user.
        # the python mysql client lib cuts long sql text to multiple sections, so do 'set sql_log_bin=0; ' at beginning of each query.
        init_sql = "set sql_log_bin=0; create user clustmgr identified by 'clustmgr_pwd'; grant  Select,Insert,Update,Delete,GROUP_REPLICATION_ADMIN,SYSTEM_VARIABLES_ADMIN on *.* to clustmgr@'%';flush privileges;" \
                + "set sql_log_bin=0; create user repl identified by 'repl_pwd'; grant replication slave,replication client, BACKUP_ADMIN, CLONE_ADMIN on *.* to 'repl'@'%' ; flush privileges;" \
                + "set sql_log_bin=0; create user agent@localhost identified by 'agent_pwd'; grant all on *.* to 'agent'@'localhost' with grant option;flush privileges;select version();"
        init_sql2 = "set sql_log_bin=0; create user pgx identified by 'pgx_pwd' ; grant Select,Insert,Update,Delete,Create,Drop,Process,References,Index,Alter,SHOW DATABASES,CREATE TEMPORARY TABLES,LOCK TABLES,Execute,CREATE VIEW,SHOW VIEW,CREATE ROUTINE,ALTER ROUTINE,Event,Trigger,REPLICATION CLIENT,REPLICATION SLAVE,reload,GROUP_REPLICATION_ADMIN,SYSTEM_VARIABLES_ADMIN on *.* to  'pgx'@'%'; flush privileges;" \
                + "set sql_log_bin=0;delete from mysql.db where Db='test\_%' and Host='%' ;delete from mysql.db where Db='test' and Host='%';flush privileges;"  \
                + '''CHANGE MASTER TO MASTER_USER='repl', MASTER_PASSWORD='repl_pwd' FOR CHANNEL 'group_replication_recovery';'''   \
                + start_mgr_sql
        sys_cmd = " ".join([install_path + '/bin/mysql', '--connect-expired-password', '-S' + data_path + '/prod/mysql.sock', '-uroot', '-p'+"'"+root_init_password+"'", '-e', '"' + change_pwd_sql + init_sql + '"', '; exit 0'])

        add_proc_cmd = " ".join([install_path + '/bin/mysql', '--connect-expired-password', '-S' + data_path + '/prod/mysql.sock', '-uroot', '-proot <' , install_path+'/dba_tools/seq_reserve_vals.sql' ])
	initcmd2 = " ".join([install_path + "/bin/mysql", "--connect-expired-password", '-S' + data_path + '/prod/mysql.sock', '-uroot -proot', '-e', '"' + init_sql2 + '"\n'])
        for idx in xrange(30):
            result = subprocess.check_output(sys_cmd, shell = True, stderr=subprocess.STDOUT)
            if result.find('version') >= 0:
                break
            os.system('sleep 5\n')
        os.system(initcmd2)
        os.system(add_proc_cmd)

        os.system("sed -e 's/#super_read_only=OFF/super_read_only=ON/' -i " + cnf_file_path)
#        uuid_cmd_str = install_path + "/bin/mysql  --silent --skip-column-names --connect-expired-password -S" + data_path + '/prod/mysql.sock -uroot -proot -e "set @uuid_str=uuid(); set global group_replication_group_name=@uuid_str; ' + start_mgr_sql+ ' select @uuid_str;"\n'
#        uuid_cmd=shlex.split(uuid_cmd_str)
#        popen_ret = subprocess.Popen(uuid_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=install_path)
#        uuid_str, errmsg = popen_ret.communicate()
#        uuid_str = uuid_str[:-1] # chop off the trailing \n
#        subprocess.call(shlex.split("sed -e 's/place_holder_mgr_group_name/" + uuid_str + "/' -i " + cnf_file_path))
        # append the new instance's port to datadir mapping into instance_list.txt
        etc_path = install_path + "/etc"
        if not os.path.exists(etc_path):
        	os.mkdir(etc_path)
		subprocess.call(["chown", user+":users", etc_path, "-R"])
        conf_list_file = etc_path+"/instances_list.txt"
        os.system("echo \"" + str(server_port) + "==>" + cnf_file_path + "\" >> " + conf_list_file)

def print_usage():
    print 'Usage: install-mysql.py mgr_config=/path/of/mgr/config/file target_node_index=idx [dbcfg=/db/config/template/path/template.cnf] [cluster_id=ID] [shard_id=N] [server_id=N]'

# install-mysql.py mgr_config=/path/of/mgr/config/file target_node_index=idx [dbcfg=/db/config/template/path/template.cnf] [cluster_id=ID] [shard_id=N] [server_id=N]
if __name__ == "__main__":
    try:
        args = dict([arg.split('=') for arg in sys.argv[1:]])
        if not args.has_key('mgr_config') or not args.has_key('target_node_index'):
            print_usage()
            raise RuntimeError('Must specify mgr_config and target_node_index arguments.')
        mgr_config_path = args["mgr_config"]
        target_node_index = int(args["target_node_index"])

        shard_id = 0
        cluster_id = 0
        if args.has_key('cluster_id'):
            cluster_id = int(args['cluster_id'])
        if args.has_key('shard_id'):
            shard_id = int(args['shard_id'])
        if args.has_key('server_id'):
            server_id = int(args['server_id'])
        else:
            server_id = str(random.randint(1,65535))

        if args.has_key('dbcfg') :
            config_template_file = args['dbcfg']
        else:
            config_template_file = "./template.cnf"
        if not os.path.exists(config_template_file):
            raise ValueError("DB config template file {} doesn't exist!".format(config_template_file))
        install_path = os.path.dirname(os.getcwd())
        print "Installing mysql instance, please wait..."
        MysqlConfig(config_template_file, install_path, server_id, cluster_id, shard_id, mgr_config_path, target_node_index)
    except KeyError, e:
        print_usage()
        print e
