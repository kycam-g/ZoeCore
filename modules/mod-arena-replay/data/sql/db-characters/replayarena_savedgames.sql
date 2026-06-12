-- ZoeCore Arena Replay V1 - SAVED REPLAYS
-- Rode no banco characters.

CREATE TABLE IF NOT EXISTS `character_saved_replays` (
  `id` int NOT NULL AUTO_INCREMENT,
  `character_id` int NOT NULL,
  `replay_id` int NOT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE INDEX `unique_saved_replay` (`character_id` ASC, `replay_id` ASC) USING BTREE,
  INDEX `idx_character_id` (`character_id`) USING BTREE,
  INDEX `idx_replay_id` (`replay_id`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_unicode_ci ROW_FORMAT = Dynamic;
