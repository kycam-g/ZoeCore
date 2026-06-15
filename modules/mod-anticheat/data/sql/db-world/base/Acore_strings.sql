-- ZoeCore Anticheat V5 - SQL limpo PT-BR
-- Rode no banco world.
-- Corrige erro de sintaxe causado por "-- DELETE" e remove uso de module_string_locale/ptBR.

DELETE FROM `module_string` WHERE `module` = 'anticheat' AND `id` IN (1,2,3,4,5,6);
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('anticheat', 1, '|cffffff00[|cffff0000ALERTA ANTICHEAT|r|cffffff00]:|r |cFFFF8C00[|Hplayer:{}|h{}|h|r|cFFFF8C00] - Latencia: {} ms - Report: {}'),
('anticheat', 2, '|cffffff00[|cffff0000ALERTA ANTICHEAT|r|cffffff00]:|r POSSIVEL TELEPORT HACK DETECTADO|cFFFF8C00 [|Hplayer:{}|h{}|h|r|cFFFF8C00]|r - Latencia: {} ms - Diferenca GPS x: {}, y: {}, z: {}'),
('anticheat', 3, '|cffffff00[|cffff0000ALERTA ANTICHEAT|r|cffffff00]:|r POSSIVEL IGNORE CONTROL HACK DETECTADO|cFFFF8C00 {}|r - Latencia: {} ms'),
('anticheat', 4, '|cffffff00[|cffff0000ALERTA ANTICHEAT|r|cffffff00]:|r TELEPORT HACK USADO DURANTE DUELO|cFFFF8C00 {}|r - Latencia: {} ms vs |cFFFF8C00 {}|r - Latencia: {} ms.'),
('anticheat', 5, '|cffffff00[|cffff0000ALERTA ANTICHEAT|r|cffffff00]:|r EXPLOIT DE TELEPORT NO INICIO DA BG DETECTADO|cFFFF8C00[|Hplayer:{}|h{}|h|r|cFFFF8C00] - Latencia: {} ms'),
('anticheat', 6, '|cffffff00[|cffff0000ALERTA CONTRAMEDIDA|r|cffffff00]:|r |cFFFF8C00{}|r |cFFFF8C00[|Hplayer:{}|h{}|h|r|cFFFF8C00]');

DELETE FROM `command` WHERE `name` IN
('anticheat','anticheat status','anticheat global','anticheat player','anticheat delete','anticheat jail','anticheat parole','anticheat purge','anticheat warn');

INSERT INTO `command` (`name`, `security`, `help`) VALUES
('anticheat', 2, 'Sintaxe: .anticheat\r\n\r\nMostra os comandos do Anticheat disponiveis para sua permissao.'),
('anticheat status', 2, 'Sintaxe: .anticheat status\r\n\r\nMostra o status atual das configs principais do Anticheat ZoeCore.'),
('anticheat global', 2, 'Sintaxe: .anticheat global\r\n\r\nMostra estatisticas globais do Anticheat.'),
('anticheat player', 2, 'Sintaxe: .anticheat player [$nome]\r\n\r\nMostra estatisticas Anticheat da sessao atual do jogador.'),
('anticheat delete', 3, 'Sintaxe: .anticheat delete [$nome]\r\n\r\nRemove estatisticas Anticheat da sessao atual do jogador.'),
('anticheat jail', 2, 'Sintaxe: .anticheat jail [$nome]\r\n\r\nPrende e restringe o jogador suspeito.'),
('anticheat parole', 3, 'Sintaxe: .anticheat parole [$nome]\r\n\r\nRemove restricoes da prisao, limpa reports e envia o jogador para a capital da faccao.'),
('anticheat purge', 3, 'Sintaxe: .anticheat purge\r\n\r\nLimpa a tabela daily_players_reports.'),
('anticheat warn', 2, 'Sintaxe: .anticheat warn [$nome]\r\n\r\nEnvia aviso ao jogador informado que ele foi sinalizado pelo Anticheat.');
