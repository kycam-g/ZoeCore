-- ZoeCore InfoLogin V1
-- Rode no banco characters.

CREATE TABLE IF NOT EXISTS `zoecore_infologin_seen` (
  `guid` int(10) unsigned NOT NULL,
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `first_seen_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_account_id` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_pvp_king_stats` (
  `guid` int(10) unsigned NOT NULL,
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `team` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `score` int(10) unsigned NOT NULL DEFAULT '0',
  `kills` int(10) unsigned NOT NULL DEFAULT '0',
  `deaths` int(10) unsigned NOT NULL DEFAULT '0',
  `streak` int(10) unsigned NOT NULL DEFAULT '0',
  `best_streak` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`),
  KEY `idx_score` (`score`),
  KEY `idx_kills` (`kills`),
  KEY `idx_best_streak` (`best_streak`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_pvp_king` (
  `id` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `player_guid` int(10) unsigned NOT NULL DEFAULT '0',
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `team` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `score` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO `zoecore_pvp_king`
(`id`, `player_guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, `updated_at`)
VALUES
(1, 0, 0, 'Nenhum', 0, 0, 0, 0, 0);


-- ZoeCore PvP King - Ranking semanal
CREATE TABLE IF NOT EXISTS `zoecore_pvp_king_stats_weekly` (
  `guid` int(10) unsigned NOT NULL,
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `team` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `score` int(10) unsigned NOT NULL DEFAULT '0',
  `kills` int(10) unsigned NOT NULL DEFAULT '0',
  `deaths` int(10) unsigned NOT NULL DEFAULT '0',
  `streak` int(10) unsigned NOT NULL DEFAULT '0',
  `best_streak` int(10) unsigned NOT NULL DEFAULT '0',
  `week_year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `week_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`, `week_year`, `week_number`),
  KEY `idx_week_score` (`week_year`, `week_number`, `score`),
  KEY `idx_week_kills` (`week_year`, `week_number`, `kills`),
  KEY `idx_week_best_streak` (`week_year`, `week_number`, `best_streak`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_pvp_king_weekly` (
  `id` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `week_year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `week_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `player_guid` int(10) unsigned NOT NULL DEFAULT '0',
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `team` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `score` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO `zoecore_pvp_king_weekly`
(`id`, `week_year`, `week_number`, `player_guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, `updated_at`)
VALUES
(1, YEAR(CURDATE()), WEEK(CURDATE(), 3), 0, 0, 'Nenhum', 0, 0, 0, 0, 0);

-- ZoeCore PvP King - Ranking mensal
CREATE TABLE IF NOT EXISTS `zoecore_pvp_king_stats_monthly` (
  `guid` int(10) unsigned NOT NULL,
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `team` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `score` int(10) unsigned NOT NULL DEFAULT '0',
  `kills` int(10) unsigned NOT NULL DEFAULT '0',
  `deaths` int(10) unsigned NOT NULL DEFAULT '0',
  `streak` int(10) unsigned NOT NULL DEFAULT '0',
  `best_streak` int(10) unsigned NOT NULL DEFAULT '0',
  `month_year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `month_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`, `month_year`, `month_number`),
  KEY `idx_month_score` (`month_year`, `month_number`, `score`),
  KEY `idx_month_kills` (`month_year`, `month_number`, `kills`),
  KEY `idx_month_best_streak` (`month_year`, `month_number`, `best_streak`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_pvp_king_monthly` (
  `id` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `month_year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `month_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `player_guid` int(10) unsigned NOT NULL DEFAULT '0',
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT 'Nenhum',
  `class` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `race` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `team` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `score` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO `zoecore_pvp_king_monthly`
(`id`, `month_year`, `month_number`, `player_guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, `updated_at`)
VALUES
(1, YEAR(CURDATE()), MONTH(CURDATE()), 0, 0, 'Nenhum', 0, 0, 0, 0, 0);

-- Controle de período para futuro módulo dedicado do Rei do PvP.
CREATE TABLE IF NOT EXISTS `zoecore_pvp_king_period_state` (
  `period` varchar(16) NOT NULL,
  `year` smallint(5) unsigned NOT NULL DEFAULT '0',
  `period_number` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `last_reset_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`period`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO `zoecore_pvp_king_period_state`
(`period`, `year`, `period_number`, `last_reset_at`)
VALUES
('weekly', YEAR(CURDATE()), WEEK(CURDATE(), 3), UNIX_TIMESTAMP()),
('monthly', YEAR(CURDATE()), MONTH(CURDATE()), UNIX_TIMESTAMP());

