# Le banc de fuzzing du firmware

**Date** : 2026-10-25
**Dernière révision** : 2026-10-26
**Statut** : actif — la campagne de fuzzing (plan de l'atelier `plans/fuzzing.md`) : F1, le banc gcc et les cibles qu'on appelle seules ; F2, la SESSION face à l'objectif fou ; F3, l'hôte fou et le harnais secondaire de `bsk_session_command` ; F4, les cibles d'octets sous libFuzzer, sur le runner, et la ligne de l'hôte en harnais d'octets
**Référencé par** : `7_Docs/SUITES.md` (« Le banc de fuzzing n'est pas la suite »), `RAPPORT.txt` (à côté)

Un banc gcc qui génère des cas depuis une graine, rejoue un cas rangé à l'identique et réduit un cas qui échoue, sous
ASan et UBSan. Il n'est **pas** la suite : ni `## Test` de `CLAUDE.md`, ni `4_Firmware/run.sh`, ni `sim/test/mutants.sh`,
ni `.gitea/workflows/ci.yml` ne l'appellent ni ne lisent ce répertoire (la raison : la sonde de pertinence rejoue
`## Test` deux fois sous un plafond de 20 min). Le rapport de sa campagne est `RAPPORT.txt`, ses cas sont sous `cas/`.

## Ce qui ressemble à un défaut et n'en est pas un

- **Les oracles sont des propriétés, pas des attendus écrits à la main.** C'est une dérogation voulue à `SUITES.md`
  (« les attendus sont écrits à la main ») : un cas tiré au hasard n'a pas d'attendu. Ce qui reste de la règle : un oracle
  n'est jamais calculé par le code testé. Le modèle de la liste blanche (`fuzz_bench.c`), la table des transitions et les
  T_max (`fuzz_rtc.c`), les conversions du SHT40 et le seuil de condensation (`fuzz_sht40.c`, `fuzz_climat.c`) sont
  écrits ici, chacun avec sa source citée ; aucun n'appelle une fonction de `bench_core.c` autre que `bsk_bench_send`. La dérogation ne s'étend pas à
  la suite.
- **Un cas rangé n'entre pas dans la suite.** Un cas de `cas/` qui échoue rendrait la CI rouge : il n'y entre que par le
  ticket qui corrige son trou, selon `SUITES.md` en entier (attendu écrit à la main, mutants tués, ASan et UBSan, garde de
  durée, `programmes.sh`).
- **Une trouvaille n'est jamais corrigée ici.** Le banc ne touche ni `components/`, ni `include/`, ni `main/` : la
  trouvaille va au rapport, sa correction est un ticket séparé, décidé par l'architecte. Aucune garde n'est ajoutée en
  aval de la frontière de confiance comme correctif (spec de l'atelier § 1, R1).
- **`bench_core` ne reçoit aucun contournement** (`CLAUDE.md` § Interdits) : ses émissions sont observées par
  `-Wl,--wrap=bsk_phy_send`, comme `test_session135`. C'est pourquoi le banc vit sous `sim/` : `garde_emission.sh`
  refuse un `bsk_phy_send` nommé hors de `sim/`.
- **Aucun octet de firmware d'objectif, aucun extrait décompilé.** Les graines sont les trames des traces
  (`sim/sources135.c`, copiées de `traces/`) et des valeurs aux bords tirées des en-têtes.
- **Les entrées `fuzz_*` de `sim/test/programmes.sh`** donnent les sources des harnais (une seule liste de sources,
  `SUITES.md`) ; `run.sh` et `mutants.sh` nomment leurs programmes un à un et n'en jouent aucune.
- **Les faux objectifs, `sim/phy_sim.c` et `sim/test/phy_script.c` ne changent pas** : le banc les lie tels quels.
  L'objectif fou (`lens_fou.c`) enveloppe un faux objectif par l'interface `sim/lens_sim.h` ; ce qu'il ne sait pas
  représenter (l'état « resté alimenté » refusé, le blocage, le hors-modèle de l'enveloppé), il le rend tel quel, et le
  banc ne juge pas la fin d'un tel cas.
- **Une émission saisie par l'objectif fou part décalée de sa propre durée**, même sans être transformée : il ne la
  connaît qu'entière. Sans règle posée, rien n'est saisi ni décalé. Ce n'est pas l'imitation d'un objectif réel : la
  cohérence de ce que dit l'objectif n'est pas à nous (`plans/survie.md` § 1).
- **Le banc de la SESSION fait un pas par tic de 1 ms** (la boucle de `main/main.c`), et saute les tics où ni la PHY ni la
  SESSION n'ont rien à faire ; la suite, elle, fait un pas à chaque événement de la PHY. Le retard du pas est l'écart à
  ce rythme, borné sous les 10 s du chien de garde.
- **La garde de durée compte le temps processeur** (`setitimer(ITIMER_PROF)`), pas l'horloge murale : un pod ou un
  runner chargé ne fait pas échouer un cas qui rend la main.

## Les commandes

Toutes passent par `fuzz.sh`, qui se lance de n'importe où ; `<sortie>` est un répertoire hors du dépôt, où sont
compilés les harnais.

    bash 4_Firmware/sim/fuzz/fuzz.sh construit <sortie>                    # les onze harnais, options de la suite + SAN de run.sh
    bash 4_Firmware/sim/fuzz/fuzz.sh campagne <sortie> <cible> <graine> <n>  # n cas ; les échecs réduits, rangés sous cas/<cible>/
    bash 4_Firmware/sim/fuzz/fuzz.sh enum <sortie> [<processus>]           # SHT40 (mots), journal (bornes), z et :GT# (couples)
    bash 4_Firmware/sim/fuzz/fuzz.sh rejoue <sortie> <cas>                 # un cas rangé, ce que la cible observe, son verdict
    bash 4_Firmware/sim/fuzz/fuzz.sh reduit <sortie> <cas> <réduit>        # tant que la même vérification échoue
    bash 4_Firmware/sim/fuzz/fuzz.sh rejoue-tout <sortie>                  # chaque cas deux fois, sorties comparées octet pour octet
    bash 4_Firmware/sim/fuzz/fuzz.sh mord <sortie> [<harnais>]             # les fautes de fautes.txt : chaque oracle doit mordre
    bash 4_Firmware/sim/fuzz/fuzz.sh libfuzzer <sortie> <cible> <s> [<g>]  # F4 : une cible d'octets sous libFuzzer, s secondes au plus

Les cibles : `rx` (la réception en flux, `components/phy/phy_common.c`), `lens_rx` (`components/session/lens_rx.c`),
`bench` (`bsk_bench_send` contre le modèle de la liste blanche), `sht40` (`components/sensor/sht40.c` contre le faux
capteur), `climat` (`z` et `:GT#` de `components/host/host.c`), `journal` (les formats de `components/journal/journal.c`),
`rtc` (la mémoire de reprise relue par `restore_init`, `components/session/restore.c`), `session` (F2 : la SESSION,
TRANSACTION, `bench_core` et le journal face à l'objectif fou, sur la PHY simulée), `hote` (F3 : la couche HOTE,
`components/host/host.c`, entre l'USB et la même SESSION), `commande` (F3 : `bsk_session_command` à commande arbitraire),
`ligne` (F4 : la couche HOTE au-dessus d'une session scriptée, des octets de l'USB en entrée).
Chaque harnais, `fuzz_<cible>`,
dit en tête son contrat de l'appelant, le format de ses cas et chaque vérification avec la propriété (P1 à P11 du plan) et
la source qu'elle tient.

Un harnais seul : `fuzz_<cible> campagne <graine> <n> <rép>`, `rejoue <cas>`, `reduit <cas> <sortie>`, `ecrit <graine>
<i> <sortie>` (le i-ième cas de la graine, sans le jouer), `premier <graine> <n>` (jusqu'au premier échec, sans rien
ranger : la commande des morsures), et ses commandes propres (`fuzz_sht40 mots`, `fuzz_journal enum`, `fuzz_climat
couples <t_de> <t_a>`).

## L'objectif fou et le moteur de scénarios (F2)

- **L'objectif fou** (`lens_fou.h`, `lens_fou.c`) : un adaptateur de `sim/lens_sim.h` qui enveloppe le faux 135 ou le
  faux standard. Des règles posées par le banc saisissent les émissions d'un type et les retardent, les perdent, les
  doublent, les échangent avec la suivante, mutent un champ ou la longueur à la trame (somme recalculée), changent la
  classe, mutent un octet brut sans somme recalculée, coupent l'émission (un début de trame sans suite), font retomber
  LENS_CS avant les derniers octets (le Sony), ou les rangent pour les rejouer plus tard (une réponse d'une session
  d'avant). Le banc y ajoute ses émissions (gabarits des traces et du protocole, octets bruts) et force LENS_CS.
- **Le moteur de scénarios** (`fuzz_session.c`) : le temps virtuel de la PHY simulée, un pas par tic, les retards du pas,
  D2 (et l'alimentation de l'objectif avec lui ou non), l'objectif resté alimenté, les FAULT des rails, les erreurs de
  bus et les trames perdues ou fausses de `phy_sim.c`, le bouton du fût du faux 135, la NVS relue (une valeur
  quelconque sous la clé que l'objectif fait calculer, des lectures ou des écritures ratées, la file pleine :
  `sim/store_sim.c`), et les commandes qu'une ligne de l'hôte peut produire.
- **Un cas de SESSION** est un en-tête de 8 octets (le modèle, le journal, le préfixe) puis des événements de 12 octets
  (l'attente depuis le précédent, l'opération, ses paramètres) : la liste développée, préfixe compris. Les préfixes sont
  tirés des suites (READY, un goto, une marque, une perte, un CUSTOM, un retour à la marque, un arrêt suivi d'un goto,
  une faute, le homing du pilote) ; la réduction d'un cas de SESSION retire des événements entiers, puis en neutralise,
  120 essais au plus (`FZ_GRAIN`, `FZ_ESSAIS` : un essai y coûte un scénario entier).
- **Les oracles de SESSION** (en tête de `fuzz_session.c`) : la table des transitions, les T_max, les délais physiques
  minimaux, D2 et le verrou, le doigt du bouton, le modèle de la liste blanche (`liste.h`, partagé avec `fuzz_bench.c`),
  chacun écrit à la main avec sa source. Le banc lit le firmware par `--wrap` sur des appels entre unités de compilation
  (`sim/test/programmes.sh`, `fuzz_session`), jamais en appelant une fonction `static`.
- **Le bilan d'une campagne de SESSION** ajoute la couverture des états profonds (`fz_bilan`) : les cas qui ont atteint
  READY, un goto en vol, RESTORING, RECOVERING, FAULT, une marque rangée, un CUSTOM en vol ; les cas par préfixe ; ce que
  les deux contrats internes ont vu ; ce que l'objectif fou a transformé.

## L'hôte fou et le harnais secondaire (F3)

- **Le même moteur.** `fuzz_hote.c` et `fuzz_commande.c` incluent `fuzz_session.c`, compilé avec `FZ_HOTE` ou
  `FZ_COMMANDE` : les cas ont le format de F2 (les opérations O_LIGNE, O_CMDX et O_MAGASIN s'ajoutent à la fin de
  l'énumération, et les cas de F2 se rejouent à l'identique), et tous les oracles de F2 restent actifs.
- **L'hôte fou** (`hote.h`, `hote.c`) : un cas porte les octets reçus par l'USB, 6 par événement O_LIGNE. Le cadrage de la
  carte (`rx_task` de `host_usb.c`, qui n'est pas compilé sur l'hôte) y est recopié à la main : `\r` ignoré, blancs de
  tête sautés, `:` … `#` pour Moonlite, 255 octets au plus, une ligne avec un NUL ou trop longue jetée. Chaque ligne livrée
  va à la vraie couche HOTE comme `bsk_host_usb_service` la donne (une par tour de boucle, après `bsk_host_observe` et
  `bsk_session_real` ; une réponse différée redemandée par `bsk_host_later` à chaque tour, aucune ligne prise avant elle :
  `bsk_host.h`, `main/main.c`). Les commandes des scénarios de F2 deviennent leurs lignes. Les lignes sont tirées d'une
  grammaire de `PROTOCOL.md` § 2 à § 4 aux bornes, en octets arbitraires que le cadrage découpe, en rafales derrière une
  ligne différée, et en sondage à 5 Hz. Le magasin peut retenir ses opérations (O_MAGASIN), comme la tâche du magasin de
  la carte : c'est ce qui rend différées les réponses de `js` et `jx`.
- **Ce qui juge une réponse** est écrit à la main en tête de `hote.c`, commande par commande, d'après `PROTOCOL.md` et la
  spec § 4.5.1 : P10 (la fin, une ligne, 15 caractères, jamais `ok` dans une erreur, le tampon, la forme), P4 côté HOTE
  (un nombre seulement si son drapeau le permet), l'admission de B4 (a) (ce que HOTE dépose ou non, contre l'état publié et
  le premier accusé de la session) et les bornes des réponses différées. Le banc n'appelle aucune fonction de `host.c`
  pour juger `host.c` : il lit l'instantané (`bsk_session_status`) et voit les dépôts par
  `--wrap=bsk_session_command`. HOTE lit la file des accusés : le banc les juge à leur dépôt (`session_ack`).
- **Le harnais secondaire** (`fuzz_commande`) dépose, mêlés aux scénarios de F2, des `bsk_cmd_t` arbitraires (O_CMDX : `op`
  hors de l'énumération une fois sur quarante, `arg` aux bornes ou quelconque). Une trouvaille n'y est un trou que si une
  ligne de l'hôte y mène : `host.c` ne dépose que les `op` de l'énumération, et pour `CMD_LENS_CUSTOM` un `arg` de -1 ou
  de 0x00 à 0x22. Sinon c'est un contrat interne.

## libFuzzer et la ligne de l'hôte (F4)

- **Les cibles d'octets sous libFuzzer** : `rx`, `lens_rx`, `bench` et `ligne`. `LLVMFuzzerTestOneInput` (`fuzz.c`, sous
  `-DFZ_LIBFUZZER`) joue le cas par la `fz_run` du harnais et le juge par ses `FZ_CHECKS` : rien n'est écrit deux fois.
  Un cas plus long que `FZ_CAP` est écarté du corpus (le pilote gcc ne le relirait pas). Un échec connu (`CONNUS`, dans
  `fuzz.c` : celui de `lens_rx`, P4 `avant_boucle`, RAPPORT.txt § 4) est compté et passé, et son compte imprimé à la fin
  (`BILAN libFuzzer`) ; toute autre vérification, et toute erreur d'un sanitizer, arrête la campagne sur son cas. La
  garde de durée y est `-timeout=10` de libFuzzer (10 s murales, les 10 s de processeur du pilote gcc).
- **Les cibles à scénario n'y sont pas** (`session`, `hote`, `commande`) : à quelques cas par seconde, libFuzzer n'y
  apprend presque rien (le plan, § 5 B). Les autres cibles de F1 (`sht40`, `journal`, `climat`, `rtc`) ne sont pas
  jouées sous libFuzzer non plus (le brief de #804) : leurs énumérations couvrent déjà leurs domaines entiers, sauf `rtc`.
- **`fuzz.sh libfuzzer`** fait tout sur une cible : le pilote gcc et `lf_<cible>` (clang, `-fsanitize=fuzzer,address,
  undefined` et le reste de SAN ; ses avertissements imprimés sans arrêter : la suite et ses options sont à gcc), les
  graines (1000 cas du générateur, graine 1, par `ecrit`, et les cas rangés de F1 de la cible ; jamais un cas de
  scénario, d'un autre format), la campagne sous `-max_total_time` (le plafond posé avant de lancer), le rejeu de chaque
  trouvaille par le pilote gcc, deux fois, sa réduction et son hexadécimal (le cas se rapatrie depuis le journal du run :
  `xxd -r -p`), puis la couverture des graines seules et du corpus final (`-runs=0 -print_coverage=1`), comparée fonction
  par fonction et ligne par ligne, dans `components/`. Il ne tourne que là où clang et libFuzzer sont : le runner
  (`.gitea/workflows/trial-804-libfuzzer.yml`) ; ni le pod ni la suite ne les ont.
- **Un cas de libFuzzer n'existe pour le projet que rejoué par le pilote gcc** (la suite est à gcc, celui que la CI
  juge) : rangé sous `cas/<cible>/` s'il se rejoue à l'identique deux fois, rapporté « non reproduit sous gcc » sinon.
- **`fuzz_ligne`**, la ligne de l'hôte en harnais d'octets : un en-tête de 32 octets choisit la session (deux états qui
  alternent, l'instantané, le capteur, `DEBUG`, les accusés), puis viennent les octets reçus par l'USB, cadrés par
  `cadre()` (`hote.c`) et servis comme `bsk_host_usb_service` les sert (une ligne par tour, `BSK_HOST_LATER` redemandé à
  chaque tour). La session est scriptée, comme celle de `sim/test/test_host.c` : la vraie coûterait un scénario de F2 par
  cas. Elle tient le contrat de `bsk_session.h` et ne donne que les raisons que la vraie session donne à chaque commande
  (`cmd.c`, `motion.c`, `mark.c`, `txn.c`) : une raison qu'elle ne donne pas ferait rendre à `f<n>` un `er link …` de
  plus de 15 caractères, une fausse trouvaille. Ses oracles sont ceux de F3 qui se jugent sans scénario : P10 (la fin,
  le tampon, une ligne, 15 caractères, jamais `ok` dans une erreur), P6 muet, la borne de la réponse différée ; la forme
  commande par commande, P4 côté HOTE et l'admission de B4 restent à `fuzz_hote`, parce qu'elles dépendent de ce que la
  vraie session fait.

## Le pilote, le format des cas, le rejeu, la réduction

- **Le pilote** (`fuzz.c`, `fuzz.h`) fournit `main` ; un harnais fournit sa cible (`fz_run`), son générateur (`fz_gen`)
  et ses vérifications (`FZ_CHECKS`). Compilé avec `-DFZ_LIBFUZZER`, il fournit `LLVMFuzzerTestOneInput` à la place de
  `main` et des commandes (F4, plus bas).
- **Le déterminisme** : ni `rand()` ni horloge murale. Le générateur est un splitmix64 à graine (`fz_seed(graine, i)` :
  le i-ième cas d'une graine est toujours le même), et le temps de chaque cible est virtuel. L'horloge murale ne sert
  qu'à imprimer le débit d'une campagne.
- **Un cas est l'entrée développée**, une suite d'octets que la cible lit (un flux, des opérations, une mémoire) : il se
  rejoue tel quel, même après un changement du générateur. Il est rangé sous `cas/<cible>/`, un fichier par cas, nommé
  `<propriété>-<vérification>-g<graine>-<i>.bin`.
- **Le rejeu** (`rejoue`) imprime ce que la cible observe puis `VERDICT passe` ou `VERDICT ECHEC <propriété>
  <vérification> (<source>) : <ce qui s'est passé>` ; deux rejeux d'un cas donnent la même sortie, octet pour octet
  (`rejoue-tout` le vérifie). Une erreur d'un sanitizer imprime aussi son rapport, sur la sortie d'erreur.
- **La réduction** coupe le cas au plus court préfixe qui échoue encore, en retire des morceaux (moitiés, quarts…
  jusqu'à l'octet), puis met des octets à zéro, tant que la **même** vérification échoue ; chaque essai tourne dans un
  processus fils, pour qu'une erreur d'un sanitizer (P1) ou la garde de durée (P2, 10 s de processeur par cas,
  `-DFZ_GARDE_S`) ne tue que l'essai. Elle ne connaît pas la structure du cas : un cas d'opérations peut rester long (le retrait d'octets décale
  les opérations), et une réduction peut glisser vers un autre lieu de la même vérification — le rejeu le dit.
- **La campagne** range le premier échec de chaque vérification, réduit, et compte les autres ; un processus tué par un
  sanitizer ou par la garde écrit le cas en cours, non réduit, dans `<rép>/P1-sanitizer-g<graine>-<i>.bin` ou
  `<rép>/P2-garde-g<graine>-<i>.bin`, et dit combien de cas il a joués (`BILAN … interrompu`) ; `reduit` le réduit
  ensuite (même issue : P1 ou P2), et `rejoue-tout` y attend le rapport du sanitizer ou la ligne de la garde.

## Ce que le banc ne joue pas

AFL++ (le plan, § 5 C : un seul outil guidé suffit). L'électrique et le temps réel de la carte (plan § 4).
Les fonctions `static` (`decode` de `mark.c`, `rx_task` de `host_usb.c`) ne sont atteintes que par l'API publique. Le
cadrage de l'USB (`rx_task`, ESP-IDF) n'est pas compilé : l'hôte fou en recopie les règles à la main (`hote.c`), et ce
qu'il fait d'une ligne de plus de 255 octets ou d'un NUL relève du banc matériel. Le capteur de `fuzz_hote` est
`sim/sensor_none.c` (la mesure inconnue) : `z` et `:GT#` mesurés sont l'affaire de `fuzz_climat` (F1).
Ce qu'aucun test hôte ne joue (`phy.c`, `host_usb.c`, `sensor_board.c`, `main.c` : `SUITES.md`) ne l'est pas non plus ici.
