# TASKS: InletGuard (русская версия — первоисточник)

> **Разработка ведётся строго по TDD.** Для каждой задачи с меткой **[TDD]**:
> 1. 🔴 **Red** — сначала пишется тест (C unit в `tests/unit/` или `.phpt` в `tests/`), запускается и падает по ожидаемой причине;
> 2. 🟢 **Green** — минимальная реализация до прохождения теста;
> 3. 🔵 **Refactor** — чистка кода при зелёных тестах + прогон под ASan.
>
> Задачи **[SPIKE]** — исследовательские прототипы без TDD; код выбрасывается, вывод фиксируется в `docs/adr/`.
> Задачи **[INFRA]** — инфраструктура (сборка, CI, документация).
>
> **Definition of Done (для всех [TDD]):** тест написан до кода и в истории коммитов виден переход red→green; всё зелёное на матрице PHP 7.0–8.4 (7.0, 7.1, 7.2, 7.3, 7.4, 8.0, 8.1, 8.2, 8.3, 8.4) × NTS/ZTS; ASan/UBSan без замечаний; `-Werror` без предупреждений; код, комментарии, сообщения и тесты — на английском.
>
> **Язык:** проект (код, комментарии, логи, тесты, коммиты, `README.md`, `docs/`, ADR) — **на английском**; `PRD.ru.md` и `TASKS.ru.md` — на русском (первоисточник); `PRD.md`, `TASKS.md` — базовые английские версии; любое изменение вносится в оба файла пары.
>
> **Модульность:** каждый класс атак — отдельный модуль (FR-10), отключаемый при сборке, при загрузке, на уровне пула/хоста и правилом на запрос. Любая задача модуля включает тест «модуль выключен → поведение и производительность не меняются» (NFR-10).
>
> Требования ссылаются на `PRD.ru.md` (FR-x / NFR-x).

---

## Фаза 0. Каркас, инфраструктура тестов, спайки

- [x] **0.1 [INFRA]** Каркас расширения: `config.m4`, `php_inletguard.h`, `inletguard.c` (MINIT/MSHUTDOWN/RINIT/RSHUTDOWN/MINFO), структура `src/core/`, `src/zend/`, `third_party/`, `tests/`, `tests/unit/`, `tests/corpus/`.
- [x] **0.2 [TDD]** Первый `.phpt`: `extension_loaded('inletguard')` и `InletGuard\version()` → сначала красный, затем минимальная реализация.
- [x] **0.3 [INFRA]** Минимальный C unit-раннер (`tests/unit/runner.c`, макросы `IG_TEST`, `IG_ASSERT_*`), цель `make unit`. Первый тест-заглушка красный → зелёный.
- [x] **0.4 [INFRA]** Сборка с ASan/UBSan (`./configure CFLAGS=-fsanitize=...`), цель `make test-asan`; Valgrind-прогон phpt.
- [x] **0.5 [INFRA]** CI `ci.yml` (PRD 9.1) — делается **до первой строки продуктового кода**: матрица 7.0–8.4 (10 версий) × NTS/ZTS на Docker-образах, закреплённых по digest; `make unit`, `make test`; ASan/UBSan-job (на 7.4 и 8.4); Valgrind-job; сборка старым тулчейном (GCC 4.8, glibc 2.17) для проверки C99; aarch64 на ARM-раннерах. Сервисы MySQL/PostgreSQL добавляются в фазе 6, Redis/Memcached — в фазе 15.
- [x] **0.5a [TDD]** Слой совместимости `src/zend/php_compat.h`: макросы/inline-обёртки для расхождений 7.0…8.4 (arginfo с типами, `RETURN_*`/`ZEND_PARSE_PARAMETERS_*`, регистрация классов и констант, `zend_string`-хелперы, object handlers, `ZEND_ACC_*`). На каждую обёртку — phpt, который проходит на всех версиях. Правило: `#if PHP_VERSION_ID` допускается только в этом файле. *Сделано: основа слоя и автоматическая проверка правила (`scripts/check-compat.sh` в `lint`); обёртки добавляются задачами, которым они нужны (см. PRD, заметка 0.5a).*
- [x] **0.5b [INFRA]** Гигиена CI: actions закреплены по SHA, Dependabot для actions и образов, кэш сборок, обязательные проверки в branch protection для `main`, шаблоны PR/issue. *Сделано: SHA-пины, Dependabot, шаблон PR, защита `main` (PR + обязательная проверка `ci-ok`, 0 одобрений, правила для админов). Digest-пины образов перенесены в 0.5d.*
- [x] **0.5c [INFRA]** `nightly.yml`: fuzzing по таргетам с лимитом времени, метрики корпусов, бенчмарки с порогами (таргеты появляются по мере фаз; на старте — пустой каркас с одним тестовым таргетом). *Сделано: полный CI по расписанию + fuzz-харнесс с автообнаружением `tests/fuzz/fuzz_*.c` (проверен тестом на временных таргетах); корпусы и бенчмарки подключатся в 1.8 и 7.3.*
- [x] **0.5d [INFRA]** `release.yml` в режиме **dry-run** (PRD 9.2): на push в `main` и по ручному запуску собирает 40 `.so` (10 версий × NTS/ZTS × x86_64/aarch64) в контейнере с glibc 2.17, упаковывает архивы (`.so`, `20-inletguard.ini`, `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES`), PECL-`.tgz`, `SHA256SUMS`, attestations; сохраняет как artifacts workflow. Публикации нет. *Сделано: 40 сборок в manylinux2014 (glibc 2.17) против PHP из проверенных исходников, phpt каждой сборки, архивы + исходный тарбол + `SHA256SUMS`. PECL-пакет перенесён в 7.7, attestations — в 0.5e (только настоящий релиз), ASan для 7.x — следующим PR (см. PRD, заметка 0.5d).*
- [x] **0.5e [INFRA]** Замки публикации в `release.yml`: шаг публикации выполняется только при `vars.RELEASES_ENABLED == 'true'` **и** теге `≥ v1.0.0`; теги `v0.*` отклоняются с ошибкой; публикация — draft-релиз в environment `release` с обязательным подтверждением. Тест: dry-run на тестовом теге `v0.0.0-test` в форке/ветке падает на проверке политики, а не на сборке. *Сделано: `check-policy.sh` (тесты), job `policy` перед сборками, job `publish` (attestations + draft-релиз) только при разрешении политики и настоящем теге, в окружении `release` (одобрение владельца, только теги `v*`). Вместо публичного тестового тега — ручной запуск с `simulate_ref` (см. PRD, заметка 0.5e).*
- [x] **0.5f [INFRA]** Проверка согласованности версий (тег = `php_inletguard.h` = `package.xml` = раздел `CHANGELOG.md`) — скрипт `scripts/check-version.sh` + запуск в `release.yml`; тесты скрипта на расхождениях. *Сделано: `check-version.sh` (тег = `ig_version.h` = раздел `CHANGELOG.md`, без `-dev`; `package.xml` — если есть) в job `policy`, release notes из CHANGELOG (`changelog-notes.sh`); заведён `CHANGELOG.md`. Версия хранится в `src/core/ig_version.h` (а `php_inletguard.h` берёт её оттуда).*
- [ ] **0.5g [INFRA]** Smoke-тесты артефактов: каждый `.so` загружается (`php -d extension=… -m`) в чистых контейнерах CentOS 7 / AlmaLinux 8–9 / Debian / Ubuntu с соответствующей версией PHP; на пустом расширении — проверка загрузки и `InletGuard\version()`, позже — сценарий FR-13.7 (задача 7.5a).
- [ ] **0.6 [INFRA]** `.clang-format`, `clang-tidy`/`cppcheck` в CI, `-Wall -Wextra -Werror`. *Частично: clang-format, cppcheck, `-Werror` (через `--enable-inletguard-dev`) готовы; `clang-tidy` — после появления кода ядра в фазе 1.*
- [ ] **0.7 [INFRA]** Вендоринг libinjection в `third_party/libinjection/` (зафиксированная версия, LICENSE), подключение к сборке.
- [ ] **0.8 [SPIKE]** Перехват входных данных через `sapi_register_input_filter` с цепочкой к ext/filter: убедиться, что `filter_input()` продолжает работать — **на 7.0 и 8.4**. → ADR-001.
- [ ] **0.9 [SPIKE]** Механизм блокировки: прекратить выполнение скрипта после RINIT с отдачей 403 и тела (варианты: hook `zend_compile_file`, `zend_execute_ex`, `zend_bailout`) в FPM, CGI, mod_php, LSAPI — **на 7.0 и 8.4**. Observer API (8.0+) не используем, т.к. его нет в 7.x. → ADR-002.
- [ ] **0.10 [SPIKE]** Перехват internal-функций и методов (`PDO::query`, `mysqli_query`): подмена handler, порядок загрузки расширений, совместимость с Xdebug 2.x (7.x) и 3.x (8.x). → ADR-003.
- [x] **0.11 [INFRA]** `README.md` **на английском** (описание, поддерживаемые версии PHP 7.0–8.4, сборка, установка, быстрый старт, запуск тестов); `CONTRIBUTING.md` (на английском) с описанием TDD-процесса и языковых соглашений.
- [x] **0.12 [INFRA]** Лицензирование (NFR-9): `LICENSE` (MIT), `THIRD_PARTY_NOTICES` (libinjection, BSD-3-Clause), SPDX-заголовок `SPDX-License-Identifier: MIT` в каждом исходнике; CI-проверка наличия заголовков и лицензий вендоренного кода (запрет GPL/LGPL).
- [ ] **0.13 [SPIKE]** Конфигурация на уровне пула/хоста: какие `PHP_INI_SYSTEM`-значения из `php_admin_value` (пул FPM, `<VirtualHost>` Apache, LSAPI) видны в MINIT, а какие только в RINIT; как это сочетается с установкой хуков в мастер-процессе FPM — **на 7.0 и 8.4**. → ADR-009 (определяет разделение директив на «загрузка» и «выполнение», FR-10.5).
- [ ] **0.14 [INFRA]** Флаги сборки модулей в `config.m4`: `--with-inletguard-modules=all|список`, `--disable-inletguard-<m>`; макросы `IG_MODULE_<M>_ENABLED`; CI-job, собирающий расширение с минимальным набором модулей.

**Результат фазы:** собирающееся пустое расширение, зелёный CI на всей матрице, релизный конвейер, который в dry-run собирает и проверяет все артефакты (без публикации), работающие оба уровня тестов, принятые ADR по четырём рискованным механизмам.

---

## Фаза 1. Ядро детекции (чистый C, без Zend)

Все задачи тестируются C unit-тестами — быстрый TDD-цикл.

- [ ] **1.1 [TDD]** Обёртка `ig_detect_sqli(const char*, size_t, ig_result_t*)` над libinjection: тесты на классические payload'ы (`' OR 1=1--`, `UNION SELECT`, `; DROP TABLE`) и на безопасные строки. Fingerprint в результате. (FR-2.1)
- [ ] **1.2 [TDD]** Обёртка `ig_detect_xss(...)`: `<script>`, `onerror=`, `javascript:`, `<svg onload>` vs обычный текст и безопасный HTML (`<b>`). (FR-2.2)
- [ ] **1.3 [TDD]** Единый интерфейс детектора `ig_detector_t` (имя, битовая маска, функция) и реестр детекторов. (FR-2.4)
- [ ] **1.4 [TDD]** Нормализатор: повторный URL-decode с лимитом раундов, удаление NUL, `%u`-последовательности. Тесты на двойное кодирование и границы буфера. (FR-1.3)
- [ ] **1.5 [TDD]** Нормализатор для XSS: декодирование HTML-сущностей (именованные, `&#x..;`, `&#..;`, без `;`). (FR-1.3)
- [ ] **1.6 [TDD]** Ограничение длины: значения > `max_len` проверяются по префиксу, флаг `truncated` в результате. (FR-1.4)
- [ ] **1.7 [TDD]** Glob-матчер для исключений (`*`, экранирование, регистр). (FR-5.4)
- [ ] **1.8 [TDD]** Корпус-раннер: `tests/corpus/attacks/{sqli,xss}.txt`, `tests/corpus/benign/*.txt` (имена, адреса, тексты на русском/английском, JSON, SQL-подобный легитимный текст, HTML из WYSIWYG). Тест падает, если метрики ниже NFR-5.
- [ ] **1.9 [TDD]** Регрессионные тесты на найденные ложные срабатывания (процесс: каждый FP → строка в benign-корпус → фикс).

**Результат фазы:** библиотека `libigcore.a`, покрытая unit-тестами, метрики точности в CI.

---

## Фаза 2. Конфигурация и модульность

> Фундамент модульности закладывается в v1.0, хотя модулей пока два (`sqli`, `xss`): все последующие классы подключаются к этому каркасу без изменения ядра.

- [ ] **2.1 [TDD]** Регистрация всех ini-директив из FR-8 как `PHP_INI_SYSTEM`; phpt: значения по умолчанию через `ini_get()`.
- [ ] **2.2 [TDD]** phpt: `ini_set('inletguard.enabled', 0)` возвращает `false` и не меняет значение.
- [ ] **2.3 [TDD]** Парсинг `mode` (`monitor|block|sanitize`), наследование `sqli.mode/xss.mode/sqli.sink.mode` от глобального. Unit-тест парсера + phpt.
- [ ] **2.4 [TDD]** Парсинг списков (`inspect_sources`, `inspect_headers`, `exclude_*` с префиксом источника `post:name`). Unit-тесты на пробелы, пустые элементы, дубликаты.
- [ ] **2.5 [TDD]** Невалидные значения → значение по умолчанию + `config_warning` (проверяется после реализации логгера в 3.x; до этого — через внутренний буфер предупреждений).
- [ ] **2.6 [TDD]** `sqli.sink.mode=sanitize` → трактуется как `block` с предупреждением. (FR-3.3)
- [ ] **2.7 [TDD]** `phpinfo()`-секция: версия, версия libinjection, эффективная конфигурация (phpt по выводу `php -i`).
- [ ] **2.8 [TDD]** Реестр модулей в `core/` (FR-10.1): дескриптор модуля (ID, описание, поддерживаемые ключи, детекторы, стоки, стратегии sanitize, значения по умолчанию для каждого профиля); ядро обращается к модулям только через реестр. `sqli` и `xss` — первые модули.
- [ ] **2.9 [TDD]** Генерация директив `inletguard.<m>.*` из дескриптора по единой схеме (FR-10.3): `enabled`, `mode`, `input/sink/output.enabled`, `input/sink.mode`, `level`, `exclude_params`, `exclude_paths`, `sanitize`, `log`, `block_status`, `block_page`; неподдерживаемый модулем ключ → `config_warning`.
- [ ] **2.10 [TDD]** `inletguard.modules` (`all`, список, `all,-m`): незагруженный модуль не ставит хуков и не выполняется (проверка через счётчик установленных хуков в тестовой сборке); модуль, не собранный в бинарник (0.14), но указанный в конфиге, → предупреждение.
- [ ] **2.11 [TDD]** Профили `off|minimal|balanced|strict|paranoid` (FR-10.4); явные директивы перекрывают профиль; тест-матрица «профиль × модуль» сверяется с `docs/profiles.md`.
- [ ] **2.12 [TDD]** Приоритет источников (FR-10.5) по ADR-009: default < профиль < глобальные < модульные < пул/хост; директивы «загрузки» в пуле игнорируются с предупреждением. phpt (CLI/CGI с `-d`) + интеграционный тест на FPM с двумя пулами.
- [ ] **2.13 [TDD]** `<m>.level` 1–4: передаётся детекторам; для `sqli`/`xss` уровень управляет глубиной декодирования, набором источников и эвристиками; матрица в `docs/levels.md`.
- [ ] **2.14 [TDD]** Обнаружение неизвестных ключей `inletguard.*` в конфигурации с подсказкой «did you mean …» (расстояние Левенштейна); `config_strict=1` — расширение не активируется при ошибках.
- [ ] **2.15 [TDD]** Интроспекция (FR-10.7): `phpinfo()`/`--ri` с источником каждого значения, маскирование секретов; `InletGuard\config()`, `InletGuard\modules()`.
- [ ] **2.16 [TDD]** `allow_user_tightening` + `InletGuard\tighten()` (FR-10.6): ужесточение работает только в текущем запросе, попытка ослабить → `false` + warning, при `allow_user_tightening=0` — всегда `false`.
- [ ] **2.17 [TDD]** Механизм alias устаревших директив с deprecation-предупреждением (FR-10.8).
- [ ] **2.18 [INFRA]** CLI `bin/inletguard-config check [--pool=…]`; примеры `inletguard.ini-recommended`, `inletguard.ini-hosting`, `inletguard.ini-paranoid` с комментариями на английском; тест «каждый пример проходит `check` без предупреждений».
- [ ] **2.19 [TDD]** Zero-config (FR-13): phpt без единой директивы `inletguard.*` — действует профиль `balanced`, `inletguard.mode` не задан, режимы модулей берутся из профиля; `inletguard.mode=monitor` перекрывает все режимы профиля.
- [ ] **2.20 [TDD]** Режим на источник `<m>.input.<source>.mode`; для `xss` в `balanced`: GET/COOKIE/URI/заголовки — `block`, POST/JSON — `monitor`.
- [ ] **2.21 [TDD]** Однократное info-сообщение «running with built-in defaults» при работе без конфигурации; не выводится, если задан `inletguard.profile`. (FR-13.6)
- [ ] **2.22 [INFRA]** Файл `20-inletguard.ini` для пакетов: одна строка `extension=inletguard.so` + закомментированные подсказки по основным директивам; тест «файл проходит `inletguard-config check`».

---

## Фаза 3. Логгер

- [ ] **3.1 [TDD]** JSON-сериализатор (core): экранирование `"`, `\`, управляющих символов, замена невалидного UTF-8 на `�`. Unit-тесты + fuzz-кандидат. (FR-6.3)
- [ ] **3.2 [TDD]** Структура события `ig_event_t` и сериализация в JSON-строку со всеми полями FR-6.2; усечение payload по `log_payload_max` без разрыва UTF-8-символа.
- [ ] **3.3 [TDD]** `log_payload=0` — поле `payload` отсутствует. (FR-6.7)
- [ ] **3.4 [TDD]** Запись в файл `inletguard.log_file`: `O_APPEND`, одна строка — один `write()`; phpt проверяет содержимое файла.
- [ ] **3.5 [TDD]** Без `log_file` — запись в `error_log`. (FR-6.1)
- [ ] **3.6 [TDD]** Недоступный путь лога → запрос не прерывается, одно предупреждение в `error_log`. (FR-6.5)
- [ ] **3.7 [TDD]** Генерация `request_id` (128-бит, hex) в RINIT; уникальность в пределах тестов. (FR-7)
- [ ] **3.8 [TDD]** `log_level=attacks|all`. (FR-6.6)
- [ ] **3.9 [TDD]** `log_rotate=external`: переоткрытие файла при смене inode/устройства раз в `log_reopen_check` секунд (совместимость с `logrotate`). (FR-6.8)
- [ ] **3.10 [TDD]** Тест конкурентной записи: N процессов CGI пишут в один лог — все строки валидный JSON.
- [ ] **3.11 [TDD]** Встроенная ротация `log_rotate=size|daily|hourly`, `log_max_size`, `log_max_files`: ротацию выполняет один процесс под `flock` lock-файла, остальные переоткрывают; стресс-тест N процессов во время ротации — ни одной потерянной или разорванной строки. (FR-6.8)
- [ ] **3.12 [TDD]** Защита от флуда: `log_rate_limit`, `log_dedup_window` (агрегация с `count`), событие `log_suppressed`. (FR-6.9)
- [ ] **3.13 [TDD]** Приватность: `log_fields_exclude`, `log_ip_anonymize` (IPv4/IPv6). (FR-6.10)
- [ ] **3.14 [TDD]** `log_format=logfmt` с тем же набором полей и экранированием. (FR-6.11)
- [ ] **3.15 [TDD]** `<m>.log=0` — события модуля не пишутся, но действие (block/sanitize) применяется. (FR-6.12)

---

## Фаза 4. Проверка входных данных запроса

- [ ] **4.1 [TDD]** phpt `--GET--` с SQLi при `mode=monitor`: скрипт выполняется, в логе событие `source:get, attack_type:sqli`. (FR-1.1, FR-4.1)
- [ ] **4.2 [TDD]** Аналогично для POST (urlencoded), COOKIE, URI path, query string.
- [ ] **4.3 [TDD]** POST multipart: проверяются текстовые поля, файлы — нет.
- [ ] **4.4 [TDD]** Заголовки из `inspect_headers` (phpt с `--ENV--` HTTP_USER_AGENT).
- [ ] **4.5 [TDD]** Ключи и вложенные массивы `a[<script>][x]=...`; лимит `inspect_max_depth`. (FR-1.2)
- [ ] **4.6 [TDD]** Лимиты `inspect_max_len`, `inspect_max_vars` → событие `limit_exceeded`. (FR-1.4)
- [ ] **4.7 [TDD]** `mode=block`: статус `block_status`/`<m>.block_status`, код скрипта **не выполняется** (маркер-вывод отсутствует). (FR-4.2) — по ADR-002.
- [ ] **4.7a [TDD]** Тело блокировки: `block_format=auto|html|json|text` (`auto` по `Accept`), шаблон `block_page`/`<m>.block_page` с подстановками `{request_id}`, `{status}`, `{time}` (значения экранируются), встроенная страница по умолчанию; заголовок `X-InletGuard-Request-Id` только при `response_header=1`.
- [ ] **4.8 [TDD]** `mode=sanitize`: стратегии `xss.sanitize=escape|strip_tags|empty` и `sqli.sanitize=empty|strip`; для `strip` — повторная проверка результата и откат к пустой строке, если значение всё ещё детектируется; `action:sanitized` в логе. (FR-4.3)
- [ ] **4.9 [TDD]** Переопределение режима по типу атаки и выбор самой строгой реакции при нескольких срабатываниях. (FR-4.4, FR-4.5)
- [ ] **4.10 [TDD]** Исключения: `exclude_params` (с источником и без), `exclude_paths`, `<m>.exclude_params`, `<m>.exclude_paths`, glob. (FR-5)
- [ ] **4.11 [TDD]** Совместимость с ext/filter: `filter_input(INPUT_GET, ...)` работает с включённым InletGuard. (NFR-4) — по ADR-001.
- [ ] **4.12 [TDD]** `cli_enabled=0` — в CLI проверка не выполняется; `=1` — выполняется. (FR-1.6)
- [ ] **4.13 [TDD]** `enabled=0`, `<m>.enabled=0`, `<m>.input.enabled=0` отключают соответствующую проверку; для каждого уровня — тест «выключено → нет событий и нет изменений в суперглобалях».
- [ ] **4.14 [TDD]** `fail_mode=open`: искусственная внутренняя ошибка (тестовый хук через `#ifdef IG_TESTING`) → `internal_error` в логе, скрипт выполняется (NFR-3); `fail_mode=closed` → запрос блокируется.
- [ ] **4.15 [TDD]** `trusted_proxies`: IP клиента из `X-Forwarded-For`/`X-Real-IP` берётся только если соединение пришло от доверенного прокси; иначе — `REMOTE_ADDR` (защита от подмены IP в логе и в правилах).

---

## Фаза 5. PHP API

- [ ] **5.1 [TDD]** `InletGuard\check_sqli(string): ?string` — fingerprint/`null`; TypeError на не-строку. (FR-7)
- [ ] **5.2 [TDD]** `InletGuard\check_xss(string): bool`.
- [ ] **5.3 [TDD]** `InletGuard\inspect($value, int $types)` (`string|array`, проверка типа вручную — union-типов нет в 7.x) — рекурсивно по массиву, возвращает список `['type','path','fingerprint']`; `TypeError` на прочие типы одинаково на 7.0 и 8.4.
- [ ] **5.4 [TDD]** `InletGuard\events()` — события текущего запроса (после 4.x); `request_id()`.
- [ ] **5.5 [TDD]** Константы `SQLI`, `XSS`, `MODE_*`; класс `InletGuard\SqlInjectionException`.
- [ ] **5.6 [TDD]** `api_log=1` — вызовы API с находками пишутся в лог со `stage:api`; API никогда не блокирует.
- [ ] **5.7 [INFRA]** Stub-файл `inletguard.stub.php` + генерация arginfo (`gen_stub.php`, есть с 8.0) для IDE и рефлексии; для 7.x — `inletguard_legacy_arginfo.h` (генерация в legacy-режиме или вручную) + phpt на Reflection, сверяющий сигнатуры между версиями.

---

## Фаза 6. Перехват SQL-функций (RASP)

- [ ] **6.1 [TDD]** SQL-токенизатор в core (строки с разными кавычками и экранированием, числа, идентификаторы в `` ` `` / `"`, комментарии `--`, `#`, `/* */`, операторы). Unit-тесты на диалекты MySQL/PostgreSQL/SQLite.
- [ ] **6.2 [TDD]** Структурная проверка `ig_sql_input_breaks_structure(query, input)`: вход внутри одного литерала → безопасно; выход за литерал → инъекция. Unit-тесты: `id=5` в `WHERE id=5` (ok), `5 OR 1=1` (атака), `O'Brien` корректно экранированный (ok), неэкранированный (атака). (FR-3.2)
- [ ] **6.3 [TDD]** Сбор пользовательских значений запроса (из фазы 4) в per-request хранилище с фильтром `sqli.sink.min_input_len`.
- [ ] **6.4 [TDD]** Хук `SQLite3::query/exec/querySingle/prepare` (SQLite всегда доступен в CI): monitor — запрос выполняется, событие `stage:sql_hook` с `function` и `script:line`.
- [ ] **6.5 [TDD]** Режим `block` + `sqli.sink.block_action=exception` → `InletGuard\SqlInjectionException`, запрос не выполнен; `=false` → `false` + warning. (FR-3.3)
- [ ] **6.6 [TDD]** Хуки `PDO::query/exec/prepare` (тесты на `pdo_sqlite`, затем MySQL/PostgreSQL в CI).
- [ ] **6.7 [TDD]** Хуки `mysqli_*` (процедурные и ООП) — phpt с `SKIPIF` при отсутствии MySQL.
- [ ] **6.8 [TDD]** Хуки `pg_*` — phpt с `SKIPIF` при отсутствии PostgreSQL.
- [ ] **6.9 [TDD]** Отсутствующее расширение (например, без mysqli) — хук не ставится, ошибок нет.
- [ ] **6.10 [TDD]** Сохранение и вызов оригинального handler; совместимость с Xdebug (phpt с `SKIPIF` без Xdebug). (по ADR-003)
- [ ] **6.11 [TDD]** `sqli.sink.enabled=0` — перехват не устанавливается.

---

## Фаза 7. Укрепление, производительность, релиз

- [ ] **7.1 [TDD]** libFuzzer-таргеты: нормализатор, JSON-сериализатор, SQL-токенизатор, структурная проверка; найденные краши → регрессионные unit-тесты. (NFR-2)
- [ ] **7.2 [TDD]** Полный прогон phpt под Valgrind и ASan на всей матрице — 0 утечек.
- [ ] **7.3 [TDD]** Бенчмарки (NFR-1) как тесты с порогом: падают при превышении бюджета времени.
- [ ] **7.4 [TDD]** Тесты обхода: корпус известных техник обхода WAF (комментарии в ключевых словах, регистр, кодировки, HPP) → метрики NFR-5.
- [ ] **7.5 [INFRA]** Интеграционный стенд: Docker (nginx + php-fpm 7.0/7.4/8.0/8.4, Apache mod_php 7.4, OpenLiteSpeed + LSAPI) с WordPress и тестовым уязвимым приложением (DVWA-подобным); прогон `sqlmap` в режиме block — 0 успешных инъекций.
- [ ] **7.5a [TDD]** Автотест «из коробки» (FR-13.7) в `release.yml` и nightly: чистый контейнер + `.so` + однострочный ini → SQLi в GET и через `PDO::query` блокируется, XSS в GET блокируется, XSS в POST логируется, легитимный трафик WordPress (установка, логин, публикация поста с HTML) проходит без блокировок.
- [ ] **7.6 [INFRA]** Документация **на английском**: `docs/quickstart.md` (установка за 1 шаг, что включено по умолчанию, как перейти в осторожный режим), `docs/configuration.md` (все директивы), `docs/deployment.md` (monitor → block, установка на shared-хостинг / cPanel / CloudLinux alt-php), `docs/log-format.md`, `docs/api.md`, `docs/compatibility.md` (таблица версий PHP × SAPI).
- [ ] **7.7 [INFRA]** Финализация пакетов (конвейер готов с 0.5d): проверка `package.xml` для PECL (7.0–8.4) через `pecl install` из `.tgz` на всей матрице, `composer.json` для PIE (8.x), примеры `inletguard.ini-*` (2.18) в архивах; генерация release notes из `CHANGELOG.md`.
- [ ] **7.8 [INFRA]** Первый публичный релиз: `CHANGELOG.md` для 1.0.0, включение `RELEASES_ENABLED=true`, тег `v1.0.0` → draft-релиз → подтверждение → публикация GitHub Release, затем PECL и Packagist. **До этой задачи релизы не публикуются.**

---

# Расширение охвата классов атак (после v1.0)

> Каталог классов, CWE и уровни обнаружения — `PRD.ru.md`, раздел 10.
>
> **Чек-лист готовности класса атак (дополняет общий DoD):**
> 1. 🔴 Сначала корпус: `tests/corpus/attacks/<class>.txt` и `tests/corpus/benign/<class>.txt`, целевые метрики записаны в задаче — тест корпуса красный.
> 2. Unit-тесты токенизатора/грамматики в `core/` (до реализации).
> 3. phpt на **каждый** сток в режимах `monitor` и `block` + тест исключений; `SKIPIF`, если функции нет в данной версии PHP/расширение не загружено.
> 4. Fuzz-таргет на каждый новый парсер.
> 5. Модуль в реестре (2.8) с директивами `inletguard.<m>.*` по единой схеме (FR-10.3), значения по умолчанию для всех профилей, флаг сборки `--disable-inletguard-<m>`; бюджет производительности в бенчмарке; тесты «выключен при сборке / не загружен / `enabled=0` / отключён правилом → нулевой эффект».
> 6. Документация модуля в `docs/attacks/<m>.md` (на английском): что ловит, директивы, уровни `level`, стратегии sanitize, примеры исключений.

---

## Фаза 8. Обобщение архитектуры и расширение входа (v1.1)

- [ ] **8.1 [SPIKE]** Стоимость сотен хуков: замер оверхеда подмены handler на 7.0 и 8.4, быстрый путь «нет пользовательского ввода». → ADR-004.
- [ ] **8.2 [TDD]** Реестр пользовательского ввода (taint-lite) в `core/`: значения с источником и путём, поиск всех вхождений в строку (Aho–Corasick), порог `min_input_len`.
- [ ] **8.3 [TDD]** Реестр стоков: декларативная таблица (функция/метод, индекс аргумента, грамматика, класс), единый диспетчер хуков. Перевод SQL-хуков фазы 6 — все тесты фазы 6 остаются зелёными.
- [ ] **8.4 [INFRA]** Шаблон модуля `src/modules/_template/` (дескриптор, детектор, сток, тесты, корпус, документация) и генератор-скрипт для нового модуля; первым по нему делается модуль `hpp` (8.7).
- [ ] **8.5 [TDD]** Проверка JSON-тела (`application/json`): рекурсивно ключи и значения, лимиты глубины/размера, невалидный JSON → событие. (класс 5)
- [ ] **8.6 [TDD]** Проверка XML-тела без раскрытия сущностей и raw-тела (`php://input`) с лимитом размера.
- [ ] **8.7 [TDD]** HPP и аномалии ввода: дубли параметров, массив вместо скаляра, NUL, overlong/невалидный UTF-8. (класс 3)
- [ ] **8.8 [TDD]** Детект сканеров и разведки: файл сигнатур UA и путей пробинга (`/.env`, `/.git/`, `*.bak`, `/phpmyadmin`), формат файла и его парсер. (класс 4)
- [ ] **8.9 [TDD]** Корпус-раннер по классам; метрики per-class в CI.
- [ ] **8.10 [TDD]** Расширение API: битовые флаги классов в `inspect()`.

**8.B Режим обучения (модуль `learn`, FR-11)**
- [ ] **8.11 [TDD]** Модуль `learn`: `learn.enabled`, `learn.force_monitor` (понижение block/sanitize до `would_block`), `learn.trusted_ips`.
- [ ] **8.12 [TDD]** Нормализатор шаблонов путей в `core/`: числа → `{id}`, UUID → `{uuid}`, hex → `{hash}`, пользовательские `learn.path_templates`.
- [ ] **8.13 [TDD]** Дополнительные поля событий: `path_template`, `status` (дописывается в RSHUTDOWN, событие буферизуется до конца запроса), `request_attacks`, `trusted`.
- [ ] **8.14 [TDD]** CLI `bin/inletguard-learn` (PHP, тесты PHPUnit на фикстурах логов): группировка, оценка уверенности, пороги `--min-ips/--min-days/--min-hits`, `--since/--until`, `--trusted-only`, `--exclude-scanners`, вывод `--format=ini` с доказательствами в комментариях.
- [ ] **8.15 [TDD]** Защита от отравления: тест-фикстуры «атака с многих IP во время обучения», «сканер», «IP с другими атаками» — не должны попадать в кандидаты.

## Фаза 9. Файловая система: LFI, RFI, загрузки, webshell (v1.2)

- [ ] **9.1 [TDD]** Нормализатор путей в `core/` (`..`, `//`, `./`, NUL, `%2e%2e`, обратные слеши, символические ссылки — через `realpath` в zend-слое) и проверка «ввод выводит путь за пределы `lfi.base_dirs`/document root».
- [ ] **9.2 [TDD]** Input-сигнатуры LFI/RFI: `../`, `/etc/passwd`, `php://filter`, `data://`, `expect://`. (классы 6–7)
- [ ] **9.3 [SPIKE]** Хук `include/require/include_once/require_once`: `zend_compile_file` vs `zend_resolve_path` vs `zend_stream_open_function`, совместимость с OPcache (кэш скомпилированных файлов). → ADR-005.
- [ ] **9.4 [TDD]** Sink: `include/require` с пользовательским вводом в пути → traversal / обёртки потоков. (класс 6–7)
- [ ] **9.5 [TDD]** Sink: файловые функции чтения (`fopen`, `file_get_contents`, `file`, `readfile`, `highlight_file`, `show_source`, `parse_ini_file`, `SplFileObject`, `fpassthru`-цепочки).
- [ ] **9.6 [TDD]** Sink: файловые функции изменения (`unlink`, `rename`, `copy`, `mkdir`, `rmdir`, `chmod`, `touch`, `symlink`, `link`).
- [ ] **9.7 [TDD]** Политика обёрток потоков: allow/deny-список схем для include и файловых функций (`phar://`, `zip://`, `data://`, `php://input`, `http(s)://`, `ftp://`, `expect://`).
- [ ] **9.8 [TDD]** Проверка `$_FILES` при старте запроса: опасные расширения, двойные расширения, `.htaccess`/`.user.ini`, PHP-теги в содержимом, несоответствие MIME. (класс 8)
- [ ] **9.9 [TDD]** Sink `move_uploaded_file`: целевой путь с исполняемым расширением внутри docroot.
- [ ] **9.10 [TDD]** `upload.noexec_paths`: запрет компиляции/исполнения PHP-файлов из указанных каталогов (через хук из ADR-005). (класс 9)
- [ ] **9.11 [TDD]** Детект записи webshell: `file_put_contents`/`fwrite`/`copy`/`rename` создают `.php`-файл с пользовательскими данными или PHP-тегами.
- [ ] **9.12 [TDD]** Сигнатуры известных webshell при компиляции файла (c99, r57, WSO, `eval(base64_decode($_POST…))`), по умолчанию `monitor`.
- [ ] **9.13 [TDD]** Zip Slip: `ZipArchive::extractTo`, `PharData::extractTo` — записи с `../` и абсолютными путями. (класс 10)

## Фаза 10. Командные и кодовые инъекции (v1.3)

- [ ] **10.1 [TDD]** Shell-токенизатор в `core/` (POSIX sh/bash: кавычки, экранирование, `$()`, backticks, `;`, `|`, `&&`, `||`, `&`, перенаправления, перевод строки, `${IFS}`, glob).
- [ ] **10.2 [TDD]** Структурная проверка: ввод должен целиком укладываться в один аргумент.
- [ ] **10.3 [TDD]** Sink: `exec`, `system`, `passthru`, `shell_exec` (вкл. backticks — тот же handler), `popen`, `proc_open` (строка и массив), `pcntl_exec`. (класс 11)
- [ ] **10.4 [TDD]** Input-сигнатуры command injection (`;id`, `|nc`, `$(...)`, `` `...` ``, `${IFS}`), по умолчанию `monitor`.
- [ ] **10.5 [TDD]** Argument/env injection: `mail()` 5-й параметр (`-X`, `-O`, `-C`), `putenv` с `LD_PRELOAD`/`LD_LIBRARY_PATH`/`PATH`, `imap_open` (`-oProxyCommand`). (класс 12)
- [ ] **10.6 [TDD]** PHP-токенизатор (минимальный) для проверки кода в `eval`: ввод внутри строкового литерала vs выход в код.
- [ ] **10.7 [TDD]** Sink `eval` через хук `zend_compile_string`. (класс 13)
- [ ] **10.8 [TDD]** Sink: строковый `assert` (7.x, `SKIPIF` на 8.x), `create_function` (≤7.4, `SKIPIF` на 8.x).
- [ ] **10.9 [TDD]** Динамические вызовы с пользовательским именем опасной функции: `call_user_func(_array)`, `array_map`, `array_filter`, `usort`/`uasort`/`uksort`, `array_walk`, `register_shutdown_function`, `ReflectionFunction`, `$f()` (опкод `INIT_DYNAMIC_CALL`/`INIT_FCALL_BY_NAME` — через `zend_execute_ex` или user opcode handler; механизм по ADR-004).

## Фаза 11. Десериализация, XXE, SSRF (v1.4)

- [ ] **11.1 [TDD]** Парсер формата `serialize()` в `core/` (только разбор структуры: `O:`, `C:`, `r:`, `R:`, вложенность), fuzz-таргет.
- [ ] **11.2 [TDD]** Sink `unserialize`: объекты в данных из запроса → атака; политика `deser.allowed_classes` (принудительно), блок известных POP-гаджетов (Monolog, Guzzle, Laravel…) по имени класса. (класс 14)
- [ ] **11.3 [TDD]** Input-сигнатуры сериализованных объектов (`O:\d+:"`), в т.ч. в base64.
- [ ] **11.4 [TDD]** phar-десериализация: `phar://` в файловых функциях (`file_exists`, `is_file`, `filesize`, `getimagesize`…) с пользовательским вводом (переиспользует 9.7).
- [ ] **11.5 [TDD]** Детектор DOCTYPE/внешних сущностей в `core/` (SYSTEM/PUBLIC, параметрические сущности), XML bomb (подсчёт раскрытия). (класс 15)
- [ ] **11.6 [TDD]** Sink: `DOMDocument::loadXML/load`, `simplexml_load_string/file`, `SimpleXMLElement::__construct`, `XMLReader::open/XML`, `xml_parse` — внешние сущности при `LIBXML_NOENT`/`LIBXML_DTDLOAD`; на 7.x — принудительный `libxml_disable_entity_loader(true)` (`SKIPIF` по версии).
- [ ] **11.7 [TDD]** URL-нормализатор в `core/` (RFC 3986 + расхождения `parse_url`/libcurl, userinfo `@`, обратные слеши, IDN) и IP-нормализатор (десятичная/восьмеричная/hex запись, `0`, IPv4-mapped IPv6, сокращения), CIDR-матчер; корпус обходов.
- [ ] **11.8 [TDD]** SSRF-политика: deny приватных (RFC 1918), loopback, link-local, CGNAT, metadata (`169.254.169.254`, `fd00:ec2::254`, `metadata.google.internal`), схем `gopher/dict/file/ldap`; `ssrf.allow_hosts`/`ssrf.deny_hosts`. (класс 16)
- [ ] **11.9 [TDD]** Sink: `curl_exec`/`curl_multi_exec` (чтение `CURLOPT_URL`, принудительный `CURLOPT_PROTOCOLS`/`CURLOPT_REDIR_PROTOCOLS`), `file_get_contents`/`fopen`/`copy` по URL, `fsockopen`, `stream_socket_client`, `get_headers`, `getimagesize`, `SoapClient`.
- [ ] **11.10 [SPIKE]** Проверка после DNS-резолва и защита от DNS rebinding (фиксация IP: `CURLOPT_RESOLVE`/`CURLOPT_OPENSOCKETFUNCTION`, свой stream context для http-обёртки), проверка каждого редиректа. → ADR-006.
- [ ] **11.11 [TDD]** Реализация решения ADR-006 для curl и http-обёртки потоков.

## Фаза 12. Прочие инъекции и заголовки (v1.5)

- [ ] **12.1 [TDD]** Токенизатор LDAP-фильтров (RFC 4515) и DN; sink `ldap_search/list/read`, `ldap_bind`. (класс 17)
- [ ] **12.2 [TDD]** Токенизатор XPath 1.0; sink `DOMXPath::query/evaluate`, `SimpleXMLElement::xpath`. (класс 18)
- [ ] **12.3 [TDD]** NoSQL: input-детект ключей-операторов (`$ne`, `$gt`, `$where`, `$regex`, `$expr`) в GET/POST/JSON; sink `MongoDB\Driver\Query`, `BulkWrite`, `Command` (`SKIPIF` без ext/mongodb). (класс 19)
- [ ] **12.4 [TDD]** SSTI-эвристики (Twig, Smarty, Blade, Latte, общие `{{7*7}}`), по умолчанию `monitor`. (класс 20)
- [ ] **12.5 [TDD]** ReDoS / regex injection: sink `preg_match*`, `preg_replace*`, `preg_split`, `preg_grep` с пользовательским вводом в шаблоне/модификаторах; опциональный локальный `pcre.backtrack_limit`. (класс 21)
- [ ] **12.6 [TDD]** CRLF / header injection: `header()`, `setcookie()`, `setrawcookie()`; email header injection: `mail()` (to, subject, additional_headers), `mb_send_mail`. (класс 22)
- [ ] **12.7 [TDD]** Open redirect: `header('Location: …')` с внешним хостом из ввода (вкл. `//evil.com`, `/\evil.com`, `https:evil.com`); `redirect.allow_hosts`. (класс 23)
- [ ] **12.8 [TDD]** Перезапись переменных: `extract()` суперглобалей без `EXTR_SKIP`, `parse_str()` без 2-го аргумента (7.x). (класс 24)
- [ ] **12.9 [TDD]** Type juggling: `0e\d+` и массивы в полях аутентификации (`auth_params`), по умолчанию `monitor`. (класс 25)

## Фаза 13. Анализ ответа (v2.0)

- [ ] **13.1 [SPIKE]** Output handler: оверхед, совместимость с `ob_gzhandler`, `zlib.output_compression`, `flush()`/стримингом, `fastcgi_finish_request`. → ADR-007.
- [ ] **13.2 [TDD]** HTML-токенизатор контекстов в `core/` (текст, тег, атрибут в кавычках/без, `<script>`, `<style>`, URL-атрибуты).
- [ ] **13.3 [TDD]** Reflected XSS: пользовательский ввод отражён в ответе без экранирования в опасном контексте; режимы monitor / block (замена ответа) / sanitize (экранирование фрагмента). (класс 26)
- [ ] **13.4 [TDD]** Утечка информации: SQL-ошибки (MySQL/PG/SQLite), PHP stack traces и warnings, вывод `phpinfo()`, секреты (AWS keys, `-----BEGIN PRIVATE KEY-----`, JWT) → лог/маскирование. (класс 27)
- [ ] **13.5 [TDD]** Защитные заголовки ответа (opt-in, не перезаписывать заданные приложением). (класс 28)
- [ ] **13.6 [TDD]** Анализ только для `text/html`/`application/json`, лимит размера анализируемого ответа, бюджет производительности.

## Фаза 14. Правила IGR (virtual patching) и hardening (v2.1)

> Спецификация формата — `PRD.ru.md`, раздел 11. Зависимости: реестры ввода и стоков (фаза 8), токенизаторы грамматик (фазы 6, 9–13) для `breaks <grammar>`, output handler (фаза 13) для стадии `response`.
> Парсер, валидатор и вычислитель живут в `core/` и не зависят от Zend: доступ к запросу/аргументам — через абстрактный интерфейс полей, поэтому почти всё тестируется быстрыми C unit-тестами на фейковых запросах.

**14.A Спецификация и инструменты**
- [ ] **14.1 [INFRA]** Перенести раздел 11 PRD в англоязычные `docs/rules.md` (нормативная спецификация `version 1`) и `docs/rules.ebnf`; примеры в `rules/examples/*.igr`. Примеры из спецификации становятся тест-фикстурами (тест «каждый пример парсится и проходит валидацию» — красный до 14.4).
- [ ] **14.2 [TDD]** Корпус файлов правил: `tests/rules/valid/*.igr`, `tests/rules/invalid/*.igr` + ожидаемые сообщения об ошибках (`*.expected`), golden-тест.

**14.B Парсер (core)**
- [ ] **14.3 [TDD]** Лексер: ключевые слова, поля с точками, строки с экранированием (`\"`, `\\`, `\n`, `\t`, `\xHH`), числа, операторы, комментарии, CRLF, позиции `line:col`, лимит 64 КБ на строку, ошибки на невалидный UTF-8. Fuzz-таргет.
- [ ] **14.4 [TDD]** Парсер → AST (рекурсивный спуск по EBNF): заголовок `version`, `rule`, все утверждения, выражения с приоритетом `not` > `and` > `or`, скобки, `any`/`all`, селекторы, цепочки трансформаций, списки. Лимит глубины 32. Fuzz-таргет.
- [ ] **14.5 [TDD]** Восстановление после ошибок до следующего `rule`, сообщения в формате `file:line:col: error: message (hint)` на английском; golden-тесты из 14.2.

**14.C Семантическая проверка и компиляция (core)**
- [ ] **14.6 [TDD]** Валидация: поддерживаемая `version`, уникальность ID между файлами, диапазоны ID (`900000+` запрещены для пользователя), обязательные `match` и `action`, допустимость полей на стадии, вывод стадии и конфликт с явным `stage`, совместимость оператора и типа (`<` только с числом, `in_cidr` — валидные CIDR), `sanitize` только на `request`, `on_block` только на `sink`, `sink`-правило обязано ограничивать `sink.function` через `==`/`in`.
- [ ] **14.7 [TDD]** Абстракция регулярных выражений `ig_regex_t` в `php_compat.h`: PCRE1 (PHP 7.0–7.2) и PCRE2 (7.3+), компиляция при загрузке, ошибки компиляции как ошибки правила, лимит backtracking при выполнении. phpt на обе ветки.
- [ ] **14.8 [TDD]** Компиляция AST в неизменяемую исполняемую форму: предкомпилированные regex, разобранные CIDR, индекс `функция → правила` для sink, разбиение `request`-правил по литеральному префиксу пути (оптимизация). Тест: одинаковый результат с индексом и без.

**14.D Вычислитель (core)**
- [ ] **14.9 [TDD]** Интерфейс доступа к полям (`ig_field_provider_t`) и фейковая реализация для unit-тестов; семантика коллекций `any`/`all`, glob-селекторы, `.keys`, отсутствующие поля, `exists`.
- [ ] **14.10 [TDD]** Операторы сравнения, `~`/`!~`, `in`, `in_cidr` (IPv4/IPv6), строковые операторы; числовое сравнение.
- [ ] **14.11 [TDD]** Трансформации — по тесту на каждую (`lower`, `upper`, `trim`, `url_decode`, `url_decode_all`, `html_decode`, `base64_decode`, `hex_decode`, `normalize_path`, `remove_nulls`, `compress_ws`, `length`, `count`); переиспользуют нормализаторы фаз 1 и 9.
- [ ] **14.12 [TDD]** Проверки: `is tainted [from …]` (реестр фазы 8), `is sqli/xss`, типы и форматы, `breaks <grammar>` (токенизаторы фаз 6, 9–13; отсутствующая грамматика → ошибка загрузки).
- [ ] **14.13 [TDD]** Разрешение действий: самое строгое из сработавших, `allow` подавляет только встроенные детекторы указанных классов и полей и не отменяет `block` правил, `set_mode` меняет режим модулей на запрос (при конфликте — самый строгий), потолок `rules.mode`, `dry_run` → `would_block`.

**14.E Интеграция с PHP**
- [ ] **14.14 [TDD]** Директивы `rules.enabled`, `rules.mode`, `rules_files` (glob, лексикографический порядок), `rules_strict`, `rules_max`; загрузка в MINIT; ошибки → `config_warning` + startup-лог; предупреждение о файлах, доступных на запись не-владельцу; лимит размера файла; секция в `phpinfo()`.
- [ ] **14.15 [TDD]** Реализация `ig_field_provider_t` для Zend: `request.*`, `get/post/cookie/header/json/args/files` (phpt с `--GET--/--POST--/--COOKIE--/--ENV--`).
- [ ] **14.16 [TDD]** Стадия `request`: `allow`/`set_mode` вычисляются **до** встроенных детекторов, остальные правила — после; действия `log`/`block` (+ `status`)/`sanitize [стратегия]`/`set_mode`; поля события `rule_id`, `rule_msg`, `severity`, `tags`, `matched_field`, `matched_value`.
- [ ] **14.17 [TDD]** Стадия `sink`: установка хуков по индексу функций через реестр стоков (только для существующих функций, иначе предупреждение и неактивное правило), `sink.arg[N]` и `sink.arg["name"]` (по arginfo, на 7.0 и 8.4), `sink.args`, `sink.file/line`; `on_block exception|false|terminate`, класс `InletGuard\BlockedException`.
- [ ] **14.18 [TDD]** Перевод ini-исключений (`exclude_params`, `*.exclude_params`, `exclude_paths`) во внутренние `allow`-правила с ID `900000+`; все тесты фазы 4.10 остаются зелёными.
- [ ] **14.19 [TDD]** Стадия `response` (после фазы 13): `response.*`, действия `log`/`block`.
- [ ] **14.20 [TDD]** ZTS: правила загружаются один раз и только читаются из потоков; стресс-тест в ZTS-сборке под TSan/ASan.

**14.F Инструменты для авторов правил**
- [ ] **14.21 [TDD]** PHP API: `InletGuard\rules_lint()`, `InletGuard\rules_eval()` (фейковый запрос/вызов → сработавшие правила и итоговое действие), `InletGuard\rules_loaded()`; stub и arginfo для 7.x и 8.x.
- [ ] **14.22 [INFRA]** CLI `bin/inletguard-rules lint|test` (PHP-скрипт поверх API); формат фикстур для `test`: `*.igr` + `*.cases.json` (запросы, которые должны/не должны срабатывать).
- [ ] **14.22a [TDD]** `inletguard-learn --format=igr`: кандидаты в виде `allow`-правил, ограниченных путём и методом; результат проходит `inletguard-rules lint`.

**14.G Производительность**
- [ ] **14.23 [TDD]** Бенчмарк-тест: 1000 `request`-правил и 1000 `sink`-правил укладываются в бюджет; вызов функции без привязанных правил не замедляется; движок с `rules.enabled=0` — нулевой оверхед.

**14.H Пакеты правил и hardening**
- [ ] **14.24 [TDD]** Политика опасных функций по каталогам — как пакет правил `rules/policy/*.igr` (пример: запрет `system/exec/…` и `eval` из каталогов загрузок), с фикстурами `*.cases.json`. (класс 30)
- [ ] **14.25 [TDD]** Пакеты правил для известных CVE (WordPress-плагины, старые CMS) в `rules/cve/` с ID `100000–899999`; на каждое правило — фикстура атаки и легитимного запроса, прогон через `inletguard-rules test` в CI.
- [ ] **14.26 [TDD]** Сессии: ID сессии из URL/GET, отсутствие `session_regenerate_id` после смены привилегий (эвристика), принудительные `use_strict_mode`/`use_only_cookies` (opt-in). (класс 29)
- [ ] **14.27 [TDD]** Подпись/шифрование cookie (opt-in, ключ в ini, `libsodium` при наличии; совместимость с 7.0 — проверка доступности функций).

## Фаза 15. Поведенческая защита и хранилище счётчиков (v2.2 / v2.3)

**15.A Хранилище shm и Behavior-модули (v2.2)**
- [ ] **15.1 [SPIKE]** Общая память: именованный сегмент (`shm_open`/`mmap`, `/dev/shm/inletguard-<uid>-<scope>`) vs анонимный `mmap` до форка; видимость между пулами FPM, mod_php, CGI, LSAPI и разными версиями PHP; блокировки для ZTS; права доступа на shared-хостинге. → ADR-008.
- [ ] **15.2 [TDD]** Интерфейс `ig_counter_store_t` (`incr`, `get`, `ban`, `is_banned`, TTL) и in-memory реализация для unit-тестов; алгоритм окна «фиксированные корзины + взвешивание предыдущей». (FR-12.1)
- [ ] **15.3 [TDD]** Бэкенд `shm`: `counters.shm_size`, `counters.shm_scope=user|pool|global`, фиксированная хеш-таблица, вытеснение LRU, восстановление после повреждённого сегмента; стресс-тест из нескольких процессов. (FR-12.3)
- [ ] **15.4 [TDD]** IP allow/deny-списки (CIDR, IPv4/IPv6), IP клиента — по `trusted_proxies` (4.15).
- [ ] **15.5 [TDD]** Модуль `ratelimit`: лимиты по путям/методам, ответ 429 с `Retry-After`. (класс 32)
- [ ] **15.6 [TDD]** Модуль `bruteforce`: счётчики по IP и по значению логин-параметра на `bruteforce.paths`. (класс 31)
- [ ] **15.7 [TDD]** Автобан: временная блокировка IP после N событий-атак за окно, TTL; события `ban`/`unban` в логе.
- [ ] **15.8 [TDD]** `learn.param_profiles` (FR-11.6): профили параметров в хранилище счётчиков и детектор аномалий, по умолчанию выключен.

**15.B Внешние хранилища Redis / Memcached (v2.3, FR-12.4–12.6)**
- [ ] **15.9 [TDD]** Минимальный клиент RESP2 в `core/` (сериализация команд, парсер ответов — fuzz-таргет), TCP и unix-сокет, `AUTH` (ACL user/password из `counters.redis_password_file`), `SELECT`, таймауты, постоянное соединение на воркер.
- [ ] **15.10 [TDD]** Бэкенд `redis`: атомарные `incr`+TTL одним pipeline, бан-ключи, `counters.key_prefix`; интеграционные тесты на Redis-сервисе CI (`SKIPIF` без него).
- [ ] **15.11 [TDD]** Минимальный клиент текстового протокола memcached (`add`/`incr`/`touch`/`get`, fuzz парсера) и бэкенд `memcached` для `counters.memcached_servers` (распределение ключей по серверам).
- [ ] **15.12 [TDD]** `counters.mode=hybrid`: локальный счёт в shm и пакетная синхронизация раз в `counters.sync_interval_ms`; тест точности лимита на нескольких «серверах» (несколько shm-сегментов + один Redis).
- [ ] **15.13 [TDD]** Недоступность хранилища: `counters.fail_mode=local|open`, circuit breaker с `counters.retry_after`, одно событие `counters_unavailable`; тесты с остановленным/медленным сервером — запрос не ждёт дольше `counters.timeout_ms`.
- [ ] **15.14 [TDD]** Секреты: пароль только из файла, маскирование в `phpinfo()`/`config()`/логах; предупреждение, если файл с паролем доступен на чтение всем.
- [ ] **15.15 [TDD]** Бюджет производительности для `redis`/`memcached`/`hybrid` в бенчмарке.

---

## Бэклог
- Офлайн-конвертер правил `inletguard-rules import --from snuffleupagus|modsecurity` → IGR (сначала Snuffleupagus, затем подмножество ModSecurity), с отчётом о неподдерживаемых конструкциях.
- IGR `version 2`: правила на пользовательские PHP-функции (observer API на 8.x / `zend_execute_ex` на 7.x), переменные и счётчики между правилами, горячая перезагрузка.
- Подсветка синтаксиса `.igr` для VS Code / JetBrains.
- TLS для Redis/Memcached, Redis Sentinel / Cluster.
- Сжатие ротированных логов.
- Эвристики CSRF (проверка `Origin`/`Referer` для изменяющих запросов).
- Экспорт событий в syslog / SIEM.
