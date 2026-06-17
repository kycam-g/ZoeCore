-- ZoeCore Auto Learn V1
-- Rode no banco characters.

CREATE TABLE IF NOT EXISTS `zoecore_auto_learn_state` (
  `guid` int(10) unsigned NOT NULL,
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `version` int(10) unsigned NOT NULL DEFAULT '0',
  `applied_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_account_id` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
