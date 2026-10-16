# Les suites hôte — ce qu'elles jouent, ce qu'elles imposent à qui y écrit

**Date** : 2026-10-10
**Dernière révision** : 2026-10-11 (ticket #606 : la table de broches du C3 seule ; `sensor_none.c`, le
capteur avant sa première lecture) ; 2026-10-11 (ticket #594 : le capteur SHT40, `test_sht40` et son faux capteur, `z`, `:GT#` et
`* sht40` dans `test_host`) ; 2026-10-11 (ticket #590 : les FAULT des rails, dans les deux PHY de test et les suites qui
les jouent) ; 2026-10-10
**Statut** : actif — l'ancienne section `## Suites` de `CLAUDE.md`, déplacée telle quelle par le ticket #584 (hygiène du `CLAUDE.md`) ; les commandes elles-mêmes restent dans `CLAUDE.md` § Test
**Référencé par** : `CLAUDE.md` (§ Conventions, § Doc)
**Dérivé de** : `CLAUDE.md` § Suites, commit `0a0a5b8`

À lire avant d'écrire dans `4_Firmware/` (code ou tests) ou dans les tests de `5_App/`. Les commandes sont
celles de `CLAUDE.md` § Test ; ce document dit ce qu'elles jouent et les règles qu'elles font tenir.

Trois suites, deux jobs CI ; aucune n'est toute la suite.

`bash 4_Firmware/run.sh` (gcc seul, rien d'écrit dans le dépôt ; `4_Firmware/traces`, les traces de
l'objectif, `4_Firmware/CMakeLists.txt` et `5_App/emount-bench.html` lus en place) joue, dans l'ordre :

1. chaque en-tête de `4_Firmware/include` compilé seul (inclus deux fois) ; la table de broches compilée
   (`-fsyntax-only`) pour le C3, puis refusée, avec son message, pour une cible sans table ;
2. le faux 135 et ses tests compilés avec `-Werror` (comme tout programme de la suite) ;
3. `test_lens135` (le faux 135 : les comportements de la référence Samyang, les pannes reçues et
   émises), `test_lens_std` (le faux Tamron F051 piloté directement, sans PHY simulée), `test_phy` (le
   code commun aux deux PHY, `phy_common.c` : le flux, E_FRAMING et ses octets, E_BUS injecté, la présence,
   le verrou et l'anti-rebond de D2, la trame perdue, la lecture des FAULT des rails, `fault_edge`), `test_bench_core` (la liste blanche de `bench_core`),
   puis `garde_emission.sh` (seul `bench_core` appelle l'émission de PHY), sur le dépôt et sur une copie
   où un appel fautif est posé ;
4. `replay` (le faux 135 contre `4_Firmware/traces` : chaque paire requête -> réponse tracée, 100 % attendu),
   `test_journal` (le journal seul : forme des lignes, périodiques tus sous `LOG ON`, plafond, anneau de
   8 Ko, `dropped`), `test_led` (le vrai `led.c` seul : des suites d'instantanés rejouées à la cadence de
   10 ms, l'intensité lue à des instants donnés), `test_sht40` (le capteur de la carte v1.0 : le vrai
   `components/sensor/sht40.c` contre le faux SHT40 bit par bit, `sim/sht40_sim.c` : une lecture et ses conversions,
   l'humidité bornée, le capteur absent, un CRC faux sur chaque mot, le bus bloqué par le capteur au milieu d'un octet
   puis libéré, le bus bloqué hors de lui puis libéré, la valeur périmée, la chauffe jamais demandée), `test_session_script` (SESSION, TRANSACTION et HOTE
   contre un répondeur minimal, `sim/test/phy_script.c` ; des FAULT de rails levés dans un démarrage qui fait sur le
   fil exactement ce qu'il fait sans eux), `test_session135` (SESSION et TRANSACTION contre
   le faux 135 ; requêtes d'init comparées à `astro-normal.txt:222-254`), `test_host` (HOTE contre une
   session scriptée : chaque lettre en READY et dans chaque colonne du tableau d'état de la spec § 4.5.1,
   `last_op`, `move_rc`, `t`, et les motifs `expectFor` lus dans `5_App/emount-bench.html`, appliqués à
   chaque réponse ; `z`, `:GT#` et la ligne `* sht40` contre une mesure scriptée, connue, négative, aux arrondis du
   demi-degré, inconnue pour chaque raison, dans chaque état), `test_host135` (des lignes à travers HOTE, SESSION, MOUVEMENT et le faux 135 ; les lignes `* rail` et les compteurs
   de `DEBUG` d'un FAULT de rail levé dans la PHY simulée),
   `test_std` (SESSION, TRANSACTION et HOTE contre le faux F051) ;
5. chaque programme de 3 et 4 recompilé sous `-fsanitize=address,undefined -fno-sanitize-recover=all` et
   rejoué : une erreur d'ASan ou d'UBSan le fait échouer (`test_session135` y reçoit `-DGARDE_S=60`, sa garde de
   durée, 10 s sinon) ;
6. les mutants de `sim/test/mutations.txt` puis de `sim/test/mutations_std.txt` (`sim/test/mutants.sh`).

Ce que la suite de `firmware` impose à qui y écrit :

- **Une seule liste de sources** : `sim/test/programmes.sh`, lue par `run.sh` et par `mutants.sh` (sous
  la racine qu'on leur donne : le dépôt, ou la copie mutée). Un programme ou une source ajoutés le sont
  là. Tout programme qui compile `components/host/host.c` compile `components/led/led.c` et est lié avec
  `-lm` ; hors `test_host`, qui les pose, il compile `sim/stacks_sim.c` et `sim/sensor_none.c` (le capteur avant sa première lecture). `test_session135`, `test_std` et `test_host135` sont liés avec `--wrap` (`LIENS`) pour noter ce
  que la carte fait sur le fil sans toucher à la PHY simulée.
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
- **La PHY scriptée** (`sim/test/phy_script.c`) n'a pas le verrou (ses commandes rendent `true`) ; chacun
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
  retient les résultats comme la tâche de la carte) : chaque banc part d'un magasin vide
  (`store_sim_reset`) ; une marque attendue au démarrage est préchargée (`store_sim_poke`, clé calculée à
  la main).
- **Les arguments des programmes** sont dans `programme_args` (`sim/test/programmes.sh`), obligatoires :
  les traces pour `test_lens135`, `replay` et `test_session135` ; la page puis `CMakeLists.txt` (PROJECT_VER)
  pour `test_host` ; la liste des constats pour `test_std`, sous la racine donnée. `run.sh` déclare les trois
  fichiers lus hors de `sim/test` (`TRACES`, `PAGE`, `VERSION`) et les passe à `mutants.sh`
  (`mutants.sh <traces> <page> <version>`). La traduction des motifs de la page par `test_host` (`js_re`)
  rend `\d` d'un ensemble (`[\d.]`).
- **Ce qu'aucun test hôte ne joue** : ce qui n'est compilé que par ESP-IDF — `components/phy/phy.c` (le
  flux et le relevé sur l'UART, l'interruption de D2, sa coupure, ses sections critiques, l'`esp_timer` de
  l'anti-rebond, son rappel et son repli, la mise en sortie des EN et leur polarité, l'interruption des FAULT des rails,
  leur configuration et leur lecture au démarrage),
  `components/store/store.c`, `components/led/led_board.c` (le
  rendu sur la carte), `components/host/host_usb.c`, `components/sensor/sensor_board.c` (les lignes du SHT40 en drain
  ouvert et leur demi-période, la tâche de lecture, sa priorité, la copie de la dernière lecture sous section critique).

`bash 5_App/run.sh` est la même commande que le job `test` de la CI : `5_App/test/test_app_vm.cjs` (qui
reçoit le répertoire des traces de l'objectif) et `5_App/test/test_app.js`, les tests de la page de banc. Le
même job joue ensuite `bash 5_App/tool_station/run.sh`, le test de l'outil `tool_station` (python3,
`5_App/tool_station/test_tool_station.py`). Les traces de l'objectif sont dans `4_Firmware/traces`, lues en
place par `4_Firmware/run.sh` et par `5_App/run.sh`.
