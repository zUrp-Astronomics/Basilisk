# Les suites hôte — ce qu'elles jouent, ce qu'elles imposent à qui y écrit

**Date** : 2026-10-10
**Dernière révision** : 2026-10-25
**Statut** : actif — l'ancienne section `## Suites` de `CLAUDE.md`, déplacée telle quelle par le ticket #584 (hygiène du `CLAUDE.md`) ; les commandes elles-mêmes restent dans `CLAUDE.md` § Test
**Référencé par** : `CLAUDE.md` (§ Conventions, § Doc)
**Dérivé de** : `CLAUDE.md` § Suites, commit `0a0a5b8`

À lire avant d'écrire dans `4_Firmware/` (code ou tests) ou dans les tests de `5_App/`. Les commandes sont
celles de `CLAUDE.md` § Test ; ce document dit ce qu'elles jouent et les règles qu'elles font tenir.

Trois suites, deux jobs CI ; aucune n'est toute la suite.

`bash 4_Firmware/run.sh` (gcc, et python3 pour l'étape 7 ; rien d'écrit dans le dépôt ; `4_Firmware/traces`, les
traces de l'objectif, `4_Firmware/CMakeLists.txt` et `5_App/emount-bench.html` lus en place) joue, dans l'ordre :

1. chaque en-tête de `4_Firmware/include` compilé seul (inclus deux fois) ; la table de broches compilée
   (`-fsyntax-only`) pour le C3, puis refusée, avec son message, pour une cible sans table ;
2. le faux 135 et ses tests compilés avec `-Werror` (comme tout programme de la suite) ;
3. `test_lens135` (le faux 135 : les comportements de la référence Samyang, les pannes reçues et
   émises), `test_lens_std` (le faux Tamron F051 piloté directement, sans PHY simulée), `test_phy` (le
   code commun aux deux PHY, `phy_common.c` : le flux, E_FRAMING et ses octets, E_BUS injecté, la fenêtre d'écoute, la présence,
   le verrou et l'anti-rebond de D2, la trame perdue, la lecture des FAULT des rails, `fault_edge`, la prise de la carte
   relue tant qu'un front arrive pendant elle, `fault_settle`, contre une broche écrite à la main qui a son statut, la mise
   en événements, `fault_events`), `test_bench_core` (la liste blanche de `bench_core`),
   puis `garde_emission.sh` (seul `bench_core` appelle l'émission de PHY), sur le dépôt et sur une copie
   où un appel fautif est posé ;
4. `replay` (le faux 135 contre `4_Firmware/traces` : chaque paire requête -> réponse tracée, 100 % attendu),
   `test_journal` (le journal seul : forme des lignes, périodiques tus sous `LOG ON`, plafond, les transitions hors
   de lui sous leur propre compte, anneau de 8 Ko, `dropped`, la génération qui tient 512 `LOG OFF`), `test_led` (le vrai `led.c` seul : des suites
   d'instantanés rejouées à la cadence de 10 ms, l'intensité lue à des instants donnés), `test_sht40` (le capteur de la carte v1.0 : le vrai
   `components/sensor/sht40.c` contre le faux SHT40 bit par bit, `sim/sht40_sim.c` : une lecture et ses conversions,
   l'humidité bornée, le capteur absent, un CRC faux sur chaque mot, le bus bloqué par le capteur au milieu d'un octet
   puis libéré, le bus bloqué hors de lui puis libéré, la valeur périmée, la chauffe jamais demandée), `test_store` (le
   magasin de la carte, le vrai `components/store/store.c`, compilé contre un ESP-IDF simulé, `sim/idf/` : les en-têtes
   qu'il lit, ses codes d'erreur ceux d'ESP-IDF v5.3.2, comparés à ceux que le test écrit à la main ; la NVS, les files et
   la tâche sont des faux du test, la tâche servant les dépôts dans l'appel : la lecture rangée, absente — l'espace `mark`
   pas encore créé, la clé absente — ou en erreur, tout autre code ; l'écriture et l'effacement faits ou ratés, l'effacement
   d'une clé absente fait ; l'état de la NVS à l'init, saine, effacée sur ses deux codes, en panne ; l'ordre des dépôts, la
   file pleine), `test_session_script` (SESSION, TRANSACTION et HOTE
   contre un répondeur minimal, `sim/test/phy_script.c` ; des FAULT de rails levés dans un démarrage qui fait sur le
   fil exactement ce qu'il fait sans eux ; la position de reprise après une perte, une perte pendant le retour vers elle, et à travers un redémarrage de la
   carte : sa mémoire donnée par le test, `p->keep`, gardée d'un `bsk_session_init` à l'autre comme la mémoire RTC de la
   carte, sa somme recalculée à part d'après la définition de FNV-1a ; des pas de session retenus, la PHY scriptée
   datant ce qui arrive pendant, et chaque délai minimal qui suit une action de la carte tenu en temps réel entre les
   appels, chaque attente de l'objectif jugée sur la date de ses événements, et chacune des cinq
   attentes de l'heure historique (`wait_for` de `session.c`) échue depuis la date de l'événement qui la pose, lu par un pas
   en retard ; les deux fenêtres de la poignée de main comptées
   depuis le mouvement réel de BODY_CS, et `p1` après `p0` et `b` déposés entre deux pas, à travers HOTE, à l'instant réel
   que le test donne comme `main.c`, `bsk_session_real` ; les valeurs reçues hors du domaine ou invalides — l'ouverture
   relue et celle de la bague, la plage du 0x08 inversée, les bornes du 0x06 vides après la marge — publiées inconnues, lues
   à travers HOTE et par le vrai `led.c` ; `q` déposé entre le 0x03 et le 0x04, son accusé final et sa réponse à travers
   HOTE au départ du 0x1C retenu, datés par la PHY scriptée, servis comme le transport de la carte ; le bit AF d'un goto
   sur 30 paires depuis son dépôt, hors de l'intervalle, entre le 0x03 et le 0x04, ou reporté derrière une émission
   retenue ; la marque annoncée une fois rangée : `js`, `jx`, CMD_SET_MARK et l'appui long, l'écriture retenue ou ratée par le magasin en mémoire, la
   réponse différée jusqu'à son résultat, la marque d'avant publiée et rien de compté pendant l'attente, l'échéance de 2 s
   écrite à la main, l'écriture rendue la session quittée ou la clé quittée ; le geste de la bague clos à sa borne sous un flux
   sans fin à +127, la consigne inchangée, et le reste de sa somme jeté à la pause quand le flux s'arrête sur la borne ;
   l'échelle des tiers de diaphragme, en codes dans la bague et en f/ dans HOTE, confrontée cran par cran à une seule liste écrite à la main, chaque nom usuel lu aussi
   à une sonde, un code à moins d'un demi-tiers du cran que l'arrondi seul ne nomme pas ; une trame qui n'est pas la
   réponse de la requête en cours, injectée pendant elle — les notifications `0x40` du 135 et la réponse à une autre
   sous-commande pendant CUSTOM, un `0x0B` et un `0x0A` qui ne répondent pas à ceux de l'init du Sony — ; CUSTOM refusé
   après son échéance, la réponse en retard rendue à personne, le refus maintenu par une reprise et par `b`, levé par une
   coupure des rails), `test_session135` (SESSION et TRANSACTION contre
   le faux 135 ; requêtes d'init comparées à `astro-normal.txt:222-254` ; rien de la réception n'atteint la session lignes
   au repos : en OFF après p0, en OFF après un retrait, les rails en cours d'établissement ; le démarrage sous `LOG ON` sous
   une charge de lignes plafonnées, des FAULT de rail et des rebonds de XDETECT, chaque transition au journal ; CMD_ATTACH
   en boucle, ses transitions bornées), `test_host` (HOTE contre une
   session scriptée : chaque lettre en READY et dans chaque colonne du tableau d'état de la spec § 4.5.1,
   `last_op`, `move_rc`, `t`, et les motifs `expectFor` lus dans `5_App/emount-bench.html`, appliqués à
   chaque réponse ; `z`, `:GT#` et la ligne `* sht40` contre une mesure scriptée, connue, négative, aux arrondis du
   demi-degré, inconnue pour chaque raison, dans chaque état ; le seuil de condensation de `z`, point de rosée et point
   de givre de part et d'autre de 0 °C, aux bords du domaine de ses formules, non défini, ses attendus calculés à part
   d'après les formules du guide CIMO de l'OMM, sans le code testé ; le seq de l'hôte rebouclé, jamais celui du bouton,
   posé juste sous le rebouclage par `sim/test/host_seq.c`), `test_host135` (des lignes à travers HOTE, SESSION, MOUVEMENT et le faux 135 ; les lignes `* rail` et les compteurs
   de `DEBUG` d'un FAULT de rail levé dans la PHY simulée),
   `test_std` (SESSION, TRANSACTION et HOTE contre le faux F051 ; le démarrage sous la même charge que dans
   `test_session135`), `test_stop_goto135` (un arrêt puis un goto à chacune
   des 167 phases d'un cycle VD, au pas de 100 µs, contre le faux 135 : au repos ou un goto en cours, dans les deux sens,
   le goto 0 et 250 µs après l'arrêt, ou une trame après pour un goto de plus de 1,2 s ; jugés sur les trames émises,
   aucun 0x1C après le 0x1D, et la position du faux 135 à l'accusé COMPLETED, jamais sur l'état publié de MOUVEMENT ;
   chaque cas juste, le goto COMPLETED en sa cible : le constat de #730, en tête du programme, chaque goto COMPLETED hors
   de sa cible, le 0x1D repris par le 135 avant que son arrêt y soit fini, réglé par #787) ;
5. chaque programme de 3 et 4 recompilé sous `-fsanitize=address,undefined -fno-sanitize-recover=all` et
   rejoué : une erreur d'ASan ou d'UBSan le fait échouer (chaque programme qui a une garde de durée, `GARDE_S`,
   y reçoit `-DGARDE_S=60` ; 10 s sinon, 20 pour `test_std`) ;
6. les mutants de `sim/test/mutations.txt` puis de `sim/test/mutations_std.txt` (`sim/test/mutants.sh`, en `-O0`, la
   garde de durée relevée : ci-dessous) ;
7. l'outillage de release, `sim/test/test_release.py` : la garde de la NVS (`garde_nvs.py`, que `build.sh` joue sur le
   `flash_args` du build) sur une table de partitions écrite à la main aux adresses de la table du build, les entrées
   du build et des copies fautives qui recouvrent la NVS ; le manifeste (`manifest.py`) comparé, octet pour octet, à
   un attendu écrit à la main, et son refus d'un mauvais nombre d'arguments.

Ce que la suite de `firmware` impose à qui y écrit :

- **Une seule liste de sources** : `sim/test/programmes.sh`, lue par `run.sh` et par `mutants.sh` (sous
  la racine qu'on leur donne : le dépôt, ou la copie mutée). Un programme ou une source ajoutés le sont
  là. `test_store` seul lit l'ESP-IDF simulé, `sim/idf/`, par ses `LIENS` : aucune autre source n'en voit les en-têtes,
  si bien qu'un composant en C pur qui inclurait un en-tête d'ESP-IDF ne compile pas sur l'hôte. Tout programme qui compile `components/host/host.c` compile `components/led/led.c` et est lié avec
  `-lm` ; hors `test_host`, qui les pose, il compile `sim/stacks_sim.c` et `sim/sensor_none.c` (le capteur avant sa première lecture).
  `test_host` compile `host.c` par `sim/test/host_seq.c`, qui l'inclut tel quel et ne lui ajoute que `host_seq_set` (le
  dernier seq alloué, posé par le test) : aucune fonction de test dans le firmware. `test_session135`, `test_std`, `test_host135` et `test_stop_goto135` sont liés avec
  `--wrap` (`LIENS`) pour noter ce que la carte fait sur le fil sans toucher à la PHY simulée.
- **Les attendus sont écrits à la main**, d'après la référence (trace, capture, décompilé, spec), jamais
  calculés par le code testé ni par ses constantes : les 2 ms de l'anti-rebond s'écrivent `DROP`, pas
  `D2_DROP_US`, qu'un mutant change ; les clés de la marque, les cibles de 0x1D et les codes d'ouverture
  attendus sont calculés à la main. Les bornes attendues sont celles du 0x06 ± 5.
- **Chaque mutant doit être tué** : un bloc `@ <programme> | <fichier relatif à 4_Firmware/> | <fonction>`
  nomme la fonction (le nom de sa définition dans le code, pas un commentaire) où il remplace un texte ; ce
  texte est cherché sur le code seul (commentaires retirés, de la source comme du bloc, qui n'en contient
  pas : `mutants.sh --code <fichier>`) et doit apparaître exactement une fois dans le corps de la fonction
  (`mutants.sh --fonctions <fichier>`, règles de repérage en tête de `mutants.sh`). Hors de toute fonction
  (table, macro, `constats_std.txt`), le bloc n'en nomme aucune et le texte est unique dans tout le
  fichier ; un bloc sans fonction dont le texte est dans une fonction est refusé. Le mutant doit compiler
  (`-Werror` compris) et faire sortir son programme en 1. Une mutation qui ne s'applique pas à exactement un
  endroit, une fonction inconnue, un mutant qui ne compile pas, qui survit ou qui meurt autrement (signal,
  délai) font échouer la suite ; `run.sh` joue `sim/test/test_mutants.sh` (ce repérage, sur une source
  jouet) avant les mutants. Les mutants d'un changement s'ajoutent en fin de fichier ; quand le code déplacé
  emporte l'ancre d'un bloc, le bloc est réancré, même règle, même programme ; un bloc dont la règle est
  renversée est retiré.
- **La garde de durée** : un programme qu'un mutant peut faire tourner sans fin a une garde (`GARDE_S`, `alarm`), 10 s
  (20 pour `test_std`) ; à son échéance il écrit « ECHEC : le programme ne termine pas en <n> s (boucle sans fin) » et
  sort en 1. `mutants.sh` la relève en `-O0` à 180 s (`GARDE_O0`, `-DGARDE_S`), dit un mutant qu'elle tue « tué … par
  la garde de durée » et le compte à part (`tués : <n> (dont <m> par la garde de durée)`, tenu par `test_mutants.sh`).
  Elle vaut au moins 10 fois la durée du programme honnête qu'un mutant vise, en `-O0`, sous la charge de `mutants.sh`
  (`nproc` mutants à la fois, jusqu'à 13 de suite sur le même programme) : un ralentissement de moins de 10× n'est
  jamais compté tué. Un programme visé sans garde (`test_bench_core`, `test_led`, `test_lens135`, `test_store`,
  `test_lens_std`) est arrêté par le `timeout` de `mutants.sh` (`GARDE_O0` + 120 s) : refusé, jamais tué. La mesure, le
  2026-10-24 : chaque programme compilé comme `mutants.sh` le compile (`-O0`, mêmes sources et options), joué seul puis
  en `nproc` copies à la fois, le plus lent retenu ; sur le runner de la CI (essai `trial-771-marge`, run 2811 :
  `ubuntu:24.04`, 12 processeurs, Intel Core Ultra 7 265H, chargé en même temps par une suite complète jouée dans un
  pod du même hôte) et dans un pod (WSL2, 12 processeurs, le même modèle, le pire des tours joués : quatre pour `test_session135`, `test_std`
  et `test_host135`, un pour les autres). Un programme qui
  passerait sous 10× relève `GARDE_O0` ou s'accélère, et la mesure est refaite ici.

  | programme | seul, runner | 12 à la fois, runner | 12 à la fois, pod | garde en `-O0` | marge (le pire) |
  |---|---|---|---|---|---|
  | `test_session135` | 4,14 s | 10,07 s | 8,16 s | 180 s | 17,9× |
  | `test_std` | 1,74 s | 4,07 s | 3,35 s | 180 s | 44× |
  | `test_host135` | 1,14 s | 2,44 s | 2,28 s | 180 s | 74× |
  | `test_session_script` | 0,11 s | 0,27 s | 0,21 s | 180 s | 667× |
  | `test_phy`, `test_host`, `test_journal`, `test_sht40` | ≤ 0,01 s | ≤ 0,09 s | — | 180 s | > 2000× |
  | `test_lens135` (sans garde) | 0,31 s | 0,73 s | 0,57 s | `timeout` 300 s | 411× |
  | `test_bench_core`, `test_led`, `test_store`, `test_lens_std` (sans garde) | ≤ 0,03 s | ≤ 0,15 s | — | `timeout` 300 s | > 2000× |

- **Les constats de `test_std`** : un scénario contre le faux F051 dont l'attendu n'est pas tenu n'est ni
  corrigé ni affaibli : il est inscrit, avec sa raison et sa citation, dans `sim/test/constats_std.txt`,
  que `test_std` lit (chemin donné par `programme_args`, sous le dépôt ou sous la copie mutée de
  `mutants.sh`) et affiche. `test_std` rougit sur un échec hors de la liste, sur un scénario de la liste qui
  passe (le constat réglé en sort) et sur une entrée qui ne nomme aucun scénario ou n'a ni raison ni
  citation.
- **Les faux objectifs** se branchent dans la PHY simulée et le banc par l'interface commune
  `sim/lens_sim.h`, chacun par un adaptateur (`lens_sim_135.c`, `lens_sim_std.c`), sans que leur code
  change ; un adaptateur refuse ce que son modèle ne sait pas représenter (l'état « resté alimenté » porte
  le mode service du 135 et la mise en page du standard). Le faux 135 a pour bornes celles de son `0x06`,
  13873 / 30738, est en READY en 16384 après son init (`POS_REST`), publie 01 à l'offset 62 (sa trace, M1 :
  ses bancs tournent bague en ouverture, bit posé) et publie un appui du bouton sur demande du test
  (`l135_inputs_t`, offset 64 bit 3, sans autre effet). Le faux F051 (`sim/lens_std.c`) est écrit à la main
  d'après `7_Docs/E-Mount/tamron.md` et l'analyse statique privée de son firmware ; ses réponses constantes sont
  reconstruites des champs que `tamron.md` publie, tout octet non publié à zéro ; chaque paramètre [NÉ] de
  `lens_std.h` est joué sur deux valeurs.
- **La PHY simulée** porte la fenêtre d'écoute de la carte (`bsk_phy.h`, `bsk_phy_lines`) : un octet de l'objectif
  n'entre dans la réception, et une E_BUS injectée (`phy_sim_bus_error`) ne remonte, que lignes pilotées ; les relâcher,
  ou la coupure de la retombée de D2, vide la réception sans rien rapporter ; LENS_CS est publiée dans tous les cas. Un
  test qui injecte une émission de l'objectif ou une E_BUS pilote donc d'abord les lignes (`phy_sim_mounted`,
  `bsk_phy_lines(true)`), ou laisse la session les piloter ; une file à remplir derrière la retombée l'est par les
  changements d'un FAULT de rail (`phy_sim_rail_fault`), qui y passent lignes au repos.
- **La PHY simulée** porte le verrou de XDETECT comme la carte : `phy_sim_d2(false)` est l'interruption
  (coupure et verrou dans l'appel même), qui arme l'échéance de 2 ms (`d2_due`, un instant de
  `phy_sim_next`) où D2 est relu (`d2_expire`). `phy_sim_next` ne compte pas l'anti-rebond de
  l'insertion : un banc qui pose D2 réveille lui-même la simulation 300 ms après. `test_lens135` et
  `replay`, qui jouent le fil sans SESSION, montent l'objectif avant le banc (`phy_sim_mounted`, sans
  événement) : sans lui, le verrou refuse la VD. La PHY simulée note l'état des rails et des lignes
  (`phy_sim_wires` : `test_session135` vérifie après chaque commande qu'aucune ligne n'est pilotée sans les
  deux rails ; des lignes relâchées y posent BODY_CS basse, le tirage bas de la carte), injecte une émission
  de l'objectif hors du faux (`phy_sim_lens_raw` ; `phy_sim_lens_raw_late`, ses derniers octets après la
  retombée de `LENS_CS`, le Sony), lève le FAULT d'un rail comme l'interruption de la carte le lit
  (`phy_sim_rail_fault` : le niveau relu, jugé par `fault_edge`, l'état du rail de `phy_sim_wires`, le compte dans
  `bsk_phy_stats`) et perd une trame dans un sens ou dans l'autre, ou la reçoit fausse (un E_FRAMING à sa place, qui
  porte ses octets), au choix du test (`phy_sim_fault`) ; les faux objectifs n'en sont pas changés.
- **La PHY scriptée** (`sim/test/phy_script.c`) n'a ni le verrou (ses commandes rendent `true`) ni la fenêtre d'écoute (le
  test remet ses événements quand il veut) ; chacun
  de ses messages est cité ; un moteur scripté (`motor()` de `test_session_script.c`, branché par `on_send`)
  isole chaque transition ; `fail_sends` fait échouer des émissions ; `ps_rail_fault` rend un FAULT de rail tel que la
  PHY le rend, avec l'état du rail de la dernière commande notée. Les trames du Sony FE 24-105 G y sont copiées des
  captures de l'humain (`7_Docs/E-Mount/traces/emount-bench-2026-09-28T…`) et décodées par
  `fr_decode`.
- **Le faux SHT40** (`sim/sht40_sim.c`) fournit les lignes du bus de `components/sensor/sht40.h` et répond bit par bit,
  sur une horloge virtuelle que seule l'attente du code avance, d'après la datasheet du capteur (`0_Datasheets/`) ; ce
  qu'elle ne dit pas et que le modèle fixe est déclaré en tête de `sim/sht40_sim.h`. Le test choisit ses signaux bruts, le
  rend absent, fausse le CRC d'un mot, le bloque au milieu d'un octet de zéros (`sht40_sim_stuck`, le pire cas d'une
  réinitialisation de la puce en pleine lecture) ou court-circuite SDA ; il note les commandes reçues, que `test_sht40`
  compare (0xFD et 0x94, jamais la chauffe). Les conversions attendues sont calculées à part, d'après les formules de la
  datasheet, et le CRC tenu par son exemple (`CRC(0xBEEF) = 0x92`).
- **Le magasin en mémoire** (`sim/store_sim.c`, même interface que `components/store/store.c` ; `hold`
  retient les résultats comme la tâche de la carte ; `fail` rate les écritures, `fail_get` rend les lectures en erreur,
  `nvs` est l'état de la NVS qu'il rend) : chaque banc part d'un magasin vide et sain
  (`store_sim_reset`) ; une marque attendue au démarrage est préchargée (`store_sim_poke`, clé calculée à
  la main).
- **Les arguments des programmes** sont dans `programme_args` (`sim/test/programmes.sh`), obligatoires :
  les traces pour `test_lens135`, `replay` et `test_session135` ; la page puis `CMakeLists.txt` (PROJECT_VER)
  pour `test_host` ; la liste des constats pour `test_std`, sous la racine donnée. `run.sh` déclare les trois
  fichiers lus hors de `sim/test` (`TRACES`, `PAGE`, `VERSION`) et les passe à `mutants.sh`
  (`mutants.sh <traces> <page> <version>`). La traduction des motifs de la page par `test_host` (`js_re`)
  rend `\d` d'un ensemble (`[\d.]`).
- **Ce qu'aucun test hôte ne joue** : ce qui n'est compilé que par ESP-IDF — `components/phy/phy.c` (le
  flux et le relevé sur l'UART, sa fenêtre d'écoute (RXD détachée de l'UART et tirée bas lignes au repos, la génération
  de la fenêtre relevée par la tâche), l'interruption de D2, sa coupure, ses sections critiques, l'`esp_timer` de
  l'anti-rebond, son rappel et son repli, la mise en sortie des EN et leur polarité, l'interruption des FAULT des rails,
  les accès de sa prise à la broche, sa file et l'ordre où `bsk_phy_poll` la rend avec la présence, démontré en lecture
  en tête de `bsk_phy_poll`, leur configuration et leur lecture au démarrage),
  du magasin de la carte, la vraie NVS et FreeRTOS (`test_store` joue `components/store/store.c` contre leurs faux : ni
  la flash, ni la durée d'une écriture, ni l'ordonnancement de sa tâche, ni le chien de garde), `main/main.c` (la
  position de reprise en mémoire RTC, et les causes de redémarrage qui la font relire), `components/led/led_board.c` (le
  rendu sur la carte), `components/host/host_usb.c`, `components/sensor/sensor_board.c` (les lignes du SHT40 en drain
  ouvert et leur demi-période, la tâche de lecture, sa priorité, la copie de la dernière lecture sous section critique).

`bash 5_App/run.sh` est la même commande que le job `test` de la CI : `5_App/test/test_app_vm.cjs` (qui
reçoit le répertoire des traces de l'objectif) et `5_App/test/test_app.js`, les tests de la page de banc (le capteur de
la carte, `z`, affiché en FR et en EN, objectif monté ou non, compris). Le
même job joue ensuite `bash 5_App/tool_station/run.sh`, le test de l'outil `tool_station` (python3,
`5_App/tool_station/test_tool_station.py`). Les traces de l'objectif sont dans `4_Firmware/traces`, lues en
place par `4_Firmware/run.sh` et par `5_App/run.sh`.

## Ce que joue la sonde de pertinence

La sonde `.gitea/workflows/probe-test-relevance.yml` (lancée par les juges, hors de la CI) joue la commande de
`CLAUDE.md` § Test deux fois : sur la tête livrée, puis sur le code de base avec le harnais de la tête (`## Harness`).
Elle la joue sous `env -i`, avec `LCARS_PROBE=1`, qu'elle seule pose. Les trois suites y sont jouées comme ci-dessus,
sauf l'étape 6 de `4_Firmware/run.sh` (`test_mutants.sh` et les mutants), que `run.sh` ne joue pas quand
`LCARS_PROBE=1`, et dit ne pas jouer. Raisons :

- la sonde demande si la suite remarque le retrait du code livré ; les mutants mesurent autre chose, la force des
  tests, et le job `firmware` de la CI les joue à chaque PR ;
- chaque programme qu'un mutant vise est l'un de ceux de `sim/test/programmes.sh` (`mutants.sh` refuse les autres),
  que les étapes 3 et 4 jouent sur le code de l'arbre et que l'étape 5 rejoue sous ASan et UBSan ;
  `test_mutants.sh` ne joue qu'une source jouet ;
- sur la passe du code de base, un bloc ancré dans du code que la livraison ajoute n'y est pas trouvé : `mutants.sh`
  échouerait par une ancre, pas par un test ;
- joués deux fois, ils menaient la sonde au plafond de son job (`timeout-minutes`, 20, la limite d'attente du rail),
  tuée sans verdict. La mesure, sans eux, est en tête de la sonde.

Hors de la sonde, `LCARS_PROBE` n'est pas posée et `run.sh` joue l'étape 6.

## Le banc de fuzzing n'est pas la suite

`4_Firmware/sim/fuzz/` (son `README.md` donne ses commandes, `RAPPORT.txt` sa campagne) est un banc gcc qui génère des
cas depuis une graine, les rejoue et les réduit, sous ASan et UBSan. Il est hors de `CLAUDE.md` § Test et de la CI :
ni `run.sh`, ni `mutants.sh`, ni `ci.yml` ne l'appellent, et ses entrées `fuzz_*` de `sim/test/programmes.sh` n'y sont
jouées par personne. Ses oracles dérogent sciemment à « les attendus sont écrits à la main » : un cas tiré au hasard est
jugé par une propriété, jamais calculée par le code testé. La dérogation ne vaut que pour le banc ; un cas qu'il trouve
n'entre dans la suite que par le ticket qui corrige son trou, selon ce document en entier.
