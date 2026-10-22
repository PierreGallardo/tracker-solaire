// ============================================================
// Config.h - Toutes les constantes materielles du tracker
// ------------------------------------------------------------
// Les valeurs ci-dessous sont MESUREES sur le materiel reel.
// Ne pas les "arrondir" ni les remplacer par des valeurs
// d'exemple : le tracker ne fonctionnerait plus.
// ============================================================

#pragma once

#include <Arduino.h>

// ============================================================
// DONNEES PERSONNELLES : Secrets.h (ignore par Git)
// ------------------------------------------------------------
// Mots de passe WiFi et OTA, nom du reseau et position du tracker.
// Sur un nouveau poste : copier Secrets.exemple.h en Secrets.h et
// le remplir.
// ============================================================
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#error "Secrets.h manquant : copier Secrets.exemple.h en Secrets.h et le remplir."
#endif

// ============================================================
// WIFI
// ============================================================

// WIFI_SSID et WIFI_PASSWORD : voir Secrets.h

#define MDNS_HOSTNAME   "tracker"      // -> http://tracker.local

// ------------------------------------------------------------
// FLASHAGE OTA (Over-The-Air, via WiFi)
// ------------------------------------------------------------
// Le mot de passe doit faire AU MOINS 8 caracteres, sinon
// l'Arduino IDE le refuse.
// OTA sur reseau local prive : raisonnablement sur. A eviter
// expose sur internet (le mot de passe transite faiblement
// protege) : passer par un VPN dans ce cas.
#define OTA_HOSTNAME    "tracker-solaire"
// OTA_PASSWORD : voir Secrets.h

// ============================================================
// POSITION GEOGRAPHIQUE (meteo en ligne / calcul solaire)
// ============================================================

// LATITUDE_DEG, LONGITUDE_DEG, ALTITUDE_M : voir Secrets.h

// ============================================================
// MOTEURS - 2 ponts en H BTS7960 / IBT_2 (l'un serigraphie HW-039)
// ------------------------------------------------------------
// Alimentation : VCC / R_EN / L_EN des drivers -> 5V du
// convertisseur SEPARE, jamais sur le VIN de l'ESP32.
// Masse commune obligatoire ESP32 / drivers / alim 12V.
// ============================================================

#define RPWM_INC   27      // inclinaison, sens +
#define LPWM_INC   14      // inclinaison, sens -
#define RPWM_AZ    26      // azimut, sens +
#define LPWM_AZ    25      // azimut, sens -

#define PWM_FREQ   20000
#define PWM_RES    8

#define VITESSE_MANUELLE  200     // rapport cyclique des commandes web
#define DUREE_MAX_MS      15000   // arret automatique de securite

// ============================================================
// I2C - BMP280 (clone HW-611, chip ID 0x58) + RTC DS3231
// ------------------------------------------------------------
// Les deux capteurs sont alimentes UNIQUEMENT par le 3.3V de
// l'ESP32. Un regulateur separe provoque une instabilite I2C
// (constatee sur le montage reel).
// ============================================================

#define PIN_SDA  18
#define PIN_SCL  19

#define I2C_CLOCK_HZ      50000    // vitesse reduite, tolerante au bruit
#define BMP280_CHIP_ID    0x58     // ID renvoye par le clone HW-611

// ============================================================
// ANEMOMETRE HWFS-1 (contact sec, 2 fils, sans polarite)
// ============================================================

// ------------------------------------------------------------
// Le HWFS-1 est un capteur ANALOGIQUE PASSIF, pas un contact sec.
// Une generatrice interne produit 0 a 3,3 V proportionnellement a
// la vitesse. Ni alimentation, ni pull-up, ni interruption.
//
// IMPERATIF : la broche doit etre sur l'ADC1. L'ADC2 (dont fait
// partie GPIO 4) est requisitionne par la radio et devient
// inutilisable des que le WiFi tourne.
// ADC1 disponibles ici : 32 et 33 (34/35 reserves au codeur
// Baumer, 36/39 pris par le potentiometre 3B3).
//
// Historique : le capteur a longtemps ete lu en INPUT_PULLUP sur
// GPIO 4. Le pull-up interne (~45 kOhm) court-circuitait la
// generatrice (48 Ohm d'impedance) et ecrasait le signal a 3 mV.
// ------------------------------------------------------------
#define PIN_ANEMOMETRE       32

// Conversion constructeur : km/h = tension(V) x 50
// (soit m/s = V x 14). A affiner contre la meteo regionale.
#define FACTEUR_VOLT_KMH     50.0

#define PERIODE_ECHANT_MS    100    // cadence de lecture de l'ADC
#define NB_LECTURES_ADC      8      // moyenne par echantillon, anti-bruit
#define FENETRE_VENT_MS      5000   // fenetre de la vitesse moyenne
#define SEUIL_VENT_SECURITE  50.0   // km/h -> retour a plat automatique

// ------------------------------------------------------------
// SORTIE DE SECURITE - volontairement tres conservatrice
// ------------------------------------------------------------
// La mise en securite ne se leve PAS sur une simple hysteresis :
// il faut que la RAFALE soit restee sous SEUIL_VENT_SORTIE
// pendant DUREE_CALME_REQUISE_MS d'affilee. Le moindre pic
// au-dessus du seuil remet le compteur a zero.
//
// Raison : un coup de vent va rarement seul. Redeployer le
// panneau entre deux rafales est le meilleur moyen de le casser.
#define SEUIL_VENT_SORTIE       30.0       // km/h, mesure sur la rafale (2 s)
#define DUREE_CALME_REQUISE_MS  3600000UL  // 1 h de calme ininterrompu

// ------------------------------------------------------------
// SURVEILLANCE DE L'ANEMOMETRE
// ------------------------------------------------------------
// Toute la securite vent repose sur ce seul capteur, et une panne
// ne se signale pas : zero impulsion se lit comme "pas de vent".
// Trois detections, de la plus sure a la plus indirecte.
//
// L'alerte reste affichee jusqu'a ACQUITTEMENT manuel : c'est a
// l'operateur de verifier (camera, controle visuel) et de lever.
// ------------------------------------------------------------

// Seuil au-dessus duquel on considere que le capteur "voit"
// quelque chose. En dessous, c'est du bruit d'ADC.
#define ANEMO_SEUIL_ACTIVITE_V     0.02       // 20 mV, soit ~1 km/h

// 1. Aucune tension significative pendant 24 h : soit le capteur
//    est debranche, soit son cablage est coupe. Sur un site meme
//    tres abrite, 24 h sans le moindre souffle est anormal.
#define ANEMO_SILENCE_SUSPECT_MS   86400000UL

// 3. Indirecte : la meteo regionale annonce du vent franc alors
//    que le capteur ne voit rien. Le site est abrite par des
//    haies, donc le seuil est haut pour eviter les fausses
//    alertes, et la duree longue.
#define ANEMO_ECART_METEO_KMH      28.0
#define ANEMO_ECART_METEO_MS       1800000UL   // 30 min

// Tendance du vent : buffer circulaire glissant
#define NB_ECHANTILLONS_TENDANCE  6     // 6 x 5s = fenetre de 30s
#define SEUIL_TENDANCE_KMH        2.0   // ecart mini, pour ignorer le bruit

// ------------------------------------------------------------
// EPISODE DE VENT FORT - suivi rapproche
// ------------------------------------------------------------
// Des qu'un echantillon depasse SEUIL_EPISODE_KMH, on releve une
// valeur toutes les PERIODE_EPISODE_MS (pic ET moyenne de la
// fenetre) et on garde les NB_VALEURS_EPISODE dernieres. La
// tendance (augmente / diminue / stable) compare la moitie
// ancienne du tampon a la moitie recente.
// L'episode se termine apres EPISODE_FENETRES_CALMES fenetres
// consecutives dont le pic reste sous le seuil ; le tampon est
// alors conserve pour consultation jusqu'au prochain episode.
//
// Purement informatif : n'agit sur aucun moteur ni aucune securite.
// ------------------------------------------------------------
#define SEUIL_EPISODE_KMH           30.0   // pic declencheur
#define PERIODE_EPISODE_MS          5000   // un releve toutes les 5 s
#define NB_VALEURS_EPISODE          15     // 15 x 5 s = 75 s d'historique
#define EPISODE_FENETRES_CALMES     6      // 6 x 5 s = 30 s sous le seuil -> fin
#define SEUIL_TENDANCE_EPISODE_KMH  2.0    // ecart mini pour parler de hausse/baisse

// ------------------------------------------------------------
// PALIER INTERMEDIAIRE - mode "vent reduit"
// ------------------------------------------------------------
// Strategie des trackers professionnels : plutot que de passer
// brutalement de "tout normal" a "retour a plat", on limite
// d'abord l'amplitude et la vitesse des mouvements.
// Ce palier est DISTINCT et EN DESSOUS de SEUIL_VENT_SECURITE.
#define SEUIL_VENT_REDUIT          40.0   // km/h -> activation du mode reduit
                                          // hysteresis : sortie sous 80% (32 km/h)
#define ANGLE_MAX_VENT_REDUIT_DEG  20.0   // plafond d'inclinaison en mode reduit
#define VITESSE_REDUITE            120    // PWM des 2 axes en mode reduit
                                          // (au lieu de VITESSE_MANUELLE = 200)

// ------------------------------------------------------------
// DETECTION DE RAFALE - fenetre courte, independante de la moyenne
// ------------------------------------------------------------
// La moyenne sur 5 s lisse les pics brefs. Une seconde fenetre,
// bien plus courte, permet de reagir a une rafale isolee.
#define GUST_WINDOW_MS          2000   // fenetre de mesure du pic
#define SEUIL_RAFALE_SECURITE   50.0   // km/h sur 2 s -> mise en securite

// ------------------------------------------------------------
// HISTORIQUE DES MISES EN SECURITE
// ------------------------------------------------------------
// Buffer circulaire en RAM UNIQUEMENT : le contenu est perdu a
// chaque redemarrage ou coupure de courant. Pas encore de
// persistance en flash (Preferences / NVS).
#define NB_EVENEMENTS_VENT  8

// ------------------------------------------------------------
// AJOUT 1 - MISE A PLAT SUR RAFALES REPETEES
// ------------------------------------------------------------
// S'ajoute aux paliers existants, qui restent prioritaires et ne
// sont jamais leves par cette regle. Mesure sur chaque echantillon
// de l'ADC (toutes les PERIODE_ECHANT_MS = 100 ms).
//   - une rafale = passage au-dessus de RAFALE_SEUIL_KMH ; pour en
//     compter une autre, le vent doit d'abord repasser sous
//     RAFALE_RESET_KMH (hysteresis anti double comptage) ;
//   - RAFALE_NB rafales dans les RAFALE_FENETRE_MS dernieres
//     millisecondes -> retour a plat ;
//   - redeploiement apres REDEPLOIE_DELAI_MS consecutives sans
//     aucun echantillon au-dessus de REDEPLOIE_CALME_KMH.
// Non reglables depuis l'appli.
#define RAFALE_SEUIL_KMH        30.0
#define RAFALE_RESET_KMH        25.0
#define RAFALE_NB               4
#define RAFALE_FENETRE_MS       180000UL   // 3 min
#define REDEPLOIE_CALME_KMH     20.0
#define REDEPLOIE_DELAI_MS      900000UL   // 15 min
#define RAFALE_HORODATAGES_MAX  16         // horodatages conserves (>= RAFALE_NB)

// ------------------------------------------------------------
// AJOUT 2 - ENREGISTREMENT DES RAFALES (bargraphe de l'appli)
// ------------------------------------------------------------
// Demarre des qu'un echantillon depasse ENREG_DEMARRAGE_KMH ; une
// barre toutes les ENREG_PAS_MS = vitesse MAX de l'intervalle ;
// s'arrete apres ENREG_ARRET_CALME_MS consecutives sous le seuil
// (tout echantillon au-dessus relance le decompte).
// 5760 barres x 15 s = 24 h. Sauvegarde dans LittleFS (partition
// "spiffs" de 128 Ko) : 5760 barres horodatees ne tiennent pas dans
// la partition NVS (20 Ko, deja partagee par les autres historiques).
#define ENREG_DEMARRAGE_KMH     7.0
#define ENREG_PAS_MS            15000UL    // une barre toutes les 15 s
#define ENREG_ARRET_CALME_MS    1800000UL  // 30 min sous le seuil -> arret
#define ENREG_NB_BARRES         5760       // 24 h de barres de 15 s
#define ENREG_SAUVEGARDE_MS     300000UL   // sauvegarde en flash toutes les 5 min
#define ENREG_FICHIER           "/rafales.bin"

// ------------------------------------------------------------
// SIMULATION DU VENT (TESTS UNIQUEMENT)
// ------------------------------------------------------------
// 1 = la vitesse mesuree est REMPLACEE par une valeur de test, reglee
// par /api/sim-vent?kmh=35 ou par le scenario /api/sim-vent?scenario=rafales.
// TOUTES les securites reagissent a la valeur simulee : moteurs
// debranches pendant les essais. Doit rester a 0 en service.
#ifndef SIMULATION_VENT
#define SIMULATION_VENT         0
#endif

// ============================================================
// POTENTIOMETRE 3B3 - inclinaison REELLE (4 fils, 2 curseurs)
// ------------------------------------------------------------
// Calibration relevee sur le tracker :
//   0 deg (a plat)   : canal A = 3918 / canal B = 137
//   40 deg (deploye) : canal A =  269 / canal B = 3684
// ============================================================

#define PIN_POT_CANAL_A      36     // SVP
#define PIN_POT_CANAL_B      39     // SVN
#define NB_ECHANTILLONS_POT   8

#define POT_A_0DEG    3918
#define POT_A_40DEG    269
#define POT_B_0DEG     137
#define POT_B_40DEG   3684
#define POT_ANGLE_MAX_DEG  40.0

// ============================================================
// RETOUR A PLAT (asservissement sur le potentiometre)
// ============================================================

#define RETOUR_A_PLAT_TOLERANCE_DEG  0.5
#define RETOUR_A_PLAT_VITESSE        180
#define RETOUR_A_PLAT_PERIODE_MS     200

// ============================================================
// AZIMUT - estimation par temps de commande
// ------------------------------------------------------------
// Provisoire : sera remplace par la mesure reelle du codeur
// Baumer RS422 (CHA/CHB) une fois celui-ci integre.
// ============================================================

#define VITESSE_AZIMUT_DEG_S  4.0

// ============================================================
// HISTORIQUE LOCAL TEMPERATURE / PRESSION (BMP280)
// ------------------------------------------------------------
// Un releve toutes les 30 min, sur 24 points = 12 h glissantes.
// La tendance compare la moyenne de la 1ere moitie a celle de la
// 2eme, comme pour le vent.
//
// La tendance de PRESSION est la plus utile : une chute annonce
// une degradation, donc du vent a venir.
// ============================================================

#define NB_ECHANT_HISTO_METEO     24        // 24 x 30 min = 12 h
#define PERIODE_HISTO_METEO_MS    1800000UL // 30 min
#define SEUIL_TENDANCE_TEMP_C     0.4       // ecart mini, en degres
#define SEUIL_TENDANCE_PRESS_HPA  0.4       // ecart mini, en hPa

// Historique du vent local, sur le meme modele : un point par
// demi-heure = vent MOYEN et RAFALE MAX de la demi-heure ecoulee
// (une valeur instantanee ne voudrait rien dire pour le vent).
#define NB_ECHANT_HISTO_VENT          24          // 24 x 30 min = 12 h
#define PERIODE_HISTO_VENT_MS         1800000UL   // 30 min
#define PERIODE_ECHANT_HISTO_VENT_MS  5000UL      // vent moyen echantillonne toutes les 5 s
#define SEUIL_TENDANCE_HISTO_VENT_KMH 2.0         // ecart mini sur le vent moyen

// Releves de verification de la constante FACTEUR_VOLT_KMH : une
// ligne par tranche de 10 min pendant laquelle l'anemometre a tourne,
// avec la tension mesuree, le vent calcule et le vent regional en face.
#define NB_RELEVES_ANEMO              30          // 30 lignes conservees (NVS)
#define PERIODE_RELEVE_ANEMO_MS       600000UL    // une ligne par tranche de 10 min
#define PERIODE_ECHANT_RELEVE_MS      1000UL      // tension lue toutes les secondes
#define RELEVE_ANEMO_ACTIF_MIN_PCT    50          // tourne au moins 50 % de la tranche
#define RELEVE_TENSION_MIN_V          0.05        // en dessous, constante non calculee
#define RELEVE_VENT_REG_MIN_KMH       5.0         // idem pour le vent regional
#define RELEVE_METEO_AGE_MAX_MS       1800000UL   // meteo de plus de 30 min : ignoree

// ============================================================
// SUIVI SOLAIRE AUTOMATIQUE
// ------------------------------------------------------------
// L'inclinaison est asservie sur l'elevation du soleil calculee
// par l'algorithme NOAA (voir Solaire.h), a partir de l'heure
// RTC synchronisee NTP.
//
// L'AZIMUT N'EST PAS ASSERVI : tant que le codeur Baumer n'est
// pas installe, la position azimutale n'est qu'une estimation
// qui derive. Commander le moteur d'azimut en aveugle sur une
// position fausse risquerait d'aller en butee mecanique.
// ============================================================

#define ELEVATION_MIN_SUIVI_DEG   5.0    // sous cette elevation : repos a plat
#define SUIVI_AUTO_PERIODE_MS     2000   // periode de recalcul + asservissement
#define SUIVI_AUTO_TOLERANCE_DEG  1.5    // zone morte, evite le pompage du verin
#define SUIVI_AUTO_VITESSE        150    // PWM de poursuite (mouvement lent)

// ============================================================
// METEO EN LIGNE (Open-Meteo, gratuit, sans cle)
// ============================================================

#define METEO_INTERVAL_MS  600000UL   // 10 minutes

// ============================================================
// DIVERS
// ============================================================

#define HEARTBEAT_MS       60000UL    // trace "[vivant]" sur le port serie

// ============================================================
// CODEUR BAUMER - OUTIL DE TEST UNIQUEMENT (page /codeur)
// ------------------------------------------------------------
// Le codeur n'est PAS integre au suivi : l'azimut reste estime.
// Cet outil sert a verifier le cablage a vide en tournant l'axe a
// la main. L'ecoute est DESACTIVEE par defaut : des entrees non
// reliees peuvent generer des interruptions en rafale, ce qui ne
// doit jamais perturber la surveillance du vent.
// ============================================================

#define PIN_CODEUR_A             34     // CHA, via MAX490 + pont diviseur
#define PIN_CODEUR_B             35     // CHB, via MAX490 + pont diviseur
#define CODEUR_DIAG_DUREE_MS     600000UL  // arret automatique apres 10 min
#define CODEUR_TAUX_MAX_PAR_S    20000     // au-dela : bruit, on coupe l'ecoute

// Resolution du codeur Baumer : 1800 impulsions par tour et par voie
// (donnee par l'utilisateur ; a confirmer par un essai "un tour exact",
// qui doit donner 7200 pas). Le decodage en quadrature compte les 4
// fronts de chaque cycle : 7200 pas par tour, soit 0,05 degre par pas.
// Ces valeurs sont celles de l'AXE DU CODEUR. Si le codeur est monte
// sur le moteur et non sur l'axe de sortie, il faudra y ajouter le
// rapport de reduction pour obtenir l'azimut du tracker.
#define CODEUR_IMPULSIONS_PAR_TOUR  1800
#define CODEUR_PAS_PAR_TOUR         (4 * CODEUR_IMPULSIONS_PAR_TOUR)

// ============================================================
// CAPTEUR HOME AZIMUT (SN04-N inductif, NPN, normalement ouvert)
// ------------------------------------------------------------
// Alimente en 12 V ; sortie (fil noir) vers GPIO 33 a travers une
// diode Schottky (cathode cote capteur) + pull-up 10 kOhm vers 3,3 V.
// Metal devant le capteur -> sortie a 0 V -> GPIO 33 a l'etat BAS.
// Pour l'instant : lecture de test seulement (/api/home), non reliee
// a l'azimut.
// ============================================================
#define PIN_HOME                 33
#define HOME_ACTIF_NIVEAU        LOW

// ============================================================
// CALIBRATION DE L'AZIMUT (page /calibration)
// ------------------------------------------------------------
// Le codeur est sur l'arbre moteur (rapport de reduction inconnu) :
// on mesure directement les pas par degre d'azimut sur deux
// lectures de boussole. Tant que la calibration n'est pas validee
// ET que le HOME n'a pas ete trouve depuis le demarrage, l'azimut
// affiche reste l'estimation par le temps moteur.
// ============================================================
#define CALIB_ECART_MIN_DEG       30.0      // ecart minimal entre les 2 points boussole
#define CALIB_VITESSE_HOME        150       // PWM pendant la recherche du HOME
#define CALIB_HOME_TIMEOUT_MS     180000UL  // abandon de la recherche apres 3 min
#define CALIB_BLOCAGE_MS          3000UL    // moteur commande mais codeur immobile...
#define CALIB_BLOCAGE_PAS_MIN     50        // ...moins de 50 pas en 3 s -> arret
#define CALIB_DEGAGEMENT_MS       2000UL    // recul supplementaire apres avoir quitte le HOME
#define CALIB_CONTROLE_SENS_DEG   2.0       // verification du sens EST apres 2 degres

// ============================================================
// BROCHES RESERVEES - PAS ENCORE UTILISEES PAR CE PROGRAMME
// ------------------------------------------------------------
// Documentees ici pour eviter tout conflit d'affectation.
//   Codeur Baumer RS422 CHA (via MAX490 + pont diviseur) : GPIO 34
//   Codeur Baumer RS422 CHB (via MAX490 + pont diviseur) : GPIO 35
//   (HOME : GPIO 33, voir ci-dessus ; GPIO 32 = anemometre)
// ============================================================
