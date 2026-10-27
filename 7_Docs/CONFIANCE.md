# Le rapport de confiance du firmware

> **Périmètre historique.** Cette évaluation porte sur le commit et les validations indiqués ci-dessous. Elle précède la revue de clôture du 26 octobre 2026 et ne valide pas les corrections encore à intégrer. Une nouvelle révision devra citer les corrections et vérifications effectivement livrées.

**Date** : 2026-10-25
**Dernière révision** : 2026-10-25
**Statut** : actif — auto-évaluation du firmware de `main` au commit `0bff024` (firmware 1.3) ; traduction anglaise complète : `TRUST.md`
**Référencé par** : `README.md` (§ La confiance, § La documentation), `7_Docs/TRUST.md`, `7_Docs/MANUEL.md` (§ Arborescence), `7_Docs/README.md`, `CLAUDE.md` (§ Doc)

Ce document dit comment le firmware de Basilisk a été produit et vérifié, ce qui est prouvé, ce qui est jugé solide et
ce qui reste faible, pour qu'on puisse décider, preuves à l'appui, de brancher sur la carte un objectif de plusieurs
centaines d'euros. C'est une **auto-évaluation** : il est écrit par un agent de la flotte qui n'a pas écrit le firmware,
et jugé comme toute livraison. Il ne revendique aucun label extérieur. Sa source externe est l'audit de **Codex**, un
autre LLM, aux biais différents de ceux du producteur ; ses rapports (2026-10-23) sont gardés dans l'atelier et ne sont
pas publiés : on en cite la conclusion (`audit-basilisk-conclusion-qualifiee.md`) et le registre de 44 observations
(`audit-basilisk-registre.csv`), dont l'annexe donne le sort.

Il décrit le code de `main`, pas la version auditée. Ni la release v1.0.0 (2026-10-20, construite sur `be2e877`, relevé de l'atelier) ni la
validation sur carte du 2026-10-19 (firmware 0.9) ne portent les corrections citées ici.

## 1. Ce que promet le firmware

**Les fonctions.** Mettre l'objectif sous tension et l'initialiser dès qu'il est monté ; piloter le focus et
l'ouverture ; publier ce que l'objectif rapporte (position, bornes, ouverture, nom) ; garder une position, la marque,
dans la carte ; traiter le retrait, la perte de liaison et le redémarrage. Le contrat est `7_Docs/PROTOCOL.md` ; les
lettres `f r e a d s c` restent compatibles Pinefeat octet pour octet.

**Le sens des résultats.** Trois stades ne se confondent pas :

- **commande acceptée** : `ok` à `f<n>` dit que le goto est admis, dans les bornes, rien de plus (`PROTOCOL.md:59`) ;
- **déplacement terminé** : se lit ensuite, `e` puis `g` : `ok` arrivé, `aborted`, `stall`, ou `unconfirmed` quand
  l'arrêt n'a été ni accusé ni suivi d'immobilité (`PROTOCOL.md:63`) ;
- **marque rangée** : `js` ne répond `ok` qu'une fois la marque écrite en NVS, `er store` sinon (`PROTOCOL.md:73`,
  ticket #747).

`q` répond une fois le `0x1C` parti, ou en erreur : un arrêt émis, pas encore confirmé (`PROTOCOL.md:80`, #743).

**Le périmètre de compatibilité.**

| Niveau | Objectifs | Preuve |
|---|---|---|
| Branchés sur la carte réelle | Samyang AF 135 F1.8 FE, Sony FE 24-105 F4 G OSS | relevé du mainteneur, 2026-10-19, non publié : init, homing, marque, goto, bouton, commutateur, `p0`/`p1`, retrait à chaud, échange d'objectif ; firmware 0.9 |
| Tirés de traces | le 135 (`4_Firmware/traces/`, 2026-09-20), le Sony (`7_Docs/E-Mount/traces/`, 2026-10-08, #544) | `replay` rejoue chaque paire tracée, 100 % attendu ; les trames du Sony nourrissent la PHY scriptée |
| Simulés seulement | Tamron F051 (`4_Firmware/sim/lens_std.c`) | jamais branché ; un écart déclaré, `sim/test/constats_std.txt` |

Les autres Samyang reconnus par signature (`PROTOCOL.md:92`) ne sont ni branchés ni simulés.

## 2. Comment il est organisé pour le tenir

| Choix | Implémentation | Exceptions |
|---|---|---|
| Une seule tâche décide | la boucle de `app_main`, `main/main.c:86-95` : la session, puis HÔTE | les interruptions de `phy.c` (la coupure au retrait), les tâches de réception, USB, journal, magasin, capteur : elles transportent, ne décident pas (O36) |
| Un seul chemin vers l'objectif | `bench_core` seul appelle l'émission, sur liste blanche (`components/bench_core/bench_core.c`, `PROTOCOL.md:35-46`) | `CUSTOM WRITE`, au seul 135, décision de l'humain (`svc135_allowed`) ; le `0x09` de l'init, que tout boîtier envoie, touche un drapeau d'accessoire du V-AF 24, réserve écrite en tête de `listed_size` |
| Un seul décodeur, qui valide | `session/lens_rx.c` : une valeur hors domaine est publiée inconnue (#728) | aucune |
| Un propriétaire par donnée | le maintien de `p0` (#709), l'admission des gotos et du retour (#763), le cycle du mouvement (#767) | HÔTE prédit encore l'admission hors `ready` (B4 (a), § 4) |
| Stockage, USB, capteur hors du contrôle | tâches et files à part ; une NVS en panne laisse le focus utilisable (#676) | — |
| Files bornées | `phy.c:808-811`, `store.c:127-128`, anneau du journal de 8 Ko ; chaque perte comptée (`evq_drop`, `dropped` de `DEBUG`) | — |
| Bornes mesurées, jamais codées | celles du `0x06`, resserrées de 5 pas (`lens_rx.c:80-81`) ; un goto hors d'elles est refusé (`motion.c:198`) | X = 200 pas d'approche sur le 135 (`PROTOCOL.md` § 7) |

## 3. Ce qui protège l'objectif

Trois propriétés distinctes : la **protection de l'objectif** (rien d'écrit, pas de ligne pilotée sans ses rails,
coupure au retrait), la **bonne exécution du mouvement** (arriver où on l'a dit), la **disponibilité du focuser**
(revenir au service).

| Situation | La carte commande | Elle observe | Si l'observation manque | Vérifié par |
|---|---|---|---|---|
| Branchement | rien avant l'insertion confirmée | le contact XDETECT, anti-rebond | aucun démarrage | `test_phy` (anti-rebond), `test_session135` ; relevé du 2026-10-19 |
| Démarrage | rail logique, rail moteur au moins 50 ms après, lignes au moins 50 ms après, poignée de main | LENS_CS, réponses de l'init | trois `0x01`, reset soft, `recovering` ; au 4e échec, `fault` (`PROTOCOL.md` § 7) | init comparée à `astro-normal.txt:222-254` (`test_session135`) ; délais sous retard (#689, #691) |
| Déplacement | un `0x1D` dans les bornes | position et mouvement du `0x06`/`0x05` | départ constaté en 1 s, immobilité en 10 s (`motion.c:20-21`), renvois, puis `stall` et `0x1C` | `test_session_script`, `test_stop_goto135` |
| Arrêt | `0x1C` | accusé, immobilité | trois envois en 4,5 s, puis `unconfirmed` | #743, #787 |
| Retrait | coupure des rails et des lignes en interruption, hors de la session (`phy.c:243-253`, `278-293`) | XDETECT | rien : la coupure ne dépend que du contact | logique jouée dans `test_phy` ; `phy.c` en lecture ; durée non mesurée (`phy.c:240-242`) ; retrait à chaud au relevé |
| Objectif muet | rien de neuf | les flux `0x05`/`0x06` | 2 s sans l'un : ses données inconnues ; sans les deux : perte, reprise 3 s après, rails maintenus (`session.c:254-261`) | `t_loss`, `t_resume_*` de `test_session_script` ; six scénarios de panne de Codex (O38) |

`test_session135` vérifie après chaque commande qu'aucune ligne n'est pilotée sans les deux rails (`SUITES.md:174`). Ce qu'aucun test hôte
ne joue (`phy.c`, la vraie NVS, `host_usb.c`, `sensor_board.c`, `led_board.c`, `main.c`) est démontré en lecture dans
son en-tête ou au banc (`7_Docs/SUITES.md:206-216`). Aucun relevé du firmware de `main` sur la carte n'est connu : le dernier, le 2026-10-19, portait le firmware 0.9.

## 4. Sur quoi repose la confiance

**Contrôles mécanisés** (un script échoue, en CI, `.gitea/workflows/ci.yml`) :

- la liste blanche, `test_bench_core`, et `garde_emission.sh` qui refuse une copie où un autre appelant émet ;
- **1 565 mutants** (`mutations.txt` 1 464, `mutations_std.txt` 101, recomptés au commit `0bff024`) : chacun doit être
  tué ;
- les 15 programmes C rejoués sous ASan et UBSan, `-Werror` partout (`4_Firmware/run.sh`) ;
- le rejeu des traces réelles (`replay`) ;
- au build, la console muette, le chien de garde en panique et l'ISR de l'UART en IRAM (`4_Firmware/build.sh:33-53`),
  la garde de la NVS (`garde_nvs.py`) ;
- le driver INDI construit et joué sous libindi contre un faux Pinefeat et une fausse Basilisk (`6_Driver/indi/test.sh`).

**Contrôles qui reposent sur la lecture d'un agent** : les attendus écrits à la main d'après la référence, jamais par
le code testé (`SUITES.md:104`) ; `PROTOCOL.md` changé dans le même commit que le code ; les faux objectifs qui
déclarent ce qu'ils simplifient ; les démonstrations en lecture ; les verdicts des juges.

**Démonstrations choisies.**

- **Arrêt puis goto.** Codex reproduit un `0x1C` ancien émis après un goto nouveau, 14873 accusé pour une cible 16623
  (O01-O02). #680 fixe l'ordre des trames (`t_stop_goto_order`). #730 joue l'arrêt et le goto à 167 phases d'un cycle
  VD, jugés sur les trames émises et la position du faux 135 (`test_stop_goto135.c`) : il trouve un défaut que l'audit
  n'avait pas vu, un goto déposé juste après un arrêt jamais exécuté. #787 le corrige : le `0x1D` attend la fin de
  l'arrêt, et chaque cas finit en sa cible (`t_stop_wait_q`).
- **Délais sous retard.** Codex mesure 49, 45, 30 puis 0 ms d'attente au lieu de 50 pour 1 à 50 ms de retard (O04).
  #689 et #691 comptent chaque délai depuis l'action réelle ; `t_lag_audit` rejoue le cas de l'audit, 150 ms sans pas,
  et exige les lignes 50 ms après le rail moteur, BODY_CS 15 ms après les lignes.
- **Donnée inconnue.** Une ouverture hors domaine, une plage inversée, des bornes vides après la marge sont publiées
  inconnues, jamais corrigées (#728 : `t_aperture_domain`, `t_aperture_inverted`, `t_limits_invalid`).
- **Reprise.** Après une perte, la carte revient à la dernière position, même si la perte survient pendant ce retour
  (`t_restore_recovery`, `t_resume_restoring_loss`).
- **Marque rangée.** `ok` seulement après l'écriture ; échec ou échéance de 2 s rendent `er store`
  (#747 : `t_mark_stored`, `t_mark_store_fail`, `t_mark_deadline`).

**Limites connues** (§ 4 de la matière, à jour au 2026-10-25) :

| Condition | Conséquence | Traitement | Pourquoi accepté |
|---|---|---|---|
| FAULT d'un rail | compté et journalisé, rien n'est coupé | coupure décidée, non codée | attend la mesure au banc d'un FAULT tenu (O29) |
| Défaut qui fait planter chaque démarrage | redémarrages en boucle par le chien de garde | backlog | décision de l'humain, 2026-10-23 |
| Réponse différée du homing (`0x10`) pendant une reprise | peut terminer une requête de son type (`PROTOCOL.md:424-426`) | laissé au fuzzing et au banc | jugé théorique ; le cas CUSTOM est corrigé (#789) |
| Le 135 s'arrête à quelques pas de la cible (±5) | `f` publie la position où il s'est arrêté | laissé à Ekos | décision de l'humain, 2026-10-23 |
| Trois contrats internes sans garde : `vline` (`journal.c:252`), `bsk_txn_held_end` (`txn.c:212`), `session_ack_last` (`session.c:161`) | aucun atteignable aujourd'hui | attendent le fuzzing, prévu, pas commencé | décision de l'humain, 2026-10-24 |
| B4 (a) : HÔTE prédit l'admission hors `ready`, le refus de CMD n'est pas journalisé | aucune divergence aujourd'hui | ouvert, non tranché | le corriger change le contrat |
| Aucun relevé du firmware corrigé sur la carte | comportement réel non mesuré | prochains essais au banc | pas un choix : en attente du banc |

## 5. Ce que cela dit de la flotte

Le firmware est produit par **LCARS**, une flotte d'agents **Claude** :

- l'humain décide, et ne code pas ;
- l'architecte cadre chaque ticket : un résultat, des critères vérifiables, un périmètre ;
- le scoper relit chaque brief contre le code et le renvoie si un fait est faux ou si une décision manque ;
- l'engineer code ;
- deux juges jugent chaque livraison : le qualifier, la preuve (les tests mordent-ils ?) ; le reviewer, le livrable
  contre ses critères ;
- Codex audite de l'extérieur.

Chiffres du registre de la flotte, relevés le 2026-10-23 : **351 jugements de brief** par le scoper, 192 acceptés tels
quels, 157 renvoyés (45 %), 2 abandonnés ; **610 verdicts** de livraison sur **182 tickets**. Le producteur ne voit pas
ce registre et ne peut pas les recompter.

Un fait daté : la sonde de pertinence des tests de la flotte n'a pas rendu de verdict pendant la plus grande partie du
projet ; les juges ont mesuré la morsure des tests par les mutants, à la main.

**Portée.** Ce cas montre que, sur un firmware embarqué réel, ce processus a produit un code compréhensible, testable
et corrigeable : un audit externe a trouvé deux défauts prioritaires, chacun a été compris, corrigé et verrouillé par un
test, et le balayage écrit pour la vérification en a trouvé un troisième. Il ne montre pas que tout code de ce
processus sera fiable. La confiance se fonde sur le code et ses essais, pas sur sa provenance.

## Annexe — le sort du registre de Codex

Correspondance de la matière (`plans/rapport-confiance.md` § 6, atelier, commit `0c6111a`). Chaque ticket se vérifie sur
`main` par le message de son merge (`from lcars/issue-<n>-engineer`) ; les tests cités sont dans `4_Firmware/sim/test/`.
« Gardée » : sur la recommandation de l'auditeur lui-même (`audit-basilisk-conclusion-qualifiee.md`, § Observations
conservées), sauf mention.

| Obs. | Objet | Sort | Ticket, preuve |
|---|---|---|---|
| O01-O02 | arrêt ancien après goto nouveau ; succès hors cible | corrigée | #680 (`t_stop_goto_order`, `t_stop_goto_variants`) ; balayage #730 (`test_stop_goto135.c`) ; son constat corrigé par #787 (`t_stop_wait_q`) |
| O03 | arrêt acquitté avant émission | corrigée | #743 (`t_stop_held`, `t_stop_held_host`) |
| O04 | délais raccourcis par un retard | corrigée | #689 (`t_lag_audit`, `t_lag_waits`), #691 (`t_lag_hs_up`, `t_lag_hs_down`) |
| O05-O06 | panne NVS masquée ; erreur prise pour absence | corrigée | #676 (`t_nvs`, `t_restore_store_error`) ; le tri des erreurs de `store.c` joué depuis par `test_store` (#747) |
| O07 | marque annoncée avant écriture | corrigée | #747 (`t_mark_stored`, `t_mark_store_fail`, `t_mark_deadline`) |
| O08-O11 | ouverture et bornes incohérentes publiées | corrigée | #728 (`t_aperture_domain`, `t_ring_domain`, `t_aperture_inverted`, `t_limits_invalid`), complété par #776 (`jg` sur bornes invalides) |
| O12-O13 | réponse ancienne prise pour la nouvelle | corrigée | #783 (`t_custom_late`, `t_custom_desync_kept`, `t_init_not_echo`) ; CUSTOM abandonné par une perte : #789 (`t_custom_lost`) |
| O14 | présence et tête de file inversées sous saturation | corrigée, prouvée en lecture | #738 : démonstration en tête de `bsk_phy_poll` (`phy.c`) ; aucun test, la PHY simulée n'a pas de producteur concurrent |
| O15 | lignes réactivées un instant après une coupure | gardée | fenêtre connue, écrite dans `phy.c` ; aucune impulsion mesurée |
| O16 | sémaphores USB non vérifiés | corrigée, prouvée en lecture | #734 ; `host_usb.c` n'est joué par aucun test hôte |
| O17 | accumulateur du geste non saturé | corrigée | #734 (`t_ring_bound`, `t_ring_bound_rest`) |
| O18 | maintien de `p0` dupliqué | corrigée | #709 (`t_hold_d2`, `t_hold_session`) |
| O19-O20 | reconnaissance du 135 répétée ; deux admissions | corrigée | #763 (mutants de `mutations.txt`, aucun test ajouté) |
| O21 | prédicats répétés | corrigée en partie | #767 (`t_thirds`) ; l'admission prédite par HÔTE reste ouverte : B4 (a) |
| O22 | deux tables de tiers de diaphragme | corrigée | #767 (`t_thirds`) |
| O23 | mutations de TXN ouvertes aux appelants | gardée | appelants disjoints aujourd'hui |
| O24 | mesures `pair_late`, `tx_wait_us` partielles | corrigée (documentée) | #738 : `PROTOCOL.md:143` et l'en-tête de `bsk_phy_send` disent ce qui est compté |
| O25 | durée d'un pas non bornée | gardée | aucune panne de l'UART démontrée |
| O26 | famine sous activité PHY soutenue | gardée | charge réelle non établie |
| O27 | génération du journal sur 8 bits | corrigée | #734 (`t_gen_wide`) |
| O28 | rebouclage des numéros | corrigée | #734 (`t_seq_wrap`) |
| O29 | FAULT sans coupure | gardée, décision de l'humain | coupure en attente de la mesure au banc (§ 4) |
| O30 | chien de garde ≠ disponibilité | gardée | limite de conclusion ; voir la boucle de redémarrages (§ 4) |
| O31 | F051 déjà alimenté | gardée | `constats_std.txt`, `reste_alimente` ; modèle ou firmware, non arbitré |
| O32 | délai INDI non global | corrigée | #672 (`6_Driver/indi/test/test_driver.py`) |
| O33 | RAM et piles non mesurées sur la cible | gardée | marges lisibles par `DEBUG` ; aucune insuffisance établie |
| O34 | suite officielle non jouée par l'auditeur | gardée | limite de l'audit ; la suite tourne en CI |
| O35 | oracles et firmware partagent le codec | gardée | attendus écrits à la main ; oracles sur les sorties observables |
| O36-O42, O44 | points soutenus : sérialisation, publication, convergence, automates, accusés, chemins multiples, bornes mémoire, données distinctes | gardée | résultats positifs de l'audit |
| O43 | tests applicatifs hors firmware | gardée | hors périmètre du firmware |
