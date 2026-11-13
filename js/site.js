/* UBMaster website – loader, glass nav, software menu, reveal, Loudness Halo (canvas), loudness modes, screenshot tabs */
(function () {
  'use strict';
  const $ = (s, r = document) => r.querySelector(s), $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const TIF = [129, 216, 208];
  const rgba = (a) => `rgba(${TIF[0]},${TIF[1]},${TIF[2]},${a})`;
  const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;

  const angleFor = (l) => (-135 + 270 * Math.max(0, Math.min(1, (l + 36) / 32))) * Math.PI / 180;   // 0 = up, clockwise
  const N = 180, bars = new Float32Array(N);
  let shown = -20;

  // ---------------------------------------------------------------- state shared by halo + loudness section
  const PLAT = [
    { name: 'Spotify', lufs: -14, ceil: -1, note: 'Spotify normalisiert auf −14 LUFS. Bist du lauter, wirst du runtergedreht – nimm dann −2 dBTP als Ceiling.' },
    { name: 'Apple Music', lufs: -16, ceil: -1, note: 'Apple Music (Sound Check) spielt auf −16 LUFS ab und dreht lautere Tracks runter.' },
    { name: 'YouTube', lufs: -14, ceil: -1, note: 'YouTube dreht nur runter: alles über −14 LUFS wird leiser abgespielt, nie lauter.' },
    { name: 'SoundCloud', lufs: -14, ceil: -1, note: 'SoundCloud normalisiert nicht überall – −14 LUFS / −1 dBTP ist der sichere Wert.' },
    { name: 'Custom', lufs: -14, ceil: -1, note: 'Eigener Zielwert – z. B. −9 LUFS für Club oder −23 LUFS für Broadcast.' },
  ];
  const st = { plat: 0, target: -14, ceil: -1 };
  const mix = { I: -9.6, S: -9.2, TP: -0.3, LRA: 4.8 };    // example: loud club mix

  // ---------------------------------------------------------------- loader
  const ld = $('#ld'), ldc = $('#ldRing'), ldx = ldc.getContext('2d'), ldPc = $('#ldPc');
  let pc = 0;
  (function tick() {
    pc = Math.min(100, pc + (document.readyState === 'complete' ? 7 : 1.4));
    ldPc.textContent = String(Math.round(pc)).padStart(3, '0');
    drawRing(ldx, 280, 280, performance.now() / 1000, pc / 100, true);
    if (pc < 100) requestAnimationFrame(tick); else setTimeout(() => ld.classList.add('out'), 120);
  })();

  // ---------------------------------------------------------------- nav + software menu + reveal
  const nav = $('#nav'); addEventListener('scroll', () => nav.classList.toggle('solid', scrollY > 40), { passive: true });
  const sw = $('#sw'), swb = $('button', sw);
  swb.addEventListener('click', (e) => { e.stopPropagation(); const o = sw.classList.toggle('open'); swb.setAttribute('aria-expanded', o); });
  document.addEventListener('click', (e) => { if (!sw.contains(e.target)) { sw.classList.remove('open'); swb.setAttribute('aria-expanded', 'false'); } });
  const io = new IntersectionObserver((es) => es.forEach((e) => { if (e.isIntersecting) { e.target.classList.add('in'); io.unobserve(e.target); } }), { threshold: 0.12 });
  $$('.reveal').forEach((el) => io.observe(el));

  // ---------------------------------------------------------------- the Loudness Halo
  function spectrum(t, i) {   // pseudo music spectrum around the ring (mirrored), animated
    const u = i / N, m = u < 0.5 ? u * 2 : (1 - u) * 2;
    const beat = Math.pow(Math.max(0, Math.sin(t * Math.PI * 2 * 1.05)), 8);
    const body = 0.62 - 0.38 * m + 0.12 * Math.sin(m * 9 + t * 1.3) + 0.09 * Math.sin(m * 23 - t * 2.1);
    const kick = beat * Math.max(0, 0.6 - m * 2.2);
    const air = 0.08 * Math.sin(m * 61 + t * 7) + 0.05 * Math.sin(i * 1.7 + t * 11);
    return Math.max(0.06, Math.min(1, body + kick + air));
  }
  function drawRing(ctx, w, h, t, level, mini) {
    ctx.clearRect(0, 0, w, h);
    const cx = w / 2, cy = h / 2, R = Math.min(w, h) * (mini ? 0.40 : 0.40), base = R * (mini ? 0.70 : 0.66), len = R * 0.24;
    if (!mini) {
      const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, R * 1.15);
      g.addColorStop(0, rgba(0.10)); g.addColorStop(1, rgba(0));
      ctx.fillStyle = g; ctx.beginPath(); ctx.arc(cx, cy, R * 1.15, 0, Math.PI * 2); ctx.fill();
      for (let k = 0; k < 7; k++) { ctx.strokeStyle = `rgba(255,255,255,${0.025 + 0.01 * (k % 2)})`; ctx.lineWidth = 1; ctx.beginPath(); ctx.arc(cx, cy, base * (0.62 + 0.055 * k), 0, Math.PI * 2); ctx.stroke(); }
    }
    ctx.lineCap = 'round';
    for (let pass = 0; pass < 2; pass++) {
      for (let i = 0; i < N; i++) {
        const a = (i / N) * Math.PI * 2 - Math.PI / 2;
        let v = mini ? spectrum(t, i) * level : bars[i];
        const r0 = base, r1 = base + 3 + len * v;
        ctx.strokeStyle = pass === 0 ? rgba(0.12) : (v > 0.62 ? `rgba(${Math.round(129 + (255 - 129) * (v - 0.62) * 2)},${Math.round(216 + 39 * (v - 0.62) * 2)},${Math.round(208 + 47 * (v - 0.62) * 2)},${0.5 + 0.5 * v})` : rgba(0.45 + 0.55 * v));
        ctx.lineWidth = pass === 0 ? (mini ? 3 : 5) : (mini ? 1.4 : 1.6) * (w / 640 * (mini ? 2 : 1) > 1 ? w / 640 * (mini ? 2 : 1) : 1);
        ctx.beginPath(); ctx.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0); ctx.lineTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1); ctx.stroke();
      }
    }
    if (mini) return;
    // gauge
    const a0 = angleFor(-36) - Math.PI / 2, a1 = angleFor(-4) - Math.PI / 2;
    ctx.lineWidth = 2; ctx.strokeStyle = 'rgba(255,255,255,.09)'; ctx.beginPath(); ctx.arc(cx, cy, R, a0, a1); ctx.stroke();
    ctx.font = `${Math.max(9, w * 0.016)}px "JetBrains Mono", monospace`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    for (let l = -36; l <= -4; l += 2) {
      const a = angleFor(l) - Math.PI / 2, maj = l % 6 === 0;
      ctx.strokeStyle = `rgba(255,255,255,${maj ? 0.22 : 0.09})`; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(cx + Math.cos(a) * (R + 6), cy + Math.sin(a) * (R + 6)); ctx.lineTo(cx + Math.cos(a) * (R + (maj ? 13 : 9)), cy + Math.sin(a) * (R + (maj ? 13 : 9))); ctx.stroke();
      if (maj && l > -36 && l < -4) { ctx.fillStyle = '#4a4a50'; ctx.fillText(String(l), cx + Math.cos(a) * (R + 26), cy + Math.sin(a) * (R + 26)); }
    }
    const av = angleFor(shown) - Math.PI / 2;
    ctx.save(); ctx.shadowColor = rgba(0.8); ctx.shadowBlur = 14; ctx.strokeStyle = rgba(1); ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(cx, cy, R, a0, av); ctx.stroke(); ctx.restore();
    // target marker
    const at = angleFor(st.target) - Math.PI / 2, mx = cx + Math.cos(at) * R, my = cy + Math.sin(at) * R;
    const on = Math.abs(shown - st.target) < 0.6;
    for (let k = 4; k >= 1; k--) { ctx.fillStyle = rgba((on ? 0.16 : 0.1) * (5 - k)); ctx.beginPath(); ctx.arc(mx, my, 4 + k * 3.6, 0, Math.PI * 2); ctx.fill(); }
    ctx.fillStyle = '#fff'; ctx.beginPath(); ctx.arc(mx, my, 4.6, 0, Math.PI * 2); ctx.fill();
  }

  const cv = $('#haloCv'), cx2 = cv.getContext('2d'), lufsEl = $('#haloLufs'), deltaEl = $('#haloDelta');
  function fit() { const r = cv.getBoundingClientRect(), d = Math.min(2, devicePixelRatio || 1); cv.width = r.width * d; cv.height = r.height * d; }
  fit(); addEventListener('resize', fit);
  let last = performance.now(), haloVisible = true, txt = '';
  new IntersectionObserver((es) => { haloVisible = es[0].isIntersecting; }).observe(cv);
  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000); last = now;
    if (haloVisible) {
      const t = now / 1000;
      for (let i = 0; i < N; i++) { const v = spectrum(t, i); bars[i] += (v - bars[i]) * (v > bars[i] ? 0.5 : 0.12); }
      // loudness "lands" on the target, then breathes ±0.4 LU like real music
      const goal = st.target + 0.35 * Math.sin(t * 1.7) + 0.2 * Math.sin(t * 4.3);
      shown += (goal - shown) * Math.min(1, dt * 2.2);
      drawRing(cx2, cv.width, cv.height, t, 1, false);
      const s = shown.toFixed(1);
      if (s !== txt) { txt = s; lufsEl.textContent = s; const d = shown - st.target; deltaEl.textContent = Math.abs(d) < 0.6 ? '● AUF ZIEL' : (d < 0 ? '▲ ' : '▼ ') + Math.abs(d).toFixed(1) + ' LU'; }
    }
    if (!reduce) requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ---------------------------------------------------------------- loudness modes + advice (same rules as the plugin)
  const pills = $$('#platPills button'), customRow = $('#customRow'), customIn = $('#customLufs');
  const setBar = (id, v, lo, hi) => { $(id).style.width = (Math.max(0, Math.min(1, (v - lo) / (hi - lo))) * 100) + '%'; };
  const setMark = (id, v, lo, hi) => { $(id).style.left = (Math.max(0, Math.min(1, (v - lo) / (hi - lo))) * 100) + '%'; };
  const f1 = (v) => (v < 0 ? '−' : '') + Math.abs(v).toFixed(1);
  function advice() {
    const p = PLAT[st.plat], d = mix.I - st.target, tips = [];
    let title;
    if (Math.abs(d) <= 0.5) { title = 'Ziel erreicht – ' + f1(mix.I) + ' LUFS ✓'; tips.push('Passt für ' + p.name + '. Jetzt nur noch mit Ohren feintunen.'); }
    else if (d < 0) { title = 'Noch ' + (-d).toFixed(1) + ' dB zu leise'; tips.push('Limiter-Gain um +' + (-d).toFixed(1) + ' dB erhöhen – oder „Auf Ziel bringen“ klicken.'); }
    else {
      title = d.toFixed(1) + ' dB lauter als das Ziel';
      tips.push(st.plat === 4 ? 'Limiter-Gain um ' + d.toFixed(1) + ' dB senken, um genau auf ' + f1(st.target) + ' LUFS zu kommen.'
        : p.name + ' dreht dich um ' + d.toFixed(1) + ' dB runter. Gain um ' + d.toFixed(1) + ' dB senken – gleiche Lautheit beim Hörer, aber mehr Punch.');
    }
    if (mix.TP > st.ceil + 0.1) tips.push('True Peak ' + f1(mix.TP) + ' dBTP liegt über ' + f1(st.ceil) + ' dBTP – Ceiling auf ' + f1(st.ceil) + ' stellen.');
    if (st.plat === 0 && mix.I > -14) tips.push('Spotify: Master lauter als −14 LUFS → Ceiling −2,0 dBTP empfohlen.');
    $('#advHead').textContent = 'EMPFEHLUNG · ' + p.name.toUpperCase() + ' ' + f1(st.target) + ' LUFS';
    $('#advTitle').textContent = title;
    $('#advList').innerHTML = tips.slice(0, 3).map((t) => '<li>' + t + '</li>').join('');
  }
  function syncLoud() {
    const p = PLAT[st.plat];
    pills.forEach((b, i) => b.classList.toggle('on', i === st.plat));
    if (st.plat !== 4) { st.target = p.lufs; st.ceil = p.ceil; customIn.value = st.target; }
    customRow.style.opacity = st.plat === 4 ? 1 : 0.35; customIn.disabled = st.plat !== 4;
    customIn.style.setProperty('--v', ((+customIn.value + 30) / 25 * 100) + '%');
    $('#tLufs').innerHTML = f1(st.target) + '<small>LUFS</small>';
    $('#tCeil').innerHTML = f1(st.ceil) + '<small>dBTP</small>';
    $('#tMix').innerHTML = f1(mix.I) + '<small>LUFS</small>';
    $('#platNote').textContent = p.note;
    setBar('#mI', mix.I, -36, 0); setMark('#mkI', st.target, -36, 0); $('#vI').textContent = f1(mix.I);
    setBar('#mS', mix.S, -36, 0); setMark('#mkS', st.target, -36, 0); $('#vS').textContent = f1(mix.S);
    setBar('#mT', mix.TP, -24, 3); setMark('#mkT', st.ceil, -24, 3); $('#vT').textContent = f1(mix.TP);
    setBar('#mL', mix.LRA, 0, 20); $('#vL').textContent = mix.LRA.toFixed(1) + ' LU';
    advice();
  }
  pills.forEach((b) => b.addEventListener('click', () => { st.plat = +b.dataset.p; syncLoud(); }));
  customIn.addEventListener('input', () => { st.target = +customIn.value; syncLoud(); });
  syncLoud();

  // ---------------------------------------------------------------- screenshot tabs
  const img = $('#shotImg');
  $$('.shot-tabs button').forEach((b) => b.addEventListener('click', () => {
    $$('.shot-tabs button').forEach((x) => x.classList.toggle('on', x === b));
    const n = b.dataset.shot;
    img.style.opacity = 0.2;
    const pre = new Image(); pre.src = 'assets/shot-' + n + '.jpg';
    pre.onload = () => { img.srcset = 'assets/shot-' + n + '-1x.jpg 992w, assets/shot-' + n + '.jpg 1984w'; img.src = pre.src; img.style.opacity = 1; };
  }));
})();
