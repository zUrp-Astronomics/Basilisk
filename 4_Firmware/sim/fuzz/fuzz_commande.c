/* Le harnais secondaire de F3 (plans/fuzzing.md, l'atelier, cible 4 du plan, § 2) : le moteur de scénarios de
 * fuzz_session.c, plus des bsk_cmd_t arbitraires (O_CMDX) déposés dans bsk_session_command, op hors de l'énumération et
 * arg quelconque. Jugé sur P1, P2, P3, P5 et ACCUSE ; ses oracles sont ceux de fuzz_session.c. Une trouvaille n'y est un
 * trou que si une ligne de l'hôte y mène : components/host/host.c ne dépose que les op de l'énumération, et pour
 * CMD_LENS_CUSTOM un arg de -1 ou de 0x00 à 0x22. */
#define FZ_COMMANDE 1
#include "fuzz_session.c"
