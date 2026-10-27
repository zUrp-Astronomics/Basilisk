# Sony E-mount interoperability

A body-side reference for projects that need to power, identify and drive an E-mount lens. It combines original captures, public prior work and explicitly labelled static analysis. It is an interface reference, not an implementation of a proprietary lens or a claim of universal compatibility.

## Read in this order

1. [Evidence and scope](provenance.md): what is observed, inferred or simulated, and what this edition omits.
2. [Common protocol](protocol.md): contacts, handshake, frames, message layouts, encodings and disagreements.
3. [Samyang](samyang.md): AF 135 behavior, model differences and the restricted Custom interface.
4. [Tamron](tamron.md): admission rules, phase changes, motion and limits of the static evidence.
5. [Capture guide](traces.md): original logs and their historical context. [Capture manifest](traces/manifest.json).

Offsets start after a message's type byte; lengths include that byte. Keep source and model/version scope with any fact reused elsewhere. Numerical section IDs in the brand references remain available for the firmware's comments.

## Guide de lecture français

Cette référence technique reste en anglais pour être réutilisable hors de Basilisk. Le [manuel FR](../MANUEL.md) couvre la réalisation et l'usage. Commencer ici par la provenance : les captures réelles, le code du contrôleur et les analyses de firmwares n'ont pas la même portée. Le simulateur n'ajoute pas une preuve matérielle.

Le protocole commun donne contacts, trames, tailles et champs ; les deux suppléments expliquent les différences utiles au contrôleur. Les conclusions d'interopérabilité sont conservées, les détails internes de calibration et de programmation usine sont écartés. Les onze captures publiées restent intactes pour préserver leur valeur de preuve.

## Licence

See the repository [licence and third-party notices](../../README.md#license). Attribution of earlier work remains in [Prior art](protocol.md#10-prior-art).
