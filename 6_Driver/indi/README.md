# Driver INDI `pinefeat_cef` — l'amont, ses correctifs, l'extension Basilisk

**Date** : 2026-09-24
**Dernière révision** : 2026-10-23 (ticket #694 : le dernier redémarrage de la carte, `x`, dans `BOARD_RESTART`, et
l'avertissement d'un redémarrage anormal, port ouvert compris) ; 2026-10-23 (ticket #672 : chaque commande rend la main au plus `CEF_TIMEOUT` après son envoi,
quoi que le port envoie : un correctif de plus, `0007`, la ligne en retard devient `0008` ; les flux sans fin joués) ;
2026-10-20 (ticket #648 : le driver en 1.2, l'extension ne monte plus la version, vérifiée
après chaque série ; le firmware en 1.3) ; 2026-10-19 (ticket #645, reprise de la revue de la PR #646 : le relatif et les fins illisibles
sous `ext=1` joués, le symptôme de chaque correctif vérifié sur l'amont, la réponse à `v` hors forme fixée) ; 2026-10-19
(ticket #645 : le driver redevient l'amont, plus deux séries de patchs proposables à
indilib, les correctifs puis l'extension ; le nom, l'identité et la numérotation de l'amont ; INDI 2 ; démontré en CI) ;
2026-10-19 (ticket #641 : le point de rosée ou de givre de la carte) ; 2026-10-11 (tickets #594, #600, #606) ; 2026-10-08
**Statut** : actif — l'amont `1dd9b34f24df6fd74ed45be17823fc937e192b47`, puis `patches/fixes`, puis `patches/basilisk`
**Référencé par** : `README.md` (§ Software), `7_Docs/MANUEL.md`, `CLAUDE.md` (§ Gotchas), `6_Driver/indi/UPSTREAM.md`,
`6_Driver/indi/CMakeLists.txt`, `6_Driver/indi/test.sh`, job `indi` de `.gitea/workflows/ci.yml`

Le driver de ce répertoire est le focuser Pinefeat d'INDI, `pinefeat_cef`, pris dans `indilib/indi`, enrichi de deux
séries de patchs, chacune proposable telle quelle à indilib comme une pull request distincte :

1. **`patches/fixes/`, les correctifs** : huit défauts de l'amont, un patch par défaut, et la version du driver
   montée à 1.2 ; ils valent pour tout contrôleur ;
2. **`patches/basilisk/`, l'extension Basilisk** : un patch, par-dessus, qui ajoute le lot de notre carte, défini
   seulement pour une carte qui l'annonce (`ext=1`) ; elle ne change pas la version, 1.2 (décision de l'humain).

Il n'y a pas de troisième couche : `pinefeat_cef.h` et `pinefeat_cef.cpp` sont exactement l'amont, puis les correctifs,
puis l'extension, et la CI le vérifie à chaque run (§ « Ce que la CI démontre »). Sur décision de l'humain
(2026-10-19), le driver garde le nom, l'identité et la numérotation de l'amont : le binaire `indi_pinefeat_cef_focus`,
le périphérique « Pinefeat EF Lens Controller », la version de `setVersion` montée comme le fait indilib : 1.2 par
les correctifs, en supposant qu'indilib les accepte ; l'extension ne la monte pas (décision de l'humain, 2026-10-20). Il n'a pas
de garde-fou de version : une carte se connecte quelle que soit la sienne. Rien n'est soumis chez indilib : l'humain
contactera Pinefeat avant d'ouvrir les deux pull requests, dont les descriptions sont prêtes dans
`patches/fixes/PULL_REQUEST.md` et `patches/basilisk/PULL_REQUEST.md`.

## L'amont

`drivers/focuser/pinefeat_cef.{h,cpp}` d'`indilib/indi` au commit `1dd9b34f24df6fd74ed45be17823fc937e192b47`, la
v2.2.5 (driver 1.1), identiques au `master` d'indilib le 2026-10-19. Leurs identifiants de blob git, que `test.sh`
vérifie : `8ba29fef695e36f8c2267e27fbfea8a28556e03b` (`.cpp`), `4d99784cafa6f8375a65a096b11f42e1dc200449` (`.h`).
Ses défauts, ceux que la série corrige et ceux qui restent, sont dans [`UPSTREAM.md`](UPSTREAM.md).

## Les correctifs — `patches/fixes/`

Un correctif ne change que ce que décrit le défaut qu'il nomme, vers le comportement que l'amont visait ; tout autre
changement visible est de l'extension (arbitrage de l'architecte, ticket #645). L'entrée d'`UPSTREAM.md` de chacun dit
le défaut, et pourquoi il est un correctif.

| Patch | Défaut corrigé (`UPSTREAM.md`) |
|---|---|
| `0001` | la réponse rendue telle que lue : sans zéro final à 15 caractères, sa fin de ligne dans les textes affichés et le journal, `nc` comparé par `strcmp(…) > 0` (L09-05) |
| `0002` | `%d` pour les déplacements, des `uint32_t` (L09-08) |
| `0003` | l'ouverture affichée sans décimale (L09-07) |
| `0004` | l'état relu à chaque passage du minuteur, 50 ms, au lieu d'une fois par seconde (L09-09) |
| `0005` | un `Busy` de déplacement ou de `CALIBRATE` sans fin quand l'état ne se lit plus |
| `0006` | la même erreur de position au journal à chaque relecture, tant que l'objectif manque |
| `0007` | la réponse bornée en octets et pas en temps : `CEF_TIMEOUT` attendu pour chaque octet, un filet sans fin de ligne retient le driver des dizaines de secondes ; désormais une seule échéance, `CEF_TIMEOUT` après l'envoi, pour toute la réponse |
| `0008` | le reste d'une ligne en retard pris pour la réponse suivante (L09-04, en partie), lu dans l'échéance de `0007` |
| `0009` | la version du driver, 1.2 |

Contre un adaptateur Pinefeat, sans `ext=1`, le driver de ce répertoire est l'amont avec ces seules différences.

## L'extension — `patches/basilisk/`

Notre carte sert le protocole Pinefeat sur `v f r e a d s c` (`7_Docs/PROTOCOL.md`), et le driver d'origine la pilote
telle quelle. L'extension ajoute ce que la carte sert de plus. **Elle est optionnelle** : rien d'observable ne change
pour un Pinefeat (arbitrage de l'architecte, ticket #645), le focus marche sans elle.

**Le lot n'existe que si `t` porte `ext=1`.** Après `v`, le driver envoie `t`. Si la ligne porte le jeton `ext=1`
(comparé en entier), le lot est défini ; sinon (`er` d'un adaptateur Pinefeat, pas de réponse, pas de jeton), aucune
propriété du lot, aucune de ses lettres n'est envoyée, pas même si un client écrit une de ses propriétés par son nom.

| Fonction | Lettres | Propriété | Note |
|---|---|---|---|
| ouverture relue | `o` | `APERTURE` (`FNUMBER`, lecture) | relue chaque seconde à l'arrêt : suit la bague de l'objectif ; `Idle` si l'objectif ne sert pas d'ouverture |
| arrêt | `q` | `FOCUS_ABORT_MOTION` | la capacité « abort » standard (`FOCUSER_CAN_ABORT`), posée à la connexion |
| marque | `j` `js` `jg` `jx` | `FOCUS_MARK` (`MARK`, lecture) ; `FOCUS_MARK_CTL` (`MARK_SET`, `MARK_GOTO`, `MARK_CLEAR`) | rangée dans la carte ; relue chaque seconde à l'arrêt (le bouton du fût la change aussi) |
| LED de statut | `k` `k<n>` | `LED_BRIGHTNESS` (`LEVEL`, 0 à 100 %, onglet Options) | la carte ne la garde pas : rangée dans la config INDI et renvoyée à chaque connexion ; la valeur affichée est celle que `k` relit |
| fin d'un déplacement | `g` | la position absolue (et relative, pour un déplacement relatif) | lue quand `e` répond `n` après un déplacement lancé par le driver (absolu, relatif, `jg`), puis pendant la fenêtre de l'arrêt (ci-dessous) |
| redémarrer l'objectif | `b` | `LENS_REBOOT` (`REBOOT`) | le dernier recours |
| nom de l'objectif | `i` | `LENS_INFO` (`LENS_NAME`, lecture, onglet « Lens ») | relu chaque seconde à l'arrêt |
| température, humidité, point de rosée ou de givre | `z` | `FOCUS_TEMPERATURE` (`TEMPERATURE`, °C) ; `FOCUS_HUMIDITY` (`HUMIDITY`, %RH) ; `FOCUS_DEW_POINT` (`DEW_POINT`, °C), en lecture | relues chaque seconde, à l'arrêt comme en mouvement ; **définies seulement tant que la carte les connaît** (ci-dessous) |
| bornes de la position | `r` | la plage de la position absolue et de `FOCUS_MAX` ; `CALIBRATE` | ci-dessous |
| dernier redémarrage de la carte | `x` | `BOARD_RESTART` (`CAUSE`, lecture, onglet Info) | le jeton de la cause ; lu à la connexion, puis chaque seconde, à l'arrêt comme en mouvement ; un redémarrage anormal est un avertissement au journal (ci-dessous) |

**La fin d'un déplacement se lit par `g`**, jamais par `DEBUG` (les majuscules ne sont pas pour un driver) :

| `g` | état de la position |
|---|---|
| `ok` | `Ok` |
| `aborted` (le `q` de `FOCUS_ABORT_MOTION`, seul à l'envoyer : le bouton du fût n'en envoie pas) | `Idle`, l'état qu'`INDI::Focuser` donne à un abandon réussi : une annulation n'est pas un échec |
| `stall`, `unconfirmed`, `-`, pas de réponse | `Alert`, et le résultat au journal du driver |

Une position que `f` ne lit pas (`nc`, `er fault`) donne `Alert`, quel que soit `g`. Cet état reste jusqu'au
déplacement suivant : la relecture de la position à l'arrêt ne le remet pas à `Ok`. Après `stall` ou `aborted`, le
driver relit `g` à chaque passage du minuteur pendant 5 s (`unconfirmed` arrive au plus 4,5 s après le premier envoi du
`0x1C`, `PROTOCOL.md` § 3) : s'il passe à `unconfirmed`, la position passe à `Alert` et le journal dit `Stop not
confirmed: the motor state is unknown.` ; s'il change autrement, ou à la fin des 5 s, le suivi s'arrête. Un déplacement
lancé dans la fenêtre l'arrête aussi. **Tout `Busy` posé par le lot se termine** quand `e` répond `n` : un déplacement
par `g`, `b` par `f` (`Ok` s'il rend une position, `Alert` sinon).

**Le dernier redémarrage de la carte se lit par `x`**, jamais par `DEBUG` : `x` rend sa cause et le temps écoulé depuis,
en ms (`PROTOCOL.md`, ligne `x`). `BOARD_RESTART` porte le jeton de la cause, `Alert` pour une cause anormale, `Ok`
sinon. La carte peut redémarrer sans que le port se ferme (on ne sait pas si l'USB du C3 survit à un panic), et elle
reprend alors son objectif (`PROTOCOL.md` § 7) : le focus bouge. Un redémarrage, c'est la première réponse de la
connexion, puis une réponse dont la cause a changé ou dont le temps est plus petit qu'il ne peut l'être sans
redémarrage (il repart de 0 à chaque redémarrage : deux redémarrages de même cause se distinguent, même rapprochés).
Sans redémarrage, le temps de la carte a grandi depuis la réponse précédente d'au moins le temps de l'hôte entre
l'arrivée de cette réponse et l'envoi de la commande suivante, quel que soit le retard de l'une ou l'autre : la carte
lit son horloge après avoir reçu la commande et avant que sa réponse arrive. La marge est de 10 ms et 0,1 % de ce
temps : la carte tronque son temps à la milliseconde, et son horloge et celle de l'hôte ne battent pas exactement au
même rythme. `x` est lu chaque seconde même pendant que `e` répond `y`, pour que l'avertissement arrive pendant la
reprise, que la carte fait avec `e` à `y`, et non après elle ; `z` aussi, pour qu'une mesure que la carte ne connaît
plus (périmée au bout de 3 s, `PROTOCOL.md`, ligne `z`) ne reste pas publiée pendant un long déplacement. Le reste du
lot est lu à l'arrêt seulement.

| Cause | Journal, une fois par redémarrage |
|---|---|
| `panic`, `int_wdt`, `task_wdt`, `wdt`, `brownout`, `unknown` | un avertissement qui la nomme en clair : `The board restarted: its firmware crashed (panic).` |
| `poweron`, `sw`, `usb` (l'ouverture du port peut redémarrer la carte), `jtag` | rien |

**Les bornes de `r` sont la plage de la position.** Contre notre carte, ce sont celles que l'objectif publie
(`PROTOCOL.md`, ligne `r`) : rien n'est mesuré, il n'y a rien à calibrer (décision de l'humain, ticket #600). À chaque
lecture réussie de `r` (la connexion, puis une fois par seconde à l'arrêt), le driver pose la plage de la position
absolue et celle de `FOCUS_MAX` sur `<min>..<max>`, et la valeur de `FOCUS_MAX` sur `<max>` ; il ne les envoie aux
clients que si elles ont changé, et le journal le dit (`Lens focus range 13873-30738.`). Une cible hors de la plage est
refusée par `INDI::Focuser` avant d'atteindre la carte. Une lecture qui échoue ne change rien : les dernières bornes
connues restent. Chaque connexion repart de la plage de l'amont, 0 à 32767. **`CALIBRATE` rend ces bornes** : il relit
`r` et les pose, sans `Busy`, sans envoyer `c` (la carte y répond `ok` sans rien faire), sans rien déplacer ; `Ok`, ou
`Alert` si `r` échoue. Contre un adaptateur Pinefeat, rien de cela : les bornes et `CALIBRATE` sont ceux de l'amont,
parce que personne ici ne sait ce que rend son `r` avant une calibration (`UPSTREAM.md`, L09-03).

**Le mineur de `v` décide toujours si `r` est lu** : c'est la règle de l'amont, que l'extension ne contourne pas. Notre
firmware répond `1.3` (décision de l'humain, pour cette règle), `r` est lu. Une carte dont le mineur serait sous 3 ferait perdre les bornes à ce driver aussi,
extension comprise : la plage resterait celle de l'amont, et `CALIBRATE` répondrait `Ok` sans rien lire.

**Une erreur n'est pas une valeur.** Dans `FOCUS_MARK` et `LENS_INFO`, une réponse `er …` laisse le champ vide ; l'état
est `Alert` si la valeur existe mais ne se lit pas pour l'instant (`er nolens`, `er busy boot`, `er fault`), `Idle` si
la carte ne la sert pas (`er nocap` : pas d'identité d'objectif pour la marque), `Ok` avec une valeur.

**La température, l'humidité et le point de rosée ne sont jamais publiés inconnus.** `FOCUS_TEMPERATURE` est la
propriété de convention que lit Ekos, pour refocaliser sur un écart de température et pour l'Adaptive Focus ; il prend
toute valeur publiée telle quelle, sans regarder l'état de la propriété. `FOCUS_HUMIDITY` et `FOCUS_DEW_POINT` sont
propres à ce driver, affichés seulement. Les trois viennent de `z` (`21.37 48.02 9.91`) : la température, l'humidité,
puis le seuil de condensation que la carte calcule, le point de rosée s'il est positif ou nul, le point de givre s'il
est négatif ; le driver le publie tel quel, sans rien calculer ni compenser. La carte rend `-` seul quand elle ne connaît
pas la mesure, et le seuil `-` quand elle ne le définit pas (`PROTOCOL.md` § 2). Chaque propriété n'existe que tant que
la carte en rend la valeur : définie à la première, mise à jour ensuite, **supprimée** dès que la carte ne la rend plus,
ne répond pas, ou répond autre chose ; un client ne voit alors aucun nombre plutôt qu'un nombre ancien. Le journal le
dit (`Board temperature and humidity known.` / `… unknown: not published.`, `Board dew or frost point known.` / `…`).

### Ce qu'un Pinefeat en voit

Avant que `t` réponde, le driver ne sait pas à qui il parle ; l'extension a donc trois effets sur tout contrôleur, et
c'est tout ce qu'un adaptateur Pinefeat en voit :

- **`t`, une fois par connexion.** Un adaptateur Pinefeat y répond `er` (`PROTOCOL.md` § 2 ; c'est ce qui le
  distingue) ; s'il ne répondait rien, la connexion attendrait 3 s de plus et le journal dirait une erreur de lecture.
- **Une ligne qui commence par `*` n'est jamais une réponse** : la page de banc peut laisser notre carte en `LOG ON`, qui
  émet des lignes `* …` (`PROTOCOL.md` § 3). Elle est sautée jusqu'à sa fin de ligne, quelle que soit sa longueur, et la
  réponse est la ligne suivante, jusqu'à 3 s après la commande (l'échéance de `0007`, que rien ne prolonge : une ligne
  `*` qui ne finit pas fait échouer la commande au bout de 3 s). Aucune réponse de Pinefeat ne commence par `*`.
- **À `v`, seule une ligne de la forme d'une version est la réponse** (`<majeur>.<mineur>`, ce que l'amont lit déjà
  comme version), lue dans un grand tampon. L'ouverture du port peut redémarrer notre carte, et les lignes de sa ROM
  (`ESP-ROM:…`, `SPIWP:0xee`…) arrivent après l'envoi de `v` : sautées jusqu'à la version ou 3 s après la commande ;
  l'amont essaie `v` trois fois. Sans elles, notre carte retrouve le refus de connexion de l'audit R5. Un adaptateur
  Pinefeat répond à `v` par sa version : il n'y voit rien. S'il répondait autre chose, il ne se connecterait plus.

Ces deux dernières sont de l'extension, pas des correctifs : elles ne corrigent aucun défaut de l'amont face à un
Pinefeat.

## Compiler, installer à côté du paquet

```
sudo apt install cmake make g++ pkg-config libindi-dev     # libindi 2 : le PPA d'INDI, ppa:mutlaqja/ppa
cmake -S 6_Driver/indi -B build && make -C build
sudo make -C build install
```

`bash 6_Driver/indi/build.sh`, depuis n'importe quel répertoire, fait les deux premières lignes dans
`6_Driver/indi/build`, sans installer. Le driver se construit contre libindi 2 seulement (2.2.5 en CI) ; contre 1.9.9,
`cmake` le refuse.

**Le binaire porte le nom de l'amont, `indi_pinefeat_cef_focus`, que le paquet `indi-bin` installe aussi**, dans
`/usr/bin`. `make install` installe le nôtre sous le préfixe de `cmake`, `/usr/local` par défaut : à côté, sans écraser
celui du paquet, ni être écrasé par une mise à jour du paquet. Ne pas passer `-DCMAKE_INSTALL_PREFIX=/usr` : ce serait
remplacer en silence le fichier du paquet. `indiserver`, que lance Ekos, trouve un driver par le `PATH`, où
`/usr/local/bin` vient avant `/usr/bin` sur une installation Ubuntu ordinaire : `command -v indi_pinefeat_cef_focus` dit
lequel sera pris. Pour revenir au driver du paquet, `sudo rm /usr/local/bin/indi_pinefeat_cef_focus`. Dans Ekos, le
driver est déjà déclaré par INDI (`drivers.xml`) : dans le profil, Focuser → « Pinefeat CEF » ; le périphérique se nomme
« Pinefeat EF Lens Controller » dans le panneau INDI ; port série de la carte, la vitesse est sans effet (un port USB).

## Ce que la CI démontre

Le job `indi` de `.gitea/workflows/ci.yml` (Ubuntu 26.04, libindi-dev 2 du PPA d'INDI) joue `build.sh`, puis
`test.sh`, qui, dans `6_Driver/indi/build-test` :

1. **retire les deux séries** des fichiers de ce répertoire, dans l'ordre inverse, et vérifie que restent les fichiers
   de l'amont (leurs identifiants de blob) ; puis **applique chaque série par `git am`** sur l'état qui la précède, et
   vérifie que l'amont, puis les correctifs, puis l'extension rendent les fichiers de ce répertoire, octet pour octet ;
2. vérifie que **les lignes touchées par chaque série suivent le style d'indilib** : passées à `astyle` avec les options
   du `README.md` d'indilib, comme le fait son `scripts/check-codestyle.sh` ;
3. **compile les trois états**, l'amont, l'amont et les correctifs, le driver, et les joue par
   [`test/test_driver.py`](test/test_driver.py) : chaque binaire lancé seul, parlé en XML INDI sur son entrée et sa
   sortie standard, contre un faux contrôleur sur un pty.
   - **Contre un faux Pinefeat** (`v f r e a d s c`, `er` à toute autre lettre), les trois états. La version que chacun
     déclare (`DRIVER_INFO`) : 1.1 l'amont, 1.2 après les correctifs, 1.2 encore après l'extension. Les contrôles du
     comportement de l'amont (la connexion à `v` = 1.3, 1.2, 0.9 et 2.0, `r` lu selon le mineur ; le goto, le
     déplacement relatif, la vitesse, l'ouverture, `CALIBRATE` d'origine ; aucune propriété du lot et aucune de ses
     lettres, même écrites par leur nom ; libindi 2 refuse lui-même une propriété non définie,
     avant le driver, comme pour l'amont) passent sur les trois : sans `ext=1`, l'extension ne se voit que par un `t` par
     connexion. Une exception, fixée par son propre contrôle : une réponse à `v` qui n'a pas la forme `<maj>.<min>`
     (`abc`) est prise pour la version par l'amont et par les correctifs, et le contrôleur n'est pas détecté par le
     driver (§ « Ce qu'un Pinefeat en voit »). Chaque contrôle d'un correctif passe sur les correctifs et sur le driver,
     et échoue sur l'amont **avec le symptôme qu'il déclare** (un fragment de son message d'échec : `'1.3\n'`,
     « lens is not attached » pour `oops`, `%.f`, deux `e` à moins de 0,1 s, l'attente d'`Alert` qui expire, l'erreur
     journalisée plus d'une fois, l'erreur d'un `e` qui ne vient pas dans la borne sous un filet d'un octet toutes les
     1,5 s, `e` suivi de `e` après la ligne en retard) : un échec d'une autre cause est un échec du jeu. Seul celui de la réponse de 15 caractères n'est pas joué sur l'amont, dont la lecture au-delà du tampon ne
     donne pas de symptôme certain. La ligne en retard est écrite après la dernière commande d'une relecture, et sa
     suite arrive devant le `e` de la suivante : la confusion avec la cadence de l'amont (L09-09) est écartée. **Un flux
     sans fin de ligne** (32 octets toutes les 5 ms, sans fin) après un `e`, joué sur les trois états : cette commande
     et les deux suivantes échouent chacune, leur `Serial read error` lue au plus tard 5 s après que le faux contrôleur
     a reçu la commande (`CEF_TIMEOUT`, et 2 s pour que l'erreur traverse le tube et l'analyseur XML jusqu'au test) ; le
     flux arrêté, les relectures reprennent (`f` envoyé). L'amont le passe (il déborde `CEF_BUF` aussitôt) : c'est la
     non-régression de `0008`, dont la suite d'une ligne en retard est lue dans l'échéance de `0007`.
   - **Contre une fausse Basilisk** (`ext=1`), le driver : la connexion à travers le redémarrage (les lignes de la ROM
     au premier `v`) et un journal `LOG ON` devant chaque réponse, une ligne coupée autour d'une commande ; le lot
     défini, `w` et `n` jamais envoyées ; les bornes de `r` et `CALIBRATE` ; la fin d'un déplacement absolu **et
     relatif** par `g` (`ok`, `stall`, `aborted` par `q`, puis `unconfirmed`, sur `ABS_FOCUS_POSITION` comme sur
     `REL_FOCUS_POSITION`) ; une fin dont `f` ne se lit plus, `Alert` malgré `g` à `ok`, et un redémarrage (`b`) après
     lequel `f` ne répond pas, `Alert` ; la marque ; la LED, posée, sauvée dans la config INDI et renvoyée (`k40`,
     relue) à une carte neuve à la connexion suivante ; le redémarrage ; `o`, `i`, `z` et ses formes inconnues ; `z`
     pendant un déplacement de 10 s (`e` à `y`) : une valeur nouvelle publiée, puis `-` qui retire les propriétés,
     chacun moins de 2 s après que la carte le rend, avant la fin du déplacement ; le dernier redémarrage de la carte
     (`x`) : chacune des dix causes dans `BOARD_RESTART` à la connexion, l'avertissement des six anormales, son texte
     écrit à la main, et d'elles seules, `BOARD_RESTART` en lecture seule, `DEBUG` jamais envoyé ; une reconnexion à une
     carte qui n'a pas redémarré depuis la connexion d'avant, toujours `panic`, son temps grandi : un avertissement à
     chaque connexion ; une connexion de quelques secondes sans redémarrage, sans avertissement ; puis un redémarrage
     port ouvert, suivi d'une reprise de 4 s (`e` à `y`) : l'avertissement en moins de 3 s, pendant la reprise, une
     seule fois ; un second redémarrage de même cause, un second avertissement ; deux redémarrages de même cause à une
     seconde d'écart, `panic 300` puis `panic 900` (le temps n'a pas reculé) : un avertissement chacun ; 30 s sans
     redémarrage, la dernière cause `panic`, chaque réponse retardée au hasard jusqu'à 0,4 s : aucun avertissement après
     celui de la connexion ; les lignes que l'extension saute, sans fin (des lignes de la ROM à `v`, en boucle : chacun
     des trois `v` échoue dans la même borne de 5 s et la carte n'est pas détectée ; une ligne `* …` qui ne finit jamais
     après un `e` : la commande et les deux suivantes échouent dans la borne, puis les relectures reprennent) ; puis un
     Pinefeat sur le même driver, sans lot et sur la plage de l'amont.

   Les contrôles attendent des événements (une commande reçue, un état publié), pas des durées : un runner lent ne les
   fait qu'attendre plus. Quand un contrôle vérifie une absence (aucune lettre envoyée), il attend que le driver ait
   traité tout ce qui précède : `FOCUS_SPEED` posé et relu. La cadence se juge sur l'écart entre deux `e` consécutifs
   (au moins 0,9 s), qu'une charge ne peut qu'allonger. Seule exception, la borne elle-même : une erreur doit arriver
   au plus 5 s après sa commande, `CEF_TIMEOUT` et 2 s de marge, quand les défauts qu'elle montre retiennent le driver
   des dizaines de secondes ou pour toujours.

Ce que ces faux contrôleurs font est écrit dans `test/test_driver.py` : le protocole de `PROTOCOL.md` pour la carte, la
lecture que fait l'amont de ses réponses pour le Pinefeat, des durées inventées (un déplacement dure 1 s). Aucun
adaptateur Pinefeat n'a été branché sur ce driver ; sur matériel, rien de ce qui précède n'est démontré par la CI.
`test.sh` n'est pas dans `## Test` du `CLAUDE.md` : la sonde de pertinence rejoue `## Test` dans une image où libindi 2
n'existe pas.

## Refaire une série

Les fichiers de ce répertoire et les patchs changent ensemble, dans le même commit, et `test.sh` refuse qu'ils
divergent. Pour changer un correctif ou l'extension : dans un dépôt où `drivers/focuser/pinefeat_cef.{h,cpp}` sont ceux
de l'amont (un clone d'indilib au commit nommé, ou l'arbre `build-test/upstream` que laisse `test.sh`), `git am` les
deux séries, modifier le commit voulu (`git rebase -i`), puis `git format-patch --zero-commit --no-signature` de chaque
série dans son répertoire, et copier les deux fichiers du driver ici. Le message de chaque commit est la description du
défaut corrigé ; `PULL_REQUEST.md` de chaque série se met à jour avec lui.

## Licence

Le driver est sous LGPL 2.1 ou ultérieure, comme le fichier d'origine, dont l'en-tête (Pinefeat LLP, puis le Moonlite de
Jasem Mutlaq) est conservé ; l'extension y ajoute sa ligne (zUrp Astronomics, 2026). Les fichiers sont modifiés par les
patchs de `patches/`, datés dans leur en-tête. Le texte intégral de la LGPL 2.1 est dans [`LICENSE`](LICENSE), à côté de
ce fichier ; il couvre `6_Driver/indi/`.
