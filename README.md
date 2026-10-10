*Ce projet a été réalisé dans le cadre du cursus 42 par fbenech et abensaid.*

# ft_irc

Un serveur IRC écrit en C++98, compatible avec de vrais clients IRC.

## Description

IRC (Internet Relay Chat) est un protocole de discussion textuelle en temps réel, défini par les RFC 1459 et 2812. Des clients se connectent à un serveur, choisissent un pseudo, rejoignent des channels et échangent des messages publics ou privés.

`ircserv` implémente la partie serveur de ce protocole :

- plusieurs clients connectés en même temps, sans `fork()` ni threads ;
- toutes les entrées/sorties sont non bloquantes et passent par un **unique `poll()`** ;
- authentification par mot de passe, pseudo et nom d'utilisateur ;
- channels publics, messages privés et messages de channel ;
- opérateurs de channel et commandes associées (`KICK`, `INVITE`, `TOPIC`, `MODE`) ;
- reconstruction des commandes reçues en plusieurs morceaux.

Il n'y a ni client IRC, ni communication entre serveurs.

## Instructions

### Compilation

```bash
make        # compile ircserv
make clean  # supprime les fichiers objets
make fclean # supprime aussi l'exécutable
make re     # recompile tout
```

Le projet est compilé avec `c++ -Wall -Wextra -Werror -std=c++98`.

### Lancement

```bash
./ircserv <port> <password>
```

- `port` : le port d'écoute (entre 1024 et 65535, par exemple `6667`)
- `password` : le mot de passe demandé à chaque client

```bash
./ircserv 6667 secret
```

### Connexion avec un client IRC

Avec **irssi** (client de référence) :

```
/connect 127.0.0.1 6667 secret
/join #general
/msg bob salut
```

### Connexion avec netcat

```bash
nc -C 127.0.0.1 6667
```

```
PASS secret
NICK bob
USER bob 0 * :Bob Dupont
JOIN #general
PRIVMSG #general :salut tout le monde
QUIT :à plus
```

L'option `-C` envoie `\r\n` en fin de ligne, comme l'exige le protocole.

## Commandes supportées

| Commande  | Usage                                   | Rôle                                         |
|-----------|-----------------------------------------|----------------------------------------------|
| `PASS`    | `PASS <password>`                       | Donner le mot de passe du serveur            |
| `NICK`    | `NICK <pseudo>`                         | Choisir ou changer de pseudo                 |
| `USER`    | `USER <username> 0 * :<nom réel>`       | Terminer l'enregistrement                    |
| `CAP`     | `CAP LS`                                | Négociation des capacités (aucune proposée)  |
| `PING`    | `PING <jeton>`                          | Maintenir la connexion                       |
| `JOIN`    | `JOIN <#chan>[,<#chan>] [<clé>]`        | Rejoindre ou créer un channel                |
| `PRIVMSG` | `PRIVMSG <pseudo\|#chan> :<texte>`      | Envoyer un message                           |
| `TOPIC`   | `TOPIC <#chan> [:<sujet>]`              | Voir ou changer le sujet d'un channel        |
| `INVITE`  | `INVITE <pseudo> <#chan>`               | Inviter un client dans un channel            |
| `KICK`    | `KICK <#chan> <pseudo> [:<raison>]`     | Expulser un membre (opérateur)               |
| `MODE`    | `MODE <#chan> <+/-mode> [param]`        | Modifier les modes d'un channel (opérateur)  |
| `QUIT`    | `QUIT [:<raison>]`                      | Se déconnecter                               |

Modes de channel :

| Mode | Effet                                                   |
|------|---------------------------------------------------------|
| `i`  | Channel sur invitation uniquement                       |
| `t`  | Seuls les opérateurs peuvent changer le topic           |
| `k`  | Mot de passe requis pour rejoindre le channel           |
| `o`  | Donner ou retirer les droits d'opérateur                |
| `l`  | Limiter le nombre de membres                            |

Le premier client à rejoindre un channel en devient opérateur.

## Fonctionnement

```
client ──TCP──▶ poll() ──▶ recv() ──▶ buffer d'entrée ──▶ ligne complète (\r\n)
                                                                │
                                                                ▼
client ◀──TCP── send() ◀── buffer de sortie ◀── commande ◀── parsing + dispatcher
```

- **Boucle réseau** (`Server`) : un seul `poll()` surveille le socket d'écoute et tous les clients. `recv()` et `send()` ne sont appelés que lorsque `poll()` signale le fd comme prêt.
- **Buffers** (`client`) : chaque client possède un buffer d'entrée, qui accumule les octets jusqu'à obtenir une ligne complète, et un buffer de sortie, qui permet de gérer les envois partiels et les clients lents.
- **Parsing** : chaque ligne est découpée en commande (convertie en majuscules) et paramètres, le dernier paramètre introduit par ` :` pouvant contenir des espaces.
- **Dispatcher** : aiguille chaque commande vers sa fonction, et refuse les commandes tant que le client n'est pas enregistré (`451`).
- **Channels** (`Channel`) : membres, opérateurs, invités, topic et modes ; `broadcast()` diffuse un message à tous les membres.
- **Déconnexion** : qu'elle soit volontaire (`QUIT`) ou brutale, le client est retiré de tous ses channels, les autres membres sont prévenus et les channels vides sont supprimés.

### Organisation des fichiers

```
.
├── Makefile
├── headers/
│   ├── Server.hpp
│   ├── client.hpp
│   ├── Channel.hpp
│   ├── commands.hpp
│   └── parsmessage.hpp
└── src/
    ├── main.cpp         # vérification des arguments, lancement du serveur
    ├── Server.cpp       # socket, boucle poll(), accept / recv / send
    ├── client.cpp       # état d'un client et ses buffers
    ├── Channel.cpp      # gestion d'un channel
    ├── parsmessage.cpp  # parsing des lignes et dispatcher
    ├── answers.cpp      # formatage des réponses numériques
    └── commands.cpp     # implémentation des commandes IRC
```

## Tests

Commande reçue en plusieurs morceaux (test du sujet) :

```bash
nc -C 127.0.0.1 6667
```

Taper `PI`, ctrl+D, `NG `, ctrl+D, puis `xyz` et Entrée : le serveur reconstruit `PING xyz` et répond `:ircserv PONG ircserv :xyz`.

Quelques vérifications utiles :

- mauvais mot de passe → `464` puis déconnexion ;
- pseudo déjà utilisé → `433` ;
- commande avant enregistrement → `451` ;
- client suspendu avec ctrl+Z pendant qu'un autre inonde le channel : le serveur ne se bloque pas, et le client reçoit les messages en attente à la reprise (`fg`) ;
- client tué avec ctrl+C : les autres membres reçoivent un `QUIT` et le serveur continue de fonctionner.

## Répartition du travail

- **abensaid** : couche réseau (socket, boucle `poll()`, connexions et déconnexions), classe `Channel`, `JOIN`, `TOPIC`, `INVITE`.
- **fbenech** : classe `client` et buffers, parsing, dispatcher, réponses numériques, enregistrement (`PASS`, `NICK`, `USER`, `CAP`, `PING`), `PRIVMSG`, `QUIT`.

## Ressources

- [RFC 1459 – Internet Relay Chat Protocol](https://www.rfc-editor.org/rfc/rfc1459)
- [RFC 2812 – Internet Relay Chat: Client Protocol](https://www.rfc-editor.org/rfc/rfc2812)
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/) : documentation à jour des commandes et réponses numériques
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) : sockets et `poll()`
- [irssi](https://irssi.org/) : client de référence

### Utilisation de l'IA

Une IA (Claude) a été utilisée comme outil d'apprentissage : explication des notions réseau (sockets, `poll()`, flux TCP), du protocole IRC et des codes de réponse, aide à la planification des étapes, et relecture du code pour repérer des bugs. Le code a été écrit, testé et vérifié par nous, et chaque partie a été relue entre nous deux.