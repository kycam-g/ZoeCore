-- ZoeCore HardEdit - item_template stat_value INT
-- Execute no banco WORLD/acore_world.
--
-- Necessário para permitir stat_value maior que SMALLINT em itens custom.
-- Mesmo que o core leia int32, o banco também precisa permitir INT.

ALTER TABLE `item_template`
  MODIFY COLUMN `stat_value1` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value2` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value3` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value4` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value5` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value6` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value7` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value8` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value9` INT NOT NULL DEFAULT 0,
  MODIFY COLUMN `stat_value10` INT NOT NULL DEFAULT 0;
