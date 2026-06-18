-- ZoeCore Lottery V2
-- Rode no banco characters.

CREATE TABLE IF NOT EXISTS `zoecore_lottery_rounds` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `type` varchar(16) NOT NULL,
  `status` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `created_at` int(10) unsigned NOT NULL DEFAULT '0',
  `draw_at` int(10) unsigned NOT NULL DEFAULT '0',
  `drawn_at` int(10) unsigned NOT NULL DEFAULT '0',
  `drawn_numbers` varchar(128) NOT NULL DEFAULT '',
  `winner_count` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_type_status` (`type`, `status`),
  KEY `idx_draw_at` (`draw_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_lottery_tickets` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `round_id` int(10) unsigned NOT NULL,
  `type` varchar(16) NOT NULL,
  `guid` int(10) unsigned NOT NULL,
  `account_id` int(10) unsigned NOT NULL DEFAULT '0',
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `numbers` varchar(128) NOT NULL DEFAULT '',
  `created_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_round_guid` (`round_id`, `guid`),
  KEY `idx_guid` (`guid`),
  KEY `idx_type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_lottery_winners` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `round_id` int(10) unsigned NOT NULL,
  `ticket_id` int(10) unsigned NOT NULL,
  `type` varchar(16) NOT NULL,
  `guid` int(10) unsigned NOT NULL,
  `player_name` varchar(12) NOT NULL DEFAULT '',
  `match_count` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `prize_item` int(10) unsigned NOT NULL DEFAULT '0',
  `prize_count` int(10) unsigned NOT NULL DEFAULT '0',
  `claimed` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `created_at` int(10) unsigned NOT NULL DEFAULT '0',
  `claimed_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_guid_claimed` (`guid`, `claimed`),
  KEY `idx_round` (`round_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_lottery_winner_prizes` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `winner_id` int(10) unsigned NOT NULL,
  `round_id` int(10) unsigned NOT NULL,
  `guid` int(10) unsigned NOT NULL,
  `item_entry` int(10) unsigned NOT NULL,
  `item_count` int(10) unsigned NOT NULL,
  `source` varchar(32) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `idx_winner` (`winner_id`),
  KEY `idx_guid` (`guid`),
  KEY `idx_round` (`round_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS `zoecore_lottery_jackpot` (
  `type` varchar(16) NOT NULL,
  `item_entry` int(10) unsigned NOT NULL DEFAULT '0',
  `current_count` int(10) unsigned NOT NULL DEFAULT '0',
  `start_count` int(10) unsigned NOT NULL DEFAULT '0',
  `updated_at` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
