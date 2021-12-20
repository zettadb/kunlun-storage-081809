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


