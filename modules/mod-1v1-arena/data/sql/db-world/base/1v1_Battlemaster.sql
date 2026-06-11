DELETE FROM `creature_template` WHERE `entry`=999991;
INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES(999991, 0, 0, 0, 0, 0, 'ZoeCore Arena 1v1', '', '', 8218, 70, 70, 2, 35, 1048577, 1.1, 1.14286, 0, 0, 1, 2000, 2000, 1, 768, 2048, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 0, 0, 1, 0, 'npc_1v1arena', 12340);

DELETE FROM `creature_template_model`  WHERE `CreatureID` In (999991);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(999991, 0, 7110, 1, 1, 23766);

DELETE FROM `battlemaster_entry` WHERE `entry`=999991;
INSERT INTO `battlemaster_entry` (`entry`, `bg_template`) VALUES (999991, 6);

-- Command
DELETE FROM `command` WHERE `name` IN ('q1v1', 'q1v1 rated', 'q1v1 unrated', 'q1v1 stats');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('q1v1', 0, 'Sintaxe .q1v1 rated/unrated\nEntrar na Arena 1v1 rated ou unrated'),
('q1v1 rated', 0, 'Sintaxe .q1v1 rated\nEntrar na Arena 1v1 rated'),
('q1v1 unrated', 0, 'Sintaxe .q1v1 unrated\nEntrar na Arena 1v1 unrated'),
('q1v1 stats', 0, 'Sintaxe .q1v1 stats\nMostrar estatisticas da Arena 1v1');

SET @NPC_TEXT_1v1="ZoeCore Arena 1v1.$B$BEntre na fila 1v1 rated ou unrated, crie seu time 1v1 e veja suas estatisticas.$B$BComandos disponiveis:$B.q1v1 rated$B.q1v1 unrated$B.q1v1 stats";
DELETE FROM `npc_text` WHERE `id`=999992;
INSERT INTO `npc_text` (`id`, `text0_0`, `text0_1`, `Probability0`) VALUES
(999992, @NPC_TEXT_1v1, @NPC_TEXT_1v1, 1);
