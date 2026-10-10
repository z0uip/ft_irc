#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tester.py — testeur « boîte noire » pour ft_irc (42)

Il lance ./ircserv, s'y connecte avec de vrais sockets TCP (comme irssi ou nc)
et vérifie les réponses du serveur. Chaque test démarre un serveur neuf sur un
port libre : un crash dans un test n'en fausse pas un autre, et le testeur sait
dire quel test a fait tomber le serveur (SIGPIPE, SIGSEGV, abort...).

Aucune dépendance : Python 3.6+ suffit.

Utilisation (depuis la racine du projet, après `make`) :
    python3 tester.py                  tous les tests sur ./ircserv
    python3 tester.py chemin/ircserv   autre binaire
    python3 tester.py -k mode -k kick  seulement les tests dont le nom contient « mode » ou « kick »
    python3 tester.py -v               affiche tout ce qui est envoyé / reçu
    python3 tester.py --list           liste les tests
    python3 tester.py --valgrind       chaque serveur tourne sous valgrind (lent)
    python3 tester.py --external 6667 --password secret
                                       teste un serveur que tu as lancé toi-même

Pour traquer les bugs mémoire, compile avec AddressSanitizer :
    make re CXXFLAGS="-Wall -Wextra -Werror -std=c++98 -I headers -g -fsanitize=address"
    python3 tester.py
Le testeur lit la sortie du serveur et signale les erreurs ASan (use-after-free...).

Légende :  ✔ OK   ✘ FAIL (bug ou crash)   ⚠ WARN (pas exigé par le sujet, mais
recommandé : comportement attendu par irssi / la RFC)
"""

import argparse
import os
import random
import re
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

HOST = "127.0.0.1"
TIME_FACTOR = 1.0
VERBOSE = False


def T(seconds):
    """Délai mis à l'échelle (--slow, --valgrind)."""
    return seconds * TIME_FACTOR


# ─────────────────────────────── affichage ───────────────────────────────

_COLOR = sys.stdout.isatty() and os.environ.get("NO_COLOR") is None


def _c(code):
    return lambda s: f"\033[{code}m{s}\033[0m" if _COLOR else str(s)


GREEN, RED, YELLOW, CYAN, DIM, BOLD = (_c("32"), _c("31"), _c("33"),
                                       _c("36"), _c("2"), _c("1"))


def shorten(line, n=160):
    return line if len(line) <= n else line[:n] + f"… ({len(line)} car.)"


def indent(text, n):
    pad = " " * n
    return "\n".join(pad + l for l in str(text).splitlines())


# ─────────────────────────────── outils ───────────────────────────────

class TestFail(Exception):
    """Échec d'un test, avec un message lisible."""


def check(cond, msg):
    if not cond:
        raise TestFail(msg)


def esc(s):
    return re.escape(s)


def NUM(code):
    """Regex d'une réponse numérique « :serveur CODE cible ... ». code peut être '411|461'."""
    return rf"^(:\S+ )?({code})(\s|$)"


def FROM(nick):
    """Regex du préfixe d'un message relayé : « :nick!user@host »."""
    return rf"^:{esc(nick)}(!\S*)?\s+"


def names_of(line):
    """Pseudos d'une ligne 353 (RPL_NAMREPLY)."""
    if " :" in line:
        return line.split(" :", 1)[1].split()
    return []


def strip_prefix(name):
    return name.lstrip("@+~&%")


def free_port():
    s = socket.socket()
    s.bind(("", 0))
    port = s.getsockname()[1]
    s.close()
    return port


# ─────────────────────────────── client IRC ───────────────────────────────

class Client:
    """Un client IRC minimal piloté par les tests."""

    def __init__(self, ctx, label, rcvbuf=None):
        self.ctx = ctx
        self.label = label
        self.nick = None
        self.buf = b""
        self.pending = []   # lignes reçues, pas encore consommées par expect()
        self.history = []   # toutes les lignes reçues (pour les messages d'erreur)
        self.closed = False
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if rcvbuf:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.settimeout(T(5))
        try:
            s.connect((HOST, ctx.port))
        except OSError as e:
            s.close()
            raise TestFail(f"{label} : impossible de se connecter au serveur ({e})")
        self.sock = s

    # ── envoi ──
    def send_raw(self, data):
        if isinstance(data, str):
            data = data.encode("utf-8")
        if VERBOSE:
            print(DIM(f"        {self.label} → {shorten(data.decode('utf-8', 'replace').rstrip())!r}"))
        try:
            self.sock.sendall(data)
        except socket.timeout:
            raise TestFail(f"{self.label} : envoi bloqué, le serveur ne lit plus cette connexion")
        except OSError as e:
            raise TestFail(f"{self.label} : envoi impossible ({e.strerror or e}), "
                           f"la connexion a été fermée par le serveur ?")

    def send(self, line):
        self.send_raw(line + "\r\n")

    # ── réception ──
    def _fill(self, timeout):
        if self.closed:
            return
        try:
            r, _, _ = select.select([self.sock], [], [], max(0.0, timeout))
        except (OSError, ValueError):
            self.closed = True
            return
        if not r:
            return
        try:
            data = self.sock.recv(65536)
        except socket.timeout:
            return
        except OSError:
            data = b""
        if not data:
            self.closed = True
            return
        self.buf += data
        while b"\n" in self.buf:
            raw, self.buf = self.buf.split(b"\n", 1)
            line = raw.rstrip(b"\r").decode("utf-8", "replace")
            if VERBOSE:
                print(DIM(f"        {self.label} ← {shorten(line)}"))
            self.pending.append(line)
            self.history.append(line)

    def _report(self, head):
        msg = f"{self.label} : {head}"
        recent = self.history[-6:]
        if recent:
            msg += "\n  derniers messages reçus par " + self.label + " :\n"
            msg += "\n".join("    " + shorten(l) for l in recent)
        else:
            msg += "\n  (" + self.label + " n'a rien reçu)"
        if self.closed:
            msg += "\n  (la connexion a été fermée par le serveur)"
        return msg

    def expect(self, pattern, timeout=2.0, what=None):
        """Attend une ligne qui correspond à la regex, la consomme et la renvoie."""
        rx = re.compile(pattern)
        deadline = time.time() + T(timeout)
        while True:
            for i, line in enumerate(self.pending):
                if rx.search(line):
                    del self.pending[i]
                    return line
            left = deadline - time.time()
            if left <= 0 or self.closed:
                break
            self._fill(min(left, 0.1))
        raise TestFail(self._report(f"attendu {what or pattern}"))

    def expect_none(self, pattern, wait=0.4, what=None):
        """Vérifie qu'aucune ligne correspondant à la regex n'arrive pendant `wait` secondes."""
        rx = re.compile(pattern)
        deadline = time.time() + T(wait)
        while not self.closed:
            left = deadline - time.time()
            if left <= 0:
                break
            self._fill(min(left, 0.1))
        for line in self.pending:
            if rx.search(line):
                raise TestFail(f"{self.label} : n'aurait pas dû recevoir {what or pattern}\n"
                               f"  reçu : {shorten(line)}")

    def drain(self, wait=0.2):
        deadline = time.time() + T(wait)
        while not self.closed and time.time() < deadline:
            self._fill(deadline - time.time())
        self.pending.clear()

    def wait_closed(self, timeout=2.0):
        deadline = time.time() + T(timeout)
        while not self.closed and time.time() < deadline:
            self._fill(min(deadline - time.time(), 0.1))
        return self.closed

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass
        self.closed = True

    def reset(self):
        """Fermeture brutale (RST), comme un client tué ou un câble arraché."""
        try:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        except OSError:
            pass
        self.close()

    # ── raccourcis IRC ──
    def register(self, nick, user=None, password=None):
        pw = self.ctx.password if password is None else password
        self.send(f"PASS {pw}")
        self.send(f"NICK {nick}")
        self.send(f"USER {user or nick} 0 * :Real {nick}")
        self.expect(NUM("001"), what=f"001 RPL_WELCOME (enregistrement de {nick})")
        self.nick = nick
        return self

    def join(self, chan, key=None):
        """JOIN complet : confirmation, 353, 366. Renvoie la liste des pseudos."""
        self.send(f"JOIN {chan}" + (f" {key}" if key else ""))
        self.expect(FROM(self.nick) + rf"JOIN :?{esc(chan)}\s*$",
                    what=f"la confirmation « :{self.nick}!… JOIN {chan} »")
        line = self.expect(NUM("353") + rf".*{esc(chan)}", what=f"353 RPL_NAMREPLY pour {chan}")
        self.expect(NUM("366"), what="366 RPL_ENDOFNAMES")
        return names_of(line)


class Ctx:
    """Contexte d'un test : port, mot de passe, pseudos/channels uniques."""

    def __init__(self, port, password, uid):
        self.port = port
        self.password = password
        self.uid = uid
        self.clients = []

    def nick(self, base):
        return f"{base}{self.uid}"

    def chan(self, base="c"):
        return f"#{base}{self.uid}"

    def client(self, label=None, **kw):
        c = Client(self, label or f"client{len(self.clients) + 1}", **kw)
        self.clients.append(c)
        return c

    def user(self, base):
        c = self.client(base)
        c.register(self.nick(base))
        return c

    def users(self, *bases):
        return [self.user(b) for b in bases]

    def channel(self, *bases, chan=None):
        """Crée des clients enregistrés qui rejoignent tous le même channel (le 1er est opérateur)."""
        ch = chan or self.chan()
        us = self.users(*bases)
        for u in us:
            u.join(ch)
        for u in us:
            u.drain(0.15)
        return (ch, *us)

    def cleanup(self):
        for c in self.clients:
            c.close()


# ─────────────────────────────── serveur ───────────────────────────────

MEMORY_PATTERNS = [
    r"ERROR: AddressSanitizer: [^\n]*",
    r"ERROR: LeakSanitizer: [^\n]*",
    r"SUMMARY: AddressSanitizer: [^\n]*",
    r"runtime error: [^\n]*",
    r"==\d+== Invalid (?:read|write|free)[^\n]*",
    r"==\d+== Conditional jump[^\n]*",
    r"==\d+== Use of uninitialised[^\n]*",
    r"==\d+==\s+definitely lost: [1-9][\d,]* bytes[^\n]*",
    r"free\(\): [^\n]*",
    r"double free[^\n]*",
    r"malloc\(\): [^\n]*",
    r"terminate called [^\n]*",
]

SIG_HINTS = {
    "SIGPIPE": None,  # rempli plus bas avec HINT_SIGPIPE
    "SIGSEGV": "accès mémoire invalide : souvent un client* resté dans un Channel alors que le "
               "client a été supprimé de la map.",
    "SIGABRT": "abort() : exception non attrapée (« terminate called… ») ou tas corrompu (double free).",
}


def describe_rc(rc):
    if rc is None:
        return "toujours en vie", None
    if rc < 0:
        try:
            name = signal.Signals(-rc).name
        except ValueError:
            name = f"signal {-rc}"
        return f"tué par {name}", SIG_HINTS.get(name)
    return f"code de sortie {rc}", None


class ServerProc:
    def __init__(self, binary, port, password, logdir, valgrind=False, args=None):
        self.port = port
        fd, self.log_path = tempfile.mkstemp(prefix=f"ircserv_{port}_", suffix=".log", dir=logdir)
        self.logf = os.fdopen(fd, "wb")
        cmd = [binary] + (args if args is not None else [str(port), password])
        if valgrind:
            cmd = ["valgrind", "--leak-check=full", "--errors-for-leak-kinds=definite"] + cmd
        env = dict(os.environ)
        env.setdefault("ASAN_OPTIONS", "detect_leaks=1")
        self.proc = subprocess.Popen(cmd, stdout=self.logf, stderr=subprocess.STDOUT,
                                     stdin=subprocess.DEVNULL, env=env)

    def wait_ready(self):
        deadline = time.time() + T(5)
        while time.time() < deadline:
            if self.proc.poll() is not None:
                return False
            try:
                s = socket.create_connection((HOST, self.port), timeout=0.2)
                s.close()
                return True
            except OSError:
                time.sleep(0.03)
        return False

    def stop(self):
        """SIGINT puis attente ; SIGKILL en dernier recours. Renvoie le code de sortie."""
        if self.proc.poll() is None:
            self.proc.send_signal(signal.SIGINT)
            try:
                self.proc.wait(timeout=T(3))
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        if not self.logf.closed:
            self.logf.close()
        return self.proc.returncode

    def log(self):
        try:
            with open(self.log_path, "rb") as f:
                return f.read().decode("utf-8", "replace")
        except OSError:
            return ""

    def findings(self):
        """Erreurs mémoire repérées dans la sortie (ASan, valgrind, glibc), avec les lignes du projet en cause."""
        text = self.log()
        out = []
        for p in MEMORY_PATTERNS:
            for m in re.finditer(p, text):
                s = re.sub(r" at pc 0x\S+ bp 0x\S+ sp 0x\S+", "", m.group(0).strip())
                if "libsanitizer" in s or s in out:
                    continue
                out.append(s)
        # piles d'appels ASan : on ne garde que les frames du projet (accès fautif, puis libération)
        for block in re.split(r"\n\s*\n", text):
            head = block.strip().splitlines()[0] if block.strip() else ""
            frames = re.findall(r"#\d+ 0x[0-9a-f]+ in (.+?) (\S+\.(?:cpp|hpp):\d+)", block)
            mine = [f"{fn.split('(')[0]}  ({os.path.basename(loc)})" for fn, loc in frames
                    if "libsanitizer" not in loc and "/usr/" not in loc][:3]
            if not mine:
                continue
            if "freed by thread" in block:
                out.append("libéré par : " + "  ←  ".join(mine))
            elif "previously allocated" in block:
                continue
            elif re.search(r"ERROR: AddressSanitizer|READ of|WRITE of", block):
                out.append("accès dans : " + "  ←  ".join(mine))
        # valgrind : un bloc par erreur, avec la pile du projet et l'endroit où la mémoire a été libérée
        seen = set()
        for blk in re.findall(r"==\d+== (Invalid (?:read|write|free)[^\n]*\n(?:==\d+== +\S[^\n]*\n)+)", text):
            head = blk.splitlines()[0]
            parts = re.split(r"free'd", blk, maxsplit=1)

            def frames(chunk):
                res = []
                for fn, inside in re.findall(r"(?:at|by) 0x[0-9A-F]+: (.+?) \(([^()]*)\)\s*$", chunk, re.M):
                    name = fn.split("(")[0].strip()
                    if name.startswith(("std::", "__gnu", "operator ")) or ".so" in inside or "valgrind" in inside:
                        continue
                    res.append(name + (f"  ({inside})" if re.match(r"\w+\.(cpp|hpp):\d+$", inside) else ""))
                return res[:3]
            acc = frames(parts[0])
            key = tuple(acc)
            if key in seen:
                continue
            seen.add(key)
            line = head.strip()
            if acc:
                line += "\n  accès dans : " + "  ←  ".join(acc)
            if len(parts) > 1 and frames(parts[1]):
                line += "\n  libéré par : " + "  ←  ".join(frames(parts[1]))
            out = [o for o in out if not o.startswith("==") or "Invalid" not in o]
            out.append(line)
        return out[:8]

    def output_before_crash(self, n=5):
        text = self.log()
        cut = re.search(r"^(={20,}|==\d+==)", text, re.M)
        if cut:
            text = text[:cut.start()]
        lines = [l for l in text.splitlines() if l.strip() and "Waiting for connection" not in l]
        return "\n".join(lines[-n:])

    def tail(self, n=10):
        out = []
        for l in self.log().splitlines():
            if not l.strip() or "Waiting for connection" in l:
                continue
            if out and out[-1][0] == l:
                out[-1][1] += 1
            else:
                out.append([l, 1])
        return "\n".join(l if k == 1 else f"{l}   (×{k})" for l, k in out[-n:])


def probe(port, password):
    """Le serveur répond-il encore ? (connexion + enregistrement + PING)"""
    tok = f"alive{random.randint(1000, 9999)}"
    nick = f"pr{random.randint(10000, 99999)}"
    try:
        s = socket.create_connection((HOST, port), timeout=T(2))
    except OSError:
        return False
    try:
        s.sendall(f"PASS {password}\r\nNICK {nick}\r\nUSER {nick} 0 * :p\r\nPING {tok}\r\n".encode())
        data = b""
        deadline = time.time() + T(2)
        while time.time() < deadline:
            r, _, _ = select.select([s], [], [], 0.1)
            if not r:
                continue
            chunk = s.recv(4096)
            if not chunk:
                return False
            data += chunk
            if tok.encode() in data:
                return True
        return False
    except OSError:
        return False
    finally:
        s.close()


# ─────────────────────────────── registre des tests ───────────────────────────────

TESTS = []


def test(cat, name, sev="fail", hint=None, standalone=False):
    def deco(fn):
        TESTS.append(dict(cat=cat, name=name, sev=sev, hint=hint, fn=fn, standalone=standalone))
        return fn
    return deco


# Pistes de correction (affichées seulement en cas d'échec)
HINT_LEAVE = ("_has_leaved passe à true (QUIT, mauvais mot de passe) mais le Server ne le lit jamais. "
              "Après le dispatcher, dans handleClientData, si le client a quitté : le retirer de ses "
              "channels, close(fd), l'enlever de _clients et _pollFds, et ne plus toucher à it->second.")
HINT_COPY = ("Server::getChannelMap() renvoie une COPIE de la map : handleQuit() retire le client des "
             "copies, pas des vrais channels, qui gardent un pointeur vers lui. Renvoie une référence "
             "(std::map<std::string, Channel> &) ou fais ce nettoyage dans Server.")
HINT_ABRUPT = ("Quand recv() renvoie 0 (ou -1), le client est effacé de _clients mais son pointeur reste "
               "dans _clients/_operators des Channel → mémoire libérée au prochain broadcast. Fais une "
               "seule fonction de déconnexion (QUIT aux membres, retrait des channels, suppression des "
               "channels vides, close, erase) appelée pour QUIT et pour recv() <= 0.")
HINT_EMPTY = ("handlePart() et handleKick() ne suppriment pas le channel quand il devient vide : il reste "
              "sans opérateur, et le prochain arrivant passe par la branche « channel existant » sans "
              "devenir opérateur. Appelle serv.removeChannel() quand getClientCount() == 0.")
HINT_SIGPIPE = ("send() vers un client qui a déjà coupé la connexion envoie SIGPIPE, qui tue le process par "
                "défaut : utilise send(fd, ..., MSG_NOSIGNAL) (ou signal(SIGPIPE, SIG_IGN)). Et quand recv() "
                "renvoie -1, déconnecte aussi le client : aujourd'hui seul « Recv error » est affiché et il "
                "reste dans la boucle.")
SIG_HINTS["SIGPIPE"] = HINT_SIGPIPE
HINT_EXCEPTION = ("Server::start() lève une exception que main() n'attrape pas → std::terminate → abort "
                  "(core dumped). Entoure serv.start() / serv.run() d'un try/catch qui affiche e.what() "
                  "et renvoie 1.")
HINT_REUSE = ("Ajoute setsockopt(_servFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) avant bind() : sinon "
              "après un arrêt avec des clients connectés, le port reste bloqué ~1 min (TIME_WAIT).")
HINT_NICK = ("handleNick() change le pseudo sans rien envoyer : renvoie « :ancien!user@ip NICK nouveau » au "
             "client et aux membres de ses channels. irssi ne met à jour le pseudo affiché qu'à "
             "réception de ce message.")
HINT_LF = ("extract_line() ne découpe que sur \"\\r\\n\". Découpe sur \"\\n\" et retire le \"\\r\" final "
           "s'il existe : nc sans -C marchera aussi.")


# ═════════════════════════════════ LANCEMENT ═════════════════════════════════

@test("Lancement", "arguments invalides → erreur et code de sortie ≠ 0", standalone=True)
def _(opts):
    cases = [[], ["6667"], ["6667", "pw", "en_trop"], ["abc", "pw"], ["66a7", "pw"],
             ["0", "pw"], ["-1", "pw"], ["70000", "pw"], ["", "pw"]]
    for args in cases:
        p = subprocess.Popen([opts.binary] + args, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)
        try:
            rc = p.wait(timeout=T(1.5))
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            raise TestFail(f"./ircserv {' '.join(repr(a) for a in args)} : le serveur a démarré "
                           f"au lieu de refuser ces arguments")
        desc, h = describe_rc(rc)
        check(rc >= 0, f"./ircserv {' '.join(repr(a) for a in args)} : crash ({desc})")
        check(rc != 0, f"./ircserv {' '.join(repr(a) for a in args)} : refusé mais code de sortie 0")


@test("Lancement", "port déjà utilisé → message d'erreur, sans crash", hint=HINT_EXCEPTION, standalone=True)
def _(opts):
    blocker = socket.socket()
    blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    blocker.bind(("", 0))
    blocker.listen(1)
    port = blocker.getsockname()[1]
    try:
        srv = ServerProc(opts.binary, port, opts.password, opts.logdir)
        try:
            rc = srv.proc.wait(timeout=T(3))
        except subprocess.TimeoutExpired:
            srv.stop()
            raise TestFail(f"le serveur tourne alors que le port {port} est déjà pris ?")
        srv.stop()
        desc, h = describe_rc(rc)
        out = srv.tail(4)
        check(rc >= 0, f"bind() échoue et le serveur crashe ({desc})\n  sortie :\n{indent(out, 4)}")
        check("terminate called" not in srv.log(),
              f"exception non attrapée (std::terminate)\n  sortie :\n{indent(out, 4)}")
    finally:
        blocker.close()


@test("Lancement", "ctrl+C (SIGINT) avec des clients connectés → arrêt propre", standalone=True)
def _(opts):
    port = free_port()
    srv = ServerProc(opts.binary, port, opts.password, opts.logdir, opts.valgrind)
    check(srv.wait_ready(), "le serveur n'a pas démarré")
    ctx = Ctx(port, opts.password, opts.tag + "s")
    try:
        ctx.channel("ali", "bob")
        srv.proc.send_signal(signal.SIGINT)
        try:
            rc = srv.proc.wait(timeout=T(3))
        except subprocess.TimeoutExpired:
            srv.stop()
            raise TestFail("le serveur ne s'arrête pas après SIGINT (ctrl+C)")
        desc, h = describe_rc(rc)
        check(rc >= 0, f"crash à l'arrêt ({desc})")
        srv.stop()
        f = srv.findings()
        check(not f, "erreurs mémoire à l'arrêt :\n" + indent("\n".join(f), 2))
    finally:
        ctx.cleanup()
        srv.stop()


@test("Lancement", "relance immédiate sur le même port après un arrêt", sev="warn",
      hint=HINT_REUSE, standalone=True)
def _(opts):
    port = free_port()
    srv = ServerProc(opts.binary, port, opts.password, opts.logdir)
    check(srv.wait_ready(), "le serveur n'a pas démarré")
    ctx = Ctx(port, opts.password, opts.tag + "r")
    try:
        ctx.channel("ali")
        srv.stop()
    finally:
        ctx.cleanup()
    time.sleep(0.2)
    srv2 = ServerProc(opts.binary, port, opts.password, opts.logdir)
    ok = srv2.wait_ready()
    srv2.stop()
    check(ok, f"impossible de relancer ./ircserv {port} juste après l'avoir arrêté "
              f"(bind() échoue tant que le port est en TIME_WAIT)\n  sortie :\n{indent(srv2.tail(3), 4)}")


# ═════════════════════════════════ ENREGISTREMENT ═════════════════════════════════

@test("Enregistrement", "PASS + NICK + USER → 001 RPL_WELCOME")
def _(ctx):
    c = ctx.client("ali")
    nick = ctx.nick("ali")
    c.send(f"PASS {ctx.password}")
    c.send(f"NICK {nick}")
    c.send(f"USER {nick} 0 * :Ali Baba")
    line = c.expect(NUM("001"), what="001 RPL_WELCOME")
    check(re.search(rf"^(:\S+ )?001 {esc(nick)}\b", line),
          f"le 001 doit être adressé au pseudo ({nick}) : {line}")


@test("Enregistrement", "négociation CAP LS / CAP END (ce que fait irssi)")
def _(ctx):
    c = ctx.client("ali")
    nick = ctx.nick("ali")
    c.send("CAP LS 302")
    c.send(f"PASS {ctx.password}")
    c.send(f"NICK {nick}")
    c.send(f"USER {nick} 0 * :Ali")
    c.expect(r"CAP \S+ LS", what="la réponse à CAP LS")
    c.send("CAP END")
    c.expect(NUM("001"), what="001 après CAP END")


@test("Enregistrement", "mauvais mot de passe → 464 ERR_PASSWDMISMATCH")
def _(ctx):
    c = ctx.client("ali")
    c.send("PASS mauvais_mdp")
    c.send(f"NICK {ctx.nick('ali')}")
    c.send(f"USER {ctx.nick('ali')} 0 * :Ali")
    c.expect(NUM("464"), what="464 (mauvais mot de passe)")


@test("Enregistrement", "mauvais mot de passe → jamais enregistré")
def _(ctx):
    c = ctx.client("ali")
    c.send("PASS mauvais_mdp")
    c.send(f"NICK {ctx.nick('ali')}")
    c.send(f"USER {ctx.nick('ali')} 0 * :Ali")
    c.expect_none(NUM("001"), what="001 (enregistré malgré un mauvais mot de passe !)")
    if not c.closed:
        c.send(f"JOIN {ctx.chan()}")
        c.expect_none(FROM(ctx.nick("ali")) + "JOIN", what="un JOIN accepté sans mot de passe valide")


@test("Enregistrement", "mauvais mot de passe → le serveur ferme la connexion", sev="warn", hint=HINT_LEAVE)
def _(ctx):
    c = ctx.client("ali")
    c.send("PASS mauvais_mdp")
    c.send(f"NICK {ctx.nick('ali')}")
    c.send(f"USER {ctx.nick('ali')} 0 * :Ali")
    c.expect(NUM("464"), what="464")
    check(c.wait_closed(2), "après le 464, la connexion reste ouverte (le README annonce "
                            "« 464 puis déconnexion »)")


@test("Enregistrement", "sans PASS → pas d'enregistrement")
def _(ctx):
    c = ctx.client("ali")
    c.send(f"NICK {ctx.nick('ali')}")
    c.send(f"USER {ctx.nick('ali')} 0 * :Ali")
    c.expect_none(NUM("001"), what="001 sans avoir donné de mot de passe")
    if not c.closed:
        c.send(f"JOIN {ctx.chan()}")
        c.expect_none(FROM(ctx.nick("ali")) + "JOIN", what="un JOIN accepté sans mot de passe")


@test("Enregistrement", "PASS sans paramètre → 461 ; NICK sans paramètre → 431")
def _(ctx):
    c = ctx.client("ali")
    c.send("PASS")
    c.expect(NUM("461"), what="461 ERR_NEEDMOREPARAMS pour « PASS »")
    c.send(f"PASS {ctx.password}")
    c.send("NICK")
    c.expect(NUM("431|461"), what="431 ERR_NONICKNAMEGIVEN pour « NICK »")


@test("Enregistrement", "pseudos invalides → 432 ERR_ERRONEUSNICKNAME")
def _(ctx):
    c = ctx.client("ali")
    c.send(f"PASS {ctx.password}")
    for bad in ["1abc", "-abc", "ab@c", "ab!c", "#abc", "ab:c", "ab,c", "ab*c"]:
        c.send(f"NICK {bad}")
        c.expect(NUM("432"), what=f"432 pour le pseudo invalide « {bad} »")
    nick = ctx.nick("ali")
    c.send(f"NICK {nick}")
    c.send(f"USER {nick} 0 * :Ali")
    c.expect(NUM("001"), what="001 avec un pseudo valide après plusieurs refus")


@test("Enregistrement", "pseudo déjà utilisé → 433 ERR_NICKNAMEINUSE")
def _(ctx):
    a = ctx.user("ali")
    b = ctx.client("bob")
    b.send(f"PASS {ctx.password}")
    b.send(f"NICK {a.nick}")
    b.expect(NUM("433"), what=f"433 (pseudo {a.nick} déjà pris)")
    b.send(f"NICK {ctx.nick('bob')}")
    b.send(f"USER {ctx.nick('bob')} 0 * :Bob")
    b.expect(NUM("001"), what="001 avec un autre pseudo")


@test("Enregistrement", "USER avec trop peu de paramètres → 461")
def _(ctx):
    c = ctx.client("ali")
    c.send(f"PASS {ctx.password}")
    c.send(f"NICK {ctx.nick('ali')}")
    c.send("USER")
    c.expect(NUM("461"), what="461 pour « USER »")
    c.send("USER ali 0 *")
    c.expect(NUM("461"), what="461 pour « USER ali 0 * » (realname manquant)")
    c.expect_none(NUM("001"), wait=0.2, what="001 avec un USER incomplet")


@test("Enregistrement", "PASS / USER après l'enregistrement → 462 ERR_ALREADYREGISTERED")
def _(ctx):
    a = ctx.user("ali")
    a.send(f"PASS {ctx.password}")
    a.expect(NUM("462"), what="462 pour un 2e PASS")
    a.send("USER autre 0 * :Autre")
    a.expect(NUM("462"), what="462 pour un 2e USER")


@test("Enregistrement", "commandes avant l'enregistrement → 451 ERR_NOTREGISTERED")
def _(ctx):
    c = ctx.client("ali")
    c.send(f"JOIN {ctx.chan()}")
    c.expect(NUM("451"), what="451 pour JOIN avant enregistrement")
    c.send(f"PASS {ctx.password}")
    c.send(f"NICK {ctx.nick('ali')}")
    c.send("PRIVMSG quelquun :coucou")
    c.expect(NUM("451"), what="451 pour PRIVMSG (USER pas encore envoyé)")


@test("Enregistrement", "commande inconnue → 421 ERR_UNKNOWNCOMMAND")
def _(ctx):
    a = ctx.user("ali")
    a.send("FOOBAR un deux")
    line = a.expect(NUM("421"), what="421 pour FOOBAR")
    check("FOOBAR" in line, f"le 421 devrait citer la commande : {line}")


@test("Enregistrement", "commandes en minuscules acceptées (join, privmsg…)", sev="warn")
def _(ctx):
    a = ctx.user("ali")
    ch = ctx.chan()
    a.send(f"join {ch}")
    a.expect(FROM(a.nick) + rf"JOIN :?{esc(ch)}", what="JOIN accepté en minuscules")


@test("Enregistrement", "PING → PONG avec le même jeton")
def _(ctx):
    a = ctx.user("ali")
    a.send("PING jeton123")
    a.expect(r"PONG .*jeton123", what="PONG … jeton123")
    a.send("PING :avec des espaces")
    a.expect(r"PONG .*avec des espaces", what="PONG … :avec des espaces")


@test("Enregistrement", "PING sans paramètre → 409 ou 461, sans crash")
def _(ctx):
    a = ctx.user("ali")
    a.send("PING")
    a.expect(NUM("409|461"), what="409 ERR_NOORIGIN (ou 461)")


@test("Enregistrement", "changer de pseudo après l'enregistrement")
def _(ctx):
    a, b = ctx.users("ali", "bob")
    old, new = a.nick, ctx.nick("neo")
    a.send(f"NICK {new}")
    time.sleep(T(0.2))
    b.send(f"PRIVMSG {new} :coucou")
    a.expect(rf"PRIVMSG {esc(new)} :coucou$", what=f"le message envoyé au nouveau pseudo {new}")
    b.send(f"PRIVMSG {old} :ancien")
    b.expect(NUM("401"), what=f"401 : l'ancien pseudo {old} ne doit plus exister")


@test("Enregistrement", "changement de pseudo annoncé : « :ancien!user@host NICK nouveau »",
      sev="warn", hint=HINT_NICK)
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    old, new = a.nick, ctx.nick("neo")
    a.send(f"NICK {new}")
    a.expect(FROM(old) + rf"NICK :?{esc(new)}$", what="la confirmation du changement de pseudo")
    b.expect(FROM(old) + rf"NICK :?{esc(new)}$", what=f"l'annonce du changement de pseudo de {old}")


@test("Enregistrement", "pseudo libéré quand son propriétaire se déconnecte")
def _(ctx):
    a = ctx.user("ali")
    nick = a.nick
    a.close()
    time.sleep(T(0.3))
    b = ctx.client("bob")
    b.register(nick)


# ═════════════════════════════════ RÉSEAU / PARSING ═════════════════════════════════

@test("Réseau", "commande en morceaux (test du sujet : « PI » + « NG » + « xyz\\r\\n »)")
def _(ctx):
    a = ctx.user("ali")
    a.send_raw("PI")
    time.sleep(T(0.25))
    a.send_raw("NG ")
    time.sleep(T(0.25))
    a.send_raw("xyz\r\n")
    a.expect(r"PONG .*xyz", what="PONG xyz (commande reconstruite)")


@test("Réseau", "\\r et \\n reçus dans deux paquets différents")
def _(ctx):
    a = ctx.user("ali")
    a.send_raw("PING coupe\r")
    time.sleep(T(0.25))
    a.send_raw("\n")
    a.expect(r"PONG .*coupe", what="PONG coupe")


@test("Réseau", "enregistrement octet par octet")
def _(ctx):
    c = ctx.client("ali")
    nick = ctx.nick("ali")
    data = f"PASS {ctx.password}\r\nNICK {nick}\r\nUSER {nick} 0 * :Lent\r\n"
    for ch in data:
        c.send_raw(ch)
        time.sleep(0.004)
    c.expect(NUM("001"), what="001 après un envoi octet par octet")


@test("Réseau", "plusieurs commandes dans un seul paquet, traitées dans l'ordre")
def _(ctx):
    a = ctx.user("ali")
    a.send_raw("PING un\r\nPING deux\r\nPING trois\r\nPING quatre\r\n")
    for tok in ("un", "deux", "trois", "quatre"):
        a.expect(rf"PONG .*\b{tok}$", what=f"PONG {tok}")
    order = [l.rsplit(":", 1)[-1] for l in a.history if " PONG " in f" {l}"]
    check(order[-4:] == ["un", "deux", "trois", "quatre"], f"ordre des PONG incorrect : {order}")


@test("Réseau", "lignes vides et espaces ignorés sans crash")
def _(ctx):
    a = ctx.user("ali")
    a.send_raw("\r\n\r\n   \r\n\t\r\n:\r\n: \r\n")
    a.send("PING encore")
    a.expect(r"PONG .*encore", what="PONG après des lignes vides")


@test("Réseau", "préfixe en début de ligne (« :pseudo PING x »)")
def _(ctx):
    a = ctx.user("ali")
    a.send(f":{a.nick} PING prefixe")
    a.expect(r"PONG .*prefixe", what="PONG prefixe")


@test("Réseau", "ligne très longue (20 000 caractères) sans crash")
def _(ctx):
    a = ctx.user("ali")
    a.send("PRIVMSG personne :" + "A" * 20000)
    a.send("PING apres")
    a.expect(r"PONG .*apres", timeout=4, what="PONG après la ligne géante")


@test("Réseau", "fin de ligne en « \\n » seul (nc sans -C)", sev="warn", hint=HINT_LF)
def _(ctx):
    a = ctx.user("ali")
    a.send_raw("PING lf\n")
    a.expect(r"PONG .*lf", timeout=1, what="PONG pour une ligne terminée par \\n seul")


@test("Réseau", "50 clients connectés en même temps")
def _(ctx):
    cs = [ctx.client(f"u{i}") for i in range(50)]
    for i, c in enumerate(cs):
        n = ctx.nick(f"u{i}")
        c.send_raw(f"PASS {ctx.password}\r\nNICK {n}\r\nUSER {n} 0 * :U\r\n")
        c.nick = n
    for c in cs:
        c.expect(NUM("001"), what="001")
    for i, c in enumerate(cs):
        c.send(f"PING p{i}")
    for i, c in enumerate(cs):
        c.expect(rf"PONG .*p{i}$", what=f"PONG p{i}")


@test("Réseau", "client qui ne lit plus (ctrl+Z) : le serveur ne bloque pas et ne perd rien")
def _(ctx):
    ch, s, f = ctx.channel("sleep", "flood")
    n, payload = 3000, "x" * 200
    for start in range(0, n, 250):
        f.send_raw("".join(f"PRIVMSG {ch} :{i:05d} {payload}\r\n" for i in range(start, min(n, start + 250))))
    f.send("PING pendant")
    f.expect(r"PONG .*pendant", timeout=5,
             what="PONG pendant que l'autre client ne lit pas (le serveur est-il bloqué ?)")
    # « fg » : le client endormi lit tout ce qui l'attendait
    got = []
    deadline = time.time() + T(15)
    rx = re.compile(rf"PRIVMSG {esc(ch)} :(\d{{5}}) ")
    while len(got) < n and time.time() < deadline and not s.closed:
        s._fill(0.2)
        keep = []
        for line in s.pending:
            m = rx.search(line)
            if m:
                got.append(int(m.group(1)))
            else:
                keep.append(line)
        s.pending = keep
    check(len(got) == n, f"le client endormi n'a reçu que {len(got)}/{n} messages")
    check(got == list(range(n)), "messages reçus dans le désordre ou en double")


@test("Réseau", "client figé puis tué pendant que le channel déborde (SIGPIPE)", hint=HINT_SIGPIPE)
def _(ctx):
    # v ne lit plus (petit buffer de réception) : le serveur accumule ~5 Mo pour lui,
    # plus que ce que le noyau peut absorber. Puis v est tué (RST) alors que des
    # données lui sont encore destinées : le prochain send() vers lui → EPIPE.
    v = ctx.client("victime", rcvbuf=4096)
    v.register(ctx.nick("vic"))
    ch = ctx.chan()
    v.send(f"JOIN {ch}")
    f = ctx.user("flood")
    f.join(ch)
    pad = "z" * 300
    for start in range(0, 16000, 1000):
        f.send_raw("".join(f"PRIVMSG {ch} :{i:05d}{pad}\r\n" for i in range(start, start + 1000)))
    time.sleep(T(0.3))
    v.reset()
    time.sleep(T(0.05))
    f.send(f"PRIVMSG {ch} :encore un")
    time.sleep(T(0.4))
    f.send("PING toujours_la")
    f.expect(r"PONG .*toujours_la", what="PONG : le serveur doit survivre")


@test("Réseau", "connexions fermées aussitôt ouvertes (normalement et par RST)")
def _(ctx):
    for i in range(30):
        s = socket.create_connection((HOST, ctx.port), timeout=T(2))
        if i % 3 == 1:
            s.sendall(b"NICK x")
        if i % 2:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.close()
    time.sleep(T(0.3))
    a = ctx.user("ali")
    a.send("PING ok")
    a.expect(r"PONG .*ok", what="PONG après 30 connexions éclair")


@test("Réseau", "octets binaires / caractères nuls sans crash")
def _(ctx):
    c = ctx.client("bin")
    rnd = random.Random(42)
    c.send_raw(bytes(rnd.randrange(256) for _ in range(3000)) + b"\r\n")
    c.send_raw(b"\x00\x00NICK \xff\xfe\r\nPRIVMSG \x00 :\x00\r\n")
    time.sleep(T(0.3))
    a = ctx.user("ali")
    a.send("PING ok")
    a.expect(r"PONG .*ok", what="PONG après des données binaires")


@test("Réseau", "commande coupée puis déconnexion")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send_raw(f"PRIVMSG {ch} :phrase pas fin")
    b.close()
    time.sleep(T(0.3))
    a.send("PING ok")
    a.expect(r"PONG .*ok", what="PONG après la déconnexion de bob")
    a.expect_none(r"phrase pas fin", wait=0.2, what="une commande incomplète")


# ═════════════════════════════════ PRIVMSG ═════════════════════════════════

@test("PRIVMSG", "message privé : « :expéditeur!user@host PRIVMSG cible :texte »")
def _(ctx):
    a, b = ctx.users("ali", "bob")
    a.send(f"PRIVMSG {b.nick} :salut à toi")
    line = b.expect(rf"PRIVMSG {esc(b.nick)} :salut à toi$", what="le message privé d'ali")
    check(re.match(rf"^:{esc(a.nick)}!\S+@\S+ ", line),
          f"préfixe attendu « :{a.nick}!user@host », reçu : {line}")
    a.expect_none(r"PRIVMSG", wait=0.2, what="son propre message privé")


@test("PRIVMSG", "texte transmis intact (espaces, « : », accents)")
def _(ctx):
    a, b = ctx.users("ali", "bob")
    txt = "a :b  c::d   é ü ✓ fin "
    a.send(f"PRIVMSG {b.nick} :{txt}")
    line = b.expect(rf"PRIVMSG {esc(b.nick)} :", what="le message")
    got = line.split(f"PRIVMSG {b.nick} :", 1)[1]
    check(got == txt, f"texte modifié :\n    envoyé : {txt!r}\n    reçu   : {got!r}")


@test("PRIVMSG", "pseudo inexistant → 401 ERR_NOSUCHNICK")
def _(ctx):
    a = ctx.user("ali")
    a.send("PRIVMSG fantome :hello")
    a.expect(NUM("401"), what="401")


@test("PRIVMSG", "sans destinataire → 411 ; sans texte → 412")
def _(ctx):
    a, b = ctx.users("ali", "bob")
    a.send("PRIVMSG")
    a.expect(NUM("411|461"), what="411 ERR_NORECIPIENT")
    a.send(f"PRIVMSG {b.nick}")
    a.expect(NUM("412|461"), what="412 ERR_NOTEXTTOSEND")
    a.send(f"PRIVMSG {b.nick} :")
    a.expect(NUM("412"), what="412 pour un texte vide")
    b.expect_none(r"PRIVMSG", wait=0.2, what="un message vide")


@test("PRIVMSG", "message de channel : tous les membres sauf l'expéditeur")
def _(ctx):
    ch, a, b, c = ctx.channel("ali", "bob", "cat")
    a.send(f"PRIVMSG {ch} :bonjour le channel")
    for u in (b, c):
        u.expect(FROM(a.nick) + rf"PRIVMSG {esc(ch)} :bonjour le channel$", what=f"le message d'ali sur {ch}")
    a.expect_none(r"PRIVMSG", wait=0.3, what="son propre message (écho)")


@test("PRIVMSG", "pas membre du channel → 404 ; channel inexistant → 403/401")
def _(ctx):
    ch, a = ctx.channel("ali")
    d = ctx.user("dan")
    d.send(f"PRIVMSG {ch} :je force")
    d.expect(NUM("404|442"), what="404 ERR_CANNOTSENDTOCHAN")
    a.expect_none(r"je force", wait=0.2, what="le message d'un non-membre")
    d.send(f"PRIVMSG {ctx.chan('nope')} :x")
    d.expect(NUM("403|401"), what="403 ERR_NOSUCHCHANNEL")


# ═════════════════════════════════ JOIN / PART ═════════════════════════════════

@test("JOIN / PART", "JOIN crée le channel : confirmation + 353 (@créateur) + 366")
def _(ctx):
    a = ctx.user("ali")
    names = a.join(ctx.chan())
    check("@" + a.nick in names, f"le créateur doit être opérateur (@{a.nick}) dans le 353 : {names}")


@test("JOIN / PART", "nouveau membre : JOIN diffusé + liste des noms à jour")
def _(ctx):
    ch, a = ctx.channel("ali")
    b = ctx.user("bob")
    names = b.join(ch)
    a.expect(FROM(b.nick) + rf"JOIN :?{esc(ch)}", what="le JOIN de bob")
    check("@" + a.nick in names, f"{a.nick} devrait apparaître avec @ : {names}")
    check(b.nick in names, f"{b.nick} devrait apparaître sans @ : {names}")


@test("JOIN / PART", "topic envoyé à l'arrivée (332) quand il existe")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"TOPIC {ch} :Bienvenue ici")
    a.expect(r"TOPIC", what="le TOPIC")
    b = ctx.user("bob")
    b.send(f"JOIN {ch}")
    line = b.expect(NUM("332"), what="332 RPL_TOPIC en arrivant")
    check("Bienvenue ici" in line, f"le 332 doit contenir le topic : {line}")


@test("JOIN / PART", "rejoindre plusieurs channels d'un coup (JOIN #a,#b)")
def _(ctx):
    a = ctx.user("ali")
    c1, c2 = ctx.chan("a"), ctx.chan("b")
    a.send(f"JOIN {c1},{c2}")
    a.expect(FROM(a.nick) + rf"JOIN :?{esc(c1)}", what=f"JOIN {c1}")
    a.expect(FROM(a.nick) + rf"JOIN :?{esc(c2)}", what=f"JOIN {c2}")


@test("JOIN / PART", "clés multiples (JOIN #a,#b cle1,cle2)")
def _(ctx):
    a = ctx.user("ali")
    c1, c2 = ctx.chan("a"), ctx.chan("b")
    a.join(c1)
    a.join(c2)
    a.send(f"MODE {c1} +k cle1")
    a.send(f"MODE {c2} +k cle2")
    a.expect(rf"MODE {esc(c2)} \+k", what="MODE +k")
    b = ctx.user("bob")
    b.send(f"JOIN {c1},{c2} cle1,cle2")
    b.expect(FROM(b.nick) + rf"JOIN :?{esc(c1)}", what=f"JOIN {c1} avec cle1")
    b.expect(FROM(b.nick) + rf"JOIN :?{esc(c2)}", what=f"JOIN {c2} avec cle2")


@test("JOIN / PART", "JOIN sans paramètre → 461 ; nom sans # → 403/476")
def _(ctx):
    a = ctx.user("ali")
    a.send("JOIN")
    a.expect(NUM("461"), what="461")
    a.send("JOIN pasdedieze")
    a.expect(NUM("403|476"), what="403 ou 476 pour un nom de channel invalide")


@test("JOIN / PART", "re-JOIN d'un channel déjà rejoint : pas de doublon")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send(f"JOIN {ch}")
    a.expect_none(FROM(b.nick) + "JOIN", wait=0.3, what="un 2e JOIN de bob")
    c = ctx.user("cat")
    names = [strip_prefix(n) for n in c.join(ch)]
    check(names.count(b.nick) == 1, f"bob apparaît {names.count(b.nick)} fois dans la liste : {names}")


@test("JOIN / PART", "PART : diffusé à tous, puis plus membre du channel")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send(f"PART {ch} :a plus")
    a.expect(FROM(b.nick) + rf"PART {esc(ch)}", what="le PART de bob")
    b.expect(FROM(b.nick) + rf"PART {esc(ch)}", what="la confirmation de son PART")
    b.send(f"PRIVMSG {ch} :toujours la ?")
    b.expect(NUM("404|442"), what="404 : bob n'est plus membre")
    a.expect_none(r"toujours la", wait=0.2, what="un message de bob après son PART")


@test("JOIN / PART", "PART : 403 (channel inexistant), 442 (pas membre), 461")
def _(ctx):
    ch, a = ctx.channel("ali")
    b = ctx.user("bob")
    b.send(f"PART {ctx.chan('nope')}")
    b.expect(NUM("403"), what="403")
    b.send(f"PART {ch}")
    b.expect(NUM("442"), what="442")
    b.send("PART")
    b.expect(NUM("461"), what="461")


@test("JOIN / PART", "channel vidé par PART puis recréé : le 1er arrivant redevient opérateur",
      hint=HINT_EMPTY)
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"PART {ch}")
    a.expect(r"PART", what="PART")
    b = ctx.user("bob")
    names = b.join(ch)
    check("@" + b.nick in names, f"bob recrée {ch} vide mais n'est pas opérateur : {names}")


# ═════════════════════════════════ KICK ═════════════════════════════════

@test("KICK", "KICK par l'opérateur : diffusé à tous (cible comprise), cible retirée")
def _(ctx):
    ch, a, b, c = ctx.channel("ali", "bob", "cat")
    a.send(f"KICK {ch} {b.nick} :dehors")
    for u in (a, b, c):
        line = u.expect(FROM(a.nick) + rf"KICK {esc(ch)} {esc(b.nick)}\b", what="le KICK")
        check("dehors" in line, f"la raison du KICK manque : {line}")
    b.send(f"PRIVMSG {ch} :je suis encore la")
    b.expect(NUM("404|442"), what="404 : bob a été kické")
    c.expect_none(r"encore la", wait=0.2, what="un message de bob après le KICK")


@test("KICK", "KICK sans raison : quand même diffusé")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"KICK {ch} {b.nick}")
    b.expect(FROM(a.nick) + rf"KICK {esc(ch)} {esc(b.nick)}", what="le KICK")


@test("KICK", "KICK par un non-opérateur → 482, rien ne se passe")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send(f"KICK {ch} {a.nick} :coup d'état")
    b.expect(NUM("482"), what="482 ERR_CHANOPRIVSNEEDED")
    a.expect_none(r"KICK", wait=0.2, what="un KICK")


@test("KICK", "KICK d'un pseudo hors du channel → 441 ; inconnu → 441/401")
def _(ctx):
    ch, a = ctx.channel("ali")
    d = ctx.user("dan")
    a.send(f"KICK {ch} {d.nick}")
    a.expect(NUM("441"), what="441 ERR_USERNOTINCHANNEL")
    a.send(f"KICK {ch} fantome")
    a.expect(NUM("441|401"), what="441 ou 401")
    d.expect_none(r"KICK", wait=0.2, what="un KICK")


@test("KICK", "KICK : 403 (channel inexistant), 442 (pas membre), 461")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    d = ctx.user("dan")
    d.send(f"KICK {ctx.chan('nope')} {b.nick}")
    d.expect(NUM("403"), what="403")
    d.send(f"KICK {ch} {b.nick}")
    d.expect(NUM("442"), what="442")
    a.send(f"KICK {ch}")
    a.expect(NUM("461"), what="461")
    a.send("KICK")
    a.expect(NUM("461"), what="461 pour « KICK » seul")


@test("KICK", "un utilisateur kické peut revenir")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"KICK {ch} {b.nick}")
    b.expect(r"KICK", what="le KICK")
    names = b.join(ch)
    check(b.nick in [strip_prefix(n) for n in names], f"bob devrait être dans la liste : {names}")


# ═════════════════════════════════ INVITE ═════════════════════════════════

@test("INVITE", "INVITE sur un channel +i : 341, INVITE reçu, la cible peut entrer")
def _(ctx):
    ch, a = ctx.channel("ali")
    b = ctx.user("bob")
    a.send(f"MODE {ch} +i")
    a.expect(rf"MODE {esc(ch)} \+i", what="MODE +i")
    a.send(f"INVITE {b.nick} {ch}")
    a.expect(NUM("341"), what="341 RPL_INVITING")
    b.expect(FROM(a.nick) + rf"INVITE {esc(b.nick)} :?{esc(ch)}", what="l'INVITE")
    b.join(ch)


@test("INVITE", "JOIN sur un channel +i sans invitation → 473")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"MODE {ch} +i")
    a.expect(rf"MODE {esc(ch)} \+i", what="MODE +i")
    b = ctx.user("bob")
    b.send(f"JOIN {ch}")
    b.expect(NUM("473"), what="473 ERR_INVITEONLYCHAN")
    a.expect_none(r"JOIN", wait=0.2, what="le JOIN de bob")


@test("INVITE", "INVITE par un non-opérateur sur un channel +i → 482")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"MODE {ch} +i")
    b.expect(rf"MODE {esc(ch)} \+i", what="MODE +i")
    c = ctx.user("cat")
    b.send(f"INVITE {c.nick} {ch}")
    b.expect(NUM("482"), what="482")
    c.expect_none(r"INVITE", wait=0.2, what="une invitation")


@test("INVITE", "INVITE sur un channel sans +i : n'importe quel membre peut inviter")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    c = ctx.user("cat")
    b.send(f"INVITE {c.nick} {ch}")
    b.expect(NUM("341"), what="341")
    c.expect(rf"INVITE {esc(c.nick)} :?{esc(ch)}", what="l'INVITE")


@test("INVITE", "INVITE : 401 (pseudo inconnu), 443 (déjà membre), 442 (pas membre), 461")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    d = ctx.user("dan")
    a.send(f"INVITE fantome {ch}")
    a.expect(NUM("401"), what="401")
    a.send(f"INVITE {b.nick} {ch}")
    a.expect(NUM("443"), what="443 ERR_USERONCHANNEL")
    d.send(f"INVITE {a.nick} {ch}")
    d.expect(NUM("442"), what="442")
    a.send("INVITE")
    a.expect(NUM("461"), what="461")


# ═════════════════════════════════ TOPIC ═════════════════════════════════

@test("TOPIC", "TOPIC sans sujet défini → 331 RPL_NOTOPIC")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"TOPIC {ch}")
    a.expect(NUM("331"), what="331")


@test("TOPIC", "l'opérateur change le topic : diffusé à tous, puis 332")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"TOPIC {ch} :Le sujet du jour")
    for u in (a, b):
        u.expect(FROM(a.nick) + rf"TOPIC {esc(ch)} :Le sujet du jour$", what="le TOPIC diffusé")
    b.send(f"TOPIC {ch}")
    line = b.expect(NUM("332"), what="332 RPL_TOPIC")
    check("Le sujet du jour" in line, f"le 332 doit contenir le topic : {line}")


@test("TOPIC", "+t : un non-opérateur ne peut pas changer le topic → 482")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"MODE {ch} +t")
    time.sleep(T(0.2))
    a.drain(0.1)
    b.drain(0.1)
    b.send(f"TOPIC {ch} :pirate")
    b.expect(NUM("482"), what="482")
    a.expect_none(r"TOPIC", wait=0.2, what="un changement de topic")


@test("TOPIC", "-t : n'importe quel membre peut changer le topic")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"MODE {ch} -t")
    time.sleep(T(0.2))
    b.send(f"TOPIC {ch} :libre")
    a.expect(FROM(b.nick) + rf"TOPIC {esc(ch)} :libre$", what="le TOPIC de bob")


@test("TOPIC", "TOPIC : 442 (pas membre), 403 (channel inexistant), 461")
def _(ctx):
    ch, a = ctx.channel("ali")
    d = ctx.user("dan")
    d.send(f"TOPIC {ch} :intrus")
    d.expect(NUM("442"), what="442")
    d.send(f"TOPIC {ctx.chan('nope')}")
    d.expect(NUM("403"), what="403")
    d.send("TOPIC")
    d.expect(NUM("461"), what="461")
    a.expect_none(r"intrus", wait=0.2, what="le topic d'un non-membre")


# ═════════════════════════════════ MODE ═════════════════════════════════

def mode_letters(line, ch):
    """Lettres de mode d'une réponse 324 (« 324 nick #chan :+itk cle » ou sans ':')."""
    after = line.split(ch, 1)[1].strip().lstrip(":")
    return after.split()[0] if after.split() else ""


@test("MODE", "MODE #chan → 324 RPL_CHANNELMODEIS")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"MODE {ch}")
    a.expect(NUM("324") + rf".*{esc(ch)}", what="324")


@test("MODE", "changement de mode par un non-opérateur → 482")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send(f"MODE {ch} +i")
    b.expect(NUM("482"), what="482")
    a.expect_none(r" MODE ", wait=0.2, what="un changement de mode")


@test("MODE", "MODE sur un channel inexistant → 403")
def _(ctx):
    a = ctx.user("ali")
    a.send(f"MODE {ctx.chan('nope')} +i")
    a.expect(NUM("403"), what="403")


@test("MODE", "changement diffusé à tout le channel : « :op!user@host MODE #chan +i »")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    a.send(f"MODE {ch} +i")
    for u in (a, b):
        u.expect(FROM(a.nick) + rf"MODE {esc(ch)} :?\+i", what="le MODE +i diffusé")


@test("MODE", "+i / -i : effet sur JOIN")
def _(ctx):
    ch, a = ctx.channel("ali")
    c = ctx.user("cat")
    a.send(f"MODE {ch} +i")
    a.expect(rf"MODE {esc(ch)} :?\+i", what="+i")
    c.send(f"JOIN {ch}")
    c.expect(NUM("473"), what="473")
    a.send(f"MODE {ch} -i")
    a.expect(rf"MODE {esc(ch)} :?-i", what="-i")
    c.join(ch)


@test("MODE", "+k : sans clé ou mauvaise clé → 475, bonne clé → OK ; -k retire la clé")
def _(ctx):
    ch, a = ctx.channel("ali")
    b, c = ctx.users("bob", "cat")
    a.send(f"MODE {ch} +k sesame")
    a.expect(rf"MODE {esc(ch)} :?\+k", what="+k")
    b.send(f"JOIN {ch}")
    b.expect(NUM("475"), what="475 sans clé")
    b.send(f"JOIN {ch} mauvaise")
    b.expect(NUM("475"), what="475 avec une mauvaise clé")
    b.join(ch, "sesame")
    a.send(f"MODE {ch} -k")
    a.expect(rf"MODE {esc(ch)} :?-k", what="-k")
    c.join(ch)


@test("MODE", "+l : limite du nombre de membres (471) ; -l la retire")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    c = ctx.user("cat")
    a.send(f"MODE {ch} +l 2")
    a.expect(rf"MODE {esc(ch)} :?\+l", what="+l 2")
    c.send(f"JOIN {ch}")
    c.expect(NUM("471"), what="471 ERR_CHANNELISFULL")
    a.send(f"MODE {ch} -l")
    a.expect(rf"MODE {esc(ch)} :?-l", what="-l")
    c.join(ch)


@test("MODE", "+o donne les droits d'opérateur, -o les retire")
def _(ctx):
    ch, a, b, c = ctx.channel("ali", "bob", "cat")
    a.send(f"MODE {ch} +o {b.nick}")
    for u in (a, b, c):
        u.expect(rf"MODE {esc(ch)} :?\+o {esc(b.nick)}", what=f"MODE +o {b.nick} diffusé")
    b.send(f"KICK {ch} {c.nick} :nouveau chef")
    c.expect(FROM(b.nick) + rf"KICK {esc(ch)} {esc(c.nick)}", what="le KICK du nouvel opérateur")
    a.send(f"MODE {ch} -o {b.nick}")
    b.expect(rf"MODE {esc(ch)} :?-o {esc(b.nick)}", what=f"MODE -o {b.nick}")
    b.send(f"MODE {ch} +i")
    b.expect(NUM("482"), what="482 : bob n'est plus opérateur")


@test("MODE", "plusieurs modes d'un coup : MODE #chan +ikl cle 5")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"MODE {ch} +ikl cle 5")
    a.expect(rf"MODE {esc(ch)} ", what="le MODE diffusé")
    a.send(f"MODE {ch}")
    line = a.expect(NUM("324"), what="324")
    letters = mode_letters(line, ch)
    for m in "ikl":
        check(m in letters, f"le mode +{m} devrait être actif, 324 reçu : {line}")
    b = ctx.user("bob")
    b.send(f"JOIN {ch}")
    b.expect(NUM("473|475"), what="473/475 (channel +i et +k)")


@test("MODE", "+k / +l / +o sans paramètre : ignorés proprement")
def _(ctx):
    ch, a = ctx.channel("ali")
    for m in ("+k", "+l", "+o", "-o"):
        a.send(f"MODE {ch} {m}")
    time.sleep(T(0.2))
    b = ctx.user("bob")
    b.join(ch)


@test("MODE", "+l invalide (abc, -5, 0, énorme) : ignoré sans crash")
def _(ctx):
    ch, a = ctx.channel("ali")
    for v in ("abc", "-5", "0", "99999999999999999999"):
        a.send(f"MODE {ch} +l {v}")
    time.sleep(T(0.2))
    b = ctx.user("bob")
    b.send(f"JOIN {ch}")
    b.expect(FROM(b.nick) + rf"JOIN :?{esc(ch)}|" + NUM("471"), what="JOIN ou 471")
    a.send("PING vivant")
    a.expect(r"PONG .*vivant", what="PONG")


@test("MODE", "+o sur un pseudo inconnu ou hors du channel : pas de crash")
def _(ctx):
    ch, a = ctx.channel("ali")
    d = ctx.user("dan")
    a.send(f"MODE {ch} +o fantome")
    a.send(f"MODE {ch} +o {d.nick}")
    time.sleep(T(0.2))
    d.send(f"MODE {ch} +i")
    d.expect(NUM("482|442"), what="482 : dan ne doit pas être devenu opérateur")


@test("MODE", "mode inconnu (+x) → 472 ERR_UNKNOWNMODE", sev="warn",
      hint="applyChannelModes() ignore les lettres inconnues : renvoie « 472 <nick> x :is unknown mode char to me ».")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send(f"MODE {ch} +x")
    a.expect(NUM("472"), what="472")


@test("MODE", "« MODE <pseudo> +i » (envoyé par irssi à la connexion) : pas de 403", sev="warn",
      hint="handleMode() traite toute cible comme un channel. Si elle ne commence pas par '#', "
           "c'est un mode utilisateur : ignore-le (ou réponds 221) au lieu d'un 403 que irssi affiche.")
def _(ctx):
    a = ctx.user("ali")
    a.send(f"MODE {a.nick} +i")
    a.expect_none(NUM("403"), wait=0.4, what="403 No such channel pour un mode utilisateur")


# ═════════════════════════════════ QUIT / DÉCONNEXION ═════════════════════════════════

@test("QUIT", "QUIT : « :nick!user@host QUIT :raison » envoyé aux membres des channels communs")
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send("QUIT :bonne nuit")
    a.expect(FROM(b.nick) + r"QUIT :bonne nuit$", what="le QUIT de bob")


@test("QUIT", "QUIT : le serveur ferme la connexion et libère le pseudo", hint=HINT_LEAVE)
def _(ctx):
    a = ctx.user("ali")
    a.send("QUIT :bye")
    check(a.wait_closed(2), "après QUIT, la connexion reste ouverte : le client n'est jamais déconnecté")
    b = ctx.client("bob")
    b.register(a.nick)


GHOST_EXPLAIN = ("  cat n'a jamais rejoint {ch} mais reçoit ses messages : le channel garde un pointeur "
                 "vers bob,\n  dont la mémoire a été libérée puis réutilisée pour le nouveau client cat.")
JOIN_SILENT = ("  cat n'a reçu aucune réponse à son JOIN : le serveur le croit déjà membre (isClientInChannel "
               "est vrai),\n  car le channel garde un pointeur vers bob, dont la mémoire a été réutilisée pour cat.")


def join_after_departure(ctx, ch, gone):
    """Un nouveau client rejoint ch : il ne doit pas hériter de la place de `gone`."""
    c = ctx.user("cat")
    try:
        names = [strip_prefix(n) for n in c.join(ch)]
    except TestFail as e:
        raise TestFail(str(e) + "\n" + JOIN_SILENT)
    check(gone.nick not in names, f"{gone.nick} est parti mais apparaît encore dans {ch} : {names}")


@test("QUIT", "QUIT : le client est retiré des channels", hint=HINT_COPY)
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.send("QUIT :bye")
    a.expect(r"QUIT", what="le QUIT de bob")
    b.close()
    time.sleep(T(0.3))
    join_after_departure(ctx, ch, b)


@test("QUIT", "QUIT du dernier membre : le channel est supprimé")
def _(ctx):
    ch, a = ctx.channel("ali")
    a.send("QUIT :bye")
    a.close()
    time.sleep(T(0.3))
    b = ctx.user("bob")
    names = b.join(ch)
    check("@" + b.nick in names, f"le channel aurait dû être recréé avec bob opérateur : {names}")


@test("QUIT", "déconnexion brutale (ctrl+C) : les autres membres reçoivent un QUIT", hint=HINT_ABRUPT)
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.close()
    a.expect(FROM(b.nick) + "QUIT", what="un QUIT pour bob (le README l'annonce)")


@test("QUIT", "déconnexion brutale : le client est retiré des channels (pas de pointeur fantôme)",
      hint=HINT_ABRUPT)
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.close()
    time.sleep(T(0.3))
    for i in range(5):
        a.send(f"PRIVMSG {ch} :message {i}")
    a.send(f"TOPIC {ch} :apres le depart")
    a.send(f"MODE {ch} +o {b.nick}")
    time.sleep(T(0.2))
    join_after_departure(ctx, ch, b)


@test("QUIT", "un nouveau client n'hérite pas des channels d'un client déconnecté", hint=HINT_ABRUPT)
def _(ctx):
    ch, a, b = ctx.channel("ali", "bob")
    b.close()
    time.sleep(T(0.3))
    c = ctx.user("cat")          # ne fait PAS de JOIN
    a.send(f"PRIVMSG {ch} :message reserve aux membres")
    try:
        c.expect_none(r"message reserve aux membres", wait=0.5, what="un message d'un channel jamais rejoint")
    except TestFail as e:
        raise TestFail(str(e) + "\n" + GHOST_EXPLAIN.format(ch=ch))


@test("QUIT", "déconnexion brutale par RST au milieu d'un channel actif", hint=HINT_ABRUPT)
def _(ctx):
    ch, a, b, c = ctx.channel("ali", "bob", "cat")
    b.reset()
    time.sleep(T(0.3))
    for i in range(20):
        a.send(f"PRIVMSG {ch} :spam {i}")
    c.expect(rf"spam 19$", what="les messages d'ali")
    a.send("PING ok")
    a.expect(r"PONG .*ok", what="PONG")


# ═════════════════════════════════ STRESS ═════════════════════════════════

@test("Stress", "100 clients arrivent, discutent et repartent (certains brutalement)")
def _(ctx):
    ch = ctx.chan()
    host = ctx.user("host")
    host.join(ch)
    cs = []
    for i in range(100):
        c = ctx.client(f"s{i}")
        n = ctx.nick(f"s{i}")
        c.nick = n
        c.send_raw(f"PASS {ctx.password}\r\nNICK {n}\r\nUSER {n} 0 * :S\r\nJOIN {ch}\r\n"
                   f"PRIVMSG {ch} :coucou de {n}\r\n")
        cs.append(c)
    for i, c in enumerate(cs):
        c.expect(NUM("366"), timeout=5, what="366 (fin du JOIN)")
    for i, c in enumerate(cs):
        if i % 3 == 0:
            c.send("QUIT :fini")
        elif i % 3 == 1:
            c.close()
        else:
            c.reset()
    time.sleep(T(0.5))
    host.send("PING fin")
    host.expect(r"PONG .*fin", timeout=5, what="PONG à la fin")


# ─────────────────────────────── exécution ───────────────────────────────

def run_one(t, idx, opts):
    res = {"status": "ok", "msg": "", "crash": None, "crash_hint": None, "findings": [], "log": ""}
    if t["standalone"]:
        try:
            t["fn"](opts)
        except TestFail as e:
            res.update(status="fail", msg=str(e))
        except Exception as e:  # bug du testeur ou environnement
            res.update(status="fail", msg=f"erreur inattendue dans le test : {e!r}")
        return res

    srv = None
    port = opts.external
    if not opts.external:
        port = free_port()
        srv = ServerProc(opts.binary, port, opts.password, opts.logdir, opts.valgrind)
        if not srv.wait_ready():
            rc = srv.stop()
            desc, h = describe_rc(rc)
            res.update(status="fail", msg=f"le serveur n'a pas démarré ({desc})", log=srv.tail())
            return res

    ctx = Ctx(port, opts.password, f"{opts.tag}{idx}")
    try:
        t["fn"](ctx)
    except TestFail as e:
        res.update(status="fail", msg=str(e))
    except Exception as e:
        res.update(status="fail", msg=f"erreur inattendue dans le test : {e!r}")
    finally:
        ctx.cleanup()

    time.sleep(T(0.05))
    if srv:
        rc = srv.proc.poll()
        if rc is not None:
            desc, h = describe_rc(rc)
            res.update(crash=f"le serveur s'est arrêté pendant le test ({desc})", crash_hint=h)
        elif not probe(port, opts.password):
            res.update(crash="le serveur ne répond plus (bloqué ?)")
        srv.stop()
        res["findings"] = srv.findings()
        if res["findings"]:
            res["log"] = srv.output_before_crash()
        elif res["crash"]:
            res["log"] = srv.tail()
    else:
        if not probe(port, opts.password):
            res.update(crash="le serveur ne répond plus", fatal=True)
    return res


def final_status(t, res):
    if res["crash"] or res["findings"]:
        return "fail"
    if res["status"] == "fail" and t["sev"] == "warn":
        return "warn"
    return res["status"]


def print_result(t, res, status):
    sym = {"ok": GREEN("  ✔"), "fail": RED("  ✘"), "warn": YELLOW("  ⚠")}[status]
    print(f"{sym} {t['name']}")
    if status == "ok":
        return
    if res["msg"]:
        print(indent(res["msg"], 6))
    if res["crash"]:
        print(RED(indent("💥 " + res["crash"], 6)))
    if res["findings"]:
        print(RED(indent("🧠 erreurs mémoire détectées dans la sortie du serveur :", 6)))
        for f in res["findings"]:
            print(RED(indent("• " + "\n  ".join(shorten(x, 200) for x in f.splitlines()), 8)))
    if res["log"]:
        print(DIM(indent("sortie du serveur (fin) :", 6)))
        print(DIM(indent(res["log"], 8)))
    hints = []
    for h in (t["hint"] if (res["status"] == "fail" or res["findings"]) else None, res["crash_hint"]):
        if h and h not in hints:
            hints.append(h)
    for h in hints:
        print(CYAN(indent("→ piste : " + h, 6)))


def main():
    global TIME_FACTOR, VERBOSE
    ap = argparse.ArgumentParser(description="Testeur boîte noire pour ft_irc (42).")
    ap.add_argument("binary", nargs="?", default="./ircserv", help="chemin de ircserv (défaut : ./ircserv)")
    ap.add_argument("-k", dest="filters", action="append", default=[],
                    help="ne lance que les tests dont la catégorie ou le nom contient ce mot (répétable)")
    ap.add_argument("-v", "--verbose", action="store_true", help="affiche tout ce qui est envoyé / reçu")
    ap.add_argument("--list", action="store_true", help="liste les tests sans les lancer")
    ap.add_argument("--password", default="secret42", help="mot de passe du serveur (défaut : secret42)")
    ap.add_argument("--external", type=int, metavar="PORT",
                    help="teste un serveur déjà lancé sur ce port (pas de test de lancement ni de détection de crash fine)")
    ap.add_argument("--valgrind", action="store_true", help="lance chaque serveur sous valgrind (lent)")
    ap.add_argument("--slow", type=float, default=1.0, help="multiplie tous les délais (machine lente)")
    ap.add_argument("--keep-logs", action="store_true", help="garde les sorties du serveur")
    opts = ap.parse_args()

    VERBOSE = opts.verbose
    TIME_FACTOR = opts.slow * (6 if opts.valgrind else 1)

    selected = []
    for t in TESTS:
        hay = (t["cat"] + " " + t["name"]).lower()
        if opts.filters and not any(f.lower() in hay for f in opts.filters):
            continue
        if opts.external and t["standalone"]:
            continue
        selected.append(t)

    if opts.list:
        cat = None
        for t in selected:
            if t["cat"] != cat:
                cat = t["cat"]
                print(BOLD(cat))
            print(f"  {'[warn] ' if t['sev'] == 'warn' else ''}{t['name']}")
        print(f"\n{len(selected)} tests")
        return 0

    if not opts.external:
        if os.path.sep not in opts.binary:
            opts.binary = os.path.join(".", opts.binary)
        if not os.path.isfile(opts.binary) or not os.access(opts.binary, os.X_OK):
            print(RED(f"Binaire introuvable : {opts.binary} — lance `make` d'abord (ou donne le chemin)."))
            return 2
        opts.binary = os.path.abspath(opts.binary)
        if opts.valgrind and not shutil.which("valgrind"):
            print(RED("valgrind n'est pas installé."))
            return 2

    opts.tag = "" if not opts.external else random.choice("abcdefghjkmnpqrstuvwxyz")
    opts.logdir = tempfile.mkdtemp(prefix="ft_irc_tester_")

    target = f"port {opts.external}" if opts.external else opts.binary
    print(BOLD(f"ft_irc tester — {len(selected)} tests sur {target}"))
    if not opts.external:
        print(DIM("  un serveur neuf est lancé pour chaque test\n"))

    counts = {"ok": 0, "fail": 0, "warn": 0}
    failed, warned = [], []
    cat = None
    t0 = time.time()
    try:
        for idx, t in enumerate(selected, 1):
            if t["cat"] != cat:
                cat = t["cat"]
                print(BOLD(f"\n{cat}"))
            res = run_one(t, idx, opts)
            st = final_status(t, res)
            counts[st] += 1
            (failed if st == "fail" else warned if st == "warn" else []).append(f"{t['cat']} › {t['name']}")
            print_result(t, res, st)
            if res.get("fatal"):
                print(RED("\nLe serveur externe ne répond plus : arrêt des tests."))
                break
    except KeyboardInterrupt:
        print(YELLOW("\ninterrompu"))

    dt = time.time() - t0
    print(BOLD("\n────────────────────────────────────────"))
    print(f"{GREEN(str(counts['ok']) + ' OK')}   {RED(str(counts['fail']) + ' FAIL')}   "
          f"{YELLOW(str(counts['warn']) + ' WARN')}   {DIM(f'({dt:.1f} s)')}")
    if failed:
        print(RED("\nÀ corriger :"))
        for n in failed:
            print(RED(f"  ✘ {n}"))
    if warned:
        print(YELLOW("\nRecommandé (irssi / RFC) :"))
        for n in warned:
            print(YELLOW(f"  ⚠ {n}"))

    if opts.keep_logs:
        print(DIM(f"\nsorties du serveur : {opts.logdir}"))
    else:
        shutil.rmtree(opts.logdir, ignore_errors=True)
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())