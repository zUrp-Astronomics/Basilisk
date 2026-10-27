/* La couche HOTE de la carte (components/host/host.c, incluse telle quelle) et, pour test_host seul, le seq qu'elle a alloué
 * en dernier, posé par le test : son rebouclage, après 2^32 − 1 commandes, se joue sans les déposer (t_seq_wrap). Rien
 * d'autre n'est ajouté : test_host compile ce fichier à la place de host.c (sim/test/programmes.sh). */
#include "../../components/host/host.c"

void host_seq_set(uint32_t seq);
void host_seq_set(uint32_t seq) { h.seq = seq; }
