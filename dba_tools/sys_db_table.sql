CREATE DATABASE IF NOT EXISTS `kunlun_sysdb`; 
CREATE TABLE IF NOT EXISTS `kunlun_sysdb`.`heartbeat` (
  `host_port` varchar(20) NOT NULL,
  `beat_time` varchar(30) DEFAULT NULL,
  PRIMARY KEY (`host_port`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `kunlun_sysdb`.`cluster_info` (
  `id` int NOT NULL AUTO_INCREMENT, 
  `cluster_name` varchar(30) DEFAULT NULL,
  `shard_name` varchar(30) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS `kunlun_sysdb`.`sequences`(
  `db` varchar(512) not null,
  `name` varchar(512) not null,
  `curval` bigint not null,
  `start` bigint not null,
  `step` int not null,
  `max_value` bigint not null,
  `min_value` bigint not null,
  `do_cycle` bool not null,
  `n_cache` int unsigned not null,
  primary key(`db`,`name`)
) engine=innodb CHARACTER SET utf8 COLLATE utf8_general_ci STATS_PERSISTENT=0 comment='Kunlun sequence metadata used by computing nodes' ROW_FORMAT=DYNAMIC;


