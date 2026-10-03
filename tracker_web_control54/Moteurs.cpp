#include "Moteurs.h"
#include "Config.h"

void setupMoteurs() {
  ledcAttach(RPWM_INC, PWM_FREQ, PWM_RES);
  ledcAttach(LPWM_INC, PWM_FREQ, PWM_RES);
  ledcAttach(RPWM_AZ, PWM_FREQ, PWM_RES);
  ledcAttach(LPWM_AZ, PWM_FREQ, PWM_RES);
  ledcWrite(RPWM_INC, 0);
  ledcWrite(LPWM_INC, 0);
  ledcWrite(RPWM_AZ, 0);
  ledcWrite(LPWM_AZ, 0);
}

void driveInclinaison(int8_t sens, uint8_t vitesse) {
  if (sens > 0)      { ledcWrite(RPWM_INC, vitesse); ledcWrite(LPWM_INC, 0); }
  else if (sens < 0) { ledcWrite(RPWM_INC, 0); ledcWrite(LPWM_INC, vitesse); }
  else               { ledcWrite(RPWM_INC, 0); ledcWrite(LPWM_INC, 0); }
}

void driveAzimut(int8_t sens, uint8_t vitesse) {
  if (sens > 0)      { ledcWrite(RPWM_AZ, vitesse); ledcWrite(LPWM_AZ, 0); }
  else if (sens < 0) { ledcWrite(RPWM_AZ, 0); ledcWrite(LPWM_AZ, vitesse); }
  else               { ledcWrite(RPWM_AZ, 0); ledcWrite(LPWM_AZ, 0); }
}
