// ============================================================
// PageWeb.h - Page HTML servie sur "/" (interface de controle)
// ------------------------------------------------------------
// La page est stockee telle quelle en flash (PROGMEM) et servie
// par server.send_P(), au lieu d'etre reconstruite a chaque
// requete par des centaines de concatenations de String.
// Contenu strictement identique a celui de la version v531.
//
// Elle embarque :
//   - Three.js (CDN) : modele 3D du tracker, azimut + inclinaison
//   - les commandes manuelles des 2 axes + le retour a plat
//   - le bloc meteo (Open-Meteo regionale + BMP280 local)
//   - les voyants de vent  : vert 0-20 / orange clignotant 20-40
//                            / rouge fixe au-dela de 40 km/h
//     (seuils volontairement plus bas que le seuil de securite
//      reel de 50 km/h, pour avertir avant le declenchement)
//   - la fleche de tendance du vent (up / down / stable)
//
// Tout est rafraichi une fois par seconde depuis /status.
// ============================================================

#pragma once

#include <Arduino.h>

static const char PAGE_INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Tracker Solaire</title>
<style>
*{box-sizing:border-box;}
body{font-family:Arial,Helvetica,sans-serif;background:#0f1420;color:#e4e8ee;text-align:left;padding:0;margin:0;}
header{background:#181f2b;color:white;padding:16px 24px;display:flex;
justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;
border-bottom:1px solid #2a3346;}
header h1{font-size:20px;margin:0;} header p{margin:2px 0 0;font-size:12px;color:#9aa5b1;}
#statutWifi{display:flex;align-items:center;gap:6px;font-size:13px;color:#c7cedb;}
#pointWifi{width:9px;height:9px;border-radius:50%;background:#4CAF50;display:inline-block;}
.dashboard{display:grid;gap:34px;max-width:1900px;margin:20px auto;padding:0 16px;
grid-template-columns:1fr;grid-template-areas:'camera' 'incl' 'az' 'meteo';}
@media(min-width:1000px){.dashboard{grid-template-columns:280px minmax(650px,1fr) 280px;
grid-template-areas:'incl camera az' '. meteo .';align-items:start;}}
.card{background:#1a2130;padding:20px;border-radius:14px;
box-shadow:0 2px 10px rgba(0,0,0,0.4);border:1px solid #262f42;}
.card h2{margin-top:0;font-size:16px;color:#e4e8ee;border-bottom:1px solid #2a3346;padding-bottom:8px;}
.center{text-align:center;}
.cam{grid-area:camera;background:#eef1f4;} .incl{grid-area:incl;} .az{grid-area:az;} .met{grid-area:meteo;}
.cam h2{color:#1a2130;border-bottom-color:#d5dbe1;}
.cam p{color:#1a2130;} .cam .petit{color:#5a6472;}
button{width:130px;height:50px;font-size:16px;margin:5px;border-radius:9px;border:none;cursor:pointer;}
.up{background:#4CAF50;color:white;} .stop{background:#FF9800;color:white;}
.down{background:#F44336;color:white;} .left{background:#2196F3;color:white;}
.right{background:#2196F3;color:white;} .flat{background:#9C27B0;color:white;width:225px;}
table{width:100%;font-size:14px;border-collapse:collapse;color:#e4e8ee;}
td{padding:6px 4px;text-align:left;border-bottom:1px solid #262f42;}
.alerte{color:#FF6B6B;font-weight:bold;}
.axeRow{display:flex;justify-content:center;flex-wrap:wrap;}
.badge{display:inline-block;font-size:11px;padding:2px 8px;border-radius:10px;color:white;margin-left:6px;}
.mesure{background:#4CAF50;} .estime{background:#FF9800;}
#threeWrap{width:100%;height:540px;margin:10px auto;border-radius:10px;overflow:hidden;background:#9ec4e4;}
.ventVoyants{display:flex;align-items:center;gap:16px;margin:14px 0;padding-top:10px;border-top:1px solid #262f42;}
.ventLampGroup{display:flex;flex-direction:column;align-items:center;gap:4px;}
.ventLamp{width:18px;height:18px;border-radius:50%;background:#2a3346;box-shadow:inset 0 0 0 1px rgba(255,255,255,0.06);}
.ventLamp.on-green{background:#4CAF50;box-shadow:0 0 10px 2px rgba(76,175,80,0.6);}
.ventLamp.on-orange{background:#FF9800;box-shadow:0 0 10px 2px rgba(255,152,0,0.6);animation:ventBlink 0.8s steps(1) infinite;}
.ventLamp.on-red{background:#F44336;box-shadow:0 0 10px 2px rgba(244,67,54,0.6);}
@keyframes ventBlink{50%{opacity:0.15;box-shadow:none;}}
.ventLampLbl{font-size:10px;color:#9aa5b1;}
.ventTrend{font-size:20px;font-weight:bold;margin-left:4px;}
.ventTrend.up{color:#FFFFFF;background-color:#FF3B30;padding:6px 10px;border-radius:5px;font-weight:bold;}
.ventTrend.down{color:#FFFFFF;background-color:#4CAF50;padding:6px 10px;border-radius:5px;font-weight:bold;}
.ventTrend.stable{color:#9aa5b1;}
.ventAvert{display:none;margin:10px 0 0;padding:8px 10px;border-radius:8px;font-size:12px;
background:rgba(255,152,0,0.12);border:1px solid rgba(255,152,0,0.45);color:#FFB74D;}
.histVent{display:none;margin-top:14px;padding-top:10px;border-top:1px solid #262f42;}
.histVent h3{margin:0 0 6px;font-size:12px;color:#9aa5b1;font-weight:600;}
.histVent ul{list-style:none;margin:0;padding:0;font-size:12px;}
.histVent li{display:flex;justify-content:space-between;gap:8px;padding:3px 0;
border-bottom:1px solid #222a3a;color:#c7cedb;}
.histVent li:last-child{border-bottom:none;}
.histVent .hHeure{color:#9aa5b1;font-variant-numeric:tabular-nums;}
.histVent .hCause{font-size:10px;padding:1px 6px;border-radius:8px;background:#2a3346;color:#9aa5b1;}
.histVent .hCause.rafale{background:#5a3a12;color:#FFB74D;}
.histVent .petitNote{font-size:10px;color:#7a8894;margin:6px 0 0;}
.episodeVent{display:none;margin-top:14px;padding-top:10px;border-top:1px solid #262f42;}
.episodeVent h3{margin:0 0 6px;font-size:12px;color:#9aa5b1;font-weight:600;}
.epEtat{font-size:10px;padding:1px 7px;border-radius:8px;margin-left:6px;background:#2a3346;color:#9aa5b1;}
.epEtat.actif{background:#5a3a12;color:#FFB74D;}
.epBarres{display:flex;align-items:flex-end;gap:2px;height:46px;margin:8px 0 4px;}
.epBarres div{flex:1;min-height:2px;border-radius:2px 2px 0 0;}
.epValeurs{font-size:10px;color:#9aa5b1;font-variant-numeric:tabular-nums;word-spacing:2px;}
.epTend{font-size:12px;margin:6px 0 0;color:#c7cedb;}
.fl{display:inline-block;font-size:12px;font-weight:bold;padding:1px 7px;
border-radius:4px;margin-left:6px;vertical-align:middle;}
.fl.rouge{color:#fff;background:#FF3B30;}
.fl.vert{color:#fff;background:#4CAF50;}
.fl.chaud{color:#fff;background:#FF9800;}
.fl.froid{color:#fff;background:#2196F3;}
.fl.plat{color:#9aa5b1;background:#2a3346;}
.histLoc{margin-top:14px;padding-top:10px;border-top:1px solid #262f42;}
.histLoc h3{margin:0 0 6px;font-size:12px;color:#9aa5b1;font-weight:600;}
.histLoc table{font-size:11px;}
.histLoc td{padding:3px 4px;}
.histLoc .hh{color:#9aa5b1;font-variant-numeric:tabular-nums;width:38%;}
.histLoc .hv{text-align:right;font-variant-numeric:tabular-nums;}
.histLoc .vide{font-size:11px;color:#7a8894;margin:2px 0 0;}
.graphMeteo{margin:10px 0 0;}
.graphMeteo .gTete{display:flex;justify-content:space-between;align-items:baseline;font-size:11px;color:#c7cedb;margin-bottom:3px;}
.graphMeteo .gTete b{font-size:12px;color:#e4e8ee;}
.graphMeteo .gPlage{color:#7a8894;font-size:10px;font-variant-numeric:tabular-nums;}
.gBarres{display:flex;align-items:flex-end;gap:2px;height:54px;background:rgba(255,255,255,0.03);border-radius:4px;padding:2px 2px 0;}
.gBarres div{flex:1;min-height:3px;border-radius:2px 2px 0 0;}
.gAxe{display:flex;justify-content:space-between;font-size:9px;color:#7a8894;margin-top:2px;font-variant-numeric:tabular-nums;}
.histTabWrap{max-height:190px;overflow-y:auto;margin-top:12px;border:1px solid #262f42;border-radius:6px;}
.histLoc th{position:sticky;top:0;background:#1f2738;font-size:10px;color:#9aa5b1;text-align:left;padding:4px;font-weight:600;}
.histLoc th.hv{text-align:right;}
/* AJOUT 2 : bargraphe des rafales */
.bargRaf{margin-top:14px;padding-top:10px;border-top:1px solid #262f42;}
.bargRaf h3{margin:0 0 6px;font-size:12px;color:#9aa5b1;font-weight:600;}
.bargEtat{font-size:11px;color:#c7cedb;margin:0 0 6px;}
.bargEtat .pt{display:inline-block;width:8px;height:8px;border-radius:50%;background:#607d8b;margin-right:6px;vertical-align:middle;}
.bargEtat .pt.on{background:#F44336;box-shadow:0 0 6px #F44336;animation:anemoBlink 1.1s ease-in-out infinite;}
.bargZoom{display:flex;gap:6px;margin:0 0 6px;}
.bargZoom button{width:auto;margin:0;height:26px;padding:0 10px;font-size:11px;border-radius:6px;border:1px solid #34405a;background:#1a2130;color:#c7cedb;cursor:pointer;}
.bargZoom button.actif{background:#2196F3;border-color:#2196F3;color:#fff;}
#bargCanvas{display:block;width:100%;height:170px;background:rgba(255,255,255,0.03);border-radius:4px;}
.bargLeg{display:flex;flex-wrap:wrap;gap:4px 10px;font-size:10px;color:#9aa5b1;margin-top:5px;}
.bargLeg i{display:inline-block;width:9px;height:9px;border-radius:2px;margin-right:4px;vertical-align:middle;}
.histDl{display:inline-block;margin-top:8px;font-size:11px;color:#64b5f6;text-decoration:none;}
.gBarres div.enCours{opacity:0.4;}
.gLegende{font-size:10px;color:#7a8894;margin:6px 0 0;}
.relResume{font-size:11px;color:#c7cedb;margin:0 0 6px;line-height:1.5;}
.relResume b{color:#e4e8ee;}
.anemoAlerte{display:none;margin:12px 0 0;padding:10px 12px;border-radius:8px;font-size:12px;
background:rgba(244,67,54,0.14);border:1px solid #F44336;color:#FF8A80;}
.anemoAlerte.vif{animation:anemoBlink 1.1s ease-in-out infinite;}
@keyframes anemoBlink{50%{background:rgba(244,67,54,0.3);}}
.anemoAlerte b{color:#FFCDD2;display:block;margin-bottom:3px;}
.anemoAlerte .cause{color:#FFAB91;font-style:italic;}
.anemoAlerte button{width:auto;height:30px;font-size:12px;margin:8px 0 0;padding:0 14px;
background:#F44336;color:#fff;border-radius:6px;}
.anemoAlerte .acq{color:#FFCC80;margin:6px 0 0;}
.anemoCompteur{font-size:10px;color:#7a8894;margin:6px 0 0;}
.modeBar{display:flex;gap:8px;justify-content:center;margin-bottom:10px;}
.modeBtn{flex:1;max-width:150px;height:42px;font-size:14px;border-radius:9px;border:1px solid #2a3346;
cursor:pointer;background:#222a3a;color:#9aa5b1;margin:0;width:auto;}
.modeBtn.actif{background:#4CAF50;color:#fff;border-color:#4CAF50;font-weight:bold;}
.soleilInfo{font-size:12px;color:#9aa5b1;margin:8px 0 0;}
.soleilInfo b{color:#e4e8ee;}
.azNote{font-size:11px;color:#FFB74D;margin:6px 0 0;}
</style></head><body>
<header>
<div><h1>☀️ TRACKER SOLAIRE</h1><p>Contrôle et monitoring</p></div>
<div id='statutWifi'><span id='pointWifi'></span><span id='infoLigne'>--</span>
<span id='heureLigne' style='margin-left:10px;'></span></div>
</header>
<div class='dashboard'>
<div class='card center incl'><h2>Inclinaison</h2>
<div class='modeBar'>
<button class='modeBtn' id='btnAuto' onclick="cmd('/suivi_auto')">☀ SUIVI AUTO</button>
<button class='modeBtn' id='btnManuel' onclick="cmd('/mode_manuel')">✋ MANUEL</button>
</div>
<p class='soleilInfo' id='soleilInfo'>Soleil : --</p>
<div class='axeRow'>
<button class='up' onclick="cmd('/monter')">▲ MONTER ▲</button>
<button class='down' onclick="cmd('/descendre')">▼ DESCENDRE ▼</button>
</div>
<p>État : <span id='etatV'>--</span><span id='secV'></span></p>
<div class='axeRow'>
<button class='stop' onclick="cmd('/stop_vertical')">■ STOP ■</button>
<button class='flat' onclick="cmd('/retour_a_plat')">⟲ RETOUR A PLAT (0°)</button>
</div></div>
<div class='card center cam'>
<h2>Modèle 3D du tracker</h2>
<div id='threeWrap'></div>
<p class='petit'>Glisse pour tourner la vue, molette/pincement pour zoomer</p>
<p class='petit' style='color:#c0392b;'>Reglage temporaire azimut (a supprimer une fois valide) :</p>
<p class='petit'>Offset : <span id='offsetVal'>-90</span>&deg;</p>
<input type='range' id='offsetSlider' min='-180' max='180' value='-90' step='5' style='width:80%;' 
oninput="offsetDebug=parseInt(this.value); document.getElementById('offsetVal').textContent=this.value;">
<p><b><span id='azVal'>--</span> °</b> azimut &nbsp;|&nbsp; 
<b><span id='inclVal'>--</span> °</b> inclinaison</p>
<p><span class='badge estime'>AZIMUT ESTIME</span> <span class='badge mesure'>INCLINAISON MESUREE</span></p>
</div>
<div class='card center az'><h2>Azimut</h2>
<div class='axeRow'>
<button class='left' onclick="cmd('/gauche')">◀ GAUCHE</button>
<button class='right' onclick="cmd('/droite')">DROITE ▶</button>
</div>
<p>État : <span id='etatH'>--</span><span id='secH'></span></p>
<button class='stop' onclick="cmd('/stop_horizontal')">■ STOP ■</button>
<p class='azNote'>Azimut toujours manuel : le suivi auto ne pilote que l'inclinaison tant que le codeur Baumer n'est pas installé.</p>
</div>
<div class='card met'><h2>Météo (Open-Meteo, régionale)</h2>
<table>
<tr><td>Température</td><td><span id='temp'>--</span> °C</td></tr>
<tr><td>Vent (régional)</td><td><span id='vent'>--</span> km/h</td></tr>
<tr><td>Vent (local)</td><td><span id='ventLocal'>--</span> km/h</td></tr>
<tr><td>Pic (2s)</td><td><span id='ventRafale'>--</span> km/h</td></tr>
<tr><td>Sécurité vent</td><td><span id='securiteVent'>--</span></td></tr>
<tr><td>Pression</td><td><span id='pression'>--</span> hPa</td></tr>
<tr><td>Pression locale</td><td><span id='pressionLocale'>--</span></td></tr>
</table>
<div class='ventVoyants'>
<div class='ventLampGroup'><span class='ventLamp' id='ventLampGreen'></span><span class='ventLampLbl'>0-20</span></div>
<div class='ventLampGroup'><span class='ventLamp' id='ventLampOrange'></span><span class='ventLampLbl'>20-40</span></div>
<div class='ventLampGroup'><span class='ventLamp' id='ventLampRed'></span><span class='ventLampLbl'>&gt;40 km/h</span></div>
<span class='ventTrend stable' id='ventTrendArrow'>–</span>
</div>
<p class='ventAvert' id='ventReduitMsg'>⚠ Vent modéré — amplitude de mouvement limitée (≤20°)</p>
<div class='episodeVent' id='episodeVent'>
<h3><span id='epTitre'>Épisode de vent</span><span class='epEtat' id='epEtat'></span></h3>
<div class='epBarres' id='epBarres'></div>
<div class='epValeurs' id='epValeurs'></div>
<p class='epTend' id='epTend'></p>
</div>
<div class='histVent' id='histVent'>
<h3>Mises en sécurité (vent)</h3>
<ul id='histListe'></ul>
<p class='petitNote'>Conservé en mémoire flash : survit aux redémarrages.</p>
</div>
<div class='bargRaf' id='bargRaf'>
<h3>Enregistrement des rafales (vitesse max toutes les 15 s, 24 h glissantes)</h3>
<p class='bargEtat' id='bargEtat'>--</p>
<div class='bargZoom' id='bargZoom'>
<button class='actif' onclick="rafZoom(86400,this)">24 h</button>
<button onclick="rafZoom(21600,this)">6 h</button>
<button onclick="rafZoom(3600,this)">1 h</button>
</div>
<canvas id='bargCanvas'></canvas>
<div class='bargLeg'>
<span><i style='background:#4CAF50'></i>7-20</span><span><i style='background:#FF9800'></i>20-40</span>
<span><i style='background:#F44336'></i>40-70</span><span><i style='background:#4B0082;outline:1px solid #7a5cc0'></i>&ge; 70 km/h</span>
<span>- - - 30 km/h · heures UTC</span>
</div>
</div>
<div class='anemoAlerte' id='ventAlerte'>
<b>⚠ MISE EN SÉCURITÉ VENT</b>
<span class='cause' id='ventAlerteTxt'></span>
<p class='acq' id='ventAlerteEtat' style='margin:4px 0 0;'></p>
<button id='ventAlerteBtn' onclick="cmd('/acquitter_vent')">ACQUITTER</button>
</div>
<div class='anemoAlerte' id='anemoAlerte'>
<b>⚠ ANÉMOMÈTRE SUSPECT</b>
<span class='cause' id='anemoCause'></span>
<p class='acq' id='anemoAcq' style='display:none;'>✓ Pris en compte — vérification en cours</p>
<button id='anemoBtn' onclick="cmd('/acquitter_anemo')">ACQUITTER</button>
</div>
<p class='anemoCompteur' id='anemoCompteur'></p>
<div class='histLoc' id='histLoc'>
<h3>Évolution locale (1 relevé toutes les 30 min, 12 h glissantes)</h3>
<p class='vide' id='histLocVide'>Premier relevé en cours...</p>
<div id='histLocContenu' style='display:none;'>
<div class='graphMeteo'>
<div class='gTete'><span><b>Température</b> <span id='gValT'></span><span id='gFlT'></span></span><span class='gPlage' id='gPlageT'></span></div>
<div class='gBarres' id='gBarresT'></div>
<div class='gAxe' id='gAxeT'></div>
</div>
<div class='graphMeteo'>
<div class='gTete'><span><b>Pression</b> <span id='gValP'></span><span id='gFlP'></span></span><span class='gPlage' id='gPlageP'></span></div>
<div class='gBarres' id='gBarresP'></div>
<div class='gAxe' id='gAxeP'></div>
</div>
<div class='histTabWrap'>
<table id='histLocTable'>
<thead><tr><th>Heure (UTC)</th><th class='hv'>Température</th><th class='hv'>Pression</th></tr></thead>
<tbody id='histLocCorps'></tbody>
</table>
</div>
<a class='histDl' href='/api/historique-meteo.csv' download='releves_meteo.csv'>⬇ Télécharger le tableau (CSV)</a>
</div>
</div>
<div class='histLoc' id='histVent30'>
<h3>Évolution du vent local (moyenne et rafale max par 30 min, 12 h glissantes)</h3>
<p class='vide' id='hv30Vide'>Premier relevé dans 30 min...</p>
<div id='hv30Contenu' style='display:none;'>
<div class='graphMeteo'>
<div class='gTete'><span><b>Vent moyen</b> <span id='gValVM'></span><span id='gFlV'></span></span><span class='gPlage' id='gPlageVM'></span></div>
<div class='gBarres' id='gBarresVM'></div>
<div class='gAxe' id='gAxeVM'></div>
</div>
<div class='graphMeteo'>
<div class='gTete'><span><b>Rafale max</b> <span id='gValVR'></span></span><span class='gPlage' id='gPlageVR'></span></div>
<div class='gBarres' id='gBarresVR'></div>
<div class='gAxe' id='gAxeVR'></div>
</div>
<p class='gLegende'>Barres : vert &lt; 20 km/h, orange 20-40, rouge &gt; 40. La dernière barre, plus pâle, est la demi-heure en cours.</p>
<div class='histTabWrap'>
<table>
<thead><tr><th>Heure (UTC)</th><th class='hv'>Vent moyen</th><th class='hv'>Rafale max</th></tr></thead>
<tbody id='hv30Corps'></tbody>
</table>
</div>
<a class='histDl' href='/api/releves-vent.csv' download='releves_vent.csv'>⬇ Télécharger le tableau (CSV)</a>
</div>
</div>
<div class='histLoc' id='relAnemo'>
<h3>Relevés de l'anémomètre (vérification de la constante vent)</h3>
<p class='relResume' id='relResume'>--</p>
<p class='vide' id='relVide'>Aucun relevé pour l'instant : une ligne s'ajoute pour chaque tranche de 10 min où l'anémomètre tourne.</p>
<div id='relContenu' style='display:none;'>
<div class='histTabWrap'>
<table>
<thead><tr><th>Heure (UTC)</th><th class='hv'>Tension</th><th class='hv'>Vent local</th><th class='hv'>Vent régional</th><th class='hv'>Constante</th></tr></thead>
<tbody id='relCorps'></tbody>
</table>
</div>
<a class='histDl' href='/api/releves-anemo.csv' download='releves_anemometre.csv'>⬇ Télécharger le tableau (CSV, avec les rafales)</a>
</div>
<p class='gLegende'>Vent local = tension × constante. Constante suggérée = vent régional ÷ tension, calculée seulement quand le vent régional dépasse 5 km/h. Le vent régional est un modèle à 10 m de hauteur sur une maille de plusieurs km : l'écart avec un site abrité est normal, il faut de nombreux relevés par jour de vent.</p>
</div>
<p id='pluieTxt'>Risque de pluie : -- %</p>
<p class='petit'>Mise à jour il y a <span id='meteoAge'>--</span>s. Vent/pression : source en ligne, pas encore de capteur local.</p>
</div>
</div>
<p class='petit center' style='margin-bottom:20px;'>Mise à jour automatique toutes les secondes.</p>
<script src='https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js'></script>
<script src='https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js'></script>
<script>
var wrap=document.getElementById('threeWrap');
var scene=new THREE.Scene();
var COUL_HZ=new THREE.Color(0xdcecf7), COUL_HAUT=new THREE.Color(0x2f6fb5);
scene.fog=new THREE.Fog(COUL_HZ.getHex(), 34, 105);
// Ciel : degrade + couche nuageuse procedurale (bruit fractal),
// derivee lentement par le vent. Aucune texture a embarquer.
var skyMat=new THREE.ShaderMaterial({
uniforms:{
cHaut:{value:COUL_HAUT.clone()}, cBas:{value:COUL_HZ.clone()},
cNuage:{value:new THREE.Color(0xffffff)},
couverture:{value:0.46}, temps:{value:0}},
vertexShader:'varying vec3 vP;void main(){vP=position;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',
fragmentShader:[
'uniform vec3 cHaut;uniform vec3 cBas;uniform vec3 cNuage;',
'uniform float couverture;uniform float temps;varying vec3 vP;',
'float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}',
'float bruit(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);',
'return mix(mix(hash(i),hash(i+vec2(1.0,0.0)),f.x),',
'mix(hash(i+vec2(0.0,1.0)),hash(i+vec2(1.0,1.0)),f.x),f.y);}',
'float fbm(vec2 p){float v=0.0,a=0.5;',
'for(int i=0;i<5;i++){v+=a*bruit(p);p*=2.02;a*=0.5;}return v;}',
'void main(){',
'vec3 d=normalize(vP);float h=d.y;',
'float t=clamp((h+0.05)*1.6,0.0,1.0);',
'vec3 col=mix(cBas,cHaut,t);',
'if(h>0.015){',
// Projection sur un plan horizontal : les nuages se resserrent
// vers l'horizon, comme une vraie couche nuageuse.
'vec2 uv=d.xz/(h+0.22)*0.75+vec2(temps*0.006,temps*0.003);',
'float n=fbm(uv*5.5);',
'float masse=smoothstep(couverture,couverture+0.26,n);',
'float fondu=smoothstep(0.015,0.20,h);',
// Ourlet plus clair sur le dessus des masses nuageuses.
'float relief=smoothstep(couverture+0.10,couverture+0.34,n);',
'vec3 nuage=mix(cNuage*0.72,cNuage,relief);',
'col=mix(col,nuage,masse*fondu*0.92);}',
'gl_FragColor=vec4(col,1.0);}'].join(''),
side:THREE.BackSide, depthWrite:false});
scene.add(new THREE.Mesh(new THREE.SphereGeometry(75,32,20), skyMat));
var camera=new THREE.PerspectiveCamera(45, wrap.clientWidth/wrap.clientHeight, 0.1, 100);
camera.position.set(10.5,5.6,11.5);
var renderer=new THREE.WebGLRenderer({antialias:true});
renderer.setSize(wrap.clientWidth, wrap.clientHeight);
renderer.setPixelRatio(Math.min(window.devicePixelRatio,2));
renderer.shadowMap.enabled=true; renderer.shadowMap.type=THREE.PCFSoftShadowMap;
// Chaine colorimetrique correcte + courbe de reponse cinema :
// c'est ce qui separe un rendu "3D d'ecolier" d'une image credible.
renderer.outputEncoding=THREE.sRGBEncoding;
renderer.toneMapping=THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure=0.82;
wrap.appendChild(renderer.domElement);
var controls=new THREE.OrbitControls(camera, renderer.domElement);
controls.enableDamping=true; controls.dampingFactor=0.08;
controls.target.set(0,1.6,0); controls.minDistance=4; controls.maxDistance=45;
controls.maxPolarAngle=Math.PI/2 - 0.02;
var ambiante=new THREE.AmbientLight(0xffffff,0.25); scene.add(ambiante);
var hemi=new THREE.HemisphereLight(0x9ec4e4,0x3f5230,0.38); scene.add(hemi);
var sun=new THREE.DirectionalLight(0xffffff,0.9);
sun.position.set(4,6,3); sun.castShadow=true;
sun.shadow.mapSize.set(2048,2048);
sun.shadow.camera.left=-16; sun.shadow.camera.right=16;
sun.shadow.camera.top=16; sun.shadow.camera.bottom=-16;
sun.shadow.camera.far=60; sun.shadow.bias=-0.0015;
sun.shadow.radius=2;
scene.add(sun);

// Herbe procedurale : un petit canvas bruite, repete. Aucune image
// a embarquer (la flash de l'ESP32 est deja bien remplie).
function texHerbe(base,t1,t2,n){
var c=document.createElement('canvas'); c.width=c.height=128;
var g=c.getContext('2d'); g.fillStyle=base; g.fillRect(0,0,128,128);
for(var i=0;i<n;i++){
g.fillStyle=(i%2)?t1:t2;
g.fillRect(Math.random()*128,Math.random()*128,2,2);
}
var t=new THREE.CanvasTexture(c);
t.encoding=THREE.sRGBEncoding;   // sinon la texture ressort delavee
t.wrapS=t.wrapT=THREE.RepeatWrapping;
return t;
}
// Terrain lointain, jusqu'a l'horizon : le brouillard fond le bord.
var texLoin=texHerbe('#3f5730','rgba(52,72,40,0.55)','rgba(76,100,56,0.5)',2600);
texLoin.repeat.set(30,30);
var terrain=new THREE.Mesh(new THREE.CircleGeometry(70,48),
new THREE.MeshStandardMaterial({map:texLoin,roughness:1}));
terrain.rotation.x=-Math.PI/2; terrain.position.y=-0.02; scene.add(terrain);
// Parcelle tondue autour du tracker, un peu plus claire.
// La pelouse du tracker forme une plateforme qui domine le champ.
// Le dessus affleure y=0 : le tracker n'a pas a etre deplace.
var NIV_CHAMP=-0.55, PL_X=19, PL_Z=15, PL_H=0.55;
terrain.position.y=NIV_CHAMP-0.02;

var texPres=texHerbe('#4a6437','rgba(60,82,45,0.5)','rgba(88,114,62,0.5)',2200);
texPres.repeat.set(5,4);
var ground=new THREE.Mesh(new THREE.BoxGeometry(PL_X,PL_H,PL_Z),
new THREE.MeshStandardMaterial({map:texPres,roughness:1}));
ground.position.y=-PL_H/2; ground.receiveShadow=true; ground.castShadow=true;
scene.add(ground);

// Demarcation : le muret bas en beton qui ceinture la pelouse.
var bordM=new THREE.MeshLambertMaterial({color:0xb8b3a6});
function bordure(x,z,lx,lz){
var b=new THREE.Mesh(new THREE.BoxGeometry(lx,0.30,lz), bordM);
b.position.set(x,-0.04,z);
b.castShadow=true; b.receiveShadow=true; scene.add(b);
}
bordure(0,  PL_Z/2, PL_X+0.3, 0.3);
bordure(0, -PL_Z/2, PL_X+0.3, 0.3);
bordure( PL_X/2, 0, 0.3, PL_Z);
bordure(-PL_X/2, 0, 0.3, PL_Z);

// --- Decor : reliefs et boisement (paysage jurassien) -------
// Tout est procedural et pose une seule fois. Generateur a graine
// fixe : la scene est identique a chaque rechargement.
var graine=20260930;
function alea(){ graine=(graine*1103515245+12345)&0x7fffffff; return graine/0x7fffffff; }

// Le site reel est une prairie plate fermee par une lisiere de
// feuillus : pas de relief a l'horizon.

// Sapins et feuillus. Geometries et materiaux partages : quelques
// dizaines d'arbres ne coutent presque rien. Pas d'ombre portee,
// ils sont hors du cadrage de la shadow camera (+-5.5).
var troncG=new THREE.CylinderGeometry(0.08,0.14,1.3,6);
var troncM=new THREE.MeshLambertMaterial({color:0x30271e});
var sapinG=new THREE.ConeGeometry(0.8,2.6,8);
var sapinM=new THREE.MeshLambertMaterial({color:0x1b2a1a});
// Verts naturels, desatures : les teintes vives "jeu video" sont
// ce qui trahit le plus une scene de synthese.
var feuG=new THREE.SphereGeometry(1,7,6);
var FEUILLAGES=[0x2e3d22,0x364727,0x3d4f2a,0x47512c,0x53552e,0x5c5730];
var feuM=FEUILLAGES.map(function(c){
return new THREE.MeshLambertMaterial({color:c,flatShading:true});
});
function matFeuillu(){ return feuM[(alea()*feuM.length)|0]; }

// Houppier en grappe : plusieurs lobes irreguliers. Beaucoup plus
// organique qu'une forme unique, pour trois fois moins cher qu'un
// vrai feuillage. Reserve aux arbres proches ; la lisiere lointaine
// se contente d'un lobe, elle est de toute facon noyee de brume.
function houppier(ech,mat,lobes){
var g=new THREE.Group();
for(var k=0;k<lobes;k++){
var s=new THREE.Mesh(feuG,mat);
s.position.set((alea()-0.5)*1.2,(alea()-0.35)*0.9,(alea()-0.5)*1.2);
s.scale.set(0.55+alea()*0.45, 0.5+alea()*0.4, 0.55+alea()*0.45);
g.add(s);
}
g.scale.setScalar(ech);
return g;
}

// tronc=false pour la lisiere lointaine : le pied est masque par
// les arbres voisins, autant economiser la moitie des objets.
// y0 : niveau du sol sous l'arbre (pelouse a 0, champ en contrebas).
function planterArbre(x,z,ech,sapin,tronc,lobes,y0){
y0=y0||0;
if(tronc){
var t=new THREE.Mesh(troncG,troncM);
t.position.set(x,y0+0.62*ech,z); t.scale.setScalar(ech);
t.castShadow=true; scene.add(t);
}
if(sapin){
var c=new THREE.Mesh(sapinG,sapinM);
c.position.set(x,y0+2.2*ech,z);
c.scale.set(ech*(0.85+alea()*0.3), ech*(0.85+alea()*0.45), ech*(0.85+alea()*0.3));
c.castShadow=true; scene.add(c);
return;
}
var h=houppier(ech,matFeuillu(),lobes||1);
// Sans tronc (rideau lointain), le houppier descend jusqu'au sol :
// une masse continue, comme une haie vue de loin.
h.position.set(x,y0+(tronc?1.75:1.05)*ech,z);
h.traverse(function(o){ if(o.isMesh) o.castShadow=true; });
scene.add(h);
}

// Haie : alignement d'arbres de (x0,z0) a (x1,z1), avec un peu de
// desordre lateral pour ne pas faire "rangee plantee au cordeau".
function haie(x0,z0,x1,z1,nb,eMin,eMax,partSapin,lobes){
for(var i=0;i<nb;i++){
var t=(nb>1)?i/(nb-1):0.5;
var x=x0+(x1-x0)*t+(alea()-0.5)*2.2;
var z=z0+(z1-z0)*t+(alea()-0.5)*2.2;
// Jamais d'arbre sur la pelouse du tracker.
if(Math.abs(x)<PL_X/2+1.2 && Math.abs(z)<PL_Z/2+1.2) continue;
planterArbre(x,z,eMin+alea()*(eMax-eMin),alea()<partSapin,true,lobes||3,NIV_CHAMP);
}
}

// Lisiere de fond : rideau continu qui ferme l'horizon, comme la
// haie du site. Quelques coniferes seulement, le reste en feuillus.
// Rideau lointain, clairseme : ferme l'horizon sans concurrencer
// les trois haies. Noye dans la brume, un seul lobe suffit.
for(var i=0;i<78;i++){
var a=(i/78)*Math.PI*2 + (alea()-0.5)*0.09;
var d=58+alea()*7;
planterArbre(Math.sin(a)*d, Math.cos(a)*d, 1.4+alea()*1.1, alea()<0.12, false, 1, NIV_CHAMP);
}

// --- Les trois haies ---------------------------------------
// Orientees dans le repere de la vue par defaut : la camera
// regarde le tracker depuis (+x,+z), donc la gauche de l'ecran
// pointe vers (+x,-z) et la droite vers (-x,+z).
haie(  5.5,-17.7, -10.0,-34.7, 20, 1.8, 3.1, 0.12, 3);  // completement a gauche
haie(-18.2,  3.8, -33.7,-13.1, 20, 1.8, 3.1, 0.12, 3);  // completement a droite
haie( 12.6, -4.0,   4.5,-12.8, 10, 1.3, 2.2, 0.10, 4);  // a gauche du pied du tracker

// Deux gros feuillus isoles, comme le grand arbre du terrain.
planterArbre(-21, 15, 2.6, false, true, 6, NIV_CHAMP);
planterArbre(26, -13, 2.2, false, true, 5, NIV_CHAMP);

// Soleil visible, place a sa VRAIE position (azimut/elevation NOAA).
var astre=new THREE.Mesh(new THREE.SphereGeometry(1.5,16,12),
new THREE.MeshBasicMaterial({color:0xfff6cf,fog:false}));
scene.add(astre);
var halo=new THREE.Mesh(new THREE.SphereGeometry(3.4,16,12),
new THREE.MeshBasicMaterial({color:0xffe9a8,transparent:true,opacity:0.2,depthWrite:false,fog:false}));
scene.add(halo);
// Dalle de fondation, comme celle coulee sous le mat sur le site.
var betonM=new THREE.MeshLambertMaterial({color:0x706d65});
var dalle=new THREE.Mesh(new THREE.BoxGeometry(1.45,0.14,1.45), betonM);
dalle.position.y=0.07; dalle.receiveShadow=true; dalle.castShadow=true;
scene.add(dalle);
var socle=new THREE.Mesh(new THREE.BoxGeometry(0.55,0.28,0.55),
new THREE.MeshStandardMaterial({color:0xaaa89c,roughness:0.9}));
socle.position.y=0.28; socle.castShadow=true; socle.receiveShadow=true; scene.add(socle);
var poleMat=new THREE.MeshStandardMaterial({color:0x15171c,metalness:0.4,roughness:0.5});
var pole=new THREE.Mesh(new THREE.BoxGeometry(0.16,1.0,0.16), poleMat);
pole.position.y=0.28+0.5; pole.castShadow=true; scene.add(pole);
var azimutGroup=new THREE.Object3D(); azimutGroup.position.y=1.28; scene.add(azimutGroup);
var moteurAz=new THREE.Mesh(new THREE.BoxGeometry(0.22,0.16,0.22),
new THREE.MeshStandardMaterial({color:0x111318}));
moteurAz.castShadow=true; azimutGroup.add(moteurAz);
var panelPivot=new THREE.Object3D(); panelPivot.position.y=0.1; azimutGroup.add(panelPivot);
var moteurIncl=new THREE.Mesh(new THREE.BoxGeometry(0.16,0.16,0.16),
new THREE.MeshStandardMaterial({color:0x333333}));
panelPivot.add(moteurIncl);
var panelMat=new THREE.MeshStandardMaterial({color:0x22252b,metalness:0.3,roughness:0.55});
var cadreMat=new THREE.LineBasicMaterial({color:0x000000});
var panelW=4.0, panelD=3.6, cols=4, rows=2, gap=0.04;
var cellW=panelW/cols - gap, cellD=panelD/rows - gap;
for(var c=0;c<cols;c++){for(var r=0;r<rows;r++){
var px=-panelW/2 + cellW/2 + c*(panelW/cols);
var pz=-panelD/2 + cellD/2 + r*(panelD/rows);
var geo=new THREE.BoxGeometry(cellW,0.03,cellD);
var m=new THREE.Mesh(geo, panelMat);
m.position.set(px,0,pz); m.castShadow=true; m.receiveShadow=true;
panelPivot.add(m);
var edges=new THREE.LineSegments(new THREE.EdgesGeometry(geo), cadreMat);
edges.position.set(px,0,pz); panelPivot.add(edges);
}}
var tube=new THREE.Mesh(new THREE.CylinderGeometry(0.04,0.04,panelD,12),
new THREE.MeshStandardMaterial({color:0x14161a,metalness:0.5,roughness:0.4}));
tube.rotation.x=Math.PI/2; tube.castShadow=true; panelPivot.add(tube);
var azCible=0, inclCible=0, offsetDebug=-90;
var azSol=140, elevSol=35, solaireDispo=false;

// Palettes : plein jour / rasant (lever-coucher) / nuit.
var JOUR ={haut:0x2f6fb5, bas:0xdcecf7, lum:0xffffff, iLum:2.1, iAmb:0.26, nua:0xeef3f9};
var RASANT={haut:0x3b5c86, bas:0xf0b878, lum:0xffb06a, iLum:1.5, iAmb:0.22, nua:0xd9986a};
var NUIT ={haut:0x0a1428, bas:0x24334d, lum:0x8fa6d8, iLum:0.25, iAmb:0.12, nua:0x2e3a50};

function melange(a,b,t,cle){
return new THREE.Color(a[cle]).lerp(new THREE.Color(b[cle]),t);
}
// Three.js r128 traite les couleurs de materiau comme lineaires.
// Avec une sortie sRGB il faut donc les convertir, sinon tout
// ressort delave. (Le ciel est un ShaderMaterial : il ecrit
// directement en sRGB, on le laisse tel quel.)
function versLineaire(c){ return c.clone().convertSRGBToLinear(); }
function ambiance(elev){
var de,vers,t;
if(elev>=12){ de=JOUR; vers=JOUR; t=0; }
else if(elev>=0){ de=RASANT; vers=JOUR; t=elev/12; }
else { de=NUIT; vers=RASANT; t=Math.min(Math.max((elev+8)/8,0),1); }
skyMat.uniforms.cHaut.value.copy(melange(de,vers,t,'haut'));
skyMat.uniforms.cBas.value.copy(melange(de,vers,t,'bas'));
skyMat.uniforms.cNuage.value.copy(melange(de,vers,t,'nua'));
scene.fog.color.copy(versLineaire(skyMat.uniforms.cBas.value.clone().lerp(skyMat.uniforms.cNuage.value,0.35)));
sun.color.copy(versLineaire(melange(de,vers,t,'lum')));
sun.intensity=de.iLum+(vers.iLum-de.iLum)*t;
ambiante.intensity=de.iAmb+(vers.iAmb-de.iAmb)*t;
astre.material.color.copy(melange(de,vers,t,'lum'));  // MeshBasic + fog off : reste en sRGB
halo.material.color.copy(melange(de,vers,t,'lum'));
var visible=elev>-2;
astre.visible=visible; halo.visible=visible;
halo.material.opacity=elev<12?0.32:0.2;   // halo plus marque au ras de l'horizon
}
function placerSoleil(){
// Meme convention d'azimut que le panneau, offsetDebug compris,
// pour que les deux restent coherents entre eux.
var th=THREE.MathUtils.degToRad(azSol+offsetDebug);
var el=THREE.MathUtils.degToRad(elevSol);
var d=48, cs=Math.cos(el);
var x=d*cs*Math.sin(th), y=d*Math.sin(el), z=d*cs*Math.cos(th);
astre.position.set(x,y,z); halo.position.set(x,y,z);
// La lumiere directionnelle suit la meme direction, plus pres pour
// que la shadow camera (+-5.5) reste bien cadree sur le tracker.
sun.position.set(x*0.18, Math.max(y*0.18,0.8), z*0.18);
ambiance(elevSol);
}
// Les couleurs de materiau ont ete saisies en sRGB ; Three.js r128
// les traite comme lineaires. On les convertit une fois, toute la
// scene construite. Les MeshBasicMaterial (soleil, halo) sont
// exclus : ils ecrivent leur couleur telle quelle.
(function(){var vus=[];scene.traverse(function(o){
if(o.isMesh&&o.material&&o.material.color&&!o.material.isMeshBasicMaterial&&vus.indexOf(o.material)<0){
vus.push(o.material);o.material.color.convertSRGBToLinear();}});})();
hemi.color.convertSRGBToLinear(); hemi.groundColor.convertSRGBToLinear();

placerSoleil();

function animate(){requestAnimationFrame(animate);
skyMat.uniforms.temps.value=performance.now()*0.001;   // derive des nuages
azimutGroup.rotation.y += (THREE.MathUtils.degToRad(azCible+offsetDebug) - azimutGroup.rotation.y)*0.08;
panelPivot.rotation.x += (THREE.MathUtils.degToRad(-inclCible) - panelPivot.rotation.x)*0.08;
if(solaireDispo) placerSoleil();
controls.update(); renderer.render(scene,camera);}
animate();
window.addEventListener('resize',function(){
camera.aspect=wrap.clientWidth/wrap.clientHeight; camera.updateProjectionMatrix();
renderer.setSize(wrap.clientWidth, wrap.clientHeight);});
function cmd(url){fetch(url).then(refresh);}
function refresh(){
fetch('/status').then(r=>r.json()).then(d=>{
azCible=d.az; inclCible=d.incl;
if(d.solaireOK){azSol=d.azSolaire; elevSol=d.elevSolaire; solaireDispo=true;}
document.getElementById('azVal').textContent=d.az.toFixed(1);
document.getElementById('inclVal').textContent=d.incl.toFixed(1);
document.getElementById('etatV').textContent=d.etatV;
document.getElementById('etatH').textContent=d.etatH;
document.getElementById('secV').textContent=(d.secV>0)?' (arret auto dans '+d.secV+'s)':'';
document.getElementById('secH').textContent=(d.secH>0)?' (arret auto dans '+d.secH+'s)':'';
document.getElementById('infoLigne').textContent='Uptime: '+d.uptime+'s | WiFi: '+d.rssi+' dBm';
document.getElementById('heureLigne').textContent=d.rtcOK ? ('| '+d.heureRTC+' (RTC)') : '| RTC indisponible';
var lg=document.getElementById('ventLampGreen'),lo=document.getElementById('ventLampOrange'),lr=document.getElementById('ventLampRed');
lg.className='ventLamp'; lo.className='ventLamp'; lr.className='ventLamp';
if(d.ventKmh<20){lg.classList.add('on-green');}else if(d.ventKmh<40){lo.classList.add('on-orange');}else{lr.classList.add('on-red');}
var ta=document.getElementById('ventTrendArrow'); ta.className='ventTrend';
if(d.trend=='up'){ta.textContent='↑';ta.classList.add('up');}
else if(d.trend=='down'){ta.textContent='↓';ta.classList.add('down');}
else{ta.textContent='→';ta.classList.add('stable');}
var vr=document.getElementById('ventRafale');
if(vr&&typeof d.ventRafaleKmh==='number') vr.textContent=d.ventRafaleKmh.toFixed(1);
var vrm=document.getElementById('ventReduitMsg');
if(vrm) vrm.style.display=d.modeVentReduit?'block':'none';
// Alerte vent : apparait a chaque mise en securite et reste affichee
// jusqu'a acquittement, meme si la securite s'est levee entre-temps.
// Acquitter = "j'ai vu" : cela ne leve pas la securite.
var av=document.getElementById('ventAlerte');
if(av){
if(d.alerteVent){
av.style.display='block';
av.className='anemoAlerte vif';
// AJOUT 1 : troisieme cause, rafales repetees.
var src=(d.alerteCause=='rafale')?'Rafale':(d.alerteCause=='rafales repetees')?'Rafales répétées (dernière)':'Vent moyen';
document.getElementById('ventAlerteTxt').textContent=
src+' à '+(+d.alerteVitesse).toFixed(1)+' km/h, à '+d.alerteHeure+' (heure RTC)';
var et=document.getElementById('ventAlerteEtat');
if(d.securiteVent){
var t=d.secAvantLevee||0;
et.textContent=(t>0)
? 'Sécurité toujours active — levée possible dans '+Math.ceil(t/60)+' min'
: "Sécurité toujours active — attente d'une accalmie";
} else if(d.miseAPlatRafales){ // AJOUT 1
var r=d.secAvantRedeploiement||0;
et.textContent=(r>0)?'Mise à plat (rafales répétées) — redéploiement dans '+Math.ceil(r/60)+' min'
:'Mise à plat (rafales répétées) — attente de 15 min sous 20 km/h';
} else { et.textContent='Sécurité levée.'; }
} else { av.style.display='none'; }
}
// Alerte capteur : reste affichee jusqu'a acquittement manuel.
// Une fois acquittee elle cesse de clignoter mais ne disparait
// pas : elle ne s'efface que si le capteur redonne signe de vie.
var aa=document.getElementById('anemoAlerte');
if(aa){
if(d.anemoSuspect){
aa.style.display='block';
aa.className='anemoAlerte'+(d.anemoAcquitte?'':' vif');
document.getElementById('anemoCause').textContent=d.anemoCause||'';
document.getElementById('anemoAcq').style.display=d.anemoAcquitte?'block':'none';
document.getElementById('anemoBtn').style.display=d.anemoAcquitte?'none':'inline-block';
} else { aa.style.display='none'; }
}
var ac=document.getElementById('anemoCompteur');
if(ac&&typeof d.anemoTensionV==='number'){
var sil=d.anemoSilence||0;
ac.textContent='Anémomètre : '+(d.anemoTensionV*1000).toFixed(0)+' mV'
+' (max vu '+(d.anemoTensionMax*1000).toFixed(0)+' mV)'
+(sil>300?' — rien depuis '+Math.floor(sil/60)+' min':'');
}
var ba=document.getElementById('btnAuto'),bm=document.getElementById('btnManuel');
if(ba&&bm){ba.className='modeBtn'+(d.suiviAuto?' actif':'');bm.className='modeBtn'+(d.suiviAuto?'':' actif');}
var si=document.getElementById('soleilInfo');
if(si){if(d.solaireOK){si.innerHTML='Soleil : <b>'+d.azSolaire.toFixed(1)+'°</b> az, <b>'+d.elevSolaire.toFixed(1)+'°</b> élév.'+(d.suiviAuto?' — cible <b>'+d.cibleAuto.toFixed(1)+'°</b>':'');}
else{si.textContent='Soleil : RTC indisponible, calcul impossible';}}
var pLoc=document.getElementById('pressionLocale');
if(pLoc){ if(d.bmeOK){
pLoc.innerHTML=d.pressionLocale.toFixed(1)+' hPa'+fleche(d.trendPress,'press')
+' | '+d.temperatureLocale.toFixed(1)+'&deg;C'+fleche(d.trendTemp,'temp'); }
else{ pLoc.textContent='capteur BMP280 non detecte'; } }
// Memes fleches de tendance dans l'en-tete des deux diagrammes.
var gft=document.getElementById('gFlT'), gfp=document.getElementById('gFlP');
if(d.bmeOK){ if(gft) gft.innerHTML=fleche(d.trendTemp,'temp'); if(gfp) gfp.innerHTML=fleche(d.trendPress,'press'); }
if(d.meteoValide){
document.getElementById('temp').textContent=d.temp.toFixed(1);
document.getElementById('vent').textContent=d.vent.toFixed(1);
var vl=document.getElementById('ventLocal');
if(vl) vl.textContent=d.ventKmh.toFixed(1);
var sv=document.getElementById('securiteVent');
if(sv){ if(d.securiteVent){
var t=d.secAvantLevee||0;
sv.textContent=(t>0)
? '⚠ VENT FORT - levee possible dans '+Math.ceil(t/60)+' min'
: "⚠ VENT FORT - attente d'une accalmie";
} else if(d.miseAPlatRafales){ // AJOUT 1
var r=d.secAvantRedeploiement||0;
sv.textContent=(r>0)?'⚠ RAFALES RÉPÉTÉES - redéploiement dans '+Math.ceil(r/60)+' min'
:'⚠ RAFALES RÉPÉTÉES - attente de 15 min sous 20 km/h';
} else { sv.textContent='OK'+(d.rafalesFenetre>0?' ('+d.rafalesFenetre+' rafale'+(d.rafalesFenetre>1?'s':'')+' > 30 km/h en 3 min)':''); }
sv.style.color=(d.securiteVent||d.miseAPlatRafales) ? '#FF6B6B' : '#4CAF50'; }
document.getElementById('pression').textContent=d.pression.toFixed(0);
document.getElementById('meteoAge').textContent=d.meteoAge;
var p=document.getElementById('pluieTxt');
if(d.pluie>=50){p.innerHTML='⚠ Risque de pluie dans l\'heure ('+d.pluie+'%)';p.className='alerte';}
else{p.innerHTML='Risque de pluie (heure suivante) : '+d.pluie+'%';p.className='';}
}
});}
// Fleche de tendance. Le code couleur suit le SENS de la mesure,
// pas la direction : une pression qui monte annonce du beau temps
// (vert), une pression qui chute annonce une degradation (rouge).
// La temperature, elle, n'est ni bonne ni mauvaise : chaud/froid.
function fleche(trend,type){
if(!trend) return '';
var c;
if(trend=='up')        c=(type=='press')?'vert':'chaud';
else if(trend=='down') c=(type=='press')?'rouge':'froid';
else                   c='plat';
var s=(trend=='up')?'&uarr;':(trend=='down')?'&darr;':'&rarr;';
return "<span class='fl "+c+"'>"+s+"</span>";
}

// Releves locaux : tableau rafraichi toutes les 60 s (les points
// n'arrivent que toutes les 30 min, inutile d'interroger plus).
// Diagramme en barres d'une serie. Les barres partent d'une base
// ajustee sur le min/max de la serie, et NON de zero : une pression
// qui varie de 937 a 938 hPa serait sinon invisible. La couleur de
// chaque barre donne le sens de variation par rapport a la
// precedente, avec le meme code que les fleches :
// pression hausse = vert, baisse = rouge ; temperature hausse =
// orange, baisse = bleu.
function tracerSerie(idBarres,idAxe,idPlage,vals,heures,type,unite,dec){
var bar=document.getElementById(idBarres); if(!bar) return;
bar.innerHTML='';
var mn=Math.min.apply(null,vals), mx=Math.max.apply(null,vals);
var marge=Math.max((mx-mn)*0.15,(type=='press')?1:0.5);
var bas=mn-marge, haut=mx+marge;
for(var i=0;i<vals.length;i++){
var b=document.createElement('div');
b.style.height=Math.max(4,((vals[i]-bas)/(haut-bas))*100)+'%';
var d=(i>0)?vals[i]-vals[i-1]:0, c='#607d8b';
if(d>0.05) c=(type=='press')?'#4CAF50':'#FF9800';
else if(d<-0.05) c=(type=='press')?'#F44336':'#2196F3';
b.style.background=c;
b.title=heures[i]+' UTC : '+vals[i].toFixed(dec)+' '+unite;
bar.appendChild(b);
}
document.getElementById(idPlage).textContent='min '+mn.toFixed(dec)+' · max '+mx.toFixed(dec)+' '+unite;
document.getElementById(idAxe).innerHTML='<span>'+heures[0]+'</span><span>'+heures[heures.length-1]+' UTC</span>';
}

function rafraichirHistoLocal(){
fetch('/api/historique-meteo').then(r=>r.json()).then(pts=>{
var ct=document.getElementById('histLocContenu'), vd=document.getElementById('histLocVide');
if(!ct||!vd) return;
if(!pts||pts.length===0){ ct.style.display='none'; vd.style.display='block'; return; }
vd.style.display='none'; ct.style.display='block';

var heures=pts.map(function(e){return e.h;});
var temps=pts.map(function(e){return e.t;});
var press=pts.map(function(e){return e.p;});
tracerSerie('gBarresT','gAxeT','gPlageT',temps,heures,'temp','°C',1);
tracerSerie('gBarresP','gAxeP','gPlageP',press,heures,'press','hPa',1);
var n=pts.length-1;
document.getElementById('gValT').textContent=temps[n].toFixed(1)+' °C';
document.getElementById('gValP').textContent=press[n].toFixed(1)+' hPa';

// Tableau complet, du plus recent au plus ancien (defilant).
var tb=document.getElementById('histLocCorps'); tb.innerHTML='';
for(var i=n;i>=0;i--){
var tr=document.createElement('tr');
var td1=document.createElement('td'); td1.className='hh'; td1.textContent=heures[i];
var td2=document.createElement('td'); td2.className='hv'; td2.textContent=temps[i].toFixed(1)+' °C';
var td3=document.createElement('td'); td3.className='hv'; td3.textContent=press[i].toFixed(1)+' hPa';
tr.appendChild(td1); tr.appendChild(td2); tr.appendChild(td3);
tb.appendChild(tr);
}
}).catch(function(){});
}

// Historique du vent local : vent moyen et rafale max par demi-heure.
// Contrairement a la temperature et a la pression, les barres partent
// de ZERO (un vent nul a un sens) et les deux series partagent la meme
// echelle, pour comparer moyenne et rafale d'un coup d'oeil. Couleur
// = niveau de vent, comme les voyants : vert <20, orange 20-40, rouge >40.
function couleurVent(v){ return (v>40)?'#F44336':(v>=20)?'#FF9800':'#4CAF50'; }
function tracerVent(idBarres,idAxe,idPlage,vals,heures,enCours,echelle){
var bar=document.getElementById(idBarres); if(!bar) return;
bar.innerHTML='';
var tout=vals.concat([enCours]);
for(var i=0;i<tout.length;i++){
var b=document.createElement('div');
var dernier=(i==tout.length-1);
b.style.height=Math.max(3,(tout[i]/echelle)*100)+'%';
b.style.background=couleurVent(tout[i]);
if(dernier) b.className='enCours';
b.title=(dernier?'Demi-heure en cours':heures[i]+' UTC')+' : '+tout[i].toFixed(1)+' km/h';
bar.appendChild(b);
}
var mn=Math.min.apply(null,vals), mx=Math.max.apply(null,vals);
document.getElementById(idPlage).textContent='min '+mn.toFixed(1)+' · max '+mx.toFixed(1)+' km/h';
document.getElementById(idAxe).innerHTML='<span>'+heures[0]+'</span><span>maintenant</span>';
}

function rafraichirReleveVent(){
fetch('/api/releves-vent').then(r=>r.json()).then(d=>{
var ct=document.getElementById('hv30Contenu'), vd=document.getElementById('hv30Vide');
if(!ct||!vd) return;
var pts=d.points||[], ec=d.enCours||{min:0,m:0,r:0};
if(pts.length===0){
ct.style.display='none'; vd.style.display='block';
vd.textContent='Premier relevé dans '+Math.max(0,30-ec.min)+' min (demi-heure en cours : moyenne '
+ec.m.toFixed(1)+' km/h, rafale max '+ec.r.toFixed(1)+' km/h).';
return;
}
vd.style.display='none'; ct.style.display='block';

var heures=pts.map(function(e){return e.h;});
var moy=pts.map(function(e){return e.m;});
var raf=pts.map(function(e){return e.r;});
var echelle=Math.max(30,ec.r,Math.max.apply(null,raf))*1.1;
tracerVent('gBarresVM','gAxeVM','gPlageVM',moy,heures,ec.m,echelle);
tracerVent('gBarresVR','gAxeVR','gPlageVR',raf,heures,ec.r,echelle);
var n=pts.length-1;
document.getElementById('gValVM').textContent=moy[n].toFixed(1)+' km/h';
document.getElementById('gValVR').textContent=raf[n].toFixed(1)+' km/h';
document.getElementById('gFlV').innerHTML=fleche(d.tendance,'vent');

// Tableau complet, du plus recent au plus ancien (defilant).
var tb=document.getElementById('hv30Corps'); tb.innerHTML='';
for(var i=n;i>=0;i--){
var tr=document.createElement('tr');
var td1=document.createElement('td'); td1.className='hh'; td1.textContent=heures[i];
var td2=document.createElement('td'); td2.className='hv'; td2.textContent=moy[i].toFixed(1)+' km/h';
var td3=document.createElement('td'); td3.className='hv'; td3.textContent=raf[i].toFixed(1)+' km/h';
td3.style.color=couleurVent(raf[i]);
tr.appendChild(td1); tr.appendChild(td2); tr.appendChild(td3);
tb.appendChild(tr);
}
}).catch(function(){});
}

// Releves de verification de la constante vent : une ligne par tranche
// de 10 min ou l'anemometre a tourne, la plus recente en haut.
function rafraichirRelevesAnemo(){
fetch('/api/releves-anemo').then(r=>r.json()).then(d=>{
var res=document.getElementById('relResume'); if(!res) return;
var ec=d.enCours||{min:0,actifPct:0};
var txt='Constante actuelle : <b>'+d.constante+'</b> km/h par volt';
if(d.suggeree!==null) txt+=' · suggérée : <b>'+d.suggeree.toFixed(1)+'</b> (médiane de '+d.nValides+' relevé'+(d.nValides>1?'s':'')+')';
else txt+=' · suggérée : pas encore assez de vent';
txt+='<br>Tranche en cours : '+ec.min+' min, anémomètre en rotation '+ec.actifPct+' % du temps';
res.innerHTML=txt;
var l=d.lignes||[];
var ct=document.getElementById('relContenu'), vd=document.getElementById('relVide');
if(l.length===0){ ct.style.display='none'; vd.style.display='block'; return; }
vd.style.display='none'; ct.style.display='block';
var tb=document.getElementById('relCorps'); tb.innerHTML='';
for(var i=l.length-1;i>=0;i--){
var e=l[i], tr=document.createElement('tr');
var cel=[e.h, e.v.toFixed(3)+' V', e.l.toFixed(1)+' km/h',
(e.r===null)?'--':e.r.toFixed(1)+' km/h', (e.k===null)?'--':e.k.toFixed(1)];
for(var j=0;j<cel.length;j++){
var td=document.createElement('td'); td.className=(j==0)?'hh':'hv'; td.textContent=cel[j];
if(j==4&&e.k!==null) td.style.color='#64b5f6';
tr.appendChild(td);
}
tr.title='Rafale locale '+e.lr.toFixed(1)+' km/h ('+e.vx.toFixed(3)+' V)'+((e.rr===null)?'':' · rafale régionale '+e.rr.toFixed(1)+' km/h');
tb.appendChild(tr);
}
}).catch(function(){});
}

// ============================================================
// AJOUT 2 - Bargraphe des rafales (canvas natif)
// Le navigateur garde toutes les barres recues et ne demande que
// les nouvelles (?depuis=), toutes les 15 s. Plusieurs barres par
// pixel (vue 24 h sur un telephone) : on dessine le MAX de chaque
// colonne de pixels, pour ne jamais masquer une rafale.
// ============================================================
var rafBarres=[], rafDernier=0, rafPortee=86400, rafInfo=null;
function rafCouleur(v){ return (v>=70)?'#4B0082':(v>=40)?'#F44336':(v>=20)?'#FF9800':(v>=7)?'#4CAF50':'#607d8b'; }
function rafHeure(t){ var d=new Date(t*1000); return ('0'+d.getUTCHours()).slice(-2)+':'+('0'+d.getUTCMinutes()).slice(-2); }
function rafZoom(s,btn){
rafPortee=s;
var bs=document.querySelectorAll('#bargZoom button');
for(var i=0;i<bs.length;i++) bs[i].className=(bs[i]===btn)?'actif':'';
dessinerRafales();
}
function rafraichirRafales(){
fetch('/api/rafales?depuis='+rafDernier).then(r=>r.json()).then(d=>{
rafInfo=d;
// Horloge revenue en arriere (RTC reglee) : on repart de zero.
if(rafDernier&&d.maintenant&&d.maintenant+60<rafDernier){ rafBarres=[]; rafDernier=0; rafraichirRafales(); return; }
(d.segments||[]).forEach(function(s){
for(var i=0;i<s[1].length;i++){ var t=s[0]+i*d.pas; if(t>rafDernier){ rafBarres.push([t,s[1][i]/10]); rafDernier=t; } }
});
var lim=(d.maintenant||0)-86400;
var k=0; while(k<rafBarres.length&&(rafBarres[k][0]<=lim||rafBarres.length-k>5760)) k++;
if(k) rafBarres=rafBarres.slice(k);
dessinerRafales();
}).catch(function(){});
}
function dessinerRafales(){
var cv=document.getElementById('bargCanvas'), d=rafInfo; if(!cv||!d) return;
var et=document.getElementById('bargEtat'), txt;
if(!d.rtc) txt="Horloge RTC absente : enregistrement impossible (pas d'heure pour les barres)";
else if(d.enregistrement) txt="<span class='pt on'></span>Enregistrement en cours depuis "+rafHeure(d.debutSession)+' UTC';
else txt="<span class='pt'></span>Enregistrement arrêté — reprend dès que le vent dépasse "+d.demarrage+' km/h';
if(!d.stockage) txt+=' (mémoire flash indisponible : barres perdues au redémarrage)';
et.innerHTML=txt;

var dpr=window.devicePixelRatio||1, W=cv.clientWidth, H=cv.clientHeight;
if(cv.width!=Math.round(W*dpr)||cv.height!=Math.round(H*dpr)){ cv.width=Math.round(W*dpr); cv.height=Math.round(H*dpr); }
var c=cv.getContext('2d'); c.setTransform(dpr,0,0,dpr,0,0); c.clearRect(0,0,W,H);
var mg=24, md=6, mh=6, mb=16, pw=W-mg-md, ph=H-mh-mb;
var fin=d.maintenant||(rafBarres.length?rafBarres[rafBarres.length-1][0]:0), debut=fin-rafPortee;

// Maximum par colonne de pixels.
var col=new Float32Array(Math.max(1,Math.floor(pw))), vmax=0, nbVues=0, larg=d.pas/rafPortee*pw;
for(var b=0;b<rafBarres.length;b++){
var t=rafBarres[b][0], v=rafBarres[b][1];
if(t+d.pas<=debut||t>fin) continue;
nbVues++; if(v>vmax) vmax=v;
var x0=(t-debut)/rafPortee*pw, i0=Math.max(0,Math.floor(x0)), i1=Math.min(col.length,Math.max(i0+1,Math.ceil(x0+larg)));
for(var i=i0;i<i1;i++) if(v>col[i]) col[i]=v;
}
var ymax=Math.max(50,Math.ceil(vmax/10)*10), pasG=(ymax>80)?20:10;
function y(v){ return mh+ph-(v/ymax)*ph; }

// Grille et graduations (km/h).
c.font='9px Arial'; c.lineWidth=1;
for(var g=0;g<=ymax;g+=pasG){
c.strokeStyle='rgba(255,255,255,0.07)'; c.beginPath(); c.moveTo(mg,Math.round(y(g))+0.5); c.lineTo(W-md,Math.round(y(g))+0.5); c.stroke();
c.fillStyle='#7a8894'; c.textAlign='right'; c.fillText(g,mg-4,y(g)+3);
}
// Barres.
for(var i=0;i<col.length;i++){
if(col[i]<=0) continue;
c.fillStyle=rafCouleur(col[i]); c.fillRect(mg+i,y(col[i]),1,mh+ph-y(col[i]));
}
// Ligne pointillee a 30 km/h.
c.setLineDash([4,3]); c.strokeStyle='rgba(255,255,255,0.6)';
c.beginPath(); c.moveTo(mg,Math.round(y(d.ligne))+0.5); c.lineTo(W-md,Math.round(y(d.ligne))+0.5); c.stroke(); c.setLineDash([]);
// Etiquette sur fond sombre, lisible meme devant les barres.
var lb=d.ligne+' km/h', lw=c.measureText(lb).width;
c.fillStyle='rgba(15,20,32,0.85)'; c.fillRect(mg+2,y(d.ligne)-12,lw+6,11);
c.fillStyle='#e4e8ee'; c.textAlign='left'; c.fillText(lb,mg+5,y(d.ligne)-3);
// Axe des heures (UTC).
c.fillStyle='#7a8894';
for(var k=0;k<=4;k++){
c.textAlign=(k==0)?'left':(k==4)?'right':'center';
c.fillText(rafHeure(debut+k*rafPortee/4),mg+k*pw/4,H-3);
}
if(nbVues==0){ c.fillStyle='#7a8894'; c.textAlign='center'; c.fillText('Aucune barre sur cette période',mg+pw/2,mh+ph/2); }
}

// Episode de vent fort : 15 derniers releves (un toutes les 5 s),
// barres colorees comme les voyants (vert <20, orange 20-40, rouge >40)
// et tendance. Masque tant qu'aucun episode n'a eu lieu.
function rafraichirEpisode(){
fetch('/api/episode-vent').then(r=>r.json()).then(ep=>{
var box=document.getElementById('episodeVent');
if(!box) return;
var v=ep.valeurs||[];
if(v.length===0){ box.style.display='none'; return; }
box.style.display='block';
document.getElementById('epTitre').textContent='Épisode de vent > 30 km/h depuis '+ep.heure;
var st=document.getElementById('epEtat');
st.textContent=ep.actif?'EN COURS':'terminé';
st.className='epEtat'+(ep.actif?' actif':'');
var mx=60;
for(var i=0;i<v.length;i++){ if(v[i].p>mx) mx=v[i].p; }
var bar=document.getElementById('epBarres'); bar.innerHTML='';
for(var i=0;i<v.length;i++){
var b=document.createElement('div');
b.style.height=Math.max(3,(v[i].p/mx)*100)+'%';
b.style.background=(v[i].p>40)?'#F44336':(v[i].p>=20)?'#FF9800':'#4CAF50';
b.title='pic '+v[i].p.toFixed(1)+' / moy '+v[i].m.toFixed(1)+' km/h';
bar.appendChild(b);
}
document.getElementById('epValeurs').textContent=v.map(function(e){return Math.round(e.p);}).join(' · ')+' km/h';
var t=document.getElementById('epTend');
if(v.length<4){ t.textContent='Tendance : pas encore assez de valeurs'; }
else if(ep.tendance=='up'){ t.innerHTML="Vent en augmentation <span class='fl rouge'>&uarr;</span>"; }
else if(ep.tendance=='down'){ t.innerHTML="Vent en diminution <span class='fl vert'>&darr;</span>"; }
else{ t.innerHTML="Vent stable <span class='fl plat'>&rarr;</span>"; }
}).catch(function(){});
}

function rafraichirHistorique(){
fetch('/api/historique-vent').then(r=>r.json()).then(evts=>{
var box=document.getElementById('histVent'), ul=document.getElementById('histListe');
if(!box||!ul) return;
if(!evts||evts.length===0){box.style.display='none';return;}
ul.innerHTML='';
for(var i=evts.length-1;i>=0;i--){
var e=evts[i];
var li=document.createElement('li');
var h=document.createElement('span'); h.className='hHeure'; h.textContent=e.h;
var v=document.createElement('span'); v.textContent=e.v.toFixed(1)+' km/h';
var c=document.createElement('span');
c.className='hCause'+(e.c=='rafale'?' rafale':''); c.textContent=e.c;
li.appendChild(h); li.appendChild(v); li.appendChild(c);
ul.appendChild(li);
}
box.style.display='block';
}).catch(function(){});
}
refresh();setInterval(refresh,1000);
rafraichirHistorique();setInterval(rafraichirHistorique,15000);
rafraichirEpisode();setInterval(rafraichirEpisode,5000);
rafraichirHistoLocal();setInterval(rafraichirHistoLocal,60000);
rafraichirReleveVent();setInterval(rafraichirReleveVent,60000);
rafraichirRelevesAnemo();setInterval(rafraichirRelevesAnemo,60000);
rafraichirRafales();setInterval(rafraichirRafales,15000);   // AJOUT 2
window.addEventListener('resize',dessinerRafales);
</script>
</body></html>)rawliteral";
