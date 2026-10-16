# indi_basilisk_focus — driver INDI pour la carte Basilisk

**Date** : 2026-09-24
**Dernière révision** : 2026-10-11 (ticket #606 : driver 0.9, comme la carte ; la XIAO de banc retirée) ; 2026-10-11 (ticket #600 : les bornes de `r` et `CALIBRATE` contre notre carte, `ext=1`) ; 2026-10-11 (ticket #594 : la température et l'humidité de la carte, `z`) ; 2026-10-08
**Statut** : actif — driver 0.9
**Référencé par** : `README.md` (§ Software), `7_Docs/MANUEL.md`, `6_Driver/indi/CMakeLists.txt`, `6_Driver/indi/UPSTREAM.md`, job `indi` de `.gitea/workflows/ci.yml`

Focuser INDI (Ekos) pour la carte Basilisk et un objectif E-mount. C'est le driver Pinefeat d'origine
(`pinefeat_cef` du cœur d'INDI) avec un lot « étendu » par-dessus. Les défauts du code d'origine sont notés
dans [`UPSTREAM.md`](UPSTREAM.md). Sur décision de l'humain, notre fourche en corrige une partie : la réponse
de 15 caractères (2026-10-04), puis les points R9 de l'audit statique du 2026-10-06 (2026-10-07, plan de
robustesse de l'atelier), puis, contre notre carte seulement (`ext=1`), les bornes de `r` (L09-03, 2026-10-11) ; les
autres restent tels quels. `UPSTREAM.md` dit lesquels.

## Base : le driver Pinefeat d'origine

`pinefeat_cef.h` et `pinefeat_cef.cpp` sont les fichiers d'`indilib/indi`,
`drivers/focuser/pinefeat_cef.{h,cpp}`, au commit `1dd9b34f24df6fd74ed45be17823fc937e192b47`. Leurs
propriétés, leurs noms et leur comportement sur `v f r e a d s c` sont repris tels quels, sauf, contre notre carte
(`ext=1`), les bornes de `r` et `CALIBRATE` (« Les bornes de `r` », plus bas) :
`FIRMWARE_VERSION`, `FOCUS_DISTANCE`, `CALIBRATE`, `ABS_APERTURE`, `REL_APERTURE`, `RANGE_APERTURE`,
et les propriétés standard des focusers INDI (position absolue et relative, `FOCUS_MAX`, vitesse).
Sans `ext=1`, `CALIBRATE` est celui d'origine : il envoie `c`, passe la position absolue et `FOCUS_MAX` à `Busy`, et
le maximum de `r` n'entre dans `FOCUS_MAX` qu'à la relecture qui suit (`UPSTREAM.md`, L09-03). La
carte ne connaît pas la distance de mise au point : `FOCUS_DISTANCE` affiche sa réponse à `d`, `-`.

Les écarts au comportement d'origine :

- **`r` est toujours lu.** L'amont ne le lit que si le mineur de `v` est au moins 3 ; ce contrôle est
  retiré.
- **Garde-fou de version.** À la connexion, le majeur de `v` doit égaler celui du driver (0). Sinon
  la connexion est refusée, avec les deux versions dans le message
  (`Board firmware 1.0 does not match driver 0.9: …`).
- **La réponse à `v` est jugée sur sa forme** (audit R5). L'ouverture du port peut redémarrer la carte, et
  les lignes de sa ROM (`ESP-ROM:…`, `SPIWP:0xee`…) arriver après l'envoi de `v`. Une ligne qui n'a pas la
  forme `<maj>.<min>` n'est pas la réponse : elle est sautée, comme une ligne `* …`, jusqu'à la réponse ou
  3 s après la commande (lue dans `EXT_BUF`, une longue ligne de la ROM est sautée entière). Sans réponse,
  la connexion essaie `v` encore, trois fois en tout ; un `v` perdu dans le redémarrage est rattrapé par le
  suivant. Sans aucune ligne de cette forme, la carte n'est pas détectée.
- **Une ligne `* …` n'est jamais une réponse.** La page de banc peut laisser la carte en `LOG ON` ou
  `LOG ALL`, qui émet des lignes `* …` (`PROTOCOL.md` § 3). Chaque commande saute celles qui arrivent
  avant sa réponse, chacune jusqu'à son `\n`, quelle que soit sa longueur, et prend la ligne suivante ;
  le saut s'arrête 3 s après la commande : une carte qui journalise sans jamais répondre échoue comme
  une carte muette. L'entrée en attente avant une commande est lue et jetée
  (l'amont la vidait par `tcflush`) : si elle finit au milieu d'une ligne, une ligne du journal coupée
  entre deux paquets USB, la suite de cette ligne est sautée aussi. Le driver n'envoie jamais `LOG OFF`
  (les majuscules ne sont pas pour un driver). Une carte Pinefeat n'émet, à notre connaissance, aucune
  ligne `*` : rien n'y change pour elle.
- **Une réponse de 15 caractères est lue en entier et terminée.** L'amont la lit dans un tampon de 16
  octets que son `\n` remplit, sans zéro final : le journal, `strstr` et `sscanf` lisent au-delà
  (`er range limits`, à `f<n>` ou `f±n` hors des bornes). Les tampons des réponses de l'amont ont un
  octet de plus, et chaque réponse est terminée par un zéro ; le `\n` y reste rangé, comme avant
  (`UPSTREAM.md`), et ce qui l'affiche le retire (ci-dessous).
- **`a` est lu dans `EXT_BUF`, comme `t`** (audit S2). La carte répond à `a` en 15 caractères au plus ;
  une carte plus ancienne rendait `er nocap aperture` (17), que le tampon de 16 octets de l'amont coupait
  en erreur de lecture, la connexion et chaque relecture échouant avec lui.
- **Les points R9 de l'audit** (`UPSTREAM.md`) : `nc` comparé en entier (L09-05) ; les déplacements
  formatés `%u` (L09-08) ; l'état relu une fois par seconde, `e` et, à l'arrêt, `e f r d a` (L09-09 :
  l'amont les envoyait à chaque passage du minuteur, 50 ms) ; `FIRMWARE_VERSION`, `FOCUS_DISTANCE`,
  `RANGE_APERTURE` et le journal sans la fin de ligne de la réponse.

## Le lot étendu : seulement si `t` porte `ext=1`

Après `v`, le driver envoie `t`. Si la ligne porte le jeton `ext=1` (comparé en entier), le lot est
défini ; sinon (`er` d'une carte Pinefeat, pas de réponse, pas de jeton), le driver est l'amont plus le
garde-fou de version : aucune propriété du lot, aucune de ses lettres n'est envoyée, pas même si un
client écrit une de ses propriétés par son nom.

| Fonction | Lettres | Propriété | Note |
|---|---|---|---|
| ouverture relue | `o` | `APERTURE` (`FNUMBER`, lecture) | relue chaque seconde à l'arrêt : suit la bague de l'objectif ; `Idle` si l'objectif ne sert pas d'ouverture |
| arrêt | `q` | `FOCUS_ABORT_MOTION` | la capacité « abort » standard (`FOCUSER_CAN_ABORT`), posée au handshake |
| marque | `j` `js` `jg` `jx` | `FOCUS_MARK` (`MARK`, lecture) ; `FOCUS_MARK_CTL` (`MARK_SET`, `MARK_GOTO`, `MARK_CLEAR`) | rangée dans la carte ; la valeur est relue chaque seconde à l'arrêt (le bouton du fût la change aussi) |
| LED de statut | `k` `k<n>` | `LED_BRIGHTNESS` (`LEVEL`, 0 à 100 %, onglet Options) | la carte ne la garde pas : rangée dans la config INDI et renvoyée à chaque connexion ; la valeur affichée est celle que `k` relit |
| fin d'un déplacement | `g` | la position absolue (et relative, pour un déplacement relatif) | lue quand `e` répond `n` après un déplacement lancé par le driver (absolu, relatif, `jg`), puis pendant la fenêtre de l'arrêt (ci-dessous) |
| redémarrer l'objectif | `b` | `LENS_REBOOT` (`REBOOT`) | le dernier recours |
| température et humidité de la carte | `z` | `FOCUS_TEMPERATURE` (`TEMPERATURE`, °C, lecture) ; `FOCUS_HUMIDITY` (`HUMIDITY`, %RH, lecture) | relues chaque seconde à l'arrêt ; **définies seulement tant que la carte les connaît** (ci-dessous). Avec une carte dont le firmware ne sert pas `z` (`er nocap`, plus envoyée de la connexion), ni l'une ni l'autre n'existe |
| onglet « Lens » | `i` `w` `n` | `LENS_INFO` (`LENS_NAME`, `LENS_FIRMWARE`, `LENS_MODULE`, lecture) | relu chaque seconde à l'arrêt ; une réponse `er …` laisse son champ vide (ci-dessous). Une lettre qui répond `er nocap` n'est pas servie, dans tout état (`PROTOCOL.md` § 2) : elle n'est plus envoyée de la connexion. Le firmware actuel de la carte ne sert ni `w` ni `n` : chacune part une fois par connexion, `LENS_FIRMWARE` et `LENS_MODULE` sont vides |

Le protocole est celui de `PROTOCOL.md`.

**La fin d'un déplacement se lit par `g`**, jamais par `DEBUG` (les majuscules ne sont pas pour un
driver). Quand `e` répond `n` après un déplacement lancé par le driver, il lit `g` :

| `g` | état de la position |
|---|---|
| `ok` | `Ok` |
| `aborted` (le `q` de `FOCUS_ABORT_MOTION`, seul à l'envoyer : le bouton du fût n'en envoie pas) | `Idle`, l'état qu'`INDI::Focuser` donne à un abandon réussi (libindi 1.9.9 et 2.1.0) : une annulation n'est pas un échec |
| `stall`, `unconfirmed`, `-`, pas de réponse | `Alert`, et le résultat au journal du driver |

Une position que `f` ne lit pas (`nc`, `er fault`) donne `Alert`, quel que soit `g`. Cet état reste
jusqu'au déplacement suivant : la relecture de la position à l'arrêt ne le remet pas à `Ok` (l'amont le
fait, à chaque relecture).

**La fenêtre de l'arrêt.** Après `stall` ou `aborted`, le driver relit `g` à chaque passage du minuteur
pendant 5 s (`unconfirmed` arrive au plus 4,5 s après le premier envoi du `0x1C`, `PROTOCOL.md` § 3). S'il
passe à `unconfirmed`, la position passe à `Alert`, et le journal dit `Stop not confirmed: the motor state
is unknown.` ; s'il change autrement, ou à la fin des 5 s, le suivi s'arrête et l'état reste. Un
déplacement lancé dans la fenêtre l'arrête aussi.

**Tout `Busy` posé par le lot étendu se termine**, quand `e` répond `n` : un déplacement par `g` (ci-dessus),
`b` par `f` (`Ok` s'il rend une position, `Alert` sinon). Un objectif retiré ou en `FAULT` (`e` à `n`, `f`
en erreur) donne donc `Alert`, jamais un `Busy` sans fin. `CALIBRATE` n'en pose aucun contre notre carte (ci-dessous) ;
sans `ext=1`, son `Busy` est celui de l'amont (`UPSTREAM.md`).

**Les bornes de `r` sont la plage de la position.** Contre notre carte, ce sont celles que l'objectif publie
(`PROTOCOL.md`, ligne `r`) : rien n'est mesuré, il n'y a rien à calibrer (décision de l'humain, ticket #600). Le driver
lit les deux nombres de `r` et pose, à la connexion et à chaque lecture réussie (une fois par seconde à l'arrêt, après
`f`), la plage de la position absolue et celle de `FOCUS_MAX` sur `<min>..<max>`, et la valeur de `FOCUS_MAX` sur
`<max>` ; il ne les envoie aux clients que si elles ont changé, et le journal le dit (`Lens focus range 13873-30738.`).
Une cible hors de la plage, absolue ou relative, est refusée par `INDI::Focuser` avant d'atteindre la carte (`Requested
position out of bound…`, libindi 1.9.9). Un nouvel objectif apporte ses bornes à la première lecture réussie ; une
lecture qui échoue (objectif retiré, `nc`) ne change rien : les dernières bornes connues restent. Chaque connexion
repart de la plage d'origine, 0 à 32767, jusqu'à sa première lecture. Un `FOCUS_MAX` écrit par un client, dont
`INDI::Focuser` fait une plage depuis 0, est remis aux bornes de `r` à la lecture suivante. **`CALIBRATE` rend ces
bornes** : il relit `r` et les pose, sans `Busy`, sans envoyer `c` (la carte y répond `ok` sans rien faire), sans rien
déplacer ; `Ok`, ou `Alert` si `r` échoue, les bornes connues gardées. Contre un adaptateur Pinefeat (sans `ext=1`),
dont `r` porte une plage calibrée, rien de cela : les bornes et `CALIBRATE` sont ceux de l'amont (`UPSTREAM.md`,
L09-03).

Joué une fois, hors de la CI (2026-10-11, un essai du ticket #600 sur le runner, non gardé dans le dépôt) : sous
`indiserver` (libindi 1.9.9), contre une fausse carte sur un pty. Avec `ext=1` : la plage 13873..30738 posée à la
connexion ; un goto à 5000 et un déplacement relatif au-delà du maximum refusés, sans qu'aucun `f` parte ; `CALIBRATE`
sans `c` ni déplacement, `FOCUS_MAX` sur le maximum de `r`, pas sur la position ; l'objectif retiré (`nc`), les bornes
gardées et `CALIBRATE` en `Alert` ; un nouvel objectif (1000-9000) appliqué à la lecture suivante ; un `FOCUS_MAX` écrit
par le client, remis à la lecture suivante ; reconnecté à une carte sans `ext=1`, la plage 0..32767. Sans `ext=1` : les
états d'`ABS_FOCUS_POSITION`, `FOCUS_MAX` et `CALIBRATE` et les commandes envoyées, étape par étape, identiques à ceux
du driver d'avant. Le même essai a compilé et lié le driver contre libindi 2.2.5, construite depuis ses sources.

**Une erreur n'est pas une valeur.** Dans `FOCUS_MARK` et `LENS_INFO`, une réponse `er …` laisse son
champ vide. L'état de la propriété : `Alert` si une lettre est refusée pour l'instant (`er nolens`,
`er busy boot`, `er fault` : la valeur existe, elle ne se lit pas) ; sinon `Ok` si un champ porte une
valeur ; sinon `Idle` (`er nocap` : la carte ne la sert pas, comme `w` et `n` aujourd'hui).

**La température et l'humidité ne sont jamais publiées inconnues.** `FOCUS_TEMPERATURE`, élément `TEMPERATURE`, en °C,
en lecture seule, est la propriété de convention que lit Ekos, pour refocaliser sur un écart de température (« Refocus
if ΔT° > ») et pour l'Adaptive Focus ; il prend toute valeur publiée telle quelle, sans regarder l'état de la propriété.
`FOCUS_HUMIDITY` (`HUMIDITY`, %RH, lecture seule) est propre à ce driver : INDI n'a pas de convention pour l'humidité
d'un focuser ; elle n'est qu'affichée. Les deux viennent de la même réponse de `z` (`21.37 48.02`) : la carte rend `-`
quand elle ne les connaît pas (pas encore lues, capteur muet, lecture fausse, bus bloqué, dernière lecture de plus de
3 s ; `PROTOCOL.md` § 2). Le driver ne publie donc les deux propriétés que tant que la carte rend deux nombres : il les
définit à la première valeur, les met à jour ensuite, et les **supprime** dès que la carte rend `-`, ne répond pas, ou
répond autre chose ; un client ne voit alors aucun nombre, plutôt qu'un nombre ancien qu'Ekos prendrait pour frais. Le
journal du driver le dit (`Board temperature and humidity known.` / `… unknown: not published.`). Le driver ne calcule
rien (pas de point de rosée) et ne compense rien.

Joué une fois, hors de la CI (2026-10-11, un essai du ticket #594 sur le runner, non gardé dans le dépôt) : sous
`indiserver` (libindi 1.9.9), contre une fausse carte sur un pty dont `z` change : `21.37 48.02` → les deux propriétés
lues par `indi_getprop` (21,37 et 48,02) ; `-` → les deux absentes ; `-3.05 87.50` → de nouveau là ; `er nocap` →
absentes, et `z` n'est plus envoyée.

## Compilation

```
sudo apt install libindi-dev cmake make g++ pkg-config
cd indi && mkdir build && cd build && cmake .. && make && sudo make install
```

`bash 6_Driver/indi/build.sh`, depuis n'importe quel répertoire, compile et lie dans `6_Driver/indi/build` (recréé à chaque fois), sans
installer.

Vérifié en CI (job `indi` de `.gitea/workflows/ci.yml`) : compilé et lié contre libindi 1.9.9
(Ubuntu 24.04). Le fichier amont est écrit pour libindi 2.x ; sous 1.9.9, les propriétés de la classe
de base `INDI::Focuser` n'ont pas encore les accesseurs qu'il emploie : `pinefeat_cef.h` les leur
donne, sous `#if INDI_VERSION_MAJOR < 2`, sans toucher au reste ; un champ y est atteint par une petite
vue qui tient l'`INumber` lui-même (`NumberField`), pas par une conversion en `WidgetView<INumber>`, qu'il
n'est pas (audit R9).

Joué une fois, hors de la CI (2026-10-08, un essai du ticket #545 sur le runner, non gardé dans le dépôt) : sous
`indiserver` (libindi 1.9.9), contre une fausse carte sur un pty — la ROM puis la réponse à `v`, le premier `v` perdu,
une ligne courte de la ROM en tête (le driver d'avant y refusait la connexion) : connecté, la version lue, `a` de 17
caractères lu en entier, `w` et `n` une fois, `e` une fois par seconde. Aucune suite ne joue le driver. `pkg-config` est nécessaire :
`libindi-dev` ne fournit pas de `FindINDI.cmake`.

Puis dans Ekos : Focusers → « Basilisk E-mount Lens », port série de la carte, 115200. Le nom
d'installation est `indi_basilisk_focus` : il ne rentre pas en collision avec `indi_pinefeat_cef_focus`.

## Licence

Le driver est sous LGPL 2.1 ou ultérieure, comme le fichier d'origine, dont l'en-tête (Pinefeat LLP,
puis le Moonlite de Jasem Mutlaq) est conservé, suivi de la ligne des modifications. Le texte intégral
de la LGPL 2.1 est dans [`LICENSE`](LICENSE), à côté de ce fichier. Il ne couvre que `6_Driver/indi/`.
