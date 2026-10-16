# SOURCE: spec de l'atelier [D120] — le capteur SHT40 de la carte v1.0
# AUTHOR: engineer
# DATE: 2026-10-11
# STATUS: actif — sht40.c est du C pur, compilé aussi par les tests hôte ; sensor_board.c est la carte seule
#
# Le SHT40 (sht40.c) et sa tâche de lecture (sensor_board.c). Les broches de la cible sont dans components/phy/pins.h,
# en-tête privé de phy : d'où "../phy". Une cible dont la table ne déclare pas le bus n'appelle rien de sht40.c, qui
# n'est alors pas lié.
idf_component_register(
    SRCS "sht40.c" "sensor_board.c"
    INCLUDE_DIRS "../../include"
    PRIV_INCLUDE_DIRS "../phy" "."
    PRIV_REQUIRES driver esp_timer)

# Avertissements de la cible, fatals sur nos sources (ticket #539, audit M9 ; 7_Docs/DOCTRINE.md) : ESP-IDF pose
# -Wall -Werror=all -Wextra, pas -Wconversion ni -Wshadow, et ses -Wextra ne sont pas fatals. Les en-têtes d'ESP-IDF
# qui les déclenchent en sont écartés à l'include, nommément (7_Docs/DOCTRINE.md dit lesquels).
target_compile_options(${COMPONENT_LIB} PRIVATE -Wextra -Wconversion -Wshadow -Werror=extra -Werror=conversion -Werror=shadow)
