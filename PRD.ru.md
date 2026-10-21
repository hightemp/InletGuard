# PRD: InletGuard (русская версия — первоисточник) — PHP-расширение для защиты от инъекций

> **Метод разработки: TDD (Test-Driven Development).** Любой функциональный код пишется только после падающего теста (Red → Green → Refactor). Подробности — в разделе [9. Стратегия тестирования](#9-стратегия-тестирования-tdd) и в `TASKS.ru.md`.

> **Языковые соглашения.** Проект ведётся **на английском**: исходный код, идентификаторы, комментарии, сообщения в логах и ошибках, PHP-предупреждения, тесты, коммиты, `README.md`, `CHANGELOG.md`, `docs/`, ADR. **На русском — только `PRD.ru.md` и `TASKS.ru.md`** (первоисточник). `PRD.md` и `TASKS.md` — базовые английские версии для внешних участников; при расхождении действует русская версия. **Любое изменение вносится в оба файла пары** (`PRD.ru.md`+`PRD.md`, `TASKS.ru.md`+`TASKS.md`); английская версия может быть короче, но не может отставать.

> **Поддерживаемые версии PHP: 7.0 – 8.4** (NTS и ZTS, Linux). Нижняя граница выбрана под shared-хостинги, где до сих пор массово встречаются 7.x (cPanel/CloudLinux alt-php, ISPmanager); охватываем всю ветку 7.x. PHP 5.x не поддерживается — у него принципиально другой Zend API.

## 1. Обзор

**InletGuard** — нативное расширение PHP на C (подключается как `curl` или `gd`: `extension=inletguard.so`), которое на уровне интерпретатора:

- проверяет входные данные запроса (`$_GET`, `$_POST`, `$_COOKIE`, URI, заголовки) на признаки атак **до выполнения PHP-скрипта**;
- перехватывает вызовы SQL-функций (`mysqli`, `PDO`, `pgsql`, `sqlite3`) и проверяет, не изменил ли пользовательский ввод структуру запроса (RASP);
- даёт PHP-функции для ручной проверки строк;
- реагирует согласно настройкам: **monitor** (только лог), **block** (HTTP 403 и прекращение выполнения), **sanitize** (очистка значения);
- пишет события в лог-файл, путь к которому задаётся в `php.ini`.

Движок детекции — встроенная библиотека **libinjection** (BSD-3-Clause), дополненная собственной нормализацией ввода.

## 2. Цели и не-цели

### Цели (v1.0)
1. Обнаружение **SQL-инъекций** и **XSS** во входных данных с низким уровнем ложных срабатываний.
2. Обнаружение SQL-инъекций в момент выполнения запроса к БД (перехват функций).
3. Настраиваемая реакция: `monitor` / `block` / `sanitize` — глобально и отдельно по типу атаки.
4. Логирование событий в файл (JSON Lines), путь задаётся в ini.
5. Работа без изменения кода приложения — достаточно установить расширение.
6. Стабильность: расширение никогда не роняет процесс PHP; при внутренней ошибке — fail-open с записью в лог.
7. **Работа из коробки:** достаточно одной строки `extension=inletguard.so` — защита сразу активна с безопасными значениями по умолчанию, без обязательных настроек (FR-13).
8. **CI и релизный конвейер с первого дня:** GitHub Actions собирает, тестирует и упаковывает расширение на каждом коммите; публикация релизов включается только начиная с базовой рабочей версии v1.0.0 (раздел 9.2).

### Долгосрочная цель
Последовательно расширить охват на **все классы атак, которые можно обнаружить на уровне PHP-рантайма**: файловые (LFI/RFI, загрузка webshell), командные и кодовые инъекции, десериализация, XXE, SSRF, LDAP/XPath/NoSQL-инъекции, SSTI, CRLF, open redirect, анализ ответа, пользовательские правила (virtual patching), поведенческая защита (rate limit, brute force). Полный каталог и релизы — в разделе [10. Дорожная карта охвата классов атак](#10-дорожная-карта-охвата-классов-атак).

### Не входит в v1.0 (запланировано в следующих релизах, см. раздел 10)
- Все классы атак, кроме SQLi и XSS.
- Проверка JSON/XML-тела запроса.

### Не-цели проекта в целом
- Атаки сетевого и транспортного уровня: HTTP request smuggling, TLS, сетевой DDoS — это зона веб-сервера/балансировщика.
- CSRF, IDOR, broken access control, ошибки бизнес-логики — требуют знания семантики приложения (возможны лишь эвристики в бэклоге).
- Криптографические ошибки приложения, supply chain.
- Windows, PHP 5.x.
- Веб-интерфейс, хранение событий в БД, отправка во внешние системы (SIEM) — только файл.
- Замена WAF/параметризованных запросов. InletGuard — дополнительный слой защиты.

## 3. Целевая аудитория и сценарии

| Роль | Сценарий |
|---|---|
| Владелец сайта / хостинг-клиент | Ставит пакет (или добавляет `extension=inletguard.so`) — и больше ничего не настраивает. |
| DevOps / админ сервера | Ставит расширение на сервер с legacy-приложениями; при желании сначала обкатывает в `monitor` + `learn`, затем включает строгий профиль. |
| Разработчик | Использует `InletGuard\check_sqli()` / `InletGuard\check_xss()` в своём коде, исключает поля с легитимным HTML. |
| Специалист по безопасности | Анализирует JSON-лог атак, настраивает исключения и правила IGR. |

**Основной сценарий — «поставил и работает»:** установка пакета → защита активна с профилем `balanced` (высокоточные проверки блокируют, рискованные по ложным срабатываниям — только логируют), события пишутся в стандартный `error_log`.

**Осторожный сценарий:** установка → `inletguard.mode=monitor` + `inletguard.learn.enabled=1` на 1–2 недели → `inletguard-learn` → применение исключений → снять `mode=monitor` (или `profile=strict`).

## 4. Функциональные требования

### FR-1. Проверка входных данных запроса
- **FR-1.1** Источники: GET, POST (urlencoded и multipart — только текстовые поля, не файлы), COOKIE, путь URI, query string, выбранные заголовки (по умолчанию `User-Agent`, `Referer`, `X-Forwarded-For`). Список источников настраивается.
- **FR-1.2** Проверяются и **значения, и ключи** (включая вложенные массивы `a[b][c]=...`) рекурсивно, с ограничением глубины.
- **FR-1.3** Нормализация перед детекцией: повторный URL-decode (до N раз), удаление NUL-байтов, для XSS — декодирование HTML-сущностей. Исходное значение не изменяется (кроме режима sanitize).
- **FR-1.4** Ограничения: максимальная длина проверяемого значения (`inspect_max_len`), максимальное число проверяемых переменных — чтобы исключить деградацию производительности. Превышение логируется (event `limit_exceeded`).
- **FR-1.5** Проверка выполняется до начала выполнения пользовательского скрипта.
- **FR-1.6** В CLI-SAPI автоматическая проверка запроса по умолчанию выключена (`inletguard.cli_enabled=0`); PHP API работает всегда.

### FR-2. Детекторы
- **FR-2.1 SQLi** — `libinjection_sqli` (fingerprint на основе токенизации). В событие пишется fingerprint.
- **FR-2.2 XSS** — `libinjection_xss`.
- **FR-2.3** Детекторы включаются/выключаются независимо (`inletguard.sqli.enabled`, `inletguard.xss.enabled`).
- **FR-2.4** Внутренний интерфейс детектора единый (`ig_detector_t`), чтобы в v2 добавить новые типы атак без изменения ядра.

### FR-3. Перехват SQL-функций (RASP)
- **FR-3.1** Перехватываются: `mysqli_query`, `mysqli_real_query`, `mysqli_multi_query`, `mysqli_prepare`, методы `mysqli::query/real_query/multi_query/prepare`; `PDO::query`, `PDO::exec`, `PDO::prepare`; `pg_query`, `pg_query_params` (только текст запроса), `pg_send_query`, `pg_prepare`; `SQLite3::query`, `SQLite3::exec`, `SQLite3::querySingle`, `SQLite3::prepare`. Перехват устанавливается только для загруженных расширений.
- **FR-3.2** Алгоритм: для каждого пользовательского значения длиной ≥ `sqli.sink.min_input_len`, которое **дословно входит** в текст SQL-запроса, проверяется, меняет ли оно структуру запроса — значение должно целиком укладываться в один токен-литерал (строку/число). Если значение «выходит» за пределы литерала (порождает операторы, ключевые слова, комментарии) — это инъекция. Дополнительно значение проверяется `libinjection_sqli`.
- **FR-3.3** Реакция: `monitor` — лог и выполнение запроса; `block` — запрос не выполняется, функция завершается с исключением `InletGuard\SqlInjectionException` (для PDO/SQLite3/mysqli в режиме исключений) или возвращает `false` с warning (настраивается `sqli.sink.block_action = exception|false`); `sanitize` для SQL **не поддерживается** (трактуется как `block`, при старте пишется предупреждение).
- **FR-3.4** Переопределение режима: `inletguard.sqli.sink.mode` (по умолчанию наследует `inletguard.mode`).

### FR-4. Режимы реакции
- **FR-4.1** `monitor` — только логирование, выполнение продолжается.
- **FR-4.2** `block` — HTTP-статус `inletguard.block_status` (по умолчанию 403; переопределяется на модуль `<m>.block_status`), выполнение скрипта прекращается. Тело ответа:
  - формат `inletguard.block_format = auto|html|json|text` (`auto` — по заголовку `Accept`: API-клиенты получают JSON);
  - шаблон из `inletguard.block_page` (или `<m>.block_page`) с подстановками `{request_id}`, `{status}`, `{time}`; без шаблона — встроенная минимальная страница;
  - заголовок `X-InletGuard-Request-Id` — только при `inletguard.response_header=1` (по умолчанию выкл., чтобы не раскрывать наличие защиты); `request_id` всегда есть в теле страницы блокировки.
- **FR-4.3** `sanitize` — значение в суперглобале очищается по стратегии модуля; событие логируется с `action: "sanitized"`:
  - `inletguard.xss.sanitize = escape|strip_tags|empty` (по умолчанию `escape` — `htmlspecialchars` с `ENT_QUOTES|ENT_SUBSTITUTE`);
  - `inletguard.sqli.sanitize = empty|strip` (по умолчанию `empty`). `strip` удаляет токены, давшие срабатывание (кавычки и строковые разделители, маркеры комментариев `--`, `#`, `/* */`, `;`, ключевые слова и операторы вне литералов по fingerprint); результат проверяется повторно, и если он всё ещё детектируется — заменяется пустой строкой;
  - новые модули объявляют свои стратегии в той же директиве `<m>.sanitize` (FR-10.3).
- **FR-4.4** Режим задаётся глобально (`inletguard.mode`) и переопределяется по типу (`inletguard.sqli.mode`, `inletguard.xss.mode`, `inletguard.sqli.sink.mode`).
- **FR-4.5** Если на один запрос сработало несколько правил, применяется самая строгая реакция (`block` > `sanitize` > `monitor`).

### FR-5. Исключения (снижение ложных срабатываний)
- **FR-5.1** `inletguard.exclude_params` — список имён параметров (с указанием источника: `post:content,cookie:session_data` или без источника — для всех), не проверяемых совсем.
- **FR-5.2** `inletguard.exclude_paths` — список префиксов URI, для которых проверка входных данных отключена.
- **FR-5.3** `inletguard.xss.exclude_params` / `inletguard.sqli.exclude_params` — исключения для конкретного детектора (например, поле с легитимным HTML).
- **FR-5.4** Исключения поддерживают простые glob-шаблоны (`*`).

### FR-6. Логирование
- **FR-6.1** Путь задаётся `inletguard.log_file`. Если не задан — события пишутся через `php_log_err` (стандартный `error_log`).
- **FR-6.2** Формат — JSON Lines, одна строка на событие. Поля:
  `ts` (ISO-8601, UTC, мс), `request_id`, `event` (`attack`/`limit_exceeded`/`internal_error`/`config_warning`), `attack_type` (`sqli`/`xss`), `stage` (`input`/`sql_hook`/`api`), `source` (`get`/`post`/`cookie`/`header`/`uri`), `param`, `fingerprint`, `payload` (усечённый до `log_payload_max` байт), `action` (`logged`/`blocked`/`sanitized`), `mode`, `ip`, `method`, `uri`, `user_agent`, `sapi`, `pid`, `function` (для sql_hook), `script` (файл и строка вызова для sql_hook).
- **FR-6.3** Корректное JSON-экранирование, невалидный UTF-8 заменяется на `�` — запись в лог никогда не ломает формат и не является вектором log injection.
- **FR-6.4** Атомарная запись: одна строка — один вызов `write()` в файл, открытый с `O_APPEND`; при строке > `PIPE_BUF` — `flock`.
- **FR-6.5** Ошибка открытия/записи лога не прерывает запрос: однократное предупреждение в `error_log` за запрос.
- **FR-6.6** `inletguard.log_level` = `attacks|all` (`all` дополнительно пишет `limit_exceeded`, `config_warning`).
- **FR-6.7** Опция `inletguard.log_payload = 1|0` — для сред, где запрещено хранить пользовательские данные.
- **FR-6.8 Ротация** настраивается `inletguard.log_rotate`:
  - `external` (по умолчанию) — ротацию делает `logrotate`; расширение раз в `log_reopen_check` секунд (по умолчанию 5) сверяет inode/устройство и переоткрывает файл;
  - `size` — ротация при превышении `log_max_size` (например `100M`);
  - `daily` / `hourly` — по времени (UTC);
  - хранится `log_max_files` архивов (`events.log.1` … `.N`). Ротацию выполняет один процесс под `flock` на lock-файле, остальные замечают смену inode и переоткрывают файл; ни одна строка не теряется и не разрывается.
- **FR-6.9 Защита от флуда лога** (атакующий не должен заполнить диск): `log_rate_limit` — максимум событий в секунду на процесс (по умолчанию 100), `log_dedup_window` — одинаковые события (модуль + параметр + fingerprint + IP) в окне агрегируются в одно с полем `count`; при подавлении пишется событие `log_suppressed` с числом пропущенных.
- **FR-6.10 Приватность:** `log_fields_exclude` — список полей, которые не пишутся (например `user_agent,payload`); `log_ip_anonymize = 0|1` — обнуление последнего октета IPv4 / последних 80 бит IPv6.
- **FR-6.11 Формат:** `log_format = json|logfmt` (по умолчанию `json`).
- **FR-6.12** Логирование отключается на уровне модуля: `inletguard.<m>.log = 0` (модуль работает, события не пишутся — например, для шумного `scanner`).

### FR-7. PHP API
```php
namespace InletGuard;

function check_sqli(string $value): ?string;   // fingerprint или null
function check_xss(string $value): bool;
function inspect($value, int $types = SQLI|XSS): array; // string|array; список найденных угроз
function events(): array;      // события текущего запроса
function request_id(): string;
function version(): string;
function config(?string $module = null): array;            // эффективная конфигурация (FR-10.7)
function modules(): array;                                 // собранные/загруженные/включённые модули
function tighten(string $module, string $mode): bool;      // только ужесточение (FR-10.6)
// v2.1: rules_lint(), rules_eval(), rules_loaded() — раздел 11.9

const SQLI = 1; const XSS = 2;
const MODE_MONITOR = 'monitor'; const MODE_BLOCK = 'block'; const MODE_SANITIZE = 'sanitize';

class SqlInjectionException extends \RuntimeException {}
```
Функции API **не применяют** реакцию (не блокируют), только возвращают результат; логирование — по `inletguard.api_log=0|1`.

Сигнатуры ограничены возможностями PHP 7.0: без union-типов в arginfo (`inspect()` принимает `mixed` и бросает `TypeError` на что-то кроме `string|array` единообразно во всех версиях); nullable-типы возврата (`?string`) объявляются в arginfo только с 7.1, на 7.0 тип возврата в arginfo опускается (поведение то же, отличается лишь Reflection). Поведение API идентично на 7.0 и 8.4.

### FR-8. Конфигурация (php.ini)

| Директива | По умолчанию | Уровень | Описание |
|---|---|---|---|
| `inletguard.enabled` | `1` | SYSTEM | Главный выключатель |
| `inletguard.mode` | *(по профилю)* | SYSTEM | `monitor`/`block`/`sanitize`; если задан — перекрывает режимы профиля для всех модулей (удобно `monitor` для обкатки) |
| `inletguard.sqli.enabled` | `1` | SYSTEM | Детектор SQLi |
| `inletguard.sqli.mode` | *(наследует)* | SYSTEM | |
| `inletguard.xss.enabled` | `1` | SYSTEM | Детектор XSS |
| `inletguard.xss.mode` | *(наследует)* | SYSTEM | |
| `inletguard.sqli.sink.enabled` | `1` | SYSTEM | Перехват SQL-функций |
| `inletguard.sqli.sink.mode` | *(наследует)* | SYSTEM | `monitor`/`block` |
| `inletguard.sqli.sink.block_action` | `exception` | SYSTEM | `exception`/`false` |
| `inletguard.sqli.sink.min_input_len` | `3` | SYSTEM | |
| `inletguard.inspect_sources` | `get,post,cookie,uri,headers` | SYSTEM | |
| `inletguard.inspect_headers` | `User-Agent,Referer,X-Forwarded-For` | SYSTEM | |
| `inletguard.inspect_max_len` | `8192` | SYSTEM | Байт на значение |
| `inletguard.inspect_max_vars` | `1000` | SYSTEM | |
| `inletguard.inspect_max_depth` | `16` | SYSTEM | |
| `inletguard.decode_rounds` | `2` | SYSTEM | Повторный URL-decode |
| `inletguard.exclude_params` | *(пусто)* | SYSTEM | |
| `inletguard.exclude_paths` | *(пусто)* | SYSTEM | |
| `inletguard.sqli.exclude_params` | *(пусто)* | SYSTEM | |
| `inletguard.xss.exclude_params` | *(пусто)* | SYSTEM | |
| `inletguard.block_status` | `403` | SYSTEM | |
| `inletguard.block_page` | *(пусто)* | SYSTEM | Путь к HTML-файлу |
| `inletguard.log_file` | *(пусто)* | SYSTEM | Путь к логу |
| `inletguard.log_level` | `attacks` | SYSTEM | |
| `inletguard.log_payload` | `1` | SYSTEM | |
| `inletguard.log_payload_max` | `512` | SYSTEM | |
| `inletguard.cli_enabled` | `0` | SYSTEM | |
| `inletguard.api_log` | `0` | SYSTEM | |
| `inletguard.profile` | `balanced` | SYSTEM (загрузка) | `off`/`minimal`/`balanced`/`strict`/`paranoid` (FR-10.4) |
| `inletguard.modules` | `all` | SYSTEM (загрузка) | Загружаемые модули: `all`, список или исключения `all,-ssti` (FR-10.2) |
| `inletguard.fail_mode` | `open` | SYSTEM | Реакция на внутреннюю ошибку: `open` — пропустить запрос, `closed` — заблокировать |
| `inletguard.config_strict` | `0` | SYSTEM (загрузка) | `1` — при ошибках конфигурации расширение не активируется и пишет фатальное сообщение в startup-лог |
| `inletguard.min_input_len` | `3` | SYSTEM | Мин. длина значения для реестра ввода; переопределяется `<m>.sink.min_input_len` |
| `inletguard.trusted_proxies` | *(пусто)* | SYSTEM | CIDR-список прокси, которым доверяем `X-Forwarded-For`/`X-Real-IP` |
| `inletguard.block_format` | `auto` | SYSTEM | `auto`/`html`/`json`/`text` |
| `inletguard.response_header` | `0` | SYSTEM | Заголовок `X-InletGuard-Request-Id` |
| `inletguard.allow_user_tightening` | `0` | SYSTEM | Разрешить приложению только ужесточать защиту через API (FR-10.6) |
| `inletguard.xss.sanitize` | `escape` | SYSTEM | `escape`/`strip_tags`/`empty` |
| `inletguard.sqli.sanitize` | `empty` | SYSTEM | `empty`/`strip` |
| `inletguard.log_format` | `json` | SYSTEM | `json`/`logfmt` |
| `inletguard.log_rotate` | `external` | SYSTEM | `external`/`size`/`daily`/`hourly` |
| `inletguard.log_reopen_check` | `5` | SYSTEM | Секунды между проверками inode |
| `inletguard.log_max_size` | `100M` | SYSTEM | Для `log_rotate=size` |
| `inletguard.log_max_files` | `10` | SYSTEM | Сколько архивов хранить |
| `inletguard.log_rate_limit` | `100` | SYSTEM | Событий/сек на процесс |
| `inletguard.log_dedup_window` | `60` | SYSTEM | Секунды агрегации одинаковых событий (`0` — выкл.) |
| `inletguard.log_fields_exclude` | *(пусто)* | SYSTEM | |
| `inletguard.log_ip_anonymize` | `0` | SYSTEM | |

Плюс единая схема директив каждого модуля `inletguard.<m>.*` (FR-10.3), директивы обучения `inletguard.learn.*` (FR-11), счётчиков `inletguard.counters.*` (FR-12) и правил `inletguard.rules*` (раздел 11.8).

- Все директивы — `PHP_INI_SYSTEM`: приложение не может отключить защиту через `ini_set()`, `.user.ini` или `php_value` в `.htaccess`. Задать их можно в `php.ini`/`conf.d`, в конфиге пула FPM (`php_admin_value`) и в `<VirtualHost>` Apache (`php_admin_value`) — см. FR-10.5.
- Директивы с пометкой «загрузка» читаются один раз при старте процесса PHP и действуют только из глобального `php.ini`/`conf.d`.
- Невалидные значения: при MINIT пишется `config_warning`, используется значение по умолчанию. Расширение не отказывается загружаться (кроме `config_strict=1`).
- `phpinfo()` показывает версию, версию libinjection и эффективную конфигурацию (FR-10.7).

### FR-10. Модульность и модель конфигурации

- **FR-10.1 Модуль = класс атаки.** Каждый класс из каталога (раздел 10.2) — отдельный модуль со стабильным ID: `sqli`, `xss`, `hpp`, `scanner`, `lfi`, `rfi`, `upload`, `webshell`, `zipslip`, `cmdi`, `argi`, `codei`, `deser`, `xxe`, `ssrf`, `ldapi`, `xpathi`, `nosqli`, `ssti`, `redos`, `crlf`, `redirect`, `varoverwrite`, `typejuggle`, `out_xss`, `leak`, `secheaders`, `session`, `cookie`, `rules`, `ratelimit`, `bruteforce`, `learn`. Модуль регистрируется в реестре модулей: ID, описание, свои директивы, детекторы, стоки, стратегии sanitize, значения по умолчанию для каждого профиля. Ядро ничего не знает о конкретных модулях; новый модуль добавляется без изменения ядра.
- **FR-10.2 Четыре уровня включения:**

  | Уровень | Механизм | Когда | Эффект выключения |
  |---|---|---|---|
  | Сборка | `./configure --enable-inletguard --with-inletguard-modules=all\|sqli,xss,…` или `--disable-inletguard-<m>` | Компиляция | Кода модуля нет в `.so` — меньше размер и поверхность атаки |
  | Загрузка | `inletguard.modules = all` / `sqli,xss,lfi` / `all,-ssti,-leak` | Старт процесса (MINIT) | Хуки не устанавливаются, нулевые накладные расходы |
  | Выполнение | `inletguard.<m>.enabled`, `.input.enabled`, `.sink.enabled`, `.output.enabled` | Каждый запрос (RINIT) | Модуль пропускает работу; можно задать для пула FPM / виртуального хоста |
  | Запрос | Правила IGR: `allow`, `set_mode` (раздел 11) | По условиям запроса | Отключение или смена режима модуля для пути, хоста, IP, метода |

  Модуль, не собранный в бинарник или не загруженный, но упомянутый в директивах, вызывает `config_warning` («module 'ssrf' is not loaded, directive ignored»).
- **FR-10.3 Единая схема директив модуля** (модуль объявляет, какие ключи поддерживает; неподдерживаемый ключ → `config_warning`):

  | Директива | Значение | Описание |
  |---|---|---|
  | `inletguard.<m>.enabled` | `0`/`1` (по профилю) | Включение модуля |
  | `inletguard.<m>.mode` | `monitor`/`block`/`sanitize` (наследует `inletguard.mode`) | Режим реакции |
  | `inletguard.<m>.input.enabled` / `.sink.enabled` / `.output.enabled` | `0`/`1` | Включение отдельных уровней обнаружения модуля |
  | `inletguard.<m>.input.mode` / `.sink.mode` | наследует `<m>.mode` | Режим для уровня |
  | `inletguard.<m>.input.<source>.mode` | наследует `<m>.input.mode` | Режим для источника (`get`, `post`, `cookie`, `header`, `uri`, `json`): например, XSS в GET — `block`, в POST — `monitor` |
  | `inletguard.<m>.level` | `1`–`4` (по профилю) | Чувствительность: 1 — только надёжные сигнатуры, минимум ложных срабатываний; 4 — максимум детекции (глубже декодирование, больше источников, агрессивные эвристики) |
  | `inletguard.<m>.exclude_params` / `.exclude_paths` | списки, glob | Исключения модуля (FR-5) |
  | `inletguard.<m>.sanitize` | стратегия модуля | См. FR-4.3 |
  | `inletguard.<m>.log` | `0`/`1` | Писать ли события модуля |
  | `inletguard.<m>.block_status` / `.block_page` | | Переопределение ответа блокировки |
  | `inletguard.<m>.<специфичная>` | | Например `ssrf.allow_hosts`, `lfi.base_dirs`, `upload.noexec_paths`, `bruteforce.paths` |

- **FR-10.4 Профили** задают значения по умолчанию для всех модулей одной директивой `inletguard.profile`; любая явная директива перекрывает профиль.

  | Профиль | Для кого | Состав |
  |---|---|---|
  | `off` | Только PHP API | Все модули выключены |
  | `minimal` | Shared-хостинг, максимальная совместимость | Только sink-проверки высокой точности (`sqli`, `lfi`, `cmdi`, `codei`, `deser`, `upload`, `webshell`), `level 1` |
  | `balanced` *(по умолчанию)* | Большинство сайтов, работа из коробки | Sink-проверки (структурный анализ в опасных функциях) — `block`; input-проверки высокой точности (`sqli`) — `block`; `xss` — `block` для GET/COOKIE/URI/заголовков и `monitor` для POST/JSON (формы с легитимным HTML); шумные (`ssti`, `typejuggle`, `scanner`) — `monitor`; Output и Behavior выключены; `level 2` |
  | `strict` | Сайты с понятным трафиком после обучения | Все модули кроме модифицирующих ответ; шумные — `monitor`; `level 3` |
  | `paranoid` | Админки, внутренние системы | Все модули, `level 4`, режим `block` |

  Итоговая матрица «профиль × модуль» ведётся в `docs/profiles.md` и проверяется тестом.
- **FR-10.5 Приоритет источников** (от низшего к высшему):
  1. встроенные значения по умолчанию;
  2. профиль;
  3. глобальные директивы (`inletguard.mode`, `inletguard.exclude_params`, …);
  4. директивы модуля (`inletguard.<m>.*`);
  5. значения пула FPM / виртуального хоста Apache / LSAPI (`php_admin_value`) — только для директив уровня «выполнение»; директивы уровня «загрузка» там игнорируются с предупреждением;
  6. правила IGR уровня запроса (`allow`, `set_mode`).

  `ini_set()`, `.user.ini`, `php_value` в `.htaccess` защиту изменить не могут.
- **FR-10.6 Ужесточение из приложения** (opt-in, `allow_user_tightening=1`): `InletGuard\tighten(string $module, string $mode): bool` — приложение может только *ужесточить* защиту в текущем запросе (`monitor` → `block`, включить выключенный модуль); попытка ослабить → `false` + warning. Пример: включить `paranoid`-поведение на странице оплаты.
- **FR-10.7 Валидация и интроспекция:**
  - неизвестные ключи `inletguard.*` (опечатки) → `config_warning` с подсказкой «did you mean 'inletguard.xss.mode'?»;
  - `phpinfo()` / `php --ri inletguard` — эффективное значение каждой директивы и его источник (`default`/`profile`/`ini`/`pool`); секреты (пароли Redis и т.п.) маскируются;
  - `InletGuard\config(?string $module = null): array` — эффективная конфигурация (только чтение);
  - CLI `bin/inletguard-config check [--pool=…]` — проверка ini и правил, вывод эффективной конфигурации и отличий от профиля;
  - примеры конфигураций с комментариями: `inletguard.ini-recommended`, `inletguard.ini-hosting`, `inletguard.ini-paranoid`.
- **FR-10.8 Стабильность конфигурации:** имена директив стабильны в пределах мажорной версии; переименованная директива работает как alias с deprecation-предупреждением ещё одну мажорную версию.

### FR-13. Работа из коробки (zero-config)

- **FR-13.1** Единственная обязательная настройка — `extension=inletguard.so`. Пакеты/архивы релиза содержат готовый `conf.d/20-inletguard.ini` из одной этой строки (остальное — закомментированные подсказки).
- **FR-13.2** Без конфигурации действует профиль `balanced` (FR-10.4): защита активна сразу, режимы подобраны так, чтобы блокировать только с высокой точностью.
- **FR-13.3** Без `log_file` события пишутся в стандартный `error_log` PHP (всегда доступен и уже ротируется хостингом/дистрибутивом). При заданном, но недоступном `log_file` — откат на `error_log` (FR-6.5).
- **FR-13.4** Каталог правил по умолчанию `/etc/inletguard/rules.d/*.igr`; отсутствие каталога — не ошибка.
- **FR-13.5** Значения по умолчанию не требуют знаний о приложении: нет обязательных путей (`base_dirs` по умолчанию — document root), CLI не проверяется, Behavior-модули и анализ ответа выключены (им нужны данные о приложении).
- **FR-13.6** При старте без конфигурации расширение один раз пишет в startup-лог `config_warning` уровня `info`: «running with built-in defaults (profile=balanced), see docs/quickstart.md» — отключается любой явной настройкой `inletguard.profile`.
- **FR-13.7** Сценарий «из коробки» покрыт автотестом: чистый контейнер + `.so` + однострочный ini → SQLi в GET и через `PDO::query` блокируется, XSS в GET блокируется, XSS в POST логируется, легитимный трафик WordPress проходит.

### FR-11. Режим обучения (модуль `learn`)

Помогает перейти от `monitor` к `block`: собирает данные о срабатываниях и предлагает кандидатов в исключения. Ничего не применяет автоматически.

- **FR-11.1** Включение: `inletguard.learn.enabled = 0|1` (по умолчанию `0`). Пока включено и `learn.force_monitor=1` (по умолчанию), все `block`/`sanitize` понижаются до `log` (`action: "would_block"`), чтобы обучение не ломало сайт.
- **FR-11.2** К событиям добавляются поля: `path_template` (нормализованный путь: числа → `{id}`, UUID → `{uuid}`, hex-хеши → `{hash}`, плюс свои шаблоны `learn.path_templates`), `status` (HTTP-код ответа; событие дописывается в RSHUTDOWN), `request_attacks` (число срабатываний в запросе), `trusted` (IP в `learn.trusted_ips`).
- **FR-11.3** CLI `bin/inletguard-learn` (PHP) анализирует лог: группирует события по `(path_template, источник, параметр, модуль)`, оценивает каждую группу по числу разных IP и дней, кодам ответа, наличию сканеров и других атак с тех же IP, однородности значений; выдаёт кандидатов с уровнем уверенности `high`/`medium`/`low` и доказательствами в комментариях.
- **FR-11.4** Настройки анализа: `--min-ips` (по умолчанию 20), `--min-days` (3), `--min-hits`, `--since/--until`, `--trusted-only`, `--exclude-scanners` (по умолчанию вкл.), `--format=ini|igr` (`igr` — после фазы 14; точнее, т.к. ограничивает исключение путём). Те же пороги можно задать в `learn.*` директивах как значения по умолчанию для CLI.
- **FR-11.5 Защита от отравления обучения:** исключения только предлагаются (никогда не применяются сами); IP с другими атаками и сканеры отбрасываются; пороги по разным IP и дням; режим «учиться только на доверенных IP»; кандидат всегда точечный (модуль + параметр + путь), а не «выключить проверку».
- **FR-11.6** *(v2.2+)* `learn.param_profiles = 1` — накопление в общей памяти профиля каждого параметра (тип, длина, алфавит) и последующий детектор аномалий «значение не похоже на обычное для этого параметра».

### FR-12. Хранилище счётчиков (Behavior-уровень)

- **FR-12.1** Все Behavior-модули (`ratelimit`, `bruteforce`, автобан, `learn.param_profiles`) работают через интерфейс `ig_counter_store_t` (`incr`, `get`, `ban`, `is_banned`, TTL). Алгоритм окна — фиксированные корзины с взвешиванием предыдущей (приближение sliding window), одинаково реализуемый на всех бэкендах.
- **FR-12.2** `inletguard.counters.backend = shm|redis|memcached` (по умолчанию `shm`).
- **FR-12.3 shm** (v2.2): именованный сегмент общей памяти (`/dev/shm/inletguard-<uid>-<scope>`), `counters.shm_size` (по умолчанию `16M`), `counters.shm_scope = user|pool|global` (по умолчанию `user` — на shared-хостинге клиенты не видят и не портят счётчики друг друга); фиксированная хеш-таблица с вытеснением LRU; работает для FPM, mod_php, CGI, LSAPI и разных версий PHP на одном сервере.
- **FR-12.4 redis / memcached** (v2.3): `counters.redis_dsn` (`tcp://host:6379/0`, `unix:///run/redis.sock`), `counters.redis_user`, `counters.redis_password_file` (пароль из файла, а не в ini — ini виден в `phpinfo()`), `counters.memcached_servers`, `counters.key_prefix` (`ig:`), `counters.timeout_ms` (по умолчанию 20), `counters.persistent = 1` (постоянное соединение на воркер). Собственные минимальные клиенты (RESP2, текстовый протокол memcached) — без внешних библиотек.
- **FR-12.5** `counters.mode = direct|hybrid`: `hybrid` — счёт локально в shm и пакетная синхронизация с внешним хранилищем раз в `counters.sync_interval_ms` (по умолчанию 200): без сетевой задержки в запросе, ценой приблизительных лимитов.
- **FR-12.6 Недоступность хранилища:** `counters.fail_mode = local|open` (по умолчанию `local` — переход на shm; `open` — Behavior-проверки пропускаются); circuit breaker с повторной попыткой через `counters.retry_after` секунд; одно событие `counters_unavailable` на период недоступности.

## 5. Нефункциональные требования

| ID | Требование |
|---|---|
| NFR-1 Производительность | Накладные расходы на типичный запрос (20 параметров по ~50 байт) — < 50 мкс; перехват SQL — < 10 мкс на запрос к БД без совпадений. Бенчмарк в CI. |
| NFR-2 Надёжность | Нет падений и утечек памяти: все тесты проходят под ASan/UBSan и Valgrind (`USE_ZEND_ALLOC=0`). Fuzzing нормализатора и детектора ≥ 1 ч без находок перед релизом. |
| NFR-3 Fail-open | Любая внутренняя ошибка → событие `internal_error` в лог, запрос продолжается. |
| NFR-4 Совместимость | PHP 7.0, 7.1, 7.2, 7.3, 7.4, 8.0, 8.1, 8.2, 8.3, 8.4; NTS и ZTS; SAPI: FPM, CGI, Apache mod_php, CLI, LiteSpeed (LSAPI — частый на хостингах). Совместимость с ext/filter (цепочка input_filter). Linux x86_64 и aarch64. Различия Zend API изолированы в `src/zend/php_compat.h`; в остальном коде нет `#if PHP_VERSION_ID`. |
| NFR-5 Точность | На корпусе атак: детекция SQLi ≥ 95 %, XSS ≥ 90 %. На корпусе легитимных данных: ложные срабатывания < 0.5 %. |
| NFR-6 Безопасность кода | `-Wall -Wextra -Werror`, cppcheck/clang-tidy без ошибок, нет `strcpy/sprintf` без ограничения длины. |
| NFR-7 Сборка | Стандартно: `phpize && ./configure --enable-inletguard && make && make test`. Код на C99 — собирается старыми тулчейнами хостингов (GCC ≥ 4.8, CentOS 7 / CloudLinux 7). libinjection вендорится в `third_party/`. Установка: PECL (`package.xml`, все версии) и PIE (`composer.json` с `type: php-ext`, для 8.x). Готовые `.so` под популярные версии — в релизах. |
| NFR-9 Лицензия | Проект — **MIT**. Сторонний код — только MIT/BSD/Apache-2.0/zlib (libinjection — BSD-3-Clause); GPL/LGPL-код не вендорится и не линкуется. Файлы `LICENSE` и `THIRD_PARTY_NOTICES` (входит в архивы релизов с `.so`), SPDX-заголовок `SPDX-License-Identifier: MIT` в каждом исходнике. |
| NFR-11 Переносимые бинарники | Готовые `.so` собираются на старой glibc (2.17, уровень CentOS 7 / CloudLinux 7) и загружаются на всех поддерживаемых дистрибутивах; проверяется smoke-тестами в CI. |
| NFR-10 Модульность | Выключенный модуль (на любом уровне FR-10.2) не влияет на поведение и производительность; это проверяется тестами для каждого модуля. |
| NFR-8 Язык | Код, комментарии, логи, сообщения, тесты, README и прочая документация — на английском; на русском только `PRD.ru.md`, `TASKS.ru.md` (английские `PRD.md`, `TASKS.md` — базовые версии). |

## 6. Архитектура (верхнеуровнево)

```
                ┌─────────────────────────── inletguard.so ───────────────────────────┐
 HTTP запрос ─▶ │ input_filter hook ─┐                                               │
                │ (цепочка с ext/filter)│                                             │
                │                    ▼                                               │
                │              Inspector ──▶ Normalizer ──▶ Detectors (libinjection) │
                │                    │                        sqli / xss             │
                │                    ▼                                               │
                │              Policy (mode, exclusions) ──▶ Action: log/block/sanit.│
                │                    │                                               │
 SQL вызов  ─▶  │ SQL hooks ─────────┘ (структурная проверка запроса)                 │
 PHP API    ─▶  │ inletguard_* функции                                                │
                │                    ▼                                               │
                │                Logger (JSON Lines ─▶ inletguard.log_file)          │
                └────────────────────────────────────────────────────────────────────┘
```

Слои и правила зависимостей:
- **core/** — чистый C без зависимостей от Zend (нормализатор, обёртка детекторов, JSON-сериализатор, glob-матчер, SQL-токенизатор для структурной проверки). Тестируется unit-тестами на C — это основа TDD-цикла (быстро, без PHP).
- **zend/** — интеграция: ini, хуки, суперглобали, перехват функций, API. Тестируется `.phpt`.
- Состояние запроса (события, request_id, вычисленный уровень реакции) — в `ZEND_BEGIN_MODULE_GLOBALS` (ZTS-safe).

Ключевые технические решения (подтверждаются спайками в фазе 0):
1. **Точка проверки входа** — `sapi_register_input_filter` с сохранением и вызовом предыдущего фильтра (как в ext/filter/Snuffleupagus); итоговое решение о блокировке — в `RINIT`, после сбора всех событий.
2. **Блокировка** — отправка заголовков/тела и предотвращение выполнения основного скрипта (через `zend_compile_file`/`zend_execute_ex` hook или `zend_bailout` — выбирается по результатам спайка).
3. **Перехват функций** — подмена `zend_internal_function.handler` в `function_table`/`class_table` в MINIT (после загрузки целевых расширений, `ZEND_MOD_OPTIONAL` зависимости для порядка загрузки).
4. **Обобщённая модель «источник → сток»** (закладывается в v1.0 для SQL, обобщается в v1.1):
   - *Реестр пользовательского ввода* (taint-lite) — все значения запроса с указанием источника; быстрый поиск вхождений в строку (Aho–Corasick).
   - *Реестр стоков* — декларативная таблица: функция/метод, индекс аргумента, грамматика (SQL, shell, путь, URL, LDAP, XPath, HTML, PHP-код), класс атаки. Один диспетчер хуков на все стоки.
   - *Грамматики* — токенизаторы в `core/`; общий вопрос для любого стока: «выходит ли пользовательский ввод за пределы одного токена-литерала?».
   - Новый класс атак = запись в реестре стоков + токенизатор + сигнатуры для input-уровня, без изменения ядра.

## 7. Метрики успеха
- Корпус-тесты: показатели NFR-5 выполнены.
- 0 известных крашей/утечек на момент релиза.
- Внедрено на тестовом стенде (WordPress + legacy-приложение) в режиме `monitor` неделю без ложных блокировок после настройки исключений.

## 8. Риски

| Риск | Митигация |
|---|---|
| Ложные срабатывания XSS на легитимном HTML (WYSIWYG) | Режим monitor по умолчанию, `xss.exclude_params`, корпус легитимных данных в тестах |
| Обход через кодировки | Нормализатор + fuzz/корпус обходов |
| Конфликт с другими расширениями (Xdebug, APM, Snuffleupagus), хукающими те же функции | Сохранять и вызывать оригинальный handler; интеграционные тесты с Xdebug |
| Различия внутреннего API между 7.0 … 8.4 (arginfo, `zend_string`/`zend_object` handlers, `ZEND_ACC_*`, `zend_parse_parameters`, сигнатуры хуков, `gen_stub.php` есть только с 8.0) | Матрица CI на все 10 версий × NTS/ZTS, единый слой `php_compat.h`, arginfo для 7.x пишется вручную/генерируется в legacy-режиме |
| Старые версии PHP (7.0–8.0) больше не обновляются официально, Docker-образы и тулчейны устаревают | Закреплённые образы в CI, собственный кэш сборок PHP; поддержка 7.x пересматривается раз в год |
| Краш в C-коде = падение сайта | ASan/Valgrind/fuzzing в CI, fail-open |
| Sanitize ломает бизнес-логику | Явно документировано; не дефолтный режим |
| Рост накладных расходов с числом стоков (сотни хуков) | Хук ставится только для включённых классов; быстрый путь «в запросе нет пользовательского ввода» без токенизации; бюджеты производительности на каждый класс в CI |
| Ложные срабатывания на «шумных» классах (SSTI, type juggling, анализ ответа) | Такие классы по умолчанию в `monitor` или выключены; метрики FP на корпусе для каждого класса |
| Различия поведения стоков между версиями PHP (`create_function`, строковый `assert`, `libxml_disable_entity_loader`, `parse_str` без 2-го аргумента) | Хуки регистрируются только если функция существует; phpt с `SKIPIF` по версии |
| Обход SSRF-защиты (DNS rebinding, различия парсеров URL `parse_url` vs curl, нестандартная запись IP) | Проверка после резолва, единый URL/IP-нормализатор, корпус обходов, отдельный спайк |

## 9. Стратегия тестирования (TDD)

**Правило:** ни одной строки продуктового кода без предварительно написанного падающего теста. Цикл на каждую задачу:
1. **Red** — пишем тест (C unit или `.phpt`), убеждаемся, что он падает по ожидаемой причине.
2. **Green** — минимальная реализация, чтобы тест прошёл.
3. **Refactor** — улучшаем код при зелёных тестах; прогон под ASan.

Уровни тестов:
| Уровень | Инструмент | Что покрывает |
|---|---|---|
| Unit (C) | минимальный встроенный тест-раннер (`tests/unit/`, без внешних зависимостей) | core/: нормализатор, детекторы, JSON, glob, SQL-токенизатор |
| Integration | `.phpt` через `make test` (`run-tests.php`, `php-cgi` для секций `--GET--/--POST--/--COOKIE--`) | ini, хуки входа, режимы, лог, API, SQL hooks (SQLite3 — всегда; MySQL/PostgreSQL — в CI через сервисы) |
| Корпус | `tests/corpus/{attacks,benign}/*.txt` + C-раннер | точность (NFR-5), регрессии ложных срабатываний |
| Memory | ASan/UBSan сборка, Valgrind (`make test TESTS=... -m`) | утечки, UB |
| Fuzz | libFuzzer таргеты на core/ | краши на произвольном вводе |
| Perf | бенчмарк-скрипт + `hyperfine`/внутренний таймер | NFR-1 |

Definition of Done для любой задачи: тесты написаны **до** кода, все тесты зелёные на матрице 7.0–8.4 × NTS/ZTS, ASan чистый, нет предупреждений компилятора. `.phpt`-тесты одни и те же для всех версий; расхождения в выводе между версиями допускаются только через `--EXPECTF--`, а не через разные тесты.

Спайки (исследовательские прототипы в фазе 0) выполняются без TDD, их код выбрасывается; выводы фиксируются в `docs/adr/`.

### 9.1 CI (GitHub Actions)

Готовится в фазе 0, **до первой строки продуктового кода** — каждый TDD-цикл сразу проверяется на всей матрице.

| Workflow | Триггер | Что делает |
|---|---|---|
| `ci.yml` | push, pull request | Lint (`clang-format --dry-run`, `cppcheck`, `clang-tidy`), проверка лицензий/SPDX, C unit-тесты, `.phpt` на матрице 7.0–8.4 × NTS/ZTS, ASan/UBSan, Valgrind, сборка старым тулчейном (GCC 4.8, glibc 2.17), aarch64 (ARM-раннеры), сборка с минимальным набором модулей; сервисы БД/Redis/Memcached — по мере появления фаз |
| `nightly.yml` | cron, ручной запуск | Fuzzing (ограниченное время на таргет), метрики корпусов, бенчмарки с порогами, полный интеграционный стенд |
| `release.yml` | тег `v*`, ручной запуск (dry-run), push в `main` (dry-run) | Сборка релизных артефактов, упаковка, smoke-тесты, публикация — по политике 9.2 |

- Все сторонние actions закреплены по SHA, обновления — Dependabot.
- Обязательные проверки для merge в `main`: `ci.yml` целиком.
- Docker-образы PHP закреплены по digest (NFR-4, риск устаревания старых версий).

### 9.2 Релизный конвейер и политика публикации

- **Артефакты** (собираются уже с фазы 0, на пустом расширении):
  - `.so` для каждой версии PHP × NTS/ZTS × x86_64/aarch64 (40 сборок), собранные в контейнере с glibc 2.17 (NFR-11);
  - архив на каждую сборку: `inletguard-<ver>-php<X.Y>-<nts|zts>-<arch>-linux-gnu.tar.gz` с `.so`, `20-inletguard.ini` (FR-13.1), `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES`;
  - PECL-пакет `inletguard-<ver>.tgz` (`package.xml`), `composer.json` для PIE;
  - `SHA256SUMS` и attestations сборки (`actions/attest-build-provenance`).
- **Smoke-тесты артефактов:** каждый `.so` загружается (`php -d extension=… -m`) в чистых контейнерах CentOS 7 / AlmaLinux 8–9 / Debian / Ubuntu соответствующих версий PHP; прогоняется сценарий FR-13.7.
- **Проверки перед публикацией:** версия в теге = версия в `php_inletguard.h` = `package.xml` = раздел в `CHANGELOG.md`; `ci.yml` зелёный на коммите тега.
- **Политика: до базовой рабочей версии релизы не публикуются.**
  - Базовая рабочая версия — **v1.0.0** (завершены фазы 0–7: SQLi + XSS на входе, перехват SQL-функций, логирование, конфигурация, работа из коробки).
  - До неё `release.yml` работает только в режиме **dry-run**: собирает и проверяет все артефакты, сохраняет их как artifacts workflow (короткий срок хранения), но не создаёт GitHub Release, не публикует в PECL/Packagist и не ставит теги.
  - Публикация защищена двумя замками: переменная репозитория `RELEASES_ENABLED=true` (включается в задаче 7.8) **и** тег `≥ v1.0.0`; теги `v0.*` отклоняются workflow.
  - Релиз создаётся как **draft** в environment `release` с обязательным подтверждением мейнтейнера; после подтверждения — публикация GitHub Release, затем PECL/Packagist.
  - Последующие релизы (v1.1, v1.2, …) идут тем же конвейером; версия — SemVer.

## 10. Дорожная карта охвата классов атак

### 10.1 Уровни обнаружения
| Уровень | Где | Сильные стороны |
|---|---|---|
| **Input** | Анализ входных данных при старте запроса (сигнатуры, токенизация) | Ловит атаку до выполнения кода, не зависит от кода приложения |
| **Sink (RASP)** | Перехват опасной функции; проверка, что аргумент содержит пользовательский ввод и ввод ломает грамматику аргумента | Высокая точность, мало ложных срабатываний, ловит обходы input-фильтров |
| **Output** | Output handler: анализ ответа | Reflected XSS, утечки информации |
| **Behavior** | Счётчики по запросам в общей памяти между воркерами | Brute force, rate limit, сканеры |

### 10.2 Каталог классов атак

| # | Класс атаки | CWE | Уровень | Стоки / метод | Релиз |
|---|---|---|---|---|---|
| 1 | SQL injection | 89 | Input + Sink | libinjection; `mysqli`, `PDO`, `pgsql`, `sqlite3` | **v1.0** |
| 2 | XSS (на входе) | 79 | Input | libinjection_xss | **v1.0** |
| 3 | HTTP Parameter Pollution, аномалии ввода (массив вместо строки, NUL, overlong UTF-8, невалидная кодировка) | 235, 20 | Input | Анализ структуры запроса | v1.1 |
| 4 | Сканеры и разведка (UA sqlmap/nikto/nuclei/acunetix, пробинг `/.env`, `/.git/`, бэкапов конфигов) | 200 | Input | Файл сигнатур | v1.1 |
| 5 | Application DoS через ввод (глубина JSON, число элементов, большие тела) | 400 | Input | Лимиты для JSON/XML | v1.1 |
| 6 | Path traversal / LFI | 22, 98 | Input + Sink | `include/require` (`zend_compile_file`, `zend_resolve_path`), `fopen`, `file_get_contents`, `file`, `readfile`, `unlink`, `copy`, `rename`, `parse_ini_file`, `highlight_file`, `SplFileObject`…; нормализация пути, выход за `base_dirs` | v1.2 |
| 7 | RFI и опасные обёртки потоков | 98 | Input + Sink | include/fopen с `http://`, `ftp://`, `data://`, `php://input`, `php://filter`, `expect://`, `zip://`, `phar://` | v1.2 |
| 8 | Загрузка вредоносных файлов | 434 | Input + Sink | `$_FILES`: `.php/.phtml/.phar/.pht/.php5`, двойные расширения, `.htaccess`, `.user.ini`, PHP-код в содержимом (polyglot); `move_uploaded_file` | v1.2 |
| 9 | Webshell: запись и запуск | 94 | Sink | Запись `.php`-файлов с пользовательскими данными (`file_put_contents`, `fwrite`, `copy`, `rename`); `noexec_paths` — запрет исполнения PHP из каталогов загрузок; сигнатуры известных webshell при компиляции | v1.2 |
| 10 | Zip Slip | 22 | Sink | `ZipArchive::extractTo`, `PharData::extractTo` — записи архива с `../` и абсолютными путями | v1.2 |
| 11 | OS command injection | 78 | Input + Sink | `exec`, `system`, `passthru`, `shell_exec`/backticks, `popen`, `proc_open`, `pcntl_exec`; shell-токенизатор | v1.3 |
| 12 | Argument / environment injection | 88 | Sink | `mail()` 5-й параметр (`-X`, `-O` sendmail), `putenv('LD_PRELOAD=…')`, `imap_open` (`-oProxyCommand`) | v1.3 |
| 13 | Code injection | 94, 95 | Input + Sink | `eval` (`zend_compile_string`), строковый `assert` (7.x), `create_function` (≤7.4), динамические вызовы `call_user_func*`, `array_map`, `usort`, `$f()` с пользовательским именем опасной функции | v1.3 |
| 14 | Insecure deserialization / PHP Object Injection | 502 | Input + Sink | `unserialize` с данными запроса (объекты `O:`/`C:`), политика `allowed_classes`; phar-десериализация через файловые функции | v1.4 |
| 15 | XXE и XML bomb | 611, 776 | Input + Sink | `DOMDocument::load*`, `simplexml_load_*`, `XMLReader`, `xml_parse`; DOCTYPE с внешними сущностями, `LIBXML_NOENT/DTDLOAD`; на 7.x — принудительный `libxml_disable_entity_loader`; экспоненциальное раскрытие сущностей | v1.4 |
| 16 | SSRF | 918 | Sink | `curl_exec`/`curl_multi_exec`, `file_get_contents`/`fopen` по URL, `fsockopen`, `stream_socket_client`, `get_headers`, `SoapClient`, `getimagesize`; запрет приватных/loopback/link-local сетей, cloud metadata (`169.254.169.254`, `fd00:ec2::254`), схем `gopher/dict/file`; проверка после DNS-резолва и на редиректах | v1.4 |
| 17 | LDAP injection | 90 | Sink | `ldap_search/list/read` (фильтр), `ldap_bind` (DN); токенизатор LDAP-фильтров | v1.5 |
| 18 | XPath injection | 643 | Sink | `DOMXPath::query/evaluate`, `SimpleXMLElement::xpath` | v1.5 |
| 19 | NoSQL injection | 943 | Input + Sink | Ключи-операторы `$ne/$gt/$where/$regex` во вводе (`user[$ne]=`); `MongoDB\Driver\Query/BulkWrite/Command` | v1.5 |
| 20 | SSTI | 1336 | Input | Эвристики `{{…}}`, `{%…%}`, `${…}`, `{php}`, Twig/Smarty/Blade-гаджеты; по умолчанию `monitor` | v1.5 |
| 21 | ReDoS / regex injection | 1333, 625 | Sink | `preg_*` с пользовательским вводом в шаблоне (в т.ч. модификаторы); локальный `pcre.backtrack_limit` | v1.5 |
| 22 | CRLF / header / email header injection | 93, 113 | Sink | `header()`, `setcookie()`, `mail()` (to, subject, additional_headers), `mb_send_mail` | v1.5 |
| 23 | Open redirect | 601 | Sink | `header('Location: …')` на внешний хост из пользовательского ввода; allowlist хостов | v1.5 |
| 24 | Перезапись переменных | 473, 621 | Sink | `extract()` суперглобалей, `parse_str()` без 2-го аргумента (7.x) | v1.5 |
| 25 | Type juggling / magic hashes | 843 | Input | `0e\d+` и массивы в полях аутентификации; по умолчанию `monitor` | v1.5 |
| 26 | Reflected XSS на выходе | 79 | Output | Пользовательский ввод отражён в HTML без экранирования в опасном контексте (тег, атрибут, `<script>`, URL) | v2.0 |
| 27 | Утечка информации | 209, 200 | Output | SQL-ошибки, stack traces, вывод `phpinfo()`, пути ФС, секреты (ключи AWS, private keys) — лог/маскирование | v2.0 |
| 28 | Защитные заголовки ответа | 693, 1021 | Output | `X-Content-Type-Options`, `X-Frame-Options`, `Referrer-Policy`, HSTS (opt-in) | v2.0 |
| 29 | Session fixation / cookie tampering | 384, 565 | Sink | ID сессии из URL, отсутствие регенерации после логина (детект), подпись/шифрование cookie (opt-in) | v2.1 |
| 30 | Virtual patching (пользовательские правила) | — | Input + Sink + Output | Формат IGR (раздел 11): условия на запрос/аргументы функций/ответ → действие; закрытие CVE в CMS/плагинах без правки кода; политики опасных функций по каталогам | v2.1 |
| 31 | Brute force / credential stuffing | 307 | Behavior | Счётчики по IP и логину на заданных путях | v2.2 |
| 32 | Rate limiting, автобан, IP allow/deny | 770 | Behavior | Sliding window, временный бан после N атак, CIDR-списки, доверенные прокси для `X-Forwarded-For` | v2.2 |

### 10.3 Общие требования к каждому новому классу (FR-9)
- **FR-9.1** Каждый класс — модуль по FR-10: регистрация в реестре модулей, единая схема директив `inletguard.<m>.*` (FR-10.3), отключаемость на всех четырёх уровнях (FR-10.2), флаг сборки `--disable-inletguard-<m>`.
- **FR-9.2** Значения по умолчанию задаются для каждого профиля (FR-10.4): классы с высокой точностью (sink-проверки) — включены в глобальном режиме; «шумные» (SSTI, type juggling, Output, Behavior) — `monitor` или выключены.
- **FR-9.3** Событие в логе использует тот же формат (FR-6.2) с новым `attack_type` и `stage` (`input`/`sink`/`output`/`behavior`).
- **FR-9.4** Хук стока регистрируется, только если функция/класс существует в текущей версии PHP и расширение загружено.
- **FR-9.5** У каждого класса — корпус атак и легитимных данных, целевые метрики точности фиксируются при старте фазы.
- **FR-9.6** Каждый новый парсер/токенизатор — fuzz-таргет.
- **FR-9.7** Бюджет производительности на класс; при выключенном классе — нулевые накладные расходы.
- **FR-9.8** PHP API расширяется единообразно: `InletGuard\inspect()` принимает новые битовые флаги классов (`LFI`, `CMDI`, `SSRF`, …), добавляются точечные функции для sink-проверок (`check_path()`, `check_url()`, `check_command()`).

## 11. Формат правил InletGuard Rules (IGR)

Собственный формат правил для virtual patching и тонкой настройки защиты. Раздел — нормативный черновик; при реализации переносится в англоязычную спецификацию `docs/rules.md` + `docs/rules.ebnf`. Релиз движка — v2.1 (фаза 14), формат — `version 1`.

### 11.1 Цели формата
1. **Читаемость и ревью.** Правило понятно без документации, хорошо смотрится в diff, допускает комментарии.
2. **Все уровни в одном формате:** запрос (`request`), вызов опасной функции (`sink`), ответ (`response`).
3. **Собственные возможности InletGuard**, которых нет в WAF-форматах: «аргумент содержит пользовательский ввод» (`is tainted`), «ввод ломает структуру SQL/shell/пути…» (`breaks sql`), встроенные детекторы (`is sqli`).
4. **Маленькая, однозначная грамматика (LL(1))**: ручной рекурсивный парсер на C без зависимостей, полностью покрытый fuzz-тестами. Никаких YAML/JSON-парсеров.
5. **Предсказуемая производительность**: всё компилируется один раз при старте (MINIT), регулярные выражения предкомпилированы, sink-правила индексируются по имени функции.
6. **Безопасные ошибки**: сломанное правило пропускается с предупреждением, остальные работают (fail-open, NFR-3).
7. **Версионирование**: обязательный заголовок `version N`; будущие версии добавляют конструкции, не меняя смысл старых.
8. **Конвертируемость**: смысловая модель близка к Snuffleupagus (функция/аргумент/значение) и ModSecurity (переменные запроса), чтобы офлайн-конвертеры (бэклог) были простыми.

### 11.2 Пример
```
# /etc/inletguard/rules.d/10-wordpress.igr
version 1

rule 10001 "CVE-2025-1234: foo-plugin PHP object injection" {
    match    request.path starts_with "/wp-content/plugins/foo/"
    match    sink.function == "unserialize"
    match    sink.arg[0] is tainted
    action   block
    severity critical
    tags     "cve:2025-1234", "wordpress", "deser"
}

rule 10002 "foo-plugin: id must be numeric" {
    match  request.path == "/wp-content/plugins/foo/ajax.php"
    match  not get["id"] is int
    action block
    status 400
}

rule 10003 "Allow HTML in the post editor" {
    match  request.path == "/wp-admin/post.php" and request.method == "POST"
    action allow xss on post["content"], post["excerpt"]
}

rule 10004 "No shell commands from upload directories" {
    match  sink.function in ("system", "exec", "shell_exec", "passthru", "popen", "proc_open")
    match  sink.file ~ "/wp-content/uploads/"
    action block
    on_block terminate
}

rule 10005 "Admin area only from office network" {
    match  request.path starts_with "/wp-admin/"
    match  not request.ip in_cidr ("203.0.113.0/24", "2001:db8::/32")
    action block
    dry_run true          # пока только лог с action "would_block"
}

rule 10006 "Search query: decoded payload must not be SQLi" {
    match  get["q"] | url_decode_all | lower is sqli
    action log
}
```

### 11.3 Лексика
- Кодировка UTF-8, переводы строк LF или CRLF; переводы строк не значимы (каждое утверждение начинается с ключевого слова).
- Комментарии: `#` до конца строки.
- Ключевые слова — только в нижнем регистре: `version rule match action stage severity tags msg status dry_run enabled on_block and or not is in on any all exists true false`.
- Строки — только в двойных кавычках; экранирование `\"`, `\\`, `\n`, `\t`, `\xHH`. Максимальная длина строки — 64 КБ.
- Числа — десятичные целые без знака.
- Поля — идентификаторы с точками: `request.path`, `sink.arg`.
- Операторы: `== != ~ !~ < > <= >= | ( ) [ ] { } ,` и словесные `in`, `in_cidr`, `contains`, `starts_with`, `ends_with`.

### 11.4 Грамматика (EBNF)
```ebnf
file        = "version" INT { rule } EOF ;
rule        = "rule" INT STRING "{" { statement } "}" ;
statement   = "stage" STAGE
            | "match" expr
            | "action" action
            | "severity" SEVERITY
            | "tags" STRING { "," STRING }
            | "msg" STRING
            | "status" INT
            | "on_block" ( "exception" | "false" | "terminate" )
            | "dry_run" BOOL
            | "enabled" BOOL ;
action      = "log"
            | "block"
            | "sanitize" [ IDENT ]                 (* стратегия модуля: escape, strip_tags, empty, strip *)
            | "allow" class_list [ "on" target { "," target } ]
            | "set_mode" MODE [ "for" class_list ] ;
MODE        = "monitor" | "block" | "sanitize" ;
class_list  = "*" | IDENT { "," IDENT } ;            (* sqli, xss, lfi, ... *)
expr        = and_expr { "or" and_expr } ;
and_expr    = unary { "and" unary } ;
unary       = "not" unary | "(" expr ")" | predicate ;
predicate   = [ "any" | "all" ] subject ( compare | check | "exists" ) ;
subject     = target { "|" TRANSFORM } ;
target      = FIELD [ "[" ( STRING | INT | "*" ) "]" ] [ "." IDENT ] ;
compare     = ( "==" | "!=" | "~" | "!~" | "<" | ">" | "<=" | ">="
              | "contains" | "starts_with" | "ends_with" ) value
            | ( "in" | "in_cidr" ) list ;
check       = "is" [ "not" ] ( "tainted" [ "from" SOURCE { "," SOURCE } ]
                             | "sqli" | "xss"
                             | "int" | "numeric" | "email" | "empty" | "array" | "string" )
            | "breaks" GRAMMAR ;                    (* sql, shell, path, url, ldap, xpath, html, php *)
list        = "(" value { "," value } ")" ;
value       = STRING | INT | BOOL ;
STAGE       = "request" | "sink" | "response" ;
SEVERITY    = "info" | "low" | "medium" | "high" | "critical" ;
```
Несколько `match` в правиле объединяются через **AND**. Приоритет: `not` > `and` > `or`. Глубина вложенности выражений — не более 32.

### 11.5 Поля (targets)

**Запрос** — доступны на всех стадиях:
| Поле | Описание |
|---|---|
| `request.method`, `request.scheme`, `request.host` | |
| `request.path` | Путь без query string, нормализованный (`//`, `/./`, `%2e`) |
| `request.uri`, `request.query` | Как пришли |
| `request.ip` | IP клиента с учётом `trusted_proxies` |
| `request.content_type`, `request.body` | Тело — с лимитом `inspect_max_len` |
| `request.script` | `SCRIPT_FILENAME` |
| `get[..]`, `post[..]`, `cookie[..]`, `header[..]` | Параметры; `header` — без учёта регистра имени |
| `json[..]` | Значение по JSON Pointer: `json["/user/name"]` |
| `args[..]` | Объединение `get`, `post`, `json` |
| `files[..].name`, `.type`, `.size`, `.ext` | Загружаемые файлы |

Селектор: `get["id"]` — конкретный параметр; `get["user_*"]` — glob; `get[*]` или `get` — все значения. Суффикс `.keys` — имена параметров вместо значений (`get.keys ~ "^\$"` — NoSQL-операторы).

**Вызов функции** (`sink`):
| Поле | Описание |
|---|---|
| `sink.function` | Имя в нижнем регистре; метод — `class::method` (`pdo::query`, `mysqli::query`) |
| `sink.arg[N]` / `sink.arg["name"]` | Аргумент по индексу (с 0) или по имени из arginfo |
| `sink.args` | Все аргументы |
| `sink.file`, `sink.line` | Место вызова в пользовательском коде |

**Ответ** (`response`, после фазы 13): `response.status`, `response.header[..]`, `response.content_type`, `response.body` (с лимитом).

### 11.6 Операторы, проверки, трансформации
- Сравнения: `==`, `!=`, `<`, `>`, `<=`, `>=` (числа сравниваются как числа), `contains`, `starts_with`, `ends_with`, `in (…)`, `in_cidr (…)` (IPv4/IPv6).
- `~` / `!~` — регулярное выражение PCRE (диалект как в `preg_*`), записывается строкой **без разделителей**; флаги — инлайн: `"(?i)^/admin"`. Компилируется при загрузке, при выполнении действует лимит `pcre.backtrack_limit`.
- Проверки `is …`:
  - `tainted [from get, post, …]` — значение содержит пользовательский ввод из реестра (FR-1, фаза 8) длиной ≥ `min_input_len`;
  - `sqli`, `xss` — встроенные детекторы libinjection;
  - `int`, `numeric`, `email`, `empty`, `array`, `string` — проверка типа/формата.
- `breaks <grammar>` — пользовательский ввод внутри значения выходит за пределы одного литерала соответствующей грамматики (SQL, shell, path, URL, LDAP, XPath, HTML, PHP). Если токенизатор грамматики ещё не реализован, правило отклоняется при загрузке.
- Трансформации (применяются слева направо, только к копии значения): `lower`, `upper`, `trim`, `url_decode`, `url_decode_all` (до стабилизации, ≤ `decode_rounds`), `html_decode`, `base64_decode`, `hex_decode`, `normalize_path`, `remove_nulls`, `compress_ws`, `length` (→ число), `count` (коллекция → число).

### 11.7 Семантика выполнения
- **Стадия** определяется по полям: есть `sink.*` → `sink`; есть `response.*` → `response`; иначе `request`. Явный `stage` допускается, но противоречие полям — ошибка загрузки. Поля `sink.*` и `response.*` в одном правиле — ошибка.
- **Коллекции**: по умолчанию `any` — предикат истинен, если подходит хотя бы одно значение; `all` — все значения (пустая коллекция для `all` — ложь). `not` применяется к результату предиката целиком.
- **Отсутствующее поле** — любой предикат ложен, кроме `not … exists`.
- **Где выполняется:**
  - `request` — в RINIT, после встроенных детекторов входа, до выполнения скрипта;
  - `sink` — при вызове функции. Правило **обязано** ограничить `sink.function` через `==` или `in` (иначе ошибка загрузки): по этому списку при старте ставятся хуки и строится индекс «функция → правила». Функция, которой нет в текущей версии PHP / незагруженное расширение, → предупреждение, правило неактивно;
  - `response` — в output handler.
- **Действия:**
  - `log` — только событие;
  - `block` — для `request`/`response` — ответ `block_status` (или `status` правила) и прекращение выполнения; для `sink` — по `on_block`: `exception` (`InletGuard\BlockedException`, по умолчанию), `false` (функция возвращает `false` + warning) или `terminate` (страница блокировки и завершение запроса);
  - `sanitize [стратегия]` — только `request`, изменяет совпавшие значения в суперглобалях; без стратегии — `<m>.sanitize` модуля (FR-4.3);
  - `allow <классы> [on <поля>]` — подавляет срабатывания **встроенных детекторов** указанных классов (`*` — всех) для указанных полей или всего запроса. `allow` никогда не отменяет `block` других правил;
  - `set_mode <режим> [for <классы>]` — только `request`, меняет режим встроенных модулей для текущего запроса (например, `monitor` для `/api/legacy/`, `block` для `/admin/`). Это уровень «Запрос» в FR-10.2/FR-10.5. Если несколько правил задают режим одному модулю, побеждает самый строгий.
- **Разрешение конфликтов**: из всех сработавших правил применяется самое строгое действие (`block` > `sanitize` > `log`); каждое сработавшее правило пишет своё событие.
- **Глобальный потолок**: класс `rules` подчиняется FR-9 — `inletguard.rules.mode=monitor` превращает все `block`/`sanitize` правил в `log`. `dry_run true` делает то же для одного правила; в логе `action: "would_block"`.
- **Событие** (FR-6.2) дополняется полями `rule_id`, `rule_msg`, `severity`, `tags`, `matched_field`, `matched_value` (усечение и `log_payload` действуют как обычно).
- **Исключения из ini** (`exclude_params`, `*.exclude_params`, `exclude_paths`) при загрузке транслируются во внутренние `allow`-правила — один механизм вместо двух; поведение FR-5 не меняется.

### 11.8 Загрузка, идентификаторы, ошибки
- Директивы (все `PHP_INI_SYSTEM`):

  | Директива | По умолчанию | Описание |
  |---|---|---|
  | `inletguard.rules.enabled` | `1` | Движок правил |
  | `inletguard.rules.mode` | *(наследует)* | Потолок действий правил |
  | `inletguard.rules_files` | `/etc/inletguard/rules.d/*.igr` | Список путей/glob через запятую; отсутствующий каталог — не ошибка (FR-13.4) |
  | `inletguard.rules_strict` | `0` | `1` — файл с ошибкой не загружается целиком |
  | `inletguard.rules_max` | `10000` | Лимит числа правил |

- Файлы читаются при старте процесса (MINIT); новые правила применяются после перезапуска/reload PHP-FPM, Apache, LSAPI. Порядок файлов — лексикографический (как `rules.d/`).
- **ID правил** уникальны среди всех файлов; дубликат — ошибка, второе правило пропускается. Диапазоны: `1–99999` — пользовательские, `100000–899999` — официальные пакеты InletGuard, `900000–999999` — зарезервированы (внутренние правила из ini-исключений, тесты).
- **Ошибки** — на английском, в формате `file:line:col: error: message (hint)`; пишутся как `config_warning` и в startup-лог PHP. Парсер восстанавливается до следующего `rule`, чтобы показать все ошибки файла за один проход.
- **Безопасность**: предупреждение, если файл правил или каталог доступен на запись не-владельцу; файлы > 1 МБ отклоняются; правила неизменяемы после загрузки и разделяются между потоками ZTS только на чтение.
- `phpinfo()` показывает загруженные файлы, число активных/неактивных правил и ошибки.

### 11.9 Инструменты для авторов правил (TDD для правил)
- `InletGuard\rules_lint(string $source): array` — список ошибок/предупреждений.
- `InletGuard\rules_eval(string $source, array $request, ?array $sink = null): array` — выполнить правила на «фейковом» запросе/вызове и вернуть сработавшие правила и итоговое действие. Позволяет писать тесты на правила в PHPUnit/phpt раньше, чем само правило.
- `InletGuard\rules_loaded(): array` — метаданные загруженных правил.
- CLI `bin/inletguard-rules lint|test <files>` поверх этих функций.
- Каждое официальное правило поставляется вместе с тест-фикстурами: запрос-атака (должно сработать) и легитимный запрос (не должно).

### 11.10 Вне `version 1` (бэклог)
- Правила на вызовы **пользовательских** PHP-функций (через observer API на 8.x и `zend_execute_ex` на 7.x).
- Переменные/счётчики между правилами (как `TX` в ModSecurity), цепочки, scoring.
- Горячая перезагрузка без рестарта PHP.
- Офлайн-конвертеры `inletguard-rules import --from snuffleupagus|modsecurity` (сначала Snuffleupagus, затем подмножество ModSecurity: `ARGS`, `REQUEST_URI`, `REQUEST_HEADERS`, `@rx/@pm/@contains/@beginsWith/@detectSQLi/@detectXSS`, базовые трансформации, `deny/pass/log`) с отчётом о неподдерживаемых конструкциях.
- Подсветка синтаксиса для VS Code / JetBrains.

## 12. Открытые вопросы
Все исходные вопросы решены:
1. ~~Семантика `sanitize` для SQLi.~~ Решено: настраивается, `sqli.sanitize = empty|strip` (FR-4.3).
2. ~~Ротация лога.~~ Решено: настраивается, `log_rotate = external|size|daily|hourly` (FR-6.8).
3. ~~Проверять ли JSON-тело в v1?~~ Решено: v1.1 (фаза 8).
4. ~~Режим обучения.~~ Решено: да, модуль `learn`, настраивается (FR-11).
5. ~~Лицензия.~~ Решено: MIT (NFR-9).
6. ~~PHP 5.6 / 7.0.~~ Решено: вся ветка 7.x (с 7.0) поддерживается, 5.x — нет.
7. ~~Формат правил virtual patching: совместимый с Snuffleupagus/ModSecurity или свой?~~ Решено: свой формат IGR (раздел 11), импорт из Snuffleupagus/ModSecurity — офлайн-конвертером в бэклоге.
8. ~~Хранение счётчиков Behavior-уровня.~~ Решено: `shm` по умолчанию (v2.2), Redis/Memcached — опционально через `counters.backend` (v2.3, FR-12).

## 13. Заметки по реализации

> Правило проекта (`AGENTS.md`): взял задачу → сначала обдумать решение и записать заметку сюда **и в `PRD.md`** (раздел 13) → потом реализация. Заметка фиксирует решение, альтернативы и план тестов (TDD). Номера — по `TASKS.ru.md`.

### 0.1 Каркас расширения *(заметка записана задним числом — правило из AGENTS.md замечено после реализации)*
- Классическая раскладка PECL: `config.m4`, `php_inletguard.h`, `inletguard.c` в корне (так ожидают `phpize`, PECL и PIE); код ядра — `src/core/` (чистый C99, без Zend), интеграция — `src/zend/`.
- `--enable-inletguard-dev` включает `-Wall -Wextra -Werror` (без `unused-parameter` и `missing-field-initializers` — их генерируют макросы Zend). `-Werror` не включается по умолчанию, чтобы сборка у пользователей на новых компиляторах не ломалась из-за новых предупреждений.
- `-std=gnu99`, а не `c99`: заголовки PHP 7.x используют GNU-расширения; ядро в `src/core/` при этом проверяется строгим `-std=c99 -pedantic` в unit-сборке.
- ZTS: стандартная схема `ZEND_TSRMLS_CACHE_DEFINE/UPDATE` при `COMPILE_DL_INLETGUARD`.

### 0.3 Unit-раннер на C *(задним числом)*
- Свой минимальный раннер вместо cmocka/Unity: ноль зависимостей (NFR-9, старые тулчейны хостингов), ~100 строк.
- Тесты саморегистрируются через `__attribute__((constructor))` (GCC/Clang, есть в GCC 4.8) — новый тест-файл достаточно положить как `tests/unit/test_*.c`.
- Отдельный `tests/unit/Makefile` (собирается без PHP — быстрый TDD-цикл), из основного Makefile доступен как `make unit` / `make unit-asan` через `Makefile.frag`. Строгие флаги `-std=c99 -pedantic -Werror` гарантируют, что ядро остаётся чистым C99.
- Фильтр по имени теста: `make -C tests/unit FILTER=version`.

### 0.2 `InletGuard\version()` — первый TDD-цикл
**Решение:**
- Единственный источник версии — `src/core/ig_version.h` (`IG_VERSION_MAJOR/MINOR/PATCH/SUFFIX` → строка `IG_VERSION`); `php_inletguard.h` берёт `PHP_INLETGUARD_VERSION` оттуда. Скрипт согласованности версий (0.5f) читает этот же файл.
- `ig_version()` в ядре — для unit-теста и будущих потребителей `libigcore`.
- Функция в пространстве имён через `ZEND_NS_NAMED_FE("InletGuard", version, zif_inletguard_version, …)` — макрос есть во всех версиях 7.0–8.4; C-имя `zif_inletguard_version`, чтобы не конфликтовать с другими `version`.
- arginfo с типом возврата `string`: сигнатура `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX` различается — в 7.0–7.1 `(name, ref, req, type, class_name, allow_null)`, с 7.2 `(name, ref, req, type, allow_null)`. Это первая обёртка слоя совместимости (0.5a): `IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO(name, req, type, allow_null)` в `src/zend/php_compat.h`.
- Параметры — `zend_parse_parameters_none()` (есть везде; `ZEND_PARSE_PARAMETERS_NONE` появился позже).

**Тесты (пишутся первыми, red):**
1. unit `test_version.c`: `ig_version()` равна `IG_VERSION`; строка собрана из MAJOR.MINOR.PATCH[-SUFFIX]. Red — не компилируется (нет `ig_version.h`).
2. phpt `002-version.phpt`: `InletGuard\version()` возвращает строку = `phpversion('inletguard')`, формат SemVer. Red — `Call to undefined function`.
3. phpt `003-version-reflection.phpt`: Reflection — функция в пространстве имён `InletGuard`, 0 параметров, тип возврата `string` на всех версиях. Проверяет обёртку arginfo.

**Проверка:** локально PHP 7.2, 7.4, 8.1, 8.4 (phvm) + 7.0 в Docker; полная матрица — после CI (0.5).

### 0.5 / 0.5b CI `ci.yml`
**Факты, на которые опирается решение (проверено):**
- Официальные образы `php:<ver>-cli` и `php:<ver>-zts` есть для всех 7.0–8.4 и содержат `phpize` и `php-cgi` (нужен для phpt-секций `--GET--/--POST--`).
- В `php:7.0-cli` glibc 2.24, а JavaScript-actions (`actions/checkout` и др.) внутри job-контейнера требуют node 20+ → glibc ≥ 2.28. Поэтому **job-level `container:` не используем**: checkout выполняется на хосте раннера, сборка — через `docker run` с примонтированным исходником.

**Решение:**
- Один скрипт `scripts/ci/phpt.sh` (сборка `phpize → configure --enable-inletguard-dev → make → make test`) — одинаково запускается в CI и локально: `docker run --rm -v "$PWD:/src:ro" php:8.4-zts sh /src/scripts/ci/phpt.sh`. Исходник монтируется read-only и копируется внутрь контейнера, чтобы артефакты сборки не попадали в рабочую копию.
- `REPORT_EXIT_STATUS=1` — иначе `run-tests.php` старых версий завершается с кодом 0 даже при падениях; при падении печатаются `.diff`.
- Jobs: `lint` (clang-format, cppcheck по `src/core`, SPDX/лицензии), `unit` (gcc и clang, обычная и ASan/UBSan сборка ядра), `unit-centos7` (ядро собирается GCC 4.8.5 в `centos:7` — проверка C99 на старом компиляторе), `phpt` (матрица 10 версий × `cli`/`zts`), `phpt-asan` (расширение с ASan, `LD_PRELOAD=libasan`, `USE_ZEND_ALLOC=0`, на 8.1 и 8.4 — см. итоги), `phpt-valgrind` (8.4, `run-tests -m`), `phpt-arm64` (7.0 и 8.4 на `ubuntu-24.04-arm`).
- `concurrency` с отменой устаревших прогонов, `permissions: contents: read`.
- Actions закреплены по SHA коммита с комментарием версии; Dependabot обновляет `github-actions` еженедельно.

**Отложено (с причиной):**
- Закрепление Docker-образов по digest — в матрице 20 образов; делаем в 0.5d вместе с релизными образами, сейчас — теги. Риск: тег `php:7.0-cli` заморожен (EOL), дрейф только у поддерживаемых веток.
- Сборка *расширения* на GCC 4.8 / glibc 2.17 требует PHP, собранного из исходников в таком окружении — это делается в 0.5d (релизные сборки). Сейчас на GCC 4.8 проверяется только ядро.

**Проверка:** workflow валидируется `actionlint`; реальный прогон — после появления GitHub-репозитория (remote пока не настроен).

**Итоги реализации (отклонения от плана, выяснено при локальном прогоне всех jobs в Docker):**
- `gcc:4.8` не запускается современным Docker (manifest v1). Заменён на `centos:7` (GCC 4.8.5, glibc 2.17, репозитории с `vault.centos.org`) — это и есть целевая платформа хостингов; скрипт `scripts/ci/unit-centos7.sh`.
- ASan: штатный PHP загружает расширения с `RTLD_DEEPBIND`, что несовместимо с ASan. Решение — LD_PRELOAD-шим, убирающий флаг в `dlopen()`, без пересборки PHP. Работает на образах 8.1+ (GCC 12+); на 7.x (GCC 10) рантайм ASan не удаётся подгрузить в неинструментированный PHP. **ASan для 7.x переносится в 0.5d** — там PHP всё равно собирается из исходников, добавим вариант с `-fsanitize=address`.
- В ASan-режиме `-Werror` выключен: заголовки PHP 8.1 переопределяют `__SANITIZE_ADDRESS__` под GCC 14, это предупреждение не отключается флагом. Строгие предупреждения проверяют обычные phpt-jobs.
- LeakSanitizer выключен (не работает в непривилегированных контейнерах и ломает тестовые программы configure); утечки ловит Valgrind-job.
- Скрипт чистит in-tree артефакты phpize, чтобы локальный запуск из «грязной» рабочей копии давал тот же результат, что CI.
- **Негативная проверка:** намеренное переполнение кучи в `InletGuard\version()` роняет и ASan-job (`heap-buffer-overflow`), и Valgrind-job (`LEAKED`/invalid write) — jobs не «зелёные вхолостую». Первая версия ловушки (`emalloc(4)`, запись в `p[4]`) не сработала: константный размер попадает в 8-байтовый класс аллокатора — ловушка должна использовать динамический размер.

### 0.6 Линтеры
- `.clang-format`: стиль PHP-исходников (табы шириной 4, скобки функций на новой строке, лимит 100 колонок). Версия clang-format закреплена (`clang-format==18.1.8` из PyPI) — разные версии форматируют по-разному.
- `cppcheck` по `src/core` (чистый C99, без заголовков PHP), `--error-exitcode=1`.
- `clang-tidy` — отложен до появления содержательного кода ядра (фаза 1): на текущем объёме даёт только шум.

### 0.11 README / CONTRIBUTING, 0.12 Лицензирование
- `README.md` и `CONTRIBUTING.md` — на английском (NFR-8). README честно говорит, что проект в разработке и релизов до v1.0.0 нет (PRD 9.2).
- `LICENSE` — MIT, правообладатель «InletGuard contributors».
- `THIRD_PARTY_NOTICES` — пока без записей (libinjection вендорится в 0.7); формат файла задаётся сейчас.
- `scripts/check-license.sh`: SPDX-заголовок в первых 3 строках каждого `*.c`, `*.h`, `*.m4`, `Makefile*`, `*.sh` вне `third_party/`; наличие `LICENSE` и `THIRD_PARTY_NOTICES`; в `third_party/` каждая библиотека содержит файл лицензии, и он не GPL/LGPL.

### Документация на двух языках и репозиторий GitHub
- `PRD.ru.md`, `TASKS.ru.md` — первоисточник (русский). `PRD.md`, `TASKS.md` — базовые английские версии: та же структура разделов и те же ID/статусы задач, но сжато. Любое изменение вносится в оба файла пары в том же коммите; заметки по реализации (раздел 13) ведутся в обоих PRD (английская — в сжатом виде).
- Репозиторий создаётся через `gh repo create` из локальной копии; описание и топики задаются там же (`gh repo edit --add-topic`). Видимость — по решению владельца. После push: включить branch protection для `main` с обязательными проверками `ci.yml` (остаток задачи 0.5b) — после первого зелёного прогона, когда имена проверок известны.

### 0.5b Защита ветки `main`
**Решение:**
- Требуется pull request и зелёные проверки; **0 обязательных одобрений** (единственный мейнтейнер не может одобрить свой PR); без требования «ветка должна быть свежей» (strict) — иначе каждый PR придётся пересинхронизировать после каждого мержа.
- Обязательная проверка одна — агрегирующий job **`ci-ok`**: зависит (`needs`) от всех остальных jobs и падает, если хоть один упал или отменён. Перечислять 31 имя jobs матрицы нельзя: при любом изменении матрицы PR будут вечно ждать проверку, которой больше нет.
- Никогда не добавлять `paths-ignore`/`paths` в `ci.yml`: обязательная проверка, которая не запустилась, навсегда блокирует PR.
- Правила действуют и на администраторов (`enforce_admins`) — процесс одинаков для всех; force-push и удаление ветки запрещены.

**Порядок внедрения:** добавить `ci-ok` в ветке → открыть PR → когда `ci-ok` отчитается зелёным, включить защиту через GitHub API → смержить PR через `gh pr merge` (заодно проверка нового процесса целиком).

### 0.5a Слой совместимости `php_compat.h`
**Решение:**
- Слой «живой»: обёртки добавляет та задача, которой они нужны (YAGNI) — заранее писать макросы для несуществующего кода значит писать их без тестов. Пример: nullable-тип возврата понадобится в 5.1 (`check_sqli(): ?string`) — тогда и появится обёртка.
- Задача 0.5a закрывает **основу**, которая не зависит от будущих обёрток:
  1. автоматическая проверка правила «проверки версий PHP — только в `src/zend/php_compat.h`»: скрипт `scripts/check-compat.sh` ищет `PHP_VERSION_ID`, `PHP_MAJOR_VERSION`, `PHP_MINOR_VERSION`, `PHP_RELEASE_VERSION`, `ZEND_MODULE_API_NO`, `ZEND_EXTENSION_API_NO` во всех `*.c`/`*.h` вне `third_party/` и падает при находке вне `php_compat.h`; запускается в job `lint`;
  2. соглашения для обёрток (комментарий в шапке `php_compat.h`): префикс `IG_`, у каждой обёртки комментарий «что различается и с какой версии», у каждой — phpt, проходящий на всей матрице.
- Почему скрипт, а не clang-tidy/cppcheck-правило: нужна проверка по имени макроса и по пути файла — `grep` проще, не требует заголовков PHP и работает в любом окружении.

**Тесты (red первыми):** `tests/scripts/test-check-compat.sh` — готовит временные деревья: (а) проверка версии только в `php_compat.h` → скрипт проходит; (б) `PHP_VERSION_ID` в другом `.c` → падает и называет файл; (в) упоминание в комментарии тоже считается нарушением (проще и строже, чем разбирать комментарии); (г) `third_party/` игнорируется. Red — скрипта ещё нет. Тест запускается в `lint`.

**Итог:** первый прогон CI упал — тест-скрипт не попал в коммит: правило `.gitignore` `/tests/**/*.sh` (вывод `run-tests.php`) молча скрыло и `tests/scripts/*.sh`; локально всё проходило, потому что файл лежал на диске. Исправлено исключением `!/tests/scripts/`, ветка пересобрана, чтобы red-коммит действительно содержал тест. Вывод для процесса: перед push проверять `git status --ignored` для новых файлов; агрегатор `ci-ok` при этом отработал верно (упал вслед за `lint`).

### 0.5c `nightly.yml`
**Решение:**
- Fuzz-таргетов, корпусов и бенчмарков ещё нет (появятся в 1.4, 1.8, 7.3), поэтому nightly — каркас с **автообнаружением**: следующие задачи только добавляют файлы, workflow не меняется.
- Job `ci`: полный `ci.yml` по расписанию через `workflow_call` — ловит дрейф незакреплённых тегов образов (`php:8.x-cli` обновляются) до того, как он сломает чужой PR. Для этого `concurrency.group` в `ci.yml` меняется на `${{ github.workflow }}-${{ github.ref }}`: при вызове из nightly `github.workflow` — имя вызывающего workflow, иначе nightly и push в `main` отменяли бы друг друга.
- Job `fuzz`: `tests/fuzz/Makefile` находит `fuzz_*.c`, собирает каждый clang с `-fsanitize=fuzzer,address,undefined` вместе с исходниками ядра и гоняет `FUZZ_SECONDS` (по умолчанию 300 в nightly, задаётся при ручном запуске). Начальный корпус — `tests/fuzz/corpus/<таргет>/` (в git), рабочий корпус переживает запуски через `actions/cache`; найденные падения (`crash-*`, `leak-*`, `timeout-*`) загружаются артефактом. Без таргетов job пишет «no fuzz targets yet» и проходит.
- Корпусы точности и бенчмарки подключаются тем же образом в 1.8 и 7.3 (в nightly — заглушки-шаги с автообнаружением не нужны, YAGNI: добавятся вместе с раннерами).
- Время запуска — 03:17 UTC (не ровный час: меньше очередь раннеров).

**Тесты (red первыми):** `tests/scripts/test-fuzz-harness.sh` проверяет сам каркас на временных таргетах: (а) нет таргетов → успех и сообщение; (б) безопасный таргет → успех; (в) таргет с ошибкой (переполнение буфера на входе `BUG`, вход лежит в начальном корпусе) → харнесс падает и оставляет `crash-*`. Запускается в `lint` CI (быстро: секунды), чтобы каркас не сломался незаметно до появления настоящих таргетов. Red — харнесса ещё нет.

### 0.5d `release.yml` в режиме dry-run
**Решение:**
- **Окружение сборки** — образы `quay.io/pypa/manylinux2014_{x86_64,aarch64}` (CentOS 7: glibc 2.17, devtoolset gcc 10), закреплены по digest. Это даёт `.so`, загружаемые на старых хостингах (NFR-11), и одинаковое окружение для обеих архитектур (aarch64 — на ARM-раннерах, без эмуляции).
- **PHP собирается из исходников** минимально (`--disable-all`, без PEAR) — нужны `phpize`, `php-config`, заголовки и CLI/CGI для прогона phpt. ZTS: `--enable-maintainer-zts` в 7.x, `--enable-zts` с 8.0. Старые PHP с gcc 10 — `CFLAGS=-fcommon` (gcc 10 по умолчанию `-fno-common`, это ломает 7.0–7.x).
- **Версии и SHA256 исходников PHP** — в `scripts/release/php-versions.txt` (последние патч-версии каждой ветки на момент задачи, с php.net). Тарбол проверяется по SHA256 перед сборкой. Патч-версия внутри ветки не влияет на ABI расширения (`ZEND_MODULE_API_NO` один на ветку), так что `.so`, собранный с 8.3.36, загружается в любой 8.3.x.
- **Кэш:** собранный PHP кэшируется (`actions/cache`, ключ — версия × вариант × архитектура × хэш скрипта сборки и файла версий), чтобы 40 сборок PHP не повторялись на каждом push.
- **Каждая сборка** = собрать расширение против своего PHP → прогнать phpt этим же PHP → упаковать `inletguard-<ver>-php<X.Y>-<nts|zts>-<arch>-linux-gnu.tar.gz` (`inletguard.so`, `20-inletguard.ini`, `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES`).
- **Итоговый job** собирает все архивы, исходный тарбол (`git archive`) и `SHA256SUMS`, загружает их одним артефактом workflow с коротким сроком хранения (7 дней). **Публикации нет вообще** — шага публикации в workflow нет, он появится в 0.5e вместе с замками.
- **Отклонения от плана (с причиной):**
  - PECL-пакет (`package.xml`) переносится в 7.7: в нём метаданные мейнтейнера и список файлов, которые нужно финализировать к релизу; сейчас — исходный тарбол.
  - Attestations (`actions/attest-build-provenance`) — только при настоящем релизе (0.5e): они пишутся в публичный журнал прозрачности Sigstore, т.е. это уже публикация.
  - Digest-пины образов `php:*` в CI-матрице не делаем: дрейф тегов теперь ловит nightly (0.5c), EOL-теги и так заморожены, а Dependabot не обновляет образы в матрице — пины только устаревали бы. Закрепляются по digest образы релизной сборки.
- `packaging/20-inletguard.ini` — пока одна строка `extension=inletguard.so` (полная версия с подсказками — задача 2.22).
- ASan для PHP 7.x (перенесено из 0.5) — отдельным PR сразу после этой задачи: тот же `build-php.sh` с `-fsanitize=address` на обычном раннере.

**Тесты (red первыми):** `tests/scripts/test-release-scripts.sh`: (а) `version.sh` печатает версию из `ig_version.h` и формат SemVer; (б) `php-versions.txt` содержит ровно ветки 7.0–8.4, у каждой — версия этой ветки и 64-символьный SHA256; (в) `package.sh` на фиктивном `.so` создаёт архив с правильным именем и ровно нужным набором файлов. Сборка PHP целиком проверяется локально в manylinux-контейнере (7.0 и 8.4, NTS и ZTS) и далее самим workflow.

### ASan для PHP 7.x (продолжение 0.5 / 0.5d)
**Факты (проверено):**
- В `Zend/zend_portability.h` PHP 7.0 и 7.4 `DL_LOAD` использует `RTLD_DEEPBIND` только если не определён `__SANITIZE_ADDRESS__`. Значит, PHP, собранный с `-fsanitize=address`, загружает расширения без `RTLD_DEEPBIND` — шим и `LD_PRELOAD` не нужны.
- PHP 7.0 и 7.4 собираются с ASan тулчейном раннера (`ubuntu-24.04`, gcc 13) тем же `build-php.sh` (`EXTRA_CFLAGS`, `LDFLAGS` через `EXTRA_CONFIGURE`).
- UBSan находит неопределённое поведение в самом PHP 7.x (`zend_alloc.c`: сдвиг `1 << 63`, `zend_hash.c`) — это не наши ошибки, и они роняли бы тесты.

**Решение:**
- Job `phpt-asan-php7` (матрица 7.0, 7.4) на хосте раннера: PHP собирается с `-fsanitize=address` (без UBSan), кэшируется; расширение собирается с `-fsanitize=address,undefined` — UBSan проверяет только наш код.
- `scripts/ci/phpt.sh --asan-native`: та же сборка расширения с санитайзерами, но без шима и без `LD_PRELOAD` (рантайм ASan уже в PHP); PHP берётся из `PATH`.
- `ci-ok` зависит и от нового job.

**Проверка:** локально на тех же сборках PHP — phpt зелёные; ловушка (переполнение кучи в `InletGuard\version()` с динамическим размером) роняет прогон на 7.0 и 7.4.

**Итог:** ловушка на 7.0 сначала **не сработала**, хотя прямой запуск `php` переполнение ловил. Причина: `run-tests.php` PHP 7.0–7.3 принудительно ставит дочерним процессам `USE_ZEND_ALLOC=1`, если не задан `-m` (Valgrind), а под аллокатором Zend ASan не видит переполнений внутри его блоков. В ASan-режимах `phpt.sh` теперь правит копию `run-tests.php` (`USE_ZEND_ALLOC` → `0`); после этого ловушка срабатывает на 7.0 и 7.4, обычный `--asan` на 8.4 по-прежнему зелёный. Вывод: каждый новый санитайзер-job проверять ловушкой на каждой версии, а не на одной.
