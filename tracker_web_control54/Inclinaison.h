// ============================================================
// Inclinaison.h - Mesure REELLE de l'inclinaison
//                 (potentiometre redondant 3B3, 2 curseurs)
// ============================================================

#pragma once

#include <Arduino.h>

// Moyenne des deux curseurs, en degres (0 = a plat, 40 = deploye).
double lireInclinaisonReelle();
