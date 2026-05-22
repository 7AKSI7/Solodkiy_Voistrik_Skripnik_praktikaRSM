#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;

enum class ScheduleType {
    Once = 1,
    Daily = 2,
    Interval = 3
};

struct Task {
    int id{};
    string name;
    string command;
    ScheduleType type{ScheduleType::Once};
    chrono::system_clock::time_point nextRun;
    int intervalMinutes{};
    bool active{true};
};

const string TASKS_FILE = "tasks.txt";

string trim(const string& value) {
    const string spaces = " \t\r\n";
    const size_t first = value.find_first_not_of(spaces);
    if (first == string::npos) {
        return "";
    }
    const size_t last = value.find_last_not_of(spaces);
    return value.substr(first, last - first + 1);
}

vector<string> split(const string& line, char delimiter) {
    vector<string> parts;
    string part;
    stringstream stream(line);

    while (getline(stream, part, delimiter)) {
        parts.push_back(part);
    }

    return parts;
}

time_t toTimeT(const chrono::system_clock::time_point& point) {
    return chrono::system_clock::to_time_t(point);
}

tm localTime(time_t value) {
    tm result{};
#ifdef _WIN32
    tm* local = localtime(&value);
    if (local != nullptr) {
        result = *local;
    }
#else
    localtime_r(&value, &result);
#endif
    return result;
}

string formatDateTime(const chrono::system_clock::time_point& point) {
    const time_t rawTime = toTimeT(point);
    const tm local = localTime(rawTime);
    stringstream stream;
    stream << put_time(&local, "%Y-%m-%d %H:%M");
    return stream.str();
}

chrono::system_clock::time_point parseDateTime(const string& text, bool& ok) {
    tm date{};
    stringstream stream(text);
    stream >> get_time(&date, "%Y-%m-%d %H:%M");
    ok = !stream.fail();

    if (!ok) {
        return chrono::system_clock::now();
    }

    date.tm_sec = 0;
    const time_t rawTime = mktime(&date);
    if (rawTime == -1) {
        ok = false;
        return chrono::system_clock::now();
    }

    return chrono::system_clock::from_time_t(rawTime);
}

chrono::system_clock::time_point todayAt(const string& text, bool& ok) {
    tm parsed{};
    stringstream stream(text);
    stream >> get_time(&parsed, "%H:%M");
    ok = !stream.fail();

    if (!ok) {
        return chrono::system_clock::now();
    }

    const time_t nowRaw = toTimeT(chrono::system_clock::now());
    tm result = localTime(nowRaw);
    result.tm_hour = parsed.tm_hour;
    result.tm_min = parsed.tm_min;
    result.tm_sec = 0;

    chrono::system_clock::time_point runAt =
        chrono::system_clock::from_time_t(mktime(&result));

    if (runAt <= chrono::system_clock::now()) {
        runAt += chrono::hours(24);
    }

    return runAt;
}

string typeToString(ScheduleType type) {
    switch (type) {
    case ScheduleType::Once:
        return "один раз";
    case ScheduleType::Daily:
        return "ежедневно";
    case ScheduleType::Interval:
        return "интервал";
    default:
        return "неизвестно";
    }
}

string typeToFileString(ScheduleType type) {
    switch (type) {
    case ScheduleType::Once:
        return "once";
    case ScheduleType::Daily:
        return "daily";
    case ScheduleType::Interval:
        return "interval";
    default:
        return "once";
    }
}

string formatTime(const chrono::system_clock::time_point& point) {
    const time_t rawTime = toTimeT(point);
    const tm local = localTime(rawTime);
    stringstream stream;
    stream << put_time(&local, "%H:%M");
    return stream.str();
}

string toLower(string text) {
    transform(text.begin(), text.end(), text.begin(), [](unsigned char symbol) {
        return static_cast<char>(tolower(symbol));
    });
    return text;
}

bool parseStatus(const string& text) {
    const string status = toLower(trim(text));
    return status == "on" || status == "1" || status == "active";
}

bool parseScheduleType(const string& text, ScheduleType& type) {
    const string value = toLower(trim(text));

    if (value == "once" || value == "1") {
        type = ScheduleType::Once;
        return true;
    }

    if (value == "daily" || value == "2") {
        type = ScheduleType::Daily;
        return true;
    }

    if (value == "interval" || value == "3") {
        type = ScheduleType::Interval;
        return true;
    }

    return false;
}

bool isInteger(const string& text) {
    if (text.empty()) {
        return false;
    }

    return all_of(text.begin(), text.end(), [](unsigned char symbol) {
        return isdigit(symbol) != 0;
    });
}

string joinParts(const vector<string>& parts, size_t startIndex, char delimiter) {
    string result;

    for (size_t i = startIndex; i < parts.size(); ++i) {
        if (i > startIndex) {
            result += delimiter;
        }
        result += parts[i];
    }

    return trim(result);
}

int nextTaskId(const vector<Task>& tasks) {
    int maxId = 0;
    for (const Task& task : tasks) {
        maxId = max(maxId, task.id);
    }
    return maxId + 1;
}

void saveTasks(const vector<Task>& tasks) {
    ofstream file(TASKS_FILE);

    file << "# Format: type|status|schedule|name|command\n";
    file << "# type: once, daily, interval\n";
    file << "# examples:\n";
    file << "# once|on|2026-05-22 18:30|Notepad|notepad.exe\n";
    file << "# daily|on|09:00|Document|\"C:\\Users\\AKSI\\Desktop\\file.docx\"\n";
    file << "# interval|on|10|Check|notepad.exe\n";

    for (const Task& task : tasks) {
        string schedule;

        if (task.type == ScheduleType::Once) {
            schedule = formatDateTime(task.nextRun);
        } else if (task.type == ScheduleType::Daily) {
            schedule = formatTime(task.nextRun);
        } else {
            schedule = to_string(task.intervalMinutes);
        }

        file << typeToFileString(task.type) << '|'
             << (task.active ? "on" : "off") << '|'
             << schedule << '|'
             << task.name << '|'
             << task.command << '\n';
    }
}

bool parsePipeTask(const string& line, Task& task) {
    vector<string> parts = split(line, '|');
    if (parts.size() < 4) {
        return false;
    }

    size_t scheduleIndex = 1;
    size_t nameIndex = 2;
    size_t commandIndex = 3;
    task.active = true;

    if (parts.size() >= 5) {
        task.active = parseStatus(parts[1]);
        scheduleIndex = 2;
        nameIndex = 3;
        commandIndex = 4;
    }

    if (!parseScheduleType(parts[0], task.type)) {
        return false;
    }

    const string schedule = trim(parts[scheduleIndex]);
    task.name = trim(parts[nameIndex]);
    task.command = joinParts(parts, commandIndex, '|');

    if (task.name.empty() || task.command.empty()) {
        return false;
    }

    bool ok = false;
    if (task.type == ScheduleType::Once) {
        task.nextRun = parseDateTime(schedule, ok);
    } else if (task.type == ScheduleType::Daily) {
        task.nextRun = todayAt(schedule, ok);
    } else {
        ok = isInteger(schedule) && stoi(schedule) > 0;
        if (ok) {
            task.intervalMinutes = stoi(schedule);
            task.nextRun = chrono::system_clock::now() + chrono::minutes(task.intervalMinutes);
        }
    }

    return ok;
}

bool parseLegacyTask(const string& line, Task& task) {
    vector<string> parts = split(line, '\t');
    if (parts.size() < 7) {
        return false;
    }

    task.id = stoi(parts[0]);
    task.type = static_cast<ScheduleType>(stoi(parts[1]));
    task.active = stoi(parts[2]) != 0;
    task.intervalMinutes = stoi(parts[3]);
    task.nextRun = chrono::system_clock::from_time_t(static_cast<time_t>(stoll(parts[4])));
    task.name = parts[5];
    task.command = parts[6];
    return true;
}

vector<Task> loadTasks() {
    vector<Task> tasks;
    ifstream file(TASKS_FILE);
    string line;

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        Task task;
        bool loaded = false;

        try {
            if (line.find('|') != string::npos) {
                loaded = parsePipeTask(line, task);
            } else {
                loaded = parseLegacyTask(line, task);
            }
        } catch (...) {
            loaded = false;
        }

        if (loaded) {
            if (task.id == 0) {
                task.id = nextTaskId(tasks);
            }
            tasks.push_back(task);
        }
    }

    return tasks;
}

void listTasks(const vector<Task>& tasks) {
    if (tasks.empty()) {
        cout << "Список задач пуст.\n";
        return;
    }

    cout << left << setw(6) << "ID" << "  "
         << setw(16) << "Тип" << "  "
         << setw(12) << "Статус" << "  "
         << setw(24) << "Следующий запуск" << "  "
         << "Название / команда\n";
    cout << string(108, '-') << '\n';

    for (const Task& task : tasks) {
        cout << left << setw(6) << task.id << "  "
             << setw(16) << typeToString(task.type) << "  "
             << setw(12) << (task.active ? "on" : "off") << "  "
             << setw(24) << formatDateTime(task.nextRun) << "  "
             << task.name << " -> " << task.command << '\n';
    }
}

string readLine(const string& prompt) {
    cout << prompt;
    string value;
    getline(cin, value);
    return trim(value);
}

int readMenuNumber(const string& prompt) {
    while (true) {
        const string text = readLine(prompt);
        if (isInteger(text)) {
            return stoi(text);
        }
        cout << "Введите число.\n";
    }
}

void addTask(vector<Task>& tasks) {
    Task task;
    task.id = nextTaskId(tasks);

    task.name = readLine("Название задачи: ");
    while (task.name.empty()) {
        cout << "Название не может быть пустым.\n";
        task.name = readLine("Название задачи: ");
    }

    task.command = readLine("Команда для запуска (например, notepad.exe): ");
    while (task.command.empty()) {
        cout << "Команда не может быть пустой.\n";
        task.command = readLine("Команда для запуска: ");
    }

    cout << "\nТип расписания:\n";
    cout << "1. Один раз в конкретную дату и время\n";
    cout << "2. Каждый день в указанное время\n";
    cout << "3. Каждые N минут\n";

    int choice = readMenuNumber("Ваш выбор: ");
    while (choice < 1 || choice > 3) {
        cout << "Выберите пункт от 1 до 3.\n";
        choice = readMenuNumber("Ваш выбор: ");
    }

    task.type = static_cast<ScheduleType>(choice);
    bool ok = false;

    if (task.type == ScheduleType::Once) {
        while (!ok) {
            const string text = readLine("Дата и время (YYYY-MM-DD HH:MM): ");
            task.nextRun = parseDateTime(text, ok);
            if (!ok) {
                cout << "Неверный формат даты.\n";
            }
        }
    } else if (task.type == ScheduleType::Daily) {
        while (!ok) {
            const string text = readLine("Время каждого дня (HH:MM): ");
            task.nextRun = todayAt(text, ok);
            if (!ok) {
                cout << "Неверный формат времени.\n";
            }
        }
    } else {
        int minutes = readMenuNumber("Интервал в минутах: ");
        while (minutes <= 0) {
            cout << "Интервал должен быть больше нуля.\n";
            minutes = readMenuNumber("Интервал в минутах: ");
        }

        task.intervalMinutes = minutes;
        task.nextRun = chrono::system_clock::now() + chrono::minutes(minutes);
    }

    tasks.push_back(task);
    saveTasks(tasks);
    cout << "Задача добавлена. ID = " << task.id << "\n";
}

void deleteTask(vector<Task>& tasks) {
    const int id = readMenuNumber("ID задачи для удаления: ");
    const auto oldSize = tasks.size();

    tasks.erase(remove_if(tasks.begin(), tasks.end(), [id](const Task& task) {
        return task.id == id;
    }), tasks.end());

    if (tasks.size() == oldSize) {
        cout << "Задача с таким ID не найдена.\n";
        return;
    }

    saveTasks(tasks);
    cout << "Задача удалена.\n";
}

void toggleTask(vector<Task>& tasks) {
    const int id = readMenuNumber("ID задачи: ");

    for (Task& task : tasks) {
        if (task.id == id) {
            task.active = !task.active;
            saveTasks(tasks);
            cout << "Новый статус: " << (task.active ? "on" : "off") << "\n";
            return;
        }
    }

    cout << "Задача с таким ID не найдена.\n";
}

void runCommand(const Task& task) {
    cout << "\n[" << formatDateTime(chrono::system_clock::now())
         << "] Запуск: " << task.name << " -> " << task.command << "\n";

#ifdef _WIN32
    const string command = "start \"\" " + task.command;
#else
    const string command = task.command + " &";
#endif
    system(command.c_str());
}

void updateNextRun(Task& task) {
    if (task.type == ScheduleType::Once) {
        task.active = false;
    } else if (task.type == ScheduleType::Daily) {
        do {
            task.nextRun += chrono::hours(24);
        } while (task.nextRun <= chrono::system_clock::now());
    } else if (task.type == ScheduleType::Interval) {
        do {
            task.nextRun += chrono::minutes(task.intervalMinutes);
        } while (task.nextRun <= chrono::system_clock::now());
    }
}

void schedulerLoop(vector<Task>& tasks) {
    cout << "Планировщик запущен. Для остановки нажмите Ctrl+C.\n";

    while (true) {
        const auto now = chrono::system_clock::now();
        bool changed = false;

        for (Task& task : tasks) {
            if (task.active && task.nextRun <= now) {
                runCommand(task);
                updateNextRun(task);
                changed = true;
            }
        }

        if (changed) {
            saveTasks(tasks);
        }

#ifdef _WIN32
        Sleep(5000);
#else
        sleep(5);
#endif
    }
}

void printMenu() {
    cout << "\n=== Mini Cron ===\n";
    cout << "1. Показать задачи\n";
    cout << "2. Добавить задачу\n";
    cout << "3. Удалить задачу\n";
    cout << "4. Включить/выключить задачу\n";
    cout << "5. Запустить планировщик\n";
    cout << "0. Выход\n";
}

int main() {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    vector<Task> tasks = loadTasks();

    while (true) {
        printMenu();
        const int choice = readMenuNumber("Ваш выбор: ");

        switch (choice) {
        case 1:
            listTasks(tasks);
            break;
        case 2:
            addTask(tasks);
            break;
        case 3:
            deleteTask(tasks);
            break;
        case 4:
            toggleTask(tasks);
            break;
        case 5:
            schedulerLoop(tasks);
            break;
        case 0:
            return 0;
        default:
            cout << "Такого пункта нет.\n";
            break;
        }
    }
}
