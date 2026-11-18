# Sources — ANS synthesizer research

Collected 2026-11-18 for the ANS MPC/VST plugin.

This repository contains only this source list and the technical report. Third-party
articles, scans, and recordings are not redistributed; the links below identify them.
References marked "local copy" belong to the author's private research archive.

## Primary / technical

- **USSR author certificate 118695**, E.A. Murzin, “Фотоэлектрический синтезатор музыки”,
  applied 1957-06-24, published 1958-11-30.
  - https://patents.google.com/patent/SU118695A1/en
  - https://findpatent.ru/patent/11/118695.html
  - Local copy (not redistributed): `patent_p1.jpg`, `patent_p2.jpg`, `patent_fig12.jpg`
- **Habr 2025**, “Как создавали уникальный советский синтезатор АНС и что с ним сейчас”,
  https://habr.com/ru/companies/sberbank/articles/961642/
  - Local copies (not redistributed): `habr_ans_article.txt`; `schematics/habr_*.png` and descriptive copies in `schematics/named/`
- **A. Smirnov**, “Евгений Мурзин и синтезатор АНС”, https://asmir.info/lib/murzin0.htm
  - Exact disc speeds, geometry and ranges for both ANS versions (Windows-1251 page).
- **S. Kreichi, O. Krivnova, A. Egorov**, “Фотооптический синтезатор АНС и его роль в развитии
  методов синтеза речи”, Speech Technologies 2013,
  https://cyberleninka.ru/article/n/fotoopticheskiy-sintezator-ans-i-ego-rol-v-razvitii-metodov-sinteza-rechi
- **S. Kreichi**, “The ANS Synthesizer: Composing on a Photoelectronic Instrument”, Theremin
  Center, 1997, https://www.theremin.ru/archive/ans.htm
  - Local copies (not redistributed): `theremin/`
- **A. Smirnov**, *Sound in Z: Experiments in Sound and Electronic Music in Early 20th Century
  Russia*, chapter 7, pp. 227–236. http://asmir.info/soundz/Sound_in_Z.pdf
  - Local copies (not redistributed): `Sound_in_Z.pdf`, `Sound_in_Z.txt`

## Museum / historical

- Yu. Dmitriukova, “У истоков электронной музыки”, Muzelectron / “Музыка и электроника” 3/2015,
  https://muzelectron.ru/pdf/dmitriukova_ans_1.pdf — local copy (not redistributed)
- Part 2: https://muzelectron.ru/pdf/dmitriukova_ans_2.pdf
- Digital Music Academy interview with Stanislav Kreichi, https://digitalmusicacademy.ru/node/576
- State Catalogue of the Russian Federation, ANS at the Glinka Museum:
  https://ar.culture.ru/ru/subject/sintezator-ans
- 120 Years of Electronic Music: https://120years.net/the-ans-synthesisereugeniy-murzinsoviet-union1958-2
- Monoskop: https://monoskop.org/ANS_synthesizer
- Wikipedia: https://en.wikipedia.org/wiki/ANS_synthesizer
- Synth DB spec sheet:
  http://synth-db.com/synths/Evgeny%20Murzin/ANS%20Synthesizer/ANS%20Synthesizer.php

## Private A/B reference (not redistributed)

- “The ANS Synthesizer playing doodles (live)”, Wikimedia Commons, CC BY-SA 3.0,
  recorded 21 Oct 2009 by octopusvision. A private copy was used only for spectral comparison;
  it informed the extended vocal harmonic series, the formant baseline and the low-mid vocal
  channel curve. No audio is included in this repository, the plugin, or the release assets.

## Secondary / context

- Red Bull Music Academy Daily, “Synth-Aesthesia: Soviet Synths And The ANS”:
  https://daily.redbullmusicacademy.com/2012/08/history-soviet-synth-ans
- Xakep, “Рисованный звук: из прошлого в будущее”:
  https://xakep.ru/2014/10/17/cinema-voice-history
- Habr/Pult.ru, “Музыкальный Леонардо советского военпрома…”:
  https://habr.com/ru/companies/pult/articles/370009
- Colta, “АНСианские хроники”:
  https://www.colta.ru/articles/music_modern/27231-denis-boyarinov-albom-muzykalnoe-prinoshenie-evgeniy-murzin-sintezator-ans

## Notes on reliability

- The 1957 patent documents the score and coder, not the production electronics.
- The Habr schematics are redrawn from museum documents; the filter's twin-T blocks lack values.
- Disc speeds/geometry from Smirnov's page agree with the physics and with the 720-tone range.
- `synth-db` is secondary; its numbers are included as the published technical-sheet values.
- Audio recordings of the ANS are copyrighted; use them only for private A/B reference, not as
  plugin content. Score images from the sources must not be redistributed; create original scores.
