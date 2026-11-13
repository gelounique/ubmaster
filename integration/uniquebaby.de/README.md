# UBMaster auf uniquebaby.de einbinden

Die Hauptseite uniquebaby.de liegt nicht in diesem Repo. Diese Bausteine kommen dort rein:

| Datei | Wohin |
|---|---|
| `software-card.html` | Software-Übersicht, als dritte Karte neben UBGrade und UBTune |
| `nav-link.html` | Navigation / Software-Dropdown |
| `ubmaster-icon-256.png` | optional lokal ablegen, sonst lädt die Karte das Icon von ubmaster.uniquebaby.de |

Credits-Zeile für die Hauptseite: **UBMaster — by unique & MELO**.

## Subdomain ubmaster.uniquebaby.de (United Domains)
DNS-Eintrag: `CNAME  ubmaster  →  gelounique.github.io.`
GitHub → Repo `gelounique/ubmaster` → Settings → Pages → Source: *Deploy from a branch* → `gh-pages` / `/ (root)`,
Custom domain `ubmaster.uniquebaby.de`, „Enforce HTTPS“ an (sobald das Zertifikat da ist).
