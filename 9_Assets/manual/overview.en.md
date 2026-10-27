---
date: 2026-10-26
status: publication candidate
---

## Dix-huit millimètres. Démerde-toi avec ça.

La monture Sony E laisse 18 mm entre la baïonnette et le capteur. Basilisk y case le fût, les contacts, le contrôleur et le port USB, en gardant de la place pour la caméra astro. Une idée discutable. Un objet très satisfaisant.

Il joue le boîtier photo pour un objectif E-mount : alimentation, mise au point et diaphragme avec un seul câble USB-C. Le pilotage passe par Ekos/INDI, ASCOM ou le banc web de cette page. L'objectif fournit ses limites de mise au point ; la carte mémorise une position de référence. Aucun moteur à sangler au fût, aucune courroie à tendre.

## Ce que vous construisez

Un adaptateur matériel ouvert autour d'un ESP32-C3, d'une carte en anneau, d'un boîtier imprimé et du fût métallique d'une bague Meike. Le projet part de Pinefeat ; la carte, le firmware et les fichiers de fabrication sont dans ce dépôt.

Le boîtier est conçu pour les caméras planétaires Player One hexagonales non refroidies : Mars-C, Mars-C II, Mars-M II, Mars 662M ; Neptune-C, Neptune-M, Neptune-C II, Neptune 664C ; Uranus-C et Uranus-M ; Saturn-C SQR et Saturn-M SQR. Les modèles refroidis sortent de cette conception. L'adaptation aux ZWO et ToupTek n'est pas établie.

Le travail au banc documenté ici porte sur un Samyang AF 135 F1.8 FE et un Sony FE 24–105 mm F4 G OSS. La [référence E-mount](../../7_Docs/E-Mount/README.md) distingue les observations et les déductions concernant les autres objectifs. Décrire un protocole ne certifie pas une compatibilité.

## Des pièces aux étoiles

[Assemblage](assembly.fr.md) couvre la carte, les pièces imprimées et l'interface caméra. [Installation](setup.fr.md) relie le firmware à vos logiciels. [Utilisation](usage.fr.md) couvre le focus, le diaphragme, la marque et le diagnostic.

Le [manuel du dépôt](../../7_Docs/MANUEL.md) détaille aussi la conception et le travail hors ligne. Les fichiers sont là pour construire, examiner et adapter. Le résultat compte ; l'atelier est le prix d'entrée.
