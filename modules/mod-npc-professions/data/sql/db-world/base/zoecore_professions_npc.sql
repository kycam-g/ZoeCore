-- ZoeCore NPC Professions V1
-- Rode no banco world.
-- NPC principal: 100011
-- Spawn: .npc add 100011

DELETE FROM `creature_template` WHERE `entry` IN (100011, 200038, 200039, 200040, 200041, 200042, 200043, 200044, 200045, 200046, 200047);

INSERT INTO `creature_template` (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`) VALUES
(100011, 0, 0, 0, 0, 0, 'Profissoes', 'ZoeCore', '', 0, 80, 80, 0, 35, 1, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 'npc_professions_zoecore', 12340),
(200038, 0, 0, 0, 0, 0, 'Reagentes Alchemy', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200039, 0, 0, 0, 0, 0, 'Reagentes Blacksmithing', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200040, 0, 0, 0, 0, 0, 'Reagentes Leatherworking', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200041, 0, 0, 0, 0, 0, 'Reagentes Tailoring', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200042, 0, 0, 0, 0, 0, 'Reagentes Engineering', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200043, 0, 0, 0, 0, 0, 'Reagentes Enchanting', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200044, 0, 0, 0, 0, 0, 'Reagentes Jewelcrafting', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200045, 0, 0, 0, 0, 0, 'Reagentes Inscription', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200046, 0, 0, 0, 0, 0, 'Reagentes Cooking', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340),
(200047, 0, 0, 0, 0, 0, 'Reagentes First Aid', 'Vendor Reagentes', '', 0, 80, 80, 0, 35, 128, 1, 1.14286, 1, 1, 20, 0, 0, 1, 2000, 2000, 1, 1, 1, 33536, 2048, 0, 0, 7, 4096, 0, 0, 0, 0, 0, 0, 0, '', 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, '', 12340);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (100011, 200038, 200039, 200040, 200041, 200042, 200043, 200044, 200045, 200046, 200047);

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES
(100011, 0, 19646, 1, 1, 0),
(200038, 0, 19646, 1, 1, 0),
(200039, 0, 19646, 1, 1, 0),
(200040, 0, 19646, 1, 1, 0),
(200041, 0, 19646, 1, 1, 0),
(200042, 0, 19646, 1, 1, 0),
(200043, 0, 19646, 1, 1, 0),
(200044, 0, 19646, 1, 1, 0),
(200045, 0, 19646, 1, 1, 0),
(200046, 0, 19646, 1, 1, 0),
(200047, 0, 19646, 1, 1, 0);
