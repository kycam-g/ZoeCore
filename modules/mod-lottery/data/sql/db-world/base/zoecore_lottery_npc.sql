-- ZoeCore Lottery V5
-- Rode no banco world.
-- NPC principal: 500130
-- Spawn: .npc add 500130
--
-- Correção V5:
-- Usa o schema completo de creature_template compatível com a core atual,
-- igual aos outros módulos ZoeCore.

SET @ENTRY := 500130;

DELETE FROM `creature_template` WHERE `entry` = @ENTRY;

INSERT INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(@ENTRY, 0, 0, 0, 0, 0, 'ZoeCore Loteria', 'Mega-Sena e Numero da Sorte', '', 0, 80, 80, 0, 35, 1, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 'npc_zoecore_lottery', 12340);

DELETE FROM `creature_template_model` WHERE `CreatureID` = @ENTRY;

INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(@ENTRY, 0, 28039, 1, 1, 0);
