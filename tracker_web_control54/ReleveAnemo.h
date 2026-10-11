// ============================================================
// ReleveAnemo.h - Releves de verification de la constante vent
// ------------------------------------------------------------
// La vitesse du vent est calculee par  km/h = tension x FACTEUR_VOLT_KMH
// (50, valeur constructeur jamais verifiee). Ce module remplit un
// tableau DES QUE L'ANEMOMETRE TOURNE : une ligne par tranche de
// 10 min, avec la tension reellement mesuree, le vent calcule et le
// vent regional (Open-Meteo) au meme moment.
//
// Constante suggeree pour une ligne = vent regional / tension moyenne.
// La mediane des lignes exploitables donne une valeur indicative.
// Rien n'est modifie automatiquement : la constante reste celle de
// Config.h tant que l'utilisateur ne la change pas.
// ============================================================

#pragma once

#include <Arduino.h>

// Charge les releves enregistres (NVS) et ouvre la premiere tranche.
void setupReleveAnemo();

// A appeler dans loop().
void updateReleveAnemo();

// {"constante":50,"suggeree":63.2 ou null,"nValides":12,
//  "lignes":[{"h","v","vx","l","lr","r","rr","k"}...],   plus recent en dernier
//  "enCours":{"min":4,"actifPct":80}}
String releveAnemoJson();

// Meme contenu pour un tableur (";" et virgule decimale).
String releveAnemoCsv();
