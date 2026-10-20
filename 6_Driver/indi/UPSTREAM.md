# Défauts du driver Pinefeat d'origine

**Date** : 2026-10-04
**Dernière révision** : 2026-10-20 (ticket #648 : le firmware répond `1.3`) ; 2026-10-19 (ticket #645 : chaque défaut corrigé l'est par un patch de `patches/fixes/`, sur
l'amont, pour tout contrôleur ; ceux qui restent disent pourquoi) ; 2026-10-11 (ticket #600) ; 2026-10-08
**Statut** : actif — constats sur l'amont ; corrigés par `patches/fixes/0001` à `0007` : la réponse rendue telle que
lue (15 caractères, fin de ligne, L09-05), L09-08, L09-07, L09-09, le `Busy` sans fin, l'erreur répétée, L09-04 en
partie ; restent : L09-03 contre un adaptateur Pinefeat, et la part de L09-04 qu'aucun driver ne peut corriger seul
**Référencé par** : `6_Driver/indi/README.md`, `6_Driver/indi/patches/fixes/PULL_REQUEST.md`

L'amont est `drivers/focuser/pinefeat_cef.{h,cpp}` d'`indilib/indi` au commit
`1dd9b34f24df6fd74ed45be17823fc937e192b47` (v2.2.5, driver 1.1 ; `README.md`). Chaque entrée nomme la fonction du
fichier amont, le symptôme, et ce qu'en fait la série des correctifs. Les numéros `L09-…` sont ceux de l'audit statique
du 2026-10-06 (lot 9) ; les entrées qui touchent la lecture série ont été vérifiées sur `indicom.c` de libindi (1.9.9,
puis 2.2.5 : `tty_nread_section` y est la même).

**La règle de classement** (arbitrage de l'architecte, ticket #645) : un correctif ne change que ce que décrit le
défaut qu'il nomme, vers le comportement que l'amont visait. Tout autre changement visible est de l'extension
(`patches/basilisk/`). Les correctifs valent pour tout contrôleur, un adaptateur Pinefeat comme notre carte ; ils ont
remplacé, le 2026-10-19, les corrections que notre fourche faisait seulement contre notre carte ou hors de toute série
(décisions du 2026-10-04, du 2026-10-07 et du ticket #600). Chaque correctif est montré par `test/test_driver.py` : son
contrôle échoue sur l'amont et passe une fois la série appliquée (`README.md`, § « Ce que la CI démontre »).

## La réponse rendue telle que lue — corrigé par `0001`

Trois symptômes d'une même cause : `sendCommand` rend les octets que `tty_nread_section` a rangés, et `tty_nread_section`
garde le délimiteur et n'écrit pas de zéro final quand la réponse remplit le tampon.

- **Une réponse de 15 caractères n'a pas de fin de chaîne** (hors de l'audit). `sendCommand` lit au plus `CEF_BUF`
  (16) octets, `\n` compris, dans des tampons de 16 : une réponse de 15 caractères et son `\n` les remplissent, et
  `strstr`, `sscanf`, `setText` et le journal lisent au-delà. Notre carte en rend une : `er range limits`, à `f<n>` et
  `f±n` hors des bornes (`7_Docs/PROTOCOL.md` § 2). Vérifié sur libindi 1.9.9 (un pty, 2026-10-04) : la lecture de
  `er range limits\n` rend `TTY_OK`, 15 octets et le `\n`, aucun zéro derrière.
- **La fin de ligne reste dans les textes affichés** (audit R9) : `FIRMWARE_VERSION` (`1.3\n`), `FOCUS_DISTANCE`,
  `RANGE_APERTURE`, et le journal (« Detected firmware version 1.3\n. »).
- **L09-05, `ERR_NC`** : `strcmp(res, "nc") > 0` est vrai pour `nc\n` (le `\n` la classe après `nc`), mais aussi pour
  toute réponse classée après `nc` (de `o` à `z`, `nd`…), journalisée « lens is not attached » à la place de son texte.

**Correction** : `sendCommand` remplace la fin de ligne (`\n`, et un `\r` devant elle : le driver ASCOM de Pinefeat
retire les deux, `TrimEnd('\r', '\n')`, et nous ne savons pas laquelle l'adaptateur envoie) par le zéro final, dans
`CEF_BUF` ; `ERR_NC` compare la réponse entière à `nc`. Pas de tampon agrandi : quand `tty_nread_section` rend `TTY_OK`,
son dernier octet est le délimiteur. Contrôles : `pf_texts`, `pf_nc_whole` (le défaut montré sur l'amont),
`pf_reply_of_15` (pas joué sur l'amont : la lecture au-delà du tampon n'y donne pas de symptôme certain).

## L09-08 — `%d` pour des `uint32_t` — corrigé par `0002`

- **Où** : `moveFocusAbs` (`"f%d\n"`), `moveFocusRel` (`"f%s%d\n"`).
- **Symptôme** : le format ne correspond pas au type. Dans les bornes des propriétés (0 à 32767), aucun effet visible ;
  au-delà de `INT32_MAX`, la commande partirait négative. `-Wformat` ne le voit pas sans `-Wformat-signedness`.
- **Correction** : `%u`. Les commandes envoyées dans les bornes ne changent pas ; aucun contrôle ne peut le montrer.

## L09-07 — l'ouverture formatée sans décimale — corrigé par `0003`

- **Où** : `initProperties`, `ApertureAbsNP[0]` et `ApertureRelNP[0]`, format `"%.f"`.
- **Symptôme** : un client affiche f/5.6 en `6`, f/1.8 en `2`, un tiers de diaphragme relatif en `0`. La valeur envoyée
  est juste (`setApertureAbs` écrit `%.6g`), seul l'affichage arrondit.
- **Correction** : `"%.1f"`. Un correctif et pas de l'extension : seul le format change, vers ce que l'amont affiche
  (des f/). Contrôle : `pf_aperture_format`.

## L09-09 — la seconde de `lastUpdate` ne cadence rien — corrigé par `0004`

- **Où** : `TimerHit`, le constructeur, `ISNewSwitch` (`CALIBRATE`).
- **Symptôme** : `TimerHit` ne relit l'état que si une seconde s'est écoulée depuis `lastUpdate`, posé seulement à la
  construction et à `CALIBRATE`. Une seconde après, le test est vrai à chaque passage du minuteur (50 ms) : `e` vingt
  fois par seconde, et à l'arrêt `e f r d a`, cent commandes par seconde.
- **Correction** : `lastUpdate` posé à chaque relecture ; `CALIBRATE` garde son délai. Contrôle : `pf_cadence` (3 à 6 `e`
  exigés en 4 s ; l'amont en a envoyé 77 à l'essai du 2026-10-19).

## Le `Busy` d'un déplacement ou de `CALIBRATE` ne finit que par une lecture réussie — corrigé par `0005`

- **Où** : `TimerHit`, `updateProperties(pos, max, dist, aper)`, `ISNewSwitch` (`CALIBRATE`), `MoveAbsFocuser`,
  `MoveRelFocuser`.
- **Symptôme** : un déplacement pose la position `Busy`, un `CALIBRATE` accepté la position et `FOCUS_MAX`. Seul
  `updateProperties` les en sort, appelé seulement si `f`, `r`, `d` et `a` répondent tous une fois `e` à `n`. Un objectif
  retiré (`f` à `nc`) les laisse `Busy` pour toujours, et un client qui attend la fin attend toujours.
- **Correction** : une fois `e` à `n`, une lecture qui échoue passe à `Alert` celles de ces propriétés qui sont encore
  `Busy`. Contrôle : `pf_busy_ends`.

## La même erreur de position au journal à chaque relecture — corrigé par `0006`

- **Où** : `TimerHit`, `readFocusPosition`.
- **Symptôme** : sans objectif, `e` répond `n` et `f` répond `nc` : « Can't read focus position: lens is not attached. »
  à chaque relecture, tant que l'objectif manque (78 lignes en 4 s sur l'amont, à l'essai du
  2026-10-19).
- **Correction** : journalisée à sa première venue, puis seulement si la réponse change ou après une position lue ;
  chaque connexion la journalise de nouveau. Contrôle : `pf_log_once`.

## L09-04 — une réponse tardive prise pour la réponse suivante — corrigé en partie par `0007`

- **Où** : `sendCommand`.
- **Symptôme** : une réponse qui arrive après l'échéance (`CEF_TIMEOUT`, 3 s) reste en route. `tcflush`, avant chaque
  commande et après chaque réponse, ne jette que ce qui est déjà reçu : quand il coupe une réponse tardive en deux, la
  suite arrive après lui et elle est lue comme la réponse de la commande suivante. Le protocole n'a pas de corrélation
  requête-réponse.
- **Correction** : l'entrée en attente avant une commande est lue et jetée au lieu d'être vidée, ce qui dit où elle
  s'arrête ; si elle finit au milieu d'une ligne, la suite de cette ligne est lue et jetée avant la réponse. L'entrée
  n'est plus vidée après la réponse : ce qui la suit est laissé à la commande suivante, qui le jette de même. Contrôle :
  `pf_stale_line`.
- **Son coût** : si la suite d'un fragment en attente n'arrive jamais (un contrôleur arrêté au milieu d'une ligne), la
  réponse de la commande suivante est prise pour cette suite et jetée, et la commande échoue au bout de `CEF_TIMEOUT`,
  comme sans réponse ; la ligne est de nouveau propre pour la commande d'après.
- **Reste** : une réponse tardive arrivée **entière** après l'envoi de la commande suivante est prise pour sa réponse.
  Aucun driver ne peut la reconnaître sans corrélation dans le protocole ; attendre que la ligne se taise supposerait le
  plus long retard d'un adaptateur Pinefeat, que personne ici ne connaît.

## L09-03 — les bornes de `r` : la basse ignorée, la haute appliquée seulement après `CALIBRATE` — reste contre un adaptateur Pinefeat

- **Où** : `readFocusMaxPosition`, `updateProperties(pos, max, dist, aper)`.
- **Symptôme** : `r` rend `<min>-<max>` ; `sscanf(res, "%*d-%d", &pos)` jette le minimum, et la position absolue garde
  le minimum 0 d'`initProperties`. Le maximum n'entre dans `FOCUS_MAX` que si `FocusMaxPosNP` est `Busy`, ce que seul
  `CALIBRATE` pose : à la connexion et à chaque relecture, `r` est lu et son maximum ignoré.
- **Reste tel quel contre un adaptateur Pinefeat** (arbitrage de l'architecte, ticket #645, sur le motif du ticket
  #600) : son `r` porte une plage calibrée, et personne ici ne sait ce qu'il rend avant une calibration. Le changer
  serait deviner. Contrôle de l'amont : `pf_calibrate`.
- **Contre notre carte, l'extension** pose ces bornes à chaque lecture et fait de `CALIBRATE` leur relecture (`README.md`,
  « Les bornes de `r` ») : un comportement nouveau, réservé à `ext=1`, pas un correctif.
- **La règle du mineur reste** : l'amont ne lit `r` que si le mineur de `v` est au moins 3, et aucun code ne la
  contourne (arbitrage de l'architecte). Notre firmware répond `1.3` : `r` est lu.
