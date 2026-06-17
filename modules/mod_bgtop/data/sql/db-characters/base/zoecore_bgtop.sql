-- ZoeCore BGTop V1
-- Rode no banco characters.
-- Usa as tabelas pvpstats_battlegrounds e pvpstats_players do AzerothCore.

CREATE TABLE IF NOT EXISTS `zoecore_bgtop_processed` (
  `battleground_id` int(10) unsigned NOT NULL,
  `processed_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battleground_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_bgtop_match_summary` (
  `battleground_id` int(10) unsigned NOT NULL,
  `bg_type` int(10) unsigned NOT NULL DEFAULT '0',
  `bg_name` varchar(64) NOT NULL DEFAULT '',
  `winner_faction` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `top_kill_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `top_kill_value` int(10) unsigned NOT NULL DEFAULT '0',
  `top_damage_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `top_damage_value` int(10) unsigned NOT NULL DEFAULT '0',
  `top_heal_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `top_heal_value` int(10) unsigned NOT NULL DEFAULT '0',
  `top_hk_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `top_hk_value` int(10) unsigned NOT NULL DEFAULT '0',
  `top_honor_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `top_honor_value` int(10) unsigned NOT NULL DEFAULT '0',
  `top_deaths_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `top_deaths_value` int(10) unsigned NOT NULL DEFAULT '0',
  `created_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battleground_id`),
  KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_bgtop_stats_total` (
  `guid` int(10) unsigned NOT NULL,
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `kills` int(10) unsigned NOT NULL DEFAULT '0',
  `honorable_kills` int(10) unsigned NOT NULL DEFAULT '0',
  `deaths` int(10) unsigned NOT NULL DEFAULT '0',
  `damage` int(10) unsigned NOT NULL DEFAULT '0',
  `healing` int(10) unsigned NOT NULL DEFAULT '0',
  `honor` int(10) unsigned NOT NULL DEFAULT '0',
  `bgs_played` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_kills` (`kills`),
  KEY `idx_honorable_kills` (`honorable_kills`),
  KEY `idx_damage` (`damage`),
  KEY `idx_healing` (`healing`),
  KEY `idx_honor` (`honor`),
  KEY `idx_deaths` (`deaths`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_bgtop_stats_weekly` (
  `guid` int(10) unsigned NOT NULL,
  `week_year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `week_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `kills` int(10) unsigned NOT NULL DEFAULT '0',
  `honorable_kills` int(10) unsigned NOT NULL DEFAULT '0',
  `deaths` int(10) unsigned NOT NULL DEFAULT '0',
  `damage` int(10) unsigned NOT NULL DEFAULT '0',
  `healing` int(10) unsigned NOT NULL DEFAULT '0',
  `honor` int(10) unsigned NOT NULL DEFAULT '0',
  `bgs_played` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`, `week_year`, `week_number`),
  KEY `idx_week_kills` (`week_year`, `week_number`, `kills`),
  KEY `idx_week_damage` (`week_year`, `week_number`, `damage`),
  KEY `idx_week_healing` (`week_year`, `week_number`, `healing`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_bgtop_stats_monthly` (
  `guid` int(10) unsigned NOT NULL,
  `month_year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `month_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `kills` int(10) unsigned NOT NULL DEFAULT '0',
  `honorable_kills` int(10) unsigned NOT NULL DEFAULT '0',
  `deaths` int(10) unsigned NOT NULL DEFAULT '0',
  `damage` int(10) unsigned NOT NULL DEFAULT '0',
  `healing` int(10) unsigned NOT NULL DEFAULT '0',
  `honor` int(10) unsigned NOT NULL DEFAULT '0',
  `bgs_played` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`, `month_year`, `month_number`),
  KEY `idx_month_kills` (`month_year`, `month_number`, `kills`),
  KEY `idx_month_damage` (`month_year`, `month_number`, `damage`),
  KEY `idx_month_healing` (`month_year`, `month_number`, `healing`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_bgtop_period_state` (
  `period` varchar(16) NOT NULL,
  `year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `period_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `last_reset_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`period`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO `zoecore_bgtop_period_state`
(`period`, `year`, `period_number`, `last_reset_at`)
VALUES
('weekly', YEAR(CURDATE()), WEEK(CURDATE(), 3), UNIX_TIMESTAMP()),
('monthly', YEAR(CURDATE()), MONTH(CURDATE()), UNIX_TIMESTAMP());

-- Evita anunciar BGs antigas ao instalar o módulo.
-- Se quiser importar histórico antigo, limpe zoecore_bgtop_processed e aguarde o módulo processar.
INSERT IGNORE INTO `zoecore_bgtop_processed` (`battleground_id`, `processed_at`)
SELECT `id`, UNIX_TIMESTAMP() FROM `pvpstats_battlegrounds`;
