// ============================================================
// PageCalib.h - Page de calibration de l'azimut (/calibration)
// ------------------------------------------------------------
// Guide pas a pas, lisible sur un telephone au pied du tracker :
// sens EST, deux points boussole, controle, recherche du HOME,
// butees, enregistrement. Accessible seulement en local (les
// commandes sont refusees a travers le reverse proxy).
// ============================================================

#pragma once

#include <Arduino.h>

static const char PAGE_CALIB_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Calibration azimut</title>
<style>
*{box-sizing:border-box;}
body{font-family:Arial,Helvetica,sans-serif;background:#0f1420;color:#e4e8ee;margin:0 auto;padding:16px;max-width:560px;}
h1{font-size:18px;margin:0 0 4px;} .sous{font-size:12px;color:#9aa5b1;margin:0 0 12px;}
.live{position:sticky;top:0;z-index:2;background:#0f1420;padding:6px 0 10px;}
.row{display:flex;gap:8px;}
.box{flex:1;background:#1a2130;border:1px solid #262f42;border-radius:10px;padding:8px;text-align:center;}
.box small{display:block;color:#9aa5b1;font-size:11px;margin-bottom:3px;}
.box b{font-size:20px;font-variant-numeric:tabular-nums;}
.dot{display:inline-block;width:14px;height:14px;border-radius:50%;background:#2a3346;vertical-align:middle;}
.dot.on{background:#4CAF50;box-shadow:0 0 8px #4CAF50;}
.msg{margin:8px 0 0;padding:8px 10px;border-radius:8px;background:#1a2130;font-size:13px;min-height:20px;}
.jog{display:flex;gap:8px;margin-top:8px;}
.jog button{flex:1;}
.etape{background:#151b28;border:1px solid #262f42;border-radius:12px;padding:12px;margin:12px 0;}
.etape h2{font-size:15px;margin:0 0 6px;display:flex;align-items:center;gap:8px;}
.etape p{font-size:12px;color:#9aa5b1;margin:4px 0 8px;line-height:1.45;}
.ok{font-size:11px;padding:2px 8px;border-radius:8px;background:#2a3346;color:#9aa5b1;}
.ok.fait{background:rgba(76,175,80,0.2);color:#A5D6A7;}
button{height:44px;border-radius:9px;border:none;font-size:14px;padding:0 14px;cursor:pointer;color:#fff;background:#2196F3;}
button.g{background:#455a64;} button.r{background:#F44336;} button.v{background:#4CAF50;}
input{height:44px;width:110px;border-radius:9px;border:1px solid #34405a;background:#0f1420;color:#e4e8ee;font-size:17px;padding:0 10px;}
.ligne{display:flex;gap:8px;align-items:center;flex-wrap:wrap;}
.res{font-size:13px;margin-top:6px;color:#cfd8e3;}
.enr{font-size:12px;color:#9aa5b1;line-height:1.5;}
a{color:#64b5f6;font-size:12px;}
</style></head><body>
<h1>Calibration de l'azimut</h1>
<p class='sous'>Telephone en mode boussole, reglage <b>nord geographique</b>, pose a plat, le haut tourne vers l'avant des panneaux.</p>

<div class='live'>
<div class='row'>
<div class='box'><small>Azimut calcule</small><b id='az'>--</b></div>
<div class='box'><small>Codeur (pas)</small><b id='pos'>--</b></div>
<div class='box'><small>HOME</small><span class='dot' id='home'></span></div>
</div>
<div class='jog'>
<button class='g' onclick="cmd('calib-jog?d=g')">&#9664; GAUCHE</button>
<button class='r' onclick="cmd('calib-stop')">STOP</button>
<button class='g' onclick="cmd('calib-jog?d=d')">DROITE &#9654;</button>
</div>
<div class='msg' id='msg'>--</div>
</div>

<div class='etape'><h2>1. Quel bouton tourne vers l'EST ? <span class='ok' id='e1'>a faire</span></h2>
<p>Donne une courte impulsion avec GAUCHE puis STOP, et regarde de quel cote les panneaux tournent.</p>
<div class='ligne'><button onclick="cmd('calib-sens?est=g')">EST = GAUCHE</button><button onclick="cmd('calib-sens?est=d')">EST = DROITE</button></div>
</div>

<div class='etape'><h2>2. Point 1, cote EST <span class='ok' id='e2'>a faire</span></h2>
<p>Amene le tracker vers l'est, en arrivant toujours dans le <b>meme sens</b>. Lis la boussole, entre la valeur, puis releve.</p>
<div class='ligne'><input id='az1' type='number' inputmode='decimal' min='0' max='359' step='0.5' placeholder='deg'><button onclick="pt(1)">Relever le point 1</button></div>
<div class='res' id='r1'></div>
</div>

<div class='etape'><h2>3. Point 2, cote OUEST <span class='ok' id='e3'>a faire</span></h2>
<p>Tourne vers l'ouest, au moins 30 deg plus loin (90 deg c'est mieux), puis releve.</p>
<div class='ligne'><input id='az2' type='number' inputmode='decimal' min='0' max='359' step='0.5' placeholder='deg'><button onclick="pt(2)">Relever le point 2</button></div>
<div class='res' id='r2'></div>
</div>

<div class='etape'><h2>4. Controle <span class='ok' id='e4'>conseille</span></h2>
<p>Place le tracker entre les deux points et compare : l'ecart doit rester sous 3 deg.</p>
<div class='ligne'><input id='az3' type='number' inputmode='decimal' min='0' max='359' step='0.5' placeholder='deg'><button onclick="pt(3)">Comparer</button></div>
<div class='res' id='r3'></div>
</div>

<div class='etape'><h2>5. Recherche du HOME <span class='ok' id='e5'>a faire</span></h2>
<p>Le tracker tourne seul vers l'EST, lentement, jusqu'au capteur. Reste a cote : STOP l'arrete a tout moment. Arret automatique si le codeur ne compte pas ou apres 3 min.</p>
<div class='ligne'><button onclick="cmd('calib-home')">Rechercher le HOME</button></div>
<div class='res' id='r5'></div>
</div>

<div class='etape'><h2>6. Butees logicielles <span class='ok' id='e6'>a faire</span></h2>
<p>Amene le tracker a chaque extremite <b>sure pour le cablage</b>, puis enregistre-la. Il ne pourra plus aller au-dela.</p>
<div class='ligne'><button onclick="cmd('calib-butee?cote=est')">Butee EST ici</button><button onclick="cmd('calib-butee?cote=ouest')">Butee OUEST ici</button></div>
<div class='res' id='r6'></div>
</div>

<div class='etape'><h2>7. Enregistrer <span class='ok' id='e7'>a faire</span></h2>
<p>A partir de la, l'azimut affiche vient du codeur. Apres une coupure de courant, il faudra relancer la recherche du HOME (etape 5).</p>
<div class='ligne'><button class='v' onclick="cmd('calib-valider')">Valider et enregistrer</button><button class='r' onclick="if(confirm('Effacer la calibration enregistree ?'))cmd('calib-effacer')">Effacer</button></div>
<div class='enr' id='enr'></div>
</div>

<p><a href='/'>&larr; Retour au tracker</a></p>
<script>
function $(i){return document.getElementById(i);}
function f1(v){return (v===undefined||v===null)?'--':Number(v).toFixed(1);}
function etat(id,fait,txt){var e=$(id);e.className='ok'+(fait?' fait':'');e.textContent=fait?'fait':txt;}
function cmd(u){fetch('/api/'+u).then(function(r){return r.text();}).then(function(t){$('msg').textContent=t;maj();}).catch(function(){});}
function pt(n){var v=$('az'+n).value.replace(',','.');if(v===''){$('msg').textContent='Entre la valeur lue sur la boussole.';return;}cmd('calib-point?n='+n+'&az='+encodeURIComponent(v));}
function maj(){
fetch('/api/calib').then(function(r){return r.json();}).then(function(d){
var s=d.session,e=d.enregistree;
$('az').textContent=(d.az===null)?'--':f1(d.az)+'°';
$('pos').textContent=d.codeurOK?d.pos:'HS';
$('home').className='dot'+(d.home?' on':'');
if(d.message)$('msg').textContent=d.message+(d.recherche!='aucune'?' ('+d.etatH+')':'');
etat('e1',d.sensEst!=0,'a faire');
etat('e2',s.p1,'a faire'); $('r1').textContent=s.p1?('Releve : '+f1(s.az1)+'° / codeur '+s.pos1):'';
etat('e3',s.p2,'a faire'); $('r2').textContent=s.p2?('Releve : '+f1(s.az2)+'° / codeur '+s.pos2+' -> '+Math.abs(s.k).toFixed(1)+' pas par degre'):'';
etat('e4',s.ctrl,'conseille'); $('r3').textContent=s.ctrl?('Calcule '+f1(s.ctrlCalcule)+'° / boussole '+f1(s.ctrlBoussole)+'° -> ecart '+f1(s.ctrlCalcule-s.ctrlBoussole)+'°'):'';
etat('e5',s.homeOk,'a faire'); $('r5').textContent=s.homeOk?('HOME a '+f1(s.azHome)+'°'):(d.recherche!='aucune'?'Recherche en cours : '+d.recherche:'');
etat('e6',s.butEstOk&&s.butOuestOk,'a faire'); $('r6').textContent=(s.butEstOk?'EST '+f1(s.butEst)+'°':'EST --')+' / '+(s.butOuestOk?'OUEST '+f1(s.butOuest)+'°':'OUEST --');
etat('e7',e.valide,'a faire');
$('enr').textContent=e.valide?('Enregistree : '+Math.abs(e.k).toFixed(1)+' pas/deg, HOME '+f1(e.azHome)+'°, butees '+f1(e.butEst)+'° a '+f1(e.butOuest)+'°. '+(d.disponible?'Azimut mesure actif.':'HOME pas encore trouve depuis le demarrage : azimut estime.')):'Aucune calibration enregistree.';
}).catch(function(){});
}
maj();setInterval(maj,500);
</script>
</body></html>)rawliteral";
