# Игровой сервер (Бэкенд)
### Описание:
Реализованный сервер для многопользовательской игры решает следующие задачи:
1. Игровая механика:
   - движение персонажей;
   - игровые коллизии;
   - статистика.
2. Передача необходимой информации для фронтенда:
   - игровое состояние;
   - списки игроков;
   - карты;
   - статистика.
3. Аутентификация и авторизация игроков.
3. Сохранение и восстановление игрового состояния.
4. Сохранение в базе данных и получение из нее достижений игроков.
5. Логирование работы сервера.
### Требования:
- операционная система: [Ubuntu 22.04](https://releases.ubuntu.com/22.04/);
- компилятор: [GCC 11.3](https://gcc.gnu.org/onlinedocs/11.3.0/);
- библиотеки:
  - [Boost 1.78.0](https://www.boost.org/doc/libs/1_78_0/);
  - [Catch2 3.1.0](https://github.com/catchorg/Catch2);
  - [PostgreSQL(libpqxx 7.7.4)](https://www.postgresql.org/);
- пакетный менеджер: [Conan 1.*](https://conan.io/);
- система сборки: [CMake 3.11 или выше](https:/cmake.org);
- автоматизация развёртывания: [Docker](https://www.docker.com/).

### Сборка:
###### *Предполагается, что на момент сборки были установлены вышеуказанные библиотеки и утилиты*  
Для сборки проекта достаточно создать папку build в корневой директории проекта, перейти в нее, далее при помощи пакетного менеджера и системы сборки собрать проект:
```bash
~/game-server$ mkdir build && cd build
~/game-server/build$ conan install .. --build=missing -s build_type=Debug -s compiler.libcxx=libstdc++11
~/game-server/build$ cmake -DCMAKE_BUILD_TYPE=Debug ..
~/game-server/build$ cmake --build .
```
### Запуск:
1. Предварительно запустить PostgreSQL в контейнере:
```bash
~/game-server$ sudo docker run -p 30432:5432 -e TZ=UTC -e POSTGRES_PASSWORD=Psql1Pswd ubuntu/postgres:14-22.04_beta
``` 
2. Создать базу данных test_bd:
```bash
~/game-server$ sudo psql postgres://postgres:Psql1Pswd@localhost:30432
~/game-server$ create database test_db;
```
3. Задать переменную среды GAME_DB_URL:
```bash
~/game-server$ export GAME_DB_URL=postgres://postgres:Mys3Cr3t@localhost:30432/test_db
```
4. Запустить сервер с указанием необходимых ключей:
```bash 
~/game-server$ build/bin/game_server -c data/config.json -w static -r -t 50 -p 1000 -s data/serialize_here
```
### Ключи запуска
| Ключ | Описание | Обязательный |
| :------: | ------ | :------: |
| `-c` | путь к конфигурационному файлу | Да |
| `-w` | путь к статическим данным | Да |
| `-t` | периодичность обновления, миллисекунд | Да |
| `-h` | вывод перечня доступных ключей | Нет |
| `-r` | включение режима рандомного появления <br /> предметов и персонажей в игре | Нет |
| `-p` | периодичность сериализации данных, миллисекунд | Нет |
| `-s` | путь к файлу сериализации | Нет |

### API
Данные возвращаемые в формате JSON:
- `/api/v1/maps` - список карт;
- `/api/v1/maps/...` - информация о запрашиваемой карте;
- `/api/v1/game/join` - присоединение к игре, получение token & id;
- `/api/v1/game/players` - список игроков (**Необходимо передать токен**);
- `/api/v1/game/state` - информация о состоянии игры (**Необходимо передать токен**);
- `/api/v1/game/records` - игровая статистика (**Необходимо передать токен**).


Для задания движения игровых персонажей используется: 
- `/api/v1/game/player/action` - направление движения.
### Структура проекта
```
game-server
└── build
    └── ... 
└── data
    └── config.json 
└── src
    └── util
    	└── ...
    └── ...    
└── static 
    └── ... 
└── tests 
    └── ...
├── CMakeLists.txt
└── conanfile.txt
```
