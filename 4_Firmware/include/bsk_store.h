/* SOURCE: spec de l'atelier § 9 — le magasin de la marque, dans la carte
 * AUTHOR: engineer
 * DATE: 2026-10-04
 * STATUS: actif — déclarations seulement ; sur la carte components/store/store.c (NVS), en test sim/store_sim.c ;
 *         ticket #676 : une lecture en erreur n'est pas une clé absente (err), l'état de la NVS (bsk_store_nvs)
 *
 * Le seul état persistant de la carte : un entier de 32 bits par clé, dans l'espace NVS `mark`. La clé et la forme de la
 * valeur sont choisies par la SESSION (components/session/mark.c) ; le magasin ne les interprète pas.
 *
 * Les tâches : bsk_store_request, bsk_store_result et bsk_store_nvs sont appelées par la tâche de SESSION seule. */

/* Asynchrone, pour que la SESSION n'attende pas la NVS : une écriture NVS peut effacer une page de flash, des dizaines de
 * millisecondes, et l'API NVS est sous un verrou : une lecture faite par la SESSION attendrait une écriture en cours.
 * Toutes les opérations passent donc par une file, servie dans l'ordre ; l'ordre de la file garantit qu'une lecture
 * déposée après une écriture de la même clé la voit. */
#ifndef BSK_STORE_H
#define BSK_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define BSK_STORE_KEY_MAX 16u   /* NUL compris : une clé NVS a 15 caractères au plus ; celle de la marque en a 13 */

/* L'échéance du résultat d'une écriture (PUT, DEL), comptée depuis son dépôt : la SESSION tient le geste pour raté si
 * elle ne l'a pas relevé avant (E_STORE, PROTOCOL.md § 2, ligne `j`). Une écriture, au pis l'effacement d'une page de
 * flash, et les dépôts qui la précèdent dans la file : des dizaines à quelques centaines de millisecondes, non mesuré
 * sur la carte. Choisie sous les 3 s qu'un driver attend une réponse (CEF_TIMEOUT du driver INDI, celle de la page de
 * banc), la ligne USB comprise. Un résultat rendu plus tard est pris quand même : ce qu'il range est publié. */
#define BSK_STORE_WRITE_MS 2000u

typedef enum { BSK_STORE_GET, BSK_STORE_PUT, BSK_STORE_DEL } bsk_store_op_t;

typedef struct {
    bsk_store_op_t op;
    char           key[BSK_STORE_KEY_MAX];
    bool           ok;       /* GET : une valeur est rangée sous la clé ; PUT, DEL : fait (DEL d'une clé absente compris) */
    bool           err;      /* GET non trouvée : la lecture a échoué sur une erreur ; faux, la clé est absente */
    uint32_t       value;    /* GET trouvée : la valeur */
} bsk_store_res_t;

/* L'état de la NVS, fixé par bsk_store_init jusqu'au redémarrage suivant (`nvs` de DEBUG, PROTOCOL.md § 3) : saine ;
 * effacée à l'init (ESP_ERR_NVS_NO_FREE_PAGES, ESP_ERR_NVS_NEW_VERSION_FOUND : les marques rangées sont perdues), puis
 * saine ; en panne (l'init, l'effacement ou la réinit en échec : aucune opération ne réussit, une lecture est une erreur,
 * nvs_open rendant ESP_ERR_NVS_NOT_INITIALIZED). Dans tous les cas la carte continue, sans marque. */
typedef enum { BSK_STORE_NVS_OK, BSK_STORE_NVS_ERASED, BSK_STORE_NVS_BROKEN } bsk_store_nvs_t;

/* Sur la carte : la NVS (nvs_flash_init) et la tâche du magasin ; avant bsk_session_init. */
void bsk_store_init(void);

bsk_store_nvs_t bsk_store_nvs(void);

/* Dépose une opération (value : celle de PUT), sans attendre. Faux si la file est pleine : rien n'est déposé. */
bool bsk_store_request(bsk_store_op_t op, const char *key, uint32_t value);

/* Le résultat suivant, dans l'ordre des dépôts ; faux s'il n'y en a pas encore. Chaque dépôt accepté en rend un. */
bool bsk_store_result(bsk_store_res_t *r);

#endif
