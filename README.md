# VoxSlap VS-1: слепбек-эхо для вокала

Форматы: **AU** (Logic, GarageBand), **VST3** (Ableton, FL Studio, Cubase, Reaper, Studio One и др.),
а также отдельное приложение **Standalone**, чтобы попробовать эффект с микрофона.

## Установка

### macOS
1. Скопируйте `VoxSlap.component` в `~/Library/Audio/Plug-Ins/Components/`,
   а `VoxSlap.vst3` в `~/Library/Audio/Plug-Ins/VST3/`
   (в Finder: Переход → Переход к папке… и вставьте путь).
2. Плагин не подписан сертификатом Apple, поэтому один раз выполните в Терминале:
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/VoxSlap.component ~/Library/Audio/Plug-Ins/VST3/VoxSlap.vst3
   ```
3. Перезапустите DAW. В Logic плагин находится в меню Audio Units → Homebrew Audio → VoxSlap.

### Windows
Скопируйте папку `VoxSlap.vst3` в `C:\Program Files\Common Files\VST3\` и пересканируйте плагины в DAW.

### Linux
Скопируйте `VoxSlap.vst3` в `~/.vst3/`.

## Управление

**Режимы (MODE)**
- **SLAP**: классическое «эхо-хлопок» по центру.
- **PING-PONG**: повторы по очереди в левое и правое ухо.
- **WIDE**: правый канал запаздывает на величину R OFFSET, получается широкий дабл или эхо.

**Основные ручки (простой режим)**
- **TIME**: время задержки. Переключатель **SYNC** вверху выбирает доли такта по темпу проекта
  (1/16, 1/8D, 1/4T…), внизу — миллисекунды.
- **FEEDBACK**: количество повторов.
- **DRIVE** и тип (Tape / Tube / Fuzz / Lo-Fi): искажения только в повторах, сухой голос остаётся чистым.
  Каждый следующий повтор грязнее предыдущего, как на плёнке.
- **UNDER / AFTER**: при 0 % эхо звучит прямо под голосом, при 100 % прячется, пока вы поёте,
  и выходит в паузах и на концах фраз.
- **MIX**: баланс. На 50 % голос и эхо звучат на полной громкости, при 100 % остаётся только эхо
  (удобно на посыле / AUX).

**ADVANCED (расширенный режим)**
- FILTER: LOW CUT / HIGH CUT для повторов (телефонный звук: 900 Hz / 3 kHz).
- STEREO: WIDTH (ширина эха) и R OFFSET (сдвиг правого канала в режиме WIDE).
- WOBBLE: RATE / DEPTH, «плавание» плёнки.
- SHIFT: PITCH, сдвиг высоты повторов ±12 полутонов (накапливается с каждым повтором).
- DUCK: THRESHOLD (с какой громкости голос «прячет» эхо) и RELEASE (как быстро эхо возвращается).
- FX: REVERSE (повторы задом наперёд) и FREEZE (зацикливает текущий хвост — удобно автоматизировать
  на последнем слове).
- OUTPUT: общий уровень на выходе.

**Приёмы управления**
- Двойной клик или Alt+клик по ручке — значение по умолчанию.
- Shift + перетаскивание — тонкая подстройка.
- Клик по правому VU-метру переключает его между OUTPUT (уровень выхода) и ECHO DUCK
  (насколько эхо «прячется» под голос).
- Тумблер POWER внизу справа — обход эффекта (то же, что кнопка bypass в DAW).

**Нижняя панель инструментов**
- Пресеты: список, стрелки, Save / Delete.
- COMPARE: A / B — две настройки для сравнения, A > B копирует текущую в другую.
- Simple / Advanced — показать или скрыть блок-расширитель.
- 100% / 125% / 150% — размер окна.

## Пресеты
16 заводских пресетов, включая **Cupsize Slap**, Classic 50s Slapback, Phone Slap, Ping-Pong,
Wide Double, Lo-Fi Tape Echo, Dark Throw (эхо только после фраз), Radio Ghost, Reverse Swell,
Octave Up Echo, Dub Feedback и другие.

Кнопка **Save** сохраняет ваш пресет, **Delete** удаляет его в корзину.
Свои пресеты лежат в `Документы/VoxSlap/Presets/` (файлы `.vspreset`), ими можно делиться с друзьями.

---

## Для разработчика: сборка из исходников

Нужны CMake ≥ 3.22, компилятор C++17 (Xcode Command Line Tools / Visual Studio 2022 / GCC) и JUCE 8:

```
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

На macOS после сборки плагины автоматически копируются в системные папки.
Универсальная сборка (Intel и Apple Silicon): добавьте `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

Тест без DAW: `-DVOXSLAP_BUILD_TOOLS=ON`, затем
`build/VoxSlapRender_artefacts/Release/VoxSlapRender out [vocal.wav]` прогонит звук через все
пресеты, сохранит WAV-файлы и скриншоты интерфейса.

**Windows / Linux без своего компьютера:** загрузите проект в репозиторий на GitHub.
Файл `.github/workflows/build.yml` сам соберёт версии для macOS, Windows и Linux.
Готовые файлы появятся во вкладке **Actions** → последний запуск → **Artifacts**.

Лицензия JUCE: бесплатна для личного использования и для проектов с доходом до $50k в год
(или под AGPLv3). Для раздачи друзьям этого достаточно.

## Текстуры
Текстуры интерфейса запечены скриптом `tools/make_textures.py` из бесплатных (CC0) фотосканов
[ambientCG](https://ambientcg.com): PaintedMetal004, Metal011, Wood066, SurfaceImperfections003.
Лицевые панели (`tools/render_faceplate.py`), ручки и тумблеры (`tools/render_controls.py`, 120 кадров
поворота на ручку) отрендерены в Blender; спрайты собирает `tools/pack_controls.py`.
Шрифты Barlow / Barlow Condensed и Share Tech Mono — SIL Open Font License (`Resources/fonts/OFL*.txt`).
