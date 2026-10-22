#include "EnregRafales.h"
#include "Config.h"
#include "CapteursI2C.h"

#include <LittleFS.h>

// ============================================================
// TAMPON CIRCULAIRE DES BARRES
// ============================================================

// 6 octets par barre : heure UTC (s depuis 1970) + vitesse x 10.
struct __attribute__((packed)) Barre {
  uint32_t t;
  uint16_t v10;
};

static Barre barres[ENREG_NB_BARRES];   // 34 560 octets de RAM
static int   tete = 0;                  // prochain emplacement a ecrire
static int   nb   = 0;                  // barres presentes

// i = 0 : la plus ancienne.
static int indexDe(int i) {
  return (tete - nb + i + ENREG_NB_BARRES) % ENREG_NB_BARRES;
}

static void pousserBarre(uint32_t t, float kmh) {
  float v = kmh * 10.0f + 0.5f;
  barres[tete].t   = t;
  barres[tete].v10 = (v > 65535.0f) ? 65535 : (uint16_t)v;
  tete = (tete + 1) % ENREG_NB_BARRES;
  if (nb < ENREG_NB_BARRES) nb++;
}

// ============================================================
// SESSION D'ENREGISTREMENT
// ============================================================
// Les heures des barres sont calculees depuis le debut de session :
// heure = heureDebut + k x 15 s. Les barres d'une meme session sont
// donc exactement espacees de 15 s, et une seule lecture RTC suffit
// par session.

static bool          actif = false;
static uint32_t      sessE0 = 0;          // heure RTC du debut de session
static unsigned long sessM0 = 0;          // millis() du debut de session
static uint32_t      sessK  = 0;          // numero de la prochaine barre
static float         maxCourant = 0;      // max de l'intervalle en cours
static unsigned long dernierDepassement = 0;
static bool          rtcManquante = false;

void enregRafalesEchantillon(float kmh) {
  if (!actif) {
    // --- Demarrage : premier echantillon au-dessus de 7 km/h ---
    if (kmh <= ENREG_DEMARRAGE_KMH) return;
    uint32_t e;
    if (!lireEpochRTC(e)) {     // sans heure, impossible de placer les barres
      rtcManquante = true;
      return;
    }
    rtcManquante = false;
    actif = true;
    sessE0 = e;
    sessM0 = millis();
    sessK  = 0;
    maxCourant = kmh;
    dernierDepassement = millis();
    Serial.printf("Bargraphe rafales : enregistrement demarre (%.1f km/h).\n", kmh);
    return;
  }

  // --- Session en cours : maximum de l'intervalle de 15 s ---
  if (kmh > maxCourant) maxCourant = kmh;
  if (kmh > ENREG_DEMARRAGE_KMH) dernierDepassement = millis();
}

bool enregRafalesActif() { return actif; }

// ============================================================
// SAUVEGARDE LITTLEFS PAR MORCEAUX
// ============================================================
// Fichier : en-tete de 16 octets + barres de la plus ancienne a la
// plus recente. Ecrit dans un fichier temporaire, puis renomme : une
// coupure de courant pendant l'ecriture laisse l'ancien fichier intact.

#define ENREG_MAGIC        0x31464152UL   // "RAF1"
#define ENREG_FICHIER_TMP  "/rafales.tmp"
#define ENREG_BARRES_PAR_TOUR 80          // 480 octets ecrits par tour de loop()

struct __attribute__((packed)) EnTete {
  uint32_t magic;
  uint16_t version;
  uint16_t tailleBarre;
  uint32_t nb;
  uint32_t reserve;
};

static bool          fsOK = false;
static bool          modifie = false;        // nouvelles barres depuis la derniere sauvegarde
static unsigned long derniereSauvegarde = 0;
static bool          ecritureEnCours = false;
static File          fic;
static int           savI = 0, savN = 0;

// Barres arrivees pendant l'ecriture : mises de cote, sinon le tampon
// circulaire bougerait sous les pieds de l'ecriture.
static Barre attente[4];
static int   nbAttente = 0;

static void ajouterBarre(uint32_t t, float kmh) {
  if (ecritureEnCours) {
    if (nbAttente < 4) {
      float v = kmh * 10.0f + 0.5f;
      attente[nbAttente].t   = t;
      attente[nbAttente].v10 = (v > 65535.0f) ? 65535 : (uint16_t)v;
      nbAttente++;
    }
    return;
  }
  pousserBarre(t, kmh);
  modifie = true;
}

static void viderAttente() {
  for (int i = 0; i < nbAttente; i++) {
    barres[tete] = attente[i];
    tete = (tete + 1) % ENREG_NB_BARRES;
    if (nb < ENREG_NB_BARRES) nb++;
    modifie = true;
  }
  nbAttente = 0;
}

static void abandonnerEcriture(const char* raison) {
  if (fic) fic.close();
  LittleFS.remove(ENREG_FICHIER_TMP);
  ecritureEnCours = false;
  derniereSauvegarde = millis();   // on reessaiera dans 5 min
  modifie = true;
  viderAttente();
  Serial.printf("Bargraphe rafales : sauvegarde abandonnee (%s).\n", raison);
}

static void demarrerEcriture() {
  fic = LittleFS.open(ENREG_FICHIER_TMP, "w");
  if (!fic) { abandonnerEcriture("ouverture impossible"); return; }
  EnTete h = { ENREG_MAGIC, 1, (uint16_t)sizeof(Barre), (uint32_t)nb, 0 };
  if (fic.write((const uint8_t*)&h, sizeof(h)) != sizeof(h)) { abandonnerEcriture("ecriture en-tete"); return; }
  savI = 0;
  savN = nb;
  modifie = false;
  ecritureEnCours = true;
}

static void poursuivreEcriture() {
  int fin = min(savI + ENREG_BARRES_PAR_TOUR, savN);
  for (; savI < fin; savI++) {
    const Barre &b = barres[indexDe(savI)];
    if (fic.write((const uint8_t*)&b, sizeof(Barre)) != sizeof(Barre)) {
      abandonnerEcriture("partition pleine ?");
      return;
    }
  }
  if (savI < savN) return;          // suite au prochain tour de loop()

  fic.close();
  LittleFS.remove(ENREG_FICHIER);
  if (!LittleFS.rename(ENREG_FICHIER_TMP, ENREG_FICHIER)) { abandonnerEcriture("renommage"); return; }
  ecritureEnCours = false;
  derniereSauvegarde = millis();
  viderAttente();
}

// Lecture d'un fichier de sauvegarde ; false s'il est absent ou invalide.
static bool chargerFichier(const char* nom) {
  File f = LittleFS.open(nom, "r");
  if (!f) return false;
  EnTete h;
  bool ok = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h)
         && h.magic == ENREG_MAGIC && h.tailleBarre == sizeof(Barre)
         && h.nb <= ENREG_NB_BARRES
         && f.size() == sizeof(h) + h.nb * sizeof(Barre);
  if (ok) ok = f.read((uint8_t*)barres, h.nb * sizeof(Barre)) == h.nb * sizeof(Barre);
  f.close();
  if (!ok) { nb = 0; tete = 0; return false; }
  nb = h.nb;
  tete = nb % ENREG_NB_BARRES;
  return true;
}

void setupEnregRafales() {
  // Formate la partition si elle n'a jamais servi (premier demarrage).
  fsOK = LittleFS.begin(true);
  if (!fsOK) {
    Serial.println("Bargraphe rafales : LittleFS indisponible, barres non conservees.");
    return;
  }
  // Le fichier temporaire sert de secours si une coupure a eu lieu
  // entre la suppression de l'ancien et le renommage.
  if (chargerFichier(ENREG_FICHIER) || chargerFichier(ENREG_FICHIER_TMP)) {
    Serial.printf("Bargraphe rafales : %d barres rechargees.\n", nb);
  }
  derniereSauvegarde = millis();
}

// ============================================================
// BOUCLE
// ============================================================

void updateEnregRafales() {
  // --- Fermeture des barres de 15 s (rattrape un eventuel retard) ---
  while (actif && millis() - sessM0 >= (sessK + 1) * ENREG_PAS_MS) {
    ajouterBarre(sessE0 + sessK * (ENREG_PAS_MS / 1000UL), maxCourant);
    maxCourant = 0;
    sessK++;

    // --- Arret : 30 min consecutives sous 7 km/h ---
    if (millis() - dernierDepassement >= ENREG_ARRET_CALME_MS) {
      actif = false;
      Serial.println("Bargraphe rafales : 30 min sous 7 km/h, enregistrement arrete.");
    }
  }

  // --- Sauvegarde : toutes les 5 min, seulement s'il y a du nouveau ---
  if (!fsOK) return;
  if (ecritureEnCours) poursuivreEcriture();
  else if (modifie && millis() - derniereSauvegarde >= ENREG_SAUVEGARDE_MS) demarrerEcriture();
}

// ============================================================
// JSON PAR MORCEAUX
// ============================================================

void enregRafalesJson(uint32_t depuis, EcrireMorceau ecrire) {
  uint32_t maintenant = 0;
  bool rtc = lireEpochRTC(maintenant);
  uint32_t limite = depuis;
  if (depuis == 0 && rtc && maintenant > 86400UL) limite = maintenant - 86400UL;

  char buf[1200];
  int n = snprintf(buf, sizeof(buf),
    "{\"enregistrement\":%s,\"rtc\":%s,\"rtcManquante\":%s,\"stockage\":%s,"
    "\"maintenant\":%lu,\"debutSession\":%lu,\"pas\":%lu,\"demarrage\":%.0f,"
    "\"ligne\":%.0f,\"nbTotal\":%d,\"segments\":[",
    actif ? "true" : "false", rtc ? "true" : "false", rtcManquante ? "true" : "false",
    fsOK ? "true" : "false",
    (unsigned long)maintenant, (unsigned long)(actif ? sessE0 : 0),
    ENREG_PAS_MS / 1000UL, (double)ENREG_DEMARRAGE_KMH, (double)RAFALE_SEUIL_KMH, nb);

  const uint32_t pas = ENREG_PAS_MS / 1000UL;
  bool segOuvert = false;
  uint32_t prec = 0;
  for (int i = 0; i < nb; i++) {
    const Barre &b = barres[indexDe(i)];
    if (b.t <= limite) continue;

    if (n > (int)sizeof(buf) - 40) { buf[n] = '\0'; ecrire(buf); n = 0; }

    if (segOuvert && b.t == prec + pas) {
      n += snprintf(buf + n, sizeof(buf) - n, ",%u", b.v10);
    } else {
      n += snprintf(buf + n, sizeof(buf) - n, "%s[%lu,[%u",
                    segOuvert ? "]]," : "", (unsigned long)b.t, b.v10);
      segOuvert = true;
    }
    prec = b.t;
  }
  n += snprintf(buf + n, sizeof(buf) - n, "%s]}", segOuvert ? "]]" : "");
  buf[n] = '\0';
  ecrire(buf);
}
