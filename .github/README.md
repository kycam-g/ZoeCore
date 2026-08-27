# ZoeCore

[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.1-4baaaa.svg)](CODE_OF_CONDUCT.md)
[![CodeFactor](https://www.codefactor.io/repository/github/azerothcore/azerothcore-wotlk/badge)](https://www.codefactor.io/repository/github/azerothcore/azerothcore-wotlk)
[![Discord](https://img.shields.io/discord/217589275766685707?logo=discord&logoColor=white)](https://discord.gg/gkt4y2x "Our community hub on Discord")

<p align="center">
  <strong>AzerothCore WotLK 3.3.5a Custom Core</strong><br>
  Base profissional para servidor World of Warcraft Wrath of the Lich King 3.3.5a com foco em módulos, scripts custom, SQL custom, PvP, eventos, bosses e sistemas exclusivos.
</p>


<p align="center">
  <img src="https://img.shields.io/badge/Base-AzerothCore-blue?style=for-the-badge" alt="AzerothCore">
  <img src="https://img.shields.io/badge/WoW-3.3.5a-orange?style=for-the-badge" alt="WoW 3.3.5a">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-informational?style=for-the-badge" alt="C++">
  <img src="https://img.shields.io/badge/Database-MySQL%20%7C%20MariaDB-success?style=for-the-badge" alt="Database">
  <img src="https://img.shields.io/badge/License-GPL--2.0-lightgrey?style=for-the-badge" alt="GPL-2.0">
</p>

---

## 📌 Sobre a ZoeCore

**ZoeCore** é uma core custom baseada em **AzerothCore**, voltada para desenvolvimento de um servidor **World of Warcraft 3.3.5a** com sistemas personalizados, módulos extras, scripts custom, SQL custom, conteúdos PvP/PvE e futuras expansões de gameplay.

O projeto mantém a base oficial do AzerothCore e adiciona uma camada de customização própria, organizada para facilitar manutenção, evolução e criação de novos sistemas sem comprometer a estrutura principal da core.

---

## 🧩 Base oficial: AzerothCore

Este projeto utiliza como base o **AzerothCore**, uma aplicação open-source de servidor MMORPG escrita em C++, criada para recriar a experiência de jogo do World of Warcraft na versão **Wrath of the Lich King 3.3.5a**.

O AzerothCore é conhecido por:

- Alta estabilidade.
- Estrutura modular.
- Grande compatibilidade com conteúdo blizzlike.
- Comunidade ativa.
- Suporte a módulos custom.
- Base sólida para aprendizado, testes e desenvolvimento de servidores privados.

A arquitetura modular do AzerothCore permite adicionar sistemas customizados através da pasta `modules/`, reduzindo a necessidade de alterações diretas no core principal.

---

## 👑 Customização ZoeCore

A customização desta core é mantida por:

### **Mayck G.**

Responsável pelas customizações, organização, expansão e implementação de sistemas exclusivos da **ZoeCore**.

Áreas de customização previstas e/ou implementadas:

- Scripts C++ custom.
- Módulos custom para AzerothCore.
- Scripts Lua através de módulos compatíveis.
- SQL custom para world, characters e auth.
- Bosses custom.
- Quests custom.
- Sistemas PvP.
- Sistemas PvE.
- NPCs custom.
- Zonas custom.
- Eventos automáticos.
- Lojas custom.
- Recompensas por progressão.
- Ajustes hard edit.
- Sistemas de ranking.
- Expansões futuras de gameplay.

---

## ⚙️ Tecnologias utilizadas

| Tecnologia | Uso |
|---|---|
| C++ | Core, scripts e módulos |
| CMake | Sistema de build |
| MySQL/MariaDB | Banco de dados |
| SQL | Dados custom, criaturas, itens, quests, spawns e sistemas |
| Lua | Scripts custom quando suportado por módulo |
| AzerothCore | Base principal do emulador |
| WotLK 3.3.5a | Versão alvo do servidor |

🤖 If you use any AI agent to work on AzerothCore, please read our [AI Agentic Engineering guidelines](https://www.azerothcore.org/wiki/agentic-engineering).
Agent instructions for this repo live in [AGENTS.md](../AGENTS.md), with task-scoped guides in [.agents/docs/](../.agents/docs/); [.agents/README.md](../.agents/README.md) explains how to hook up your agent.

Click on the "⭐ Star" button to help us gain more visibility on GitHub!

---

## 📁 Estrutura principal do projeto

```txt
ZoeCore/
├── apps/                  # Aplicações e utilitários auxiliares
├── bin/                   # Binários/arquivos gerados conforme ambiente
├── conf/                  # Arquivos de configuração
├── data/                  # Dados, SQL base e arquivos auxiliares
├── deps/                  # Dependências
├── doc/                   # Documentação
├── modules/               # Módulos custom e módulos da comunidade
├── sql/
│   └── custom/
│       └── world/         # SQL custom do mundo
├── src/                   # Código-fonte principal da core
├── tools/                 # Ferramentas auxiliares
├── acore.json             # Metadados da core AzerothCore
├── CMakeLists.txt         # Configuração principal de build
├── LICENSE                # Licença GPL-2.0
└── README.md              # Documentação do projeto
```

---

## 📦 Módulos presentes

A pasta `modules/` concentra sistemas extras e customizações modulares. Entre os módulos presentes nesta core estão:

```txt
mod-1v1-arena
mod-ale
mod-anticheat
mod-arena-3v3-solo-queue
mod-arena-replay
mod-auto-learn
mod-bg-reward
mod-breaking-news-override
mod-cfbg
mod-desertion-warnings
mod-duel-reset
mod-guildhouse
mod-killstreak
mod-lottery
mod-npc-professions
mod-npc-spectator
mod-player-tags
mod-server-auto-shutdown
mod-transmog
mod_bgtop
mod_infologin
mod_reidopvp
```

Esses módulos podem ser usados como base para expansão de sistemas custom da ZoeCore, como rankings PvP, recompensas por BG, duel reset, transmog, anti-cheat, espectador de arena, sistema de guild house e outros recursos.

---

## 🗃️ SQL custom

O projeto possui estrutura dedicada para SQL custom:

```txt
sql/custom/world/
```

Arquivo custom identificado:

```txt
zz_zoecore_hardedit_item_template_stat_value_int.sql
```

Essa pasta deve ser usada para organizar alterações próprias da ZoeCore, como:

- Itens custom.
- NPCs custom.
- Bosses custom.
- Quests custom.
- Spawns custom.
- Vendors custom.
- GameObjects custom.
- Ajustes hard edit.
- Tabelas auxiliares para módulos.
- Dados de eventos e recompensas.

Recomendação de organização futura:

```txt
sql/custom/world/zoecore_npcs.sql
sql/custom/world/zoecore_vendors.sql
sql/custom/world/zoecore_bosses.sql
sql/custom/world/zoecore_quests.sql
sql/custom/world/zoecore_events.sql
sql/custom/world/zoecore_items.sql
sql/custom/world/zoecore_teleports.sql
```

---

## 🚀 Instalação e compilação

> A instalação deve seguir a documentação oficial do AzerothCore, respeitando o sistema operacional utilizado.

### 1. Clonar o projeto

```bash
git clone https://github.com/kycam-g/ZoeCore.git
cd ZoeCore
```

### 2. Criar diretório de build

```bash
mkdir build
cd build
```

### 3. Gerar arquivos com CMake

Exemplo Linux:

```bash
cmake ../ -DCMAKE_INSTALL_PREFIX=/opt/zoecore
```

### 4. Compilar

```bash
make -j$(nproc)
make install
```

### 5. Configurar arquivos `.conf`

Copie os arquivos `.conf.dist` necessários e configure de acordo com seu ambiente:

```txt
worldserver.conf
authserver.conf
```

Configure principalmente:

```txt
LoginDatabaseInfo
WorldDatabaseInfo
CharacterDatabaseInfo
DataDir
LogsDir
```

### 6. Importar bancos de dados

Importe os bancos base do AzerothCore e depois aplique os SQLs custom da ZoeCore.

Ordem recomendada:

```txt
1. Auth database
2. Characters database
3. World database
4. SQL custom da ZoeCore
```

---

## 🧠 Padrão recomendado para novos módulos

Para manter o projeto organizado, novos sistemas devem ser criados preferencialmente como módulos dentro de `modules/`.

Exemplo:

```txt
modules/mod_zoecore_hub/
├── CMakeLists.txt
├── conf/
│   └── mod_zoecore_hub.conf.dist
├── src/
│   ├── mod_zoecore_hub.cpp
│   └── loader.cpp
└── data/
    └── sql/
        └── world/
            └── mod_zoecore_hub.sql
```

Sugestões de módulos futuros:

```txt
mod_zoecore_hub
mod_zoecore_currency
mod_zoecore_daily_rewards
mod_zoecore_custom_bosses
mod_zoecore_battlepass
mod_zoecore_events
mod_zoecore_vip
mod_zoecore_pvp_rank
mod_zoecore_quest_system
```

---

## 🎮 Ideias de sistemas custom para ZoeCore

A ZoeCore pode evoluir com sistemas como:

### PvP

- KillStreak avançado.
- Ranking diário/semanal/mensal.
- Rei do PvP.
- Bounty por jogador dominante.
- Recompensa por shutdown.
- Top BG.
- Sistema de arena solo.
- Títulos PvP.
- Auras visuais por rank.

### PvE

- Bosses custom.
- Boss mundial por horário.
- Dungeons custom.
- Eventos de invasão.
- Loot tokenizado.
- Progressão por dificuldade.
- Quests diárias e semanais.

### Economia

- Moeda custom.
- Tokens PvP.
- Tokens PvE.
- Loja VIP.
- Loja de transmog.
- Loja de montarias.
- Loja de cosméticos.
- Sistema de troca de emblemas.

### Qualidade de vida

- NPC hub principal.
- Teleporter.
- Buffer.
- Professions NPC.
- Reset de talentos.
- Informações do servidor.
- Login info.
- Auto learn.
- Duel reset.
- Transmog.

---

## 🧪 Ambiente de desenvolvimento

Recomendações para desenvolvimento:

- Trabalhar em branch separada para cada sistema.
- Evitar alterações diretas no core quando o sistema puder ser feito por módulo.
- Separar SQL custom por funcionalidade.
- Criar `.conf.dist` para todo módulo configurável.
- Documentar comandos, tabelas e entries utilizadas.
- Testar scripts em ambiente local antes de aplicar em produção.
- Fazer backup do banco antes de importar SQL custom.

---

## 🧾 Boas práticas de customização

### Não recomendado

```txt
Editar arquivos principais da core sem necessidade.
Misturar SQL custom com SQL base.
Criar sistemas sem configuração.
Usar entries sem controle.
Aplicar SQL sem backup.
Remover créditos oficiais.
```

### Recomendado

```txt
Criar módulos independentes.
Usar SQL custom separado.
Manter changelog das alterações.
Documentar entries custom.
Usar prefixo ZoeCore em sistemas próprios.
Preservar os créditos oficiais do AzerothCore.
Testar antes de publicar.
```

---

## 🏷️ Padrão sugerido de entries custom

Para evitar conflito com conteúdo original, recomenda-se reservar faixas de entries para customizações.

Exemplo:

| Tipo | Faixa sugerida |
|---|---:|
| NPCs custom | 900000 - 909999 |
| Itens custom | 910000 - 919999 |
| GameObjects custom | 920000 - 929999 |
| Quests custom | 930000 - 939999 |
| Spells/Referências custom | 940000 - 949999 |
| Vendors/Shops | 950000 - 959999 |

> Ajuste essas faixas conforme a organização do banco e o padrão adotado pelo projeto.

---

## 📜 Créditos oficiais preservados

A ZoeCore preserva os créditos oficiais do projeto AzerothCore.

### AzerothCore

- Projeto oficial: [AzerothCore](https://www.azerothcore.org/)
- Repositório oficial: [github.com/azerothcore/azerothcore-wotlk](https://github.com/azerothcore/azerothcore-wotlk)
- Documentação: [www.azerothcore.org/wiki](https://www.azerothcore.org/wiki/)
- Catálogo de módulos: [www.azerothcore.org/catalogue](https://www.azerothcore.org/catalogue.html)
- Discord oficial: [discord.gg/gkt4y2x](https://discord.gg/gkt4y2x)

O AzerothCore nasceu em 2016 baseado em SunwellCore e também possui histórico técnico relacionado a projetos como MaNGOS e TrinityCore. Todos os créditos oficiais pertencem aos seus respectivos autores, mantenedores e contribuidores.

### Projetos e comunidades relacionados

- MaNGOS
- TrinityCore
- SunwellCore
- Comunidade AzerothCore
- Todos os autores e contribuidores listados no arquivo `AUTHORS`

---

## 🛠️ Créditos de customização

### ZoeCore Custom

Customização, organização e expansão:

```txt
Mayck G.
```

Responsável pelas customizações da ZoeCore, incluindo planejamento de sistemas, módulos custom, scripts, SQL custom, bosses, quests, eventos e melhorias gerais de gameplay.

---

## ⚖️ Licença

A base AzerothCore é distribuída sob a licença:

```txt
GNU General Public License v2.0
```

Consulte o arquivo:

```txt
LICENSE
```

Este projeto mantém a licença original da base AzerothCore.

---

## ⚠️ Aviso legal

Este projeto é destinado a fins educacionais, estudo, desenvolvimento, testes e pesquisa sobre emulação de servidores MMORPG.

AzerothCore não é um produto oficial da Blizzard Entertainment e não é afiliado, patrocinado ou endossado pela Blizzard Entertainment ou pela marca World of Warcraft.

World of Warcraft e Blizzard Entertainment são marcas de seus respectivos proprietários.

O uso deste projeto é de responsabilidade de quem o executa, devendo sempre respeitar leis locais, direitos autorais e termos aplicáveis.

---

## 🗺️ Roadmap ZoeCore

Possíveis próximos passos do projeto:

- [ ] Criar `mod_zoecore_hub`.
- [ ] Criar NPC principal ZoeCore.
- [ ] Criar sistema de moeda custom.
- [ ] Criar loja PvP e PvE.
- [ ] Criar bosses custom por dificuldade.
- [ ] Criar quests diárias custom.
- [ ] Criar sistema de ranking visual.
- [ ] Expandir KillStreak.
- [ ] Melhorar Rei do PvP.
- [ ] Criar sistema de eventos automáticos.
- [ ] Criar documentação de entries custom.
- [ ] Criar changelog oficial da ZoeCore.

---

## 📘 Documentação recomendada

Para desenvolvimento, instalação e manutenção da base, utilize sempre a documentação oficial do AzerothCore:

- [AzerothCore Wiki](https://www.azerothcore.org/wiki/)
- [AzerothCore GitHub](https://github.com/azerothcore/azerothcore-wotlk)
- [AzerothCore Module Catalogue](https://www.azerothcore.org/catalogue.html)

---

## 💬 Observação final

A ZoeCore é uma base custom construída sobre o AzerothCore com o objetivo de oferecer uma experiência diferenciada no World of Warcraft 3.3.5a, mantendo organização, modularidade, créditos oficiais e espaço para evolução contínua.

**Base oficial:** AzerothCore  
**Customização:** Mayck G.  
**Projeto custom:** ZoeCore
