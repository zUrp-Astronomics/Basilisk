"""Faux miroir de l'archive Ubuntu, pour l'essai du repli d'apt_update (.gitea/essai_apt/essai.sh).

Il écoute sur 127.0.0.1:80, où /etc/hosts envoie les hôtes de l'archive, relaie chaque requête à l'archive
réelle, par le proxy http qu'apt aurait pris (le cache du runner) ou, sans proxy, aux adresses résolues AVANT
le détournement, et altère un octet de ce qu'il sert sous dists/<suite>/main/binary-amd64/ (index et
by-hash). L'InRelease est le vrai, signé : ses hachés ne correspondent plus à l'index reçu. C'est le Hash Sum mismatch du 2026-10-24 (noble-updates/main, run 2823).

usage : python3 miroir.py <suite> <proxy hôte:port | DIRECT> <hôte>=<adresse> [<hôte>=<adresse>...]
"""
import http.client
import http.server
import sys

SUITE = sys.argv[1]
PROXY = None if sys.argv[2] == 'DIRECT' else sys.argv[2].rsplit(':', 1)
AMONT = dict(arg.split('=', 1) for arg in sys.argv[3:])
ALTERE = f'/dists/{SUITE}/main/binary-amd64/'


class Relais(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        hote = self.headers.get('Host', '').split(':')[0]
        if hote not in AMONT:
            self.send_error(502, f'hôte non relayé : {hote}')
            return
        entetes = {'Host': hote, 'User-Agent': self.headers.get('User-Agent', '')}
        if PROXY:
            amont = http.client.HTTPConnection(PROXY[0], int(PROXY[1]), timeout=120)
            amont.request('GET', f'http://{hote}{self.path}', headers=entetes)
        else:
            amont = http.client.HTTPConnection(AMONT[hote], 80, timeout=120)
            amont.request('GET', self.path, headers=entetes)
        reponse = amont.getresponse()
        corps = reponse.read()
        if reponse.status == 200 and ALTERE in self.path and corps:
            corps = bytes([corps[0] ^ 0xFF]) + corps[1:]
            print(f'miroir : servi altéré http://{hote}{self.path}', file=sys.stderr, flush=True)
        self.send_response(reponse.status)
        for entete in ('Content-Type', 'Last-Modified'):
            if reponse.getheader(entete):
                self.send_header(entete, reponse.getheader(entete))
        self.send_header('Content-Length', str(len(corps)))
        self.end_headers()
        self.wfile.write(corps)

    def log_message(self, *args):
        pass


http.server.ThreadingHTTPServer(('127.0.0.1', 80), Relais).serve_forever()
