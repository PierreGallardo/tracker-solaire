// ============================================================
// PageCodeur.h - Page de test du codeur Baumer (/codeur)
// ------------------------------------------------------------
// Page minimale pour l'essai a vide : on tourne l'axe a la main et
// on lit position, niveaux des voies et erreurs en direct. Lisible
// sur un telephone pose sur l'etabli.
// ============================================================

#pragma once

#include <Arduino.h>

static const char PAGE_CODEUR_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Test codeur</title>
<style>
*{box-sizing:border-box;}
body{font-family:Arial,Helvetica,sans-serif;background:#0f1420;color:#e4e8ee;margin:0 auto;padding:16px;max-width:520px;}
h1{font-size:18px;margin:0 0 4px;} .sous{font-size:12px;color:#9aa5b1;margin:0 0 14px;}
.big{font-size:60px;font-weight:bold;text-align:center;margin:10px 0;font-variant-numeric:tabular-nums;}
.row{display:flex;gap:10px;margin:8px 0;}
.box{flex:1;background:#1a2130;border:1px solid #262f42;border-radius:10px;padding:10px;text-align:center;}
.box small{display:block;color:#9aa5b1;font-size:11px;margin-bottom:4px;}
.box b{font-size:22px;font-variant-numeric:tabular-nums;}
.dot{display:inline-block;width:18px;height:18px;border-radius:50%;background:#2a3346;vertical-align:middle;margin-right:6px;}
.dot.on{background:#4CAF50;box-shadow:0 0 10px #4CAF50;}
.etat{text-align:center;font-size:13px;margin:6px 0 12px;padding:8px;border-radius:8px;background:#1a2130;}
.etat.actif{background:rgba(76,175,80,0.15);color:#A5D6A7;}
.etat.bruit{background:rgba(244,67,54,0.18);color:#FF8A80;}
.btns{display:flex;gap:8px;flex-wrap:wrap;justify-content:center;margin:12px 0;}
button{height:46px;border-radius:9px;border:none;font-size:15px;padding:0 18px;cursor:pointer;color:#fff;}
.on{background:#4CAF50;} .off{background:#F44336;} .zero{background:#2196F3;}
.aide{font-size:12px;color:#9aa5b1;line-height:1.5;margin-top:14px;}
.aide b{color:#e4e8ee;}
a{color:#64b5f6;font-size:12px;}
</style></head><body>
<h1>Test du codeur Baumer</h1>
<p class='sous'>Essai a vide : tourne l'axe a la main. Voies lues sur GPIO 34 (A) et 35 (B).</p>
<div class='etat' id='etat'>--</div>
<div class='big' id='pos'>--</div>
<div class='row'>
<div class='box'><small>Voie A</small><span class='dot' id='dA'></span><b id='tA'>0</b></div>
<div class='box'><small>Voie B</small><span class='dot' id='dB'></span><b id='tB'>0</b></div>
<div class='box'><small>Erreurs</small><b id='err'>0</b></div>
</div>
<div class='btns'>
<button class='on' onclick="act(1)">ACTIVER</button>
<button class='off' onclick="act(0)">COUPER</button>
<button class='zero' onclick="zero()">REMISE A ZERO</button>
</div>
<div class='aide'>
<b>Que verifier :</b><br>
1. <b>Les deux pastilles</b> doivent chacune s'allumer et s'eteindre quand tu tournes (meme ecoute coupee). Une pastille qui ne bouge jamais = voie non cablee ou coupee.<br>
2. <b>Position</b> : elle doit changer quand tu tournes. Si elle <b>diminue</b> dans le sens ou tu la voulais croissante, permute CHA et CHB (ou A et /A).<br>
3. <b>Erreurs</b> : doivent rester a 0. Elles signalent du bruit, un mauvais cablage ou une rotation trop rapide.<br>
4. Revenir au point de depart a la main doit ramener la position a ~0.<br><br>
L'ecoute s'arrete seule apres 10 min, ou si une entree non reliee fait du bruit.
</div>
<p><a href='/'>&larr; Retour au tracker</a></p>
<script>
function act(on){fetch('/api/codeur-actif?on='+on).then(maj);}
function zero(){fetch('/api/codeur-zero').then(maj);}
function maj(){
fetch('/api/codeur').then(function(r){return r.json();}).then(function(d){
document.getElementById('pos').textContent=d.position;
document.getElementById('tA').textContent=d.transA;
document.getElementById('tB').textContent=d.transB;
document.getElementById('err').textContent=d.erreurs;
document.getElementById('dA').className='dot'+(d.niveauA?' on':'');
document.getElementById('dB').className='dot'+(d.niveauB?' on':'');
var e=document.getElementById('etat');
if(d.bruit){ e.className='etat bruit'; e.textContent='Bruit detecte : ecoute coupee (une entree non reliee ?)'; }
else if(d.actif){ e.className='etat actif'; e.textContent='ECOUTE ACTIVE - arret auto dans '+Math.floor(d.secRestantes/60)+' min '+(d.secRestantes%60)+' s'; }
else{ e.className='etat'; e.textContent='Ecoute coupee - appuie sur ACTIVER pour compter'; }
}).catch(function(){});
}
maj();setInterval(maj,300);
</script>
</body></html>)rawliteral";
