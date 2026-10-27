/* L'hôte fou (F3 de plans/fuzzing.md, l'atelier ; cible 4 du plan, § 2) : le moteur de scénarios de fuzz_session.c,
 * avec la vraie couche HOTE (components/host/host.c) entre le banc et la SESSION. Ce qui change, le contrat de l'appelant
 * de bsk_host_line que le banc tient, le format des cas et les oracles ajoutés : en tête de fuzz_session.c (F3) et de
 * hote.c. */
#define FZ_HOTE 1
#include "fuzz_session.c"
