#include "Vent.h"
#include "Config.h"
#include "CapteursI2C.h"   // horodatage RTC des evenements
#include "Meteo.h"         // croisement avec le vent regional
#include "EnregRafales.h"  // AJOUT 2 : bargraphe des rafales (un appel par echantillon)

#include <ArduinoJson.h>
#include <Preferences.h>   // NVS : persistance de l'historique

static Preferences prefs;

// AJOUT 1 : cause des mises a plat sur rafales repetees (historique NVS
// et alerte). Litteral unique : histCause garde un pointeur dessus.
static const char CAUSE_RAFALES_REPETEES[] = "rafales repetees";
// Evenements journalises sans alerte : entree dans le palier 40-50 km/h
// et fin de toute mise a plat (le tracker peut se redeployer).
static const char CAUSE_PALIER[]          = "palier";
static const char CAUSE_REDEPLOIEMENT[]   = "redeploiement";
static bool nvsOuvert = false;

// --- Acquisition analogique ---------------------------------
// Le HWFS-1 delivre une tension continue proportionnelle a la
// vitesse. On echantillonne l'ADC toutes les PERIODE_ECHANT_MS,
// puis deux fenetres exploitent ces echantillons :
//   - 5 s : la MOYENNE, pour la vitesse etablie
//   - 2 s : le MAXIMUM, pour la rafale (un pic ne se moyenne pas)
static float         vitesseVentKmh   = 0.0;
static float         vitesseRafaleKmh = 0.0;
static float         tensionActuelleV = 0.0;
static unsigned long lastMesureVent   = 0;
static unsigned long lastMesureRafale = 0;
static unsigned long lastEchantillon  = 0;

static double sommeFenetre = 0.0;   // accumulateur de la moyenne 5 s
static int    nbFenetre    = 0;
static float  maxRafale    = 0.0;   // pic courant de la fenetre 2 s

static bool          securiteVentActive = false;  // palier 2 : retour a plat
static unsigned long debutCalme = 0;   // 0 = aucun delai de sortie en cours
static bool modeVentReduit     = false;   // palier 1 : amplitude reduite

// --- Buffer circulaire pour la tendance ---------------------
static float  bufferVent[NB_ECHANTILLONS_TENDANCE];
static int    bufferVentIndex  = 0;
static int    bufferVentRempli = 0;
static String tendanceVent     = "stable"; // "up" / "down" / "stable"

// --- Historique des evenements vent (persiste en NVS) -------
// Les evenements enregistres avant la v54 NTP n'ont que "HH:MM:SS"
// (heure RTC, UTC) : ils sont recharges tels quels.
static char        histHeure[NB_EVENEMENTS_VENT][20]; // "JJ/MM/AAAA HH:MM:SS", heure de Paris
static float       histVitesse[NB_EVENEMENTS_VENT];
static const char* histCause[NB_EVENEMENTS_VENT];     // "moyenne" / "rafale" / ...
static int         histIndex  = 0;
static int         histRempli = 0;

// ============================================================
// HISTORIQUE
// ============================================================

// Heure NTP de Paris au format strftime donne, ou `absent` si l'heure
// NTP n'est pas encore obtenue. La RTC ne sert plus a l'affichage.
static void horodatageNTP(char* dest, size_t taille, const char* format, const char* absent) {
  if (!formaterHeureParis(dest, taille, format)) {
    strncpy(dest, absent, taille - 1);
    dest[taille - 1] = '\0';
  }
}

static void heureCourteNTP(char* dest, size_t taille) {
  horodatageNTP(dest, taille, "%H:%M:%S", "??:??:??");
}

// --- Persistance NVS ----------------------------------------
// Format compact : "horodatage,vitesse,cause|" par evenement,
// du plus ancien au plus recent.

static void sauvegarderHistoriqueVent() {
  if (!nvsOuvert) return;

  String data = "";
  for (int i = 0; i < histRempli; i++) {
    int idx = (histIndex - histRempli + i + NB_EVENEMENTS_VENT) % NB_EVENEMENTS_VENT;
    data += String(histHeure[idx]) + "," + String(histVitesse[idx], 1) + "," + histCause[idx] + "|";
  }
  prefs.putString("histVent", data);
}

static void chargerHistoriqueVent() {
  String data = prefs.getString("histVent", "");
  if (data.length() == 0) return; // aucun historique sauvegarde

  histIndex  = 0;
  histRempli = 0;

  int pos = 0;
  while (pos < (int)data.length()) {
    int delim1 = data.indexOf(',', pos);
    int delim2 = data.indexOf(',', delim1 + 1);
    int delim3 = data.indexOf('|', delim2 + 1);
    if (delim1 == -1 || delim2 == -1 || delim3 == -1) break;

    String h = data.substring(pos, delim1);
    float  v = data.substring(delim1 + 1, delim2).toFloat();
    String c = data.substring(delim2 + 1, delim3);

    strncpy(histHeure[histIndex], h.c_str(), sizeof(histHeure[histIndex]) - 1);
    histHeure[histIndex][sizeof(histHeure[histIndex]) - 1] = '\0';
    histVitesse[histIndex] = v;
    // On repointe vers les litteraux statiques : histCause ne doit
    // jamais pointer vers une String temporaire.
    // AJOUT 1 : troisieme cause possible, "rafales repetees".
    histCause[histIndex] = (c == "rafale") ? "rafale"
                         : (c == CAUSE_RAFALES_REPETEES) ? CAUSE_RAFALES_REPETEES
                         : (c == CAUSE_PALIER) ? CAUSE_PALIER
                         : (c == CAUSE_REDEPLOIEMENT) ? CAUSE_REDEPLOIEMENT
                         : "moyenne";

    histIndex = (histIndex + 1) % NB_EVENEMENTS_VENT;
    if (histRempli < NB_EVENEMENTS_VENT) histRempli++;

    pos = delim3 + 1;
  }
  histIndex = histRempli % NB_EVENEMENTS_VENT; // remet l'index au bon endroit
  Serial.printf("Historique vent charge : %d evenements.\n", histRempli);
}

void initHistoriqueVentNVS() {
  nvsOuvert = prefs.begin("tracker", false); // namespace "tracker", lecture-ecriture
  if (!nvsOuvert) {
    Serial.println("NVS indisponible : l'historique vent ne sera pas conserve.");
    return;
  }
  chargerHistoriqueVent();
}

// --- Acquittement de l'alerte vent --------------------------
// Une mise en securite leve une alerte qui reste affichee jusqu'a
// ce que l'operateur l'acquitte, MEME SI la securite s'est levee
// entre-temps : quelqu'un qui revient apres une heure d'absence
// doit voir qu'un evenement s'est produit.
// L'acquittement dit "j'ai vu" - il ne leve PAS la securite et ne
// change rien au comportement des moteurs.
static bool          alerteVentEnAttente = false;
static float         alerteVitesse       = 0.0;
// AJOUT 1 : 17 octets (au lieu de 9) pour contenir "rafales repetees".
static char          alerteCause[17]     = "";
static char          alerteHeure[9]      = "";

// Ajoute une ligne a l'historique, sans lever d'alerte. `cause` doit
// etre un litteral statique (histCause garde le pointeur).
static void journaliserEvenementVent(float vitesse, const char* cause) {
  horodatageNTP(histHeure[histIndex], sizeof(histHeure[histIndex]),
                "%d/%m/%Y %H:%M:%S", "heure inconnue");
  histVitesse[histIndex] = vitesse;
  histCause[histIndex]   = cause;

  histIndex = (histIndex + 1) % NB_EVENEMENTS_VENT;
  if (histRempli < NB_EVENEMENTS_VENT) histRempli++;

  sauvegarderHistoriqueVent(); // persiste immediatement en flash
}

// Mise a plat : alerte a acquitter + ligne d'historique.
static void enregistrerEvenementVent(float vitesse, const char* cause) {
  alerteVentEnAttente = true;
  alerteVitesse       = vitesse;
  strncpy(alerteCause, cause, sizeof(alerteCause) - 1);
  alerteCause[sizeof(alerteCause) - 1] = '\0';
  heureCourteNTP(alerteHeure, sizeof(alerteHeure));

  journaliserEvenementVent(vitesse, cause);
}

String historiqueVentJson() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  // Du plus ancien au plus recent.
  for (int i = 0; i < histRempli; i++) {
    int idx = (histIndex - histRempli + i + NB_EVENEMENTS_VENT) % NB_EVENEMENTS_VENT;
    JsonObject o = arr.add<JsonObject>();
    o["h"] = histHeure[idx];
    o["v"] = roundf(histVitesse[idx] * 10.0f) / 10.0f;
    o["c"] = histCause[idx];
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// ============================================================
// SURVEILLANCE DU CAPTEUR
// ============================================================

static float         tensionMaxVue     = 0.0;  // plus haute tension depuis le boot
static unsigned long derniereActivite  = 0;    // dernier depassement du seuil
static unsigned long debutEcartMeteo   = 0;

static bool   anemoSuspect  = false;  // une condition de defaut est vraie
static bool   anemoAcquitte = false;  // l'operateur a pris connaissance
static String anemoCause    = "";

void updateDiagnosticAnemo() {
  bool defaut = false;
  const char* cause = "";

  // --- 1. Aucune tension significative depuis 24 h ---
  // Capteur debranche, fil coupe, ou rotor bloque. Sur un site
  // meme tres abrite, 24 h sans le moindre souffle est anormal.
  if (derniereActivite != 0 &&
      millis() - derniereActivite >= ANEMO_SILENCE_SUSPECT_MS) {
    defaut = true;
    cause = "aucune tension depuis 24 h : capteur debranche ou rotor bloque";
  }

  // --- 2. Ecart avec la meteo regionale ---
  // Pas une preuve : le site est abrite, il peut legitimement
  // etre a zero quand la plaine souffle. D'ou seuil haut + duree.
  if (!defaut && meteo.valide &&
      meteo.ventKmh >= ANEMO_ECART_METEO_KMH && vitesseVentKmh == 0.0) {
    if (debutEcartMeteo == 0) debutEcartMeteo = millis();
    else if (millis() - debutEcartMeteo >= ANEMO_ECART_METEO_MS) {
      defaut = true;
      cause = "vent regional annonce mais rien en local depuis 30 min";
    }
  } else {
    debutEcartMeteo = 0;
  }

  if (defaut && !anemoSuspect) {
    anemoSuspect  = true;
    anemoAcquitte = false;          // toute nouvelle alerte redemande un acquittement
    anemoCause    = cause;
    Serial.printf("ANEMOMETRE SUSPECT : %s\n", cause);
  } else if (!defaut && anemoSuspect) {
    // Le capteur est reparti : l'alerte se resout seule et se
    // rearme pour la prochaine fois.
    anemoSuspect  = false;
    anemoAcquitte = false;
    anemoCause    = "";
    Serial.println("Anemometre : retour a la normale.");
  }
}

void acquitterAnemo() {
  if (!anemoSuspect) return;
  anemoAcquitte = true;
  Serial.println("Alerte anemometre acquittee par l'operateur.");
}

bool anemoEstSuspect()   { return anemoSuspect; }
bool anemoEstAcquitte()  { return anemoAcquitte; }
const String& anemoCauseSuspicion() { return anemoCause; }

// Tension brute lue a l'instant : le repere de diagnostic le plus
// direct. 0 V au repos, elle monte avec la vitesse.
float anemoTensionV()    { return tensionActuelleV; }
float anemoTensionMaxV() { return tensionMaxVue; }

// Secondes depuis le dernier depassement du seuil d'activite.
unsigned long anemoSecondesDepuisActivite() {
  if (derniereActivite == 0) return 0;
  return (millis() - derniereActivite) / 1000UL;
}

// ============================================================
// TENDANCE
// ============================================================

// Compare la moyenne de la 1ere moitie du buffer a celle de la
// 2eme moitie. SEUIL_TENDANCE_KMH evite de reagir au bruit.
static String calculerTendanceVent() {
  if (bufferVentRempli < 4) return "stable"; // pas assez d'historique pour juger

  int moitie = bufferVentRempli / 2;
  float sommeAncien = 0, sommeRecent = 0;
  int nAncien = 0, nRecent = 0;

  for (int i = 0; i < bufferVentRempli; i++) {
    int idx = (bufferVentIndex - bufferVentRempli + i + NB_ECHANTILLONS_TENDANCE) % NB_ECHANTILLONS_TENDANCE;
    if (i < moitie) { sommeAncien += bufferVent[idx]; nAncien++; }
    else            { sommeRecent += bufferVent[idx]; nRecent++; }
  }

  float diff = (sommeRecent / nRecent) - (sommeAncien / nAncien);
  if (diff > SEUIL_TENDANCE_KMH)  return "up";
  if (diff < -SEUIL_TENDANCE_KMH) return "down";
  return "stable";
}

// ============================================================
// EPISODE DE VENT FORT - suivi rapproche
// ============================================================

static float  epPic[NB_VALEURS_EPISODE];    // pic de chaque fenetre de 5 s
static float  epMoy[NB_VALEURS_EPISODE];    // moyenne de chaque fenetre
static int    epIndex  = 0;
static int    epRempli = 0;

static bool   epActif          = false;
static char   epHeure[9]       = "";        // heure RTC du declenchement
static String epTendance       = "stable";
static int    epFenetresCalmes = 0;

static unsigned long epDebutFenetre = 0;
static float         epMaxFenetre   = 0.0;
static double        epSommeFenetre = 0.0;
static int           epNbFenetre    = 0;

static void epAjouter(float pic, float moy) {
  epPic[epIndex] = pic;
  epMoy[epIndex] = moy;
  epIndex = (epIndex + 1) % NB_VALEURS_EPISODE;
  if (epRempli < NB_VALEURS_EPISODE) epRempli++;
}

// Moitie ancienne contre moitie recente du tampon, sur les MOYENNES
// (moins bruitees que les pics). Meme principe que la tendance 30 s.
static String epCalculerTendance() {
  if (epRempli < 4) return "stable";   // pas assez de valeurs pour juger

  int moitie = epRempli / 2;
  float sommeAncien = 0, sommeRecent = 0;
  int nAncien = 0, nRecent = 0;

  for (int i = 0; i < epRempli; i++) {
    int idx = (epIndex - epRempli + i + NB_VALEURS_EPISODE) % NB_VALEURS_EPISODE;
    if (i < moitie) { sommeAncien += epMoy[idx]; nAncien++; }
    else            { sommeRecent += epMoy[idx]; nRecent++; }
  }

  float diff = (sommeRecent / nRecent) - (sommeAncien / nAncien);
  if (diff >  SEUIL_TENDANCE_EPISODE_KMH) return "up";
  if (diff < -SEUIL_TENDANCE_EPISODE_KMH) return "down";
  return "stable";
}

// Appelee pour chaque echantillon de l'ADC (toutes les 100 ms).
static void traiterEpisode(float kmh) {
  // --- Declenchement : un pic au-dela du seuil ouvre l'episode ---
  if (!epActif) {
    if (kmh < SEUIL_EPISODE_KMH) return;

    epActif  = true;
    epIndex  = 0;
    epRempli = 0;                       // nouvel episode : tampon neuf
    epFenetresCalmes = 0;
    epMaxFenetre = 0.0; epSommeFenetre = 0.0; epNbFenetre = 0;
    epDebutFenetre = millis();
    heureCourteNTP(epHeure, sizeof(epHeure));

    // Premier point = le pic declencheur lui-meme, a t = 0.
    epAjouter(kmh, kmh);
    epTendance = epCalculerTendance();
    Serial.printf("EPISODE VENT : pic a %.1f km/h - releve toutes les %lu s.\n",
                  kmh, PERIODE_EPISODE_MS / 1000UL);
    return;
  }

  // --- Episode en cours : on accumule la fenetre de 5 s ---
  epSommeFenetre += kmh;
  epNbFenetre++;
  if (kmh > epMaxFenetre) epMaxFenetre = kmh;

  if (millis() - epDebutFenetre < PERIODE_EPISODE_MS) return;

  // Fenetre terminee : on enregistre pic et moyenne.
  float pic = epMaxFenetre;
  float moy = (epNbFenetre > 0) ? (float)(epSommeFenetre / epNbFenetre) : 0.0f;
  epAjouter(pic, moy);
  epTendance = epCalculerTendance();

  epDebutFenetre = millis();
  epMaxFenetre = 0.0; epSommeFenetre = 0.0; epNbFenetre = 0;

  // --- Fin d'episode : 30 s consecutives sous le seuil ---
  if (pic < SEUIL_EPISODE_KMH) epFenetresCalmes++;
  else                         epFenetresCalmes = 0;

  if (epFenetresCalmes >= EPISODE_FENETRES_CALMES) {
    epActif = false;
    Serial.printf("Episode vent termine : %d valeurs conservees, tendance %s.\n",
                  epRempli, epTendance.c_str());
  }
}

bool          ventEpisodeActif()     { return epActif; }
const String& ventEpisodeTendance()  { return epTendance; }
int           ventEpisodeNbValeurs() { return epRempli; }

String episodeVentJson() {
  JsonDocument doc;
  doc["actif"]    = epActif;
  doc["heure"]    = epHeure;
  doc["tendance"] = epTendance;
  JsonArray arr = doc["valeurs"].to<JsonArray>();

  // Du plus ancien au plus recent.
  for (int i = 0; i < epRempli; i++) {
    int idx = (epIndex - epRempli + i + NB_VALEURS_EPISODE) % NB_VALEURS_EPISODE;
    JsonObject o = arr.add<JsonObject>();
    o["p"] = roundf(epPic[idx] * 10.0f) / 10.0f;
    o["m"] = roundf(epMoy[idx] * 10.0f) / 10.0f;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// ============================================================
// ACQUISITION
// ============================================================

// Lecture de la tension du capteur, moyennee pour lisser le bruit
// de l'ADC. analogReadMilliVolts applique l'etalonnage d'usine de
// l'ESP32 : il corrige la non-linearite bien connue de son ADC,
// ce qu'un analogRead() brut ne fait pas.
// Offset mesure au demarrage, capteur au repos. L'ADC de l'ESP32
// ne descend pas jusqu'a zero : il retourne ~140 mV pour une
// entree nulle. Sans cette correction, le tracker affichait 7 km/h
// en permanence par temps calme - un biais qui fausse des seuils
// places a 30, 40 et 50 km/h.
static float offsetAdcV = 0.0;

static float lireTensionAnemoV() {
  uint32_t somme = 0;
  for (int i = 0; i < NB_LECTURES_ADC; i++) {
    somme += analogReadMilliVolts(PIN_ANEMOMETRE);
  }
  float v = (somme / (float)NB_LECTURES_ADC) / 1000.0f - offsetAdcV;
  return (v > 0.0f) ? v : 0.0f;
}

void setupAnemometre() {
  // Ni pinMode, ni pull-up, ni interruption : le capteur impose
  // lui-meme sa tension. Un pull-up la court-circuiterait.
  // ADC_6db (0-2,2 V, soit 110 km/h) plutot que 11db : l'ESP32 y
  // est plus lineaire et moins bruite. On perd le haut de
  // l'echelle, sans consequence puisque la mise en securite se
  // declenche des 50 km/h.
  analogSetPinAttenuation(PIN_ANEMOMETRE, ADC_6db);

  // Etalonnage du zero : au demarrage les coupelles sont a l'arret
  // dans l'immense majorite des cas. Si du vent souffle deja, la
  // valeur lue serait trop haute : on la plafonne pour ne jamais
  // masquer plus que le bruit de fond de l'ADC.
  delay(50);
  uint32_t somme = 0;
  for (int i = 0; i < 32; i++) { somme += analogReadMilliVolts(PIN_ANEMOMETRE); delay(2); }
  offsetAdcV = (somme / 32.0f) / 1000.0f;
  if (offsetAdcV > 0.25f) offsetAdcV = 0.25f;
  Serial.printf("Anemometre : offset ADC mesure = %.0f mV\n", offsetAdcV * 1000.0f);
  lastMesureVent   = millis();
  lastMesureRafale = millis();
  lastEchantillon  = millis();
  derniereActivite = millis();   // amorce : pas d'alerte "silence" des le boot
}

// ============================================================
// AJOUT 1 - MISE A PLAT SUR RAFALES REPETEES
// ------------------------------------------------------------
// Une rafale isolee a 35 km/h ne declenche rien ; quatre en trois
// minutes signalent un vent instable qui fatigue la structure.
// Cette regle a son propre etat (miseAPlatRafales) : elle ne lit ni
// ne modifie securiteVentActive, si bien qu'elle ne peut jamais lever
// une mise en securite des paliers existants. Le retour a plat est
// demande tant que l'une OU l'autre est active (ventMiseAPlatRequise).
// ============================================================

static unsigned long rafHorodatages[RAFALE_HORODATAGES_MAX]; // millis() de chaque rafale
static int           rafIndex  = 0;
static int           rafRempli = 0;
static bool          rafAuDessus = false;   // hysteresis : en cours de rafale
static bool          miseAPlatRafales = false;
static unsigned long debutCalmeRafales = 0; // debut des 15 min sans > 20 km/h

// Nombre de rafales dont l'horodatage est dans la fenetre glissante.
static int rafalesDansFenetre() {
  int n = 0;
  unsigned long now = millis();
  for (int i = 0; i < rafRempli; i++) {
    if (now - rafHorodatages[i] <= RAFALE_FENETRE_MS) n++;
  }
  return n;
}

// Appelee pour chaque echantillon de l'ADC (toutes les 100 ms).
static void traiterRafalesRepetees(float kmh) {
  unsigned long now = millis();

  // --- Comptage avec hysteresis : 30 km/h pour compter, 25 pour rearmer ---
  if (!rafAuDessus && kmh > RAFALE_SEUIL_KMH) {
    rafAuDessus = true;
    rafHorodatages[rafIndex] = now;
    rafIndex = (rafIndex + 1) % RAFALE_HORODATAGES_MAX;
    if (rafRempli < RAFALE_HORODATAGES_MAX) rafRempli++;

    int n = rafalesDansFenetre();
    Serial.printf("Rafale > %.0f km/h (%.1f) : %d dans les %lu dernieres minutes.\n",
                  (double)RAFALE_SEUIL_KMH, kmh, n, RAFALE_FENETRE_MS / 60000UL);

    // --- Declenchement : 4 rafales dans la fenetre ---
    if (n >= RAFALE_NB && !miseAPlatRafales) {
      miseAPlatRafales = true;
      debutCalmeRafales = 0;
      enregistrerEvenementVent(kmh, CAUSE_RAFALES_REPETEES);
      Serial.printf("RAFALES REPETEES : %d en %lu min - retour a plat automatique !\n",
                    n, RAFALE_FENETRE_MS / 60000UL);
    }
  } else if (rafAuDessus && kmh < RAFALE_RESET_KMH) {
    rafAuDessus = false;
  }

  // --- Redeploiement : 15 min consecutives sans echantillon > 20 km/h ---
  if (miseAPlatRafales) {
    if (kmh > REDEPLOIE_CALME_KMH) {
      if (debutCalmeRafales != 0) {
        Serial.printf("Vent a %.1f km/h : delai de redeploiement (rafales) remis a zero.\n", kmh);
      }
      debutCalmeRafales = 0;
    } else if (debutCalmeRafales == 0) {
      debutCalmeRafales = now;
    } else if (now - debutCalmeRafales >= REDEPLOIE_DELAI_MS) {
      miseAPlatRafales = false;
      debutCalmeRafales = 0;
      // Historique seul : redeploiement si les paliers ne tiennent plus le tracker a plat.
      if (!securiteVentActive) journaliserEvenementVent(kmh, CAUSE_REDEPLOIEMENT);
      Serial.println("Rafales repetees : 15 min de calme, fin de la mise a plat.");
    }
  }
}

// ============================================================
// SIMULATION DU VENT (compilee seulement si SIMULATION_VENT = 1)
// ------------------------------------------------------------
// Remplace la vitesse calculee depuis l'ADC par une valeur de test,
// pour verifier toute la chaine (securites, rafales repetees,
// bargraphe) sans attendre un jour de vent.
// ============================================================
#if SIMULATION_VENT
#warning "SIMULATION_VENT active : la vitesse du vent est SIMULEE. A remettre a 0 avant usage reel."

static float simKmh = 0.0;

// Scenario "rafales" : 4 rafales a 34-38 km/h en moins de 2 min,
// separees par des retombees a 15 km/h (sous le rearmement a 25),
// puis 12 km/h : le bargraphe continue d'enregistrer, le delai de
// redeploiement court (12 < 20).
struct EtapeSim { unsigned long dureeMs; float kmh; };
static const EtapeSim SCENARIO_RAFALES[] = {
  {10000, 10}, {4000, 35}, {15000, 15}, {4000, 36}, {15000, 15},
  {4000, 34}, {15000, 15}, {4000, 38}, {0, 12}   // duree 0 = derniere etape, sans fin
};
static bool          simScenario = false;
static int           simEtape = 0;
static unsigned long simDebutEtape = 0;

void simulationVentFixer(float kmh) {
  simScenario = false;
  simKmh = kmh;
  Serial.printf("SIMULATION : vent fixe a %.1f km/h\n", kmh);
}

void simulationVentScenario() {
  simScenario = true;
  simEtape = 0;
  simDebutEtape = millis();
  Serial.println("SIMULATION : scenario rafales repetees demarre.");
}

static float simulationVitesse() {
  if (simScenario) {
    const EtapeSim &e = SCENARIO_RAFALES[simEtape];
    if (e.dureeMs != 0 && millis() - simDebutEtape >= e.dureeMs) {
      simEtape++;
      simDebutEtape = millis();
      Serial.printf("SIMULATION : etape %d, %.0f km/h\n", simEtape, SCENARIO_RAFALES[simEtape].kmh);
    }
    simKmh = SCENARIO_RAFALES[simEtape].kmh;
  }
  return simKmh;
}
#endif

// Echantillonnage + fenetre longue (5 s) : moyenne, tendance, paliers.
void updateVent() {
  // --- Echantillonnage regulier de l'ADC ---
  if (millis() - lastEchantillon >= PERIODE_ECHANT_MS) {
    lastEchantillon = millis();
    tensionActuelleV = lireTensionAnemoV();
    float kmh = tensionActuelleV * FACTEUR_VOLT_KMH;
#if SIMULATION_VENT
    kmh = simulationVitesse();                     // TESTS : vitesse simulee
#endif

    sommeFenetre += kmh;
    nbFenetre++;
    if (kmh > maxRafale) maxRafale = kmh;          // pic de la fenetre 2 s
    traiterEpisode(kmh);                           // suivi rapproche > 30 km/h
    traiterRafalesRepetees(kmh);                   // AJOUT 1 : rafales repetees
    enregRafalesEchantillon(kmh);                  // AJOUT 2 : bargraphe des rafales

    if (tensionActuelleV >= ANEMO_SEUIL_ACTIVITE_V) derniereActivite = millis();
    if (tensionActuelleV > tensionMaxVue) tensionMaxVue = tensionActuelleV;
  }

  if (millis() - lastMesureVent >= FENETRE_VENT_MS) {
    vitesseVentKmh = (nbFenetre > 0) ? (float)(sommeFenetre / nbFenetre) : 0.0f;
    sommeFenetre = 0.0;
    nbFenetre    = 0;

    bufferVent[bufferVentIndex] = vitesseVentKmh;
    bufferVentIndex = (bufferVentIndex + 1) % NB_ECHANTILLONS_TENDANCE;
    if (bufferVentRempli < NB_ECHANTILLONS_TENDANCE) bufferVentRempli++;
    tendanceVent = calculerTendanceVent();

    // --- Palier 1 : mode reduit (amplitude + vitesse limitees) ---
    if (vitesseVentKmh >= SEUIL_VENT_REDUIT && !modeVentReduit) {
      modeVentReduit = true;
      journaliserEvenementVent(vitesseVentKmh, CAUSE_PALIER);   // historique seul
      Serial.printf("VENT MODERE : %.1f km/h - amplitude limitee a %.0f deg, PWM %d\n",
                    vitesseVentKmh, (double)ANGLE_MAX_VENT_REDUIT_DEG, VITESSE_REDUITE);
    }
    if (vitesseVentKmh < SEUIL_VENT_REDUIT * 0.8) {
      modeVentReduit = false; // desactivation avec hysteresis (32 km/h)
    }

    // --- Palier 2 : mise en securite sur la moyenne ---
    if (vitesseVentKmh >= SEUIL_VENT_SECURITE && !securiteVentActive) {
      securiteVentActive = true;
      debutCalme = 0;
      enregistrerEvenementVent(vitesseVentKmh, "moyenne");
      Serial.printf("ALERTE VENT : %.1f km/h - retour a plat automatique !\n", vitesseVentKmh);
    }
    // La sortie de securite n'est PAS traitee ici : elle depend
    // uniquement de la rafale, voir updateRafale().

    lastMesureVent = millis();
  }
}

// Fenetre courte (2 s) : pic instantane. Permet de reagir a une
// rafale breve que la moyenne sur 5 s aurait lissee.
void updateRafale() {
  if (millis() - lastMesureRafale >= GUST_WINDOW_MS) {
    // Le MAXIMUM des echantillons, pas leur moyenne : une rafale
    // de 2 s noyee dans une moyenne n'est plus une rafale.
    vitesseRafaleKmh = maxRafale;
    maxRafale = 0.0;

    if (vitesseRafaleKmh >= SEUIL_RAFALE_SECURITE && !securiteVentActive) {
      securiteVentActive = true;
      debutCalme = 0;
      enregistrerEvenementVent(vitesseRafaleKmh, "rafale");
      Serial.printf("ALERTE RAFALE : %.1f km/h sur %d s - retour a plat automatique !\n",
                    vitesseRafaleKmh, (int)(GUST_WINDOW_MS / 1000));
    }

    // --- Sortie de securite : 1 h de calme ININTERROMPU ---
    // Le compteur ne demarre que sous SEUIL_VENT_SORTIE et repart
    // de zero au moindre pic au-dessus. Un tracker redeploye entre
    // deux rafales est un tracker qu'on casse.
    if (securiteVentActive) {
      if (vitesseRafaleKmh < SEUIL_VENT_SORTIE) {
        if (debutCalme == 0) {
          debutCalme = millis();
          Serial.printf("Vent retombe sous %.0f km/h : debut du delai de %lu min.\n",
                        (double)SEUIL_VENT_SORTIE, DUREE_CALME_REQUISE_MS / 60000UL);
        } else if (millis() - debutCalme >= DUREE_CALME_REQUISE_MS) {
          securiteVentActive = false;
          debutCalme = 0;
          // Historique seul : redeploiement si aucune autre mise a plat ne tient.
          if (!miseAPlatRafales) journaliserEvenementVent(vitesseRafaleKmh, CAUSE_REDEPLOIEMENT);
          Serial.println("Calme confirme sur toute la duree : fin de la mise en securite.");
        }
      } else if (debutCalme != 0) {
        Serial.printf("Rafale a %.1f km/h : delai de securite remis a zero.\n", vitesseRafaleKmh);
        debutCalme = 0;
      }
    }

    lastMesureRafale = millis();
  }
}

// ============================================================
// ACCESSEURS
// ============================================================

float ventVitesseKmh()       { return vitesseVentKmh; }
float ventRafaleKmh()        { return vitesseRafaleKmh; }
bool  ventSecuriteActive()   { return securiteVentActive; }
bool  ventModeReduit()       { return modeVentReduit; }
const String& ventTendance() { return tendanceVent; }

uint8_t ventVitesseMoteur() {
  return modeVentReduit ? VITESSE_REDUITE : VITESSE_MANUELLE;
}

// Secondes restantes avant que la securite puisse etre levee.
// 0 si la securite est inactive, ou si le delai n'a pas demarre
// (le vent est encore au-dessus du seuil de sortie).
unsigned long ventSecondesAvantLevee() {
  if (!securiteVentActive || debutCalme == 0) return 0;
  unsigned long ecoule = millis() - debutCalme;
  if (ecoule >= DUREE_CALME_REQUISE_MS) return 0;
  return (DUREE_CALME_REQUISE_MS - ecoule) / 1000UL;
}

// ============================================================
// ACQUITTEMENT DE L'ALERTE VENT
// ============================================================

void acquitterVent() {
  if (!alerteVentEnAttente) return;
  alerteVentEnAttente = false;
  Serial.println("Alerte vent acquittee par l'operateur.");
}

bool          ventAlerteEnAttente() { return alerteVentEnAttente; }
float         ventAlerteVitesse()   { return alerteVitesse; }
const char*   ventAlerteCause()     { return alerteCause; }
const char*   ventAlerteHeure()     { return alerteHeure; }

// ============================================================
// AJOUT 1 - ACCESSEURS DES RAFALES REPETEES
// ============================================================

// Retour a plat demande par les paliers existants OU par les rafales
// repetees. C'est ce que lit le controle moteur ; ventSecuriteActive()
// garde son sens d'origine (paliers existants seuls).
bool ventMiseAPlatRequise() { return securiteVentActive || miseAPlatRafales; }

bool ventMiseAPlatRafales() { return miseAPlatRafales; }
int  ventNbRafalesFenetre() { return rafalesDansFenetre(); }

// Secondes restantes avant le redeploiement de la regle rafales.
// 0 si la mise a plat rafales est inactive, ou si le decompte n'a
// pas demarre (vent encore au-dessus de 20 km/h).
unsigned long ventSecondesAvantRedeploiement() {
  if (!miseAPlatRafales || debutCalmeRafales == 0) return 0;
  unsigned long ecoule = millis() - debutCalmeRafales;
  if (ecoule >= REDEPLOIE_DELAI_MS) return 0;
  return (REDEPLOIE_DELAI_MS - ecoule) / 1000UL;
}

// ============================================================
// BANDEAU D'ETAT DE L'INTERFACE - affichage uniquement
// ------------------------------------------------------------
// Lit les etats tenus ci-dessus, dans leur ordre de priorite. Les
// seuils 70 et 20 km/h ne servent qu'a choisir le texte affiche :
// ils ne commandent rien.
// ============================================================

static const float AFFICHAGE_TRES_FORT_KMH = 70.0;
static const float AFFICHAGE_MODERE_KMH    = 20.0;

const char* ventEtatAffichage() {
  if (securiteVentActive) {
    return (vitesseVentKmh >= AFFICHAGE_TRES_FORT_KMH) ? "plat-tres-fort" : "plat";
  }
  if (miseAPlatRafales) return "plat-rafales";
  if (modeVentReduit)   return "palier";
  return (vitesseVentKmh >= AFFICHAGE_MODERE_KMH) ? "modere" : "faible";
}
