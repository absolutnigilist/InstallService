# InstallService

Кроссплатформенная CLI-утилита для установки/обновления, удаления, запуска и остановки службы.
На Windows использует `sc.exe`, на Linux systemd.

Проект организован так, чтобы:
- парсить CLI единообразно,
- формировать спецификацию службы (`ServiceSpec`),
- делегировать platform-specific операции в backend (`IServiceBackend`),
- корректно запускать внешние процессы (`svcinst::process::run`) на Windows/Linux,
- безопасно удалять директории (InstallDir/DataRoot) после uninstall.

---

# Запуск
Утилита ожидает **ровно одну** команду из:
- `--install`
- `--uninstall`
- `--start`
- `--stop`

Если команда не указана — выводится справка.

# Команды

# 1) Установка / обновление службы
**Команда:** `--install`

**Обязательные параметры:**
- `--name=<service_name>`
- `--exe=<path_to_exe>`

**Опционально:**
- `--args="..."` — строка аргументов, которая будет добавлена к binPath.
- `--desc="..."` — описание службы. Если пустое/пробельное — используется имя службы.
- `--run` — после установки выполнить запуск службы.

## Примеры
service-installer --install --name=Valenta --exe=Valenta.exe --run
service-installer --install --name=Valenta --exe="C:\Program Files\Valenta\Valenta.exe" --args="--config=C:\ProgramData\Valenta\config.ini"

# 2) Удаление 

**Команда** `--uninstall`

**Обязательные параметры:**
- `--stop-first` - сначала попытаться остановить службу, затем удалить
- `--delete=none|data|install|all` - политика очистки после удаления службы
- `--data-root=<path>` - путь к данным. нужен если `--delete=data|all`
- `--from-inno` - Windows: означает, что вызов пришёл из Inno Setup, и installDir не трогаем (Inno сам удалит {app}).

## Примеры
service-installer --uninstall --name=Valenta
service-installer --uninstall --name=Valenta --stop-first
service-installer --uninstall --name=Valenta --stop-first --delete=data --data-root="C:\ProgramData\Valenta"
service-installer --uninstall --name=Valenta --stop-first --delete=all --data-root="C:\ProgramData\Valenta" --from-inno

# 3) Запуск службы

**Команда:** `--start`

**Обязательные параметры:**
- `--name=<service_name>`

## Пример
service-installer --start --name=Valenta

# 4) Остановка службы

**Команда:** `--stop`

**Обязательные параметры:**
- `--name=<service_name>`

## Пример
service-installer --stop --name=Valenta




