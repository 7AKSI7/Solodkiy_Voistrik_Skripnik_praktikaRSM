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

bool isInteger(const string& text) {
    if (text.empty()) {
        return false;
    }

    return all_of(text.begin(), text.end(), [](unsigned char symbol) {
        return isdigit(symbol) != 0;
    });
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

    for (const Task& task : tasks) {
        file << task.id << '\t'
             << static_cast<int>(task.type) << '\t'
             << task.active << '\t'
             << task.intervalMinutes << '\t'
             << toTimeT(task.nextRun) << '\t'
             << task.name << '\t'
             << task.command << '\n';
    }
}

vector<Task> loadTasks() {
    vector<Task> tasks;
    ifstream file(TASKS_FILE);
    string line;

    while (getline(file, line)) {
        vector<string> parts = split(line, '\t');
        if (parts.size() < 7) {
            continue;
        }

        Task task;
        task.id = stoi(parts[0]);
        task.type = static_cast<ScheduleType>(stoi(parts[1]));
        task.active = stoi(parts[2]) != 0;
        task.intervalMinutes = stoi(parts[3]);
        task.nextRun = chrono::system_clock::from_time_t(static_cast<time_t>(stoll(parts[4])));
        task.name = parts[5];
        task.command = parts[6];

        tasks.push_back(task);
    }

    return tasks;
}

void listTasks(const vector<Task>& tasks) {
    if (tasks.empty()) {
        cout << "Список задач пуст.\n";
        return;
    }

    cout << left << setw(5) << "ID"
         << setw(12) << "Тип"
         << setw(8) << "Статус"
         << setw(18) << "Следующий запуск"
         << "Название / команда\n";
    cout << string(85, '-') << '\n';

    for (const Task& task : tasks) {
        cout << left << setw(5) << task.id
             << setw(12) << typeToString(task.type)
             << setw(8) << (task.active ? "on" : "off")
             << setw(18) << formatDateTime(task.nextRun)
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
