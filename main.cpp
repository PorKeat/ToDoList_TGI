#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <limits>
#include <filesystem>
#include <ctime>

using namespace std;

// reg add HKEY_CURRENT_USER\Console /v VirtualTerminalLevel /t REG_DWORD /d 1
// ANSI color codes for styling
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

struct Date {
    int day, month, year;

    Date(int d = 1, int m = 1, int y = 1970) : day(d), month(m), year(y) {}

    static Date fromString(const string& dateStr) {
        Date date;
        if (dateStr.size() != 10 || dateStr[2] != '-' || dateStr[5] != '-') {
            return Date(0, 0, 0); // Invalid date
        }

        try {
            date.day = stoi(dateStr.substr(0, 2));
            date.month = stoi(dateStr.substr(3, 2));
            date.year = stoi(dateStr.substr(6, 4));
        } catch (...) {
            return Date(0, 0, 0); // Invalid date
        }
        return date;
    }

    bool isValid() const {
        if (month < 1 || month > 12 || day < 1 || year < 1970 || year > 9999) {
            return false;
        }

        int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
            daysInMonth[1] = 29;
        }
        return day <= daysInMonth[month - 1];
    }

    bool isOverdue() const {
        time_t now = time(nullptr);
        tm* current = localtime(&now);
        Date today(current->tm_mday, current->tm_mon + 1, current->tm_year + 1900);

        if (year < today.year) return true;
        if (year > today.year) return false;
        if (month < today.month) return true;
        if (month > today.month) return false;
        return day < today.day;
    }

    string toString() const {
        ostringstream oss;
        oss << setfill('0') << setw(2) << day << "-"
            << setw(2) << month << "-" << year;
        return oss.str();
    }
};

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    if (first == string::npos || last == string::npos) return "";
    return str.substr(first, last - first + 1);
}

bool parseInt(const string& input, int& result) {
    try {
        size_t pos;
        result = stoi(input, &pos);
        if (pos != input.size()) return false;
        return true;
    } catch (...) {
        return false;
    }
}

bool isValidDueDate(const string& dueDate) {
    Date date = Date::fromString(dueDate);
    return date.isValid();
}

bool isValidPriority(int priority) {
    return priority >= 1 && priority <= 5;
}

struct Task {
    int id;
    string name;
    int priority;
    string dueDate;
    bool done;
    string category;
    string owner;

    Task(int _id, const string& _name, int _priority, const string& _dueDate, bool _done = false,
         const string& _category = "General", const string& _owner = "")
        : id(_id), name(_name), priority(_priority), dueDate(_dueDate), done(_done),
          category(_category), owner(_owner) {}
};

class ToDoList {
private:
    vector<Task> tasks;
    int nextId;
    string currentUser;
    bool isAdmin;
    string taskFileName;

    string getTaskFileName(const string& user = "") {
        string sanitizedUser = user.empty() ? currentUser : user;
        replace(sanitizedUser.begin(), sanitizedUser.end(), ' ', '_');
        return "tasks_" + sanitizedUser + ".txt";
    }

    void saveToFile(const string& user = "") {
        string fileName = getTaskFileName(user.empty() ? currentUser : user);
        ofstream outFile(fileName);
        if (!outFile.is_open()) {
            cout << RED << "[ERROR] Cannot open file for writing: " << fileName << RESET << endl;
            return;
        }

        outFile << "[\n";
        for (size_t i = 0; i < tasks.size(); ++i) {
            const Task& task = tasks[i];
            if (user.empty() || task.owner == user) {
                outFile << "  {";
                outFile << "\"id\":" << task.id << ",";
                outFile << "\"name\":\"" << task.name << "\",";
                outFile << "\"priority\":" << task.priority << ",";
                outFile << "\"dueDate\":\"" << task.dueDate << "\",";
                outFile << "\"done\":" << (task.done ? "true" : "false") << ",";
                outFile << "\"category\":\"" << task.category << "\",";
                outFile << "\"owner\":\"" << task.owner << "\"";
                outFile << "}" << (i < tasks.size() - 1 ? "," : "") << "\n";
            }
        }
        outFile << "]\n";
        outFile.close();
    }

    void loadFromFile(const string& user = "") {
        if (user.empty() && !isAdmin) {
            tasks.clear();
        }
        string fileName = getTaskFileName(user);
        ifstream inFile(fileName);
        if (!inFile.is_open()) {
            if (user.empty()) nextId = 1;
            cout << YELLOW << "[INFO] No task file found for user: " << (user.empty() ? currentUser : user) << RESET << endl;
            return;
        }

        string line, jsonContent;
        while (getline(inFile, line)) {
            jsonContent += line + "\n";
        }
        inFile.close();

        vector<string> taskLines;
        size_t pos = 0;
        while (pos < jsonContent.size()) {
            size_t start = jsonContent.find('{', pos);
            if (start == string::npos) break;
            size_t end = jsonContent.find('}', start);
            if (end == string::npos) break;
            string taskStr = jsonContent.substr(start, end - start + 1);
            taskStr = trim(taskStr);
            if (!taskStr.empty()) taskLines.push_back(taskStr);
            pos = end + 1;
        }

        for (const auto& taskStr : taskLines) {
            int id = 0;
            string name, dueDate, category = "General", owner = user.empty() ? currentUser : user;
            int priority = 0;
            bool done = false;

            auto extractValue = [](const string& str, const string& key) {
                size_t pos = str.find("\"" + key + "\":");
                if (pos == string::npos) return string("");
                pos += key.size() + 3;
                if (str[pos] == '"') {
                    pos++;
                    size_t end = str.find("\"", pos);
                    if (end == string::npos) return string("");
                    return str.substr(pos, end - pos);
                }
                size_t end = str.find(",", pos);
                if (end == string::npos) end = str.find("}", pos);
                return trim(str.substr(pos, end - pos));
            };

            string idStr = extractValue(taskStr, "id");
            if (parseInt(idStr, id)) {
                name = extractValue(taskStr, "name");
                string priorityStr = extractValue(taskStr, "priority");
                parseInt(priorityStr, priority);
                dueDate = extractValue(taskStr, "dueDate");
                string doneStr = extractValue(taskStr, "done");
                done = (doneStr == "true");
                string categoryStr = extractValue(taskStr, "category");
                if (!categoryStr.empty()) category = categoryStr;
                string ownerStr = extractValue(taskStr, "owner");
                if (!ownerStr.empty()) owner = ownerStr;

                if (!name.empty() && isValidDueDate(dueDate) && isValidPriority(priority)) {
                    tasks.push_back(Task(id, name, priority, dueDate, done, category, owner));
                    nextId = max(nextId, id + 1);
                } else {
                    cout << YELLOW << "[WARNING] Skipping invalid task in " << fileName << ": " << taskStr << RESET << endl;
                }
            }
        }
    }

    void loadAllUsersTasks() {
        tasks.clear();
        nextId = 1;
        bool foundFiles = false;
        for (const auto& entry : filesystem::directory_iterator(".")) {
            string fileName = entry.path().filename().string();
            if (fileName.find("tasks_") == 0 && fileName.ends_with(".txt")) {
                string user = fileName.substr(6, fileName.size() - 10);
                loadFromFile(user);
                foundFiles = true;
            }
        }
        if (!foundFiles) {
            cout << YELLOW << "[INFO] No task files found in directory." << RESET << endl;
        }
    }

public:
    ToDoList(const string& user, bool admin = false) : currentUser(user), isAdmin(admin), nextId(1) {
        taskFileName = getTaskFileName();
        if (isAdmin) {
            loadAllUsersTasks();
        } else {
            loadFromFile();
        }
        checkOverdueTasks();
    }

    size_t getTaskCount() const { return tasks.size(); }
    bool getIsAdmin() const { return isAdmin; }
    string getCurrentUser() const { return currentUser; }

    void addTask(const string& name, int priority, const string& dueDate, const string& category = "General") {
        if (name.empty()) {
            cout << RED << "[ERROR] Task name cannot be empty." << RESET << endl;
            return;
        }
        if (!isValidPriority(priority)) {
            cout << RED << "[ERROR] Priority must be between 1 and 5." << RESET << endl;
            return;
        }
        Date date = Date::fromString(dueDate);
        if (!date.isValid()) {
            cout << RED << "[ERROR] Invalid due date format. Use DD-MM-YYYY." << RESET << endl;
            return;
        }

        tasks.push_back(Task(nextId++, name, priority, date.toString(), false, category, currentUser));
        saveToFile();
        cout << GREEN << "[INFO] Task added successfully." << RESET << endl;
    }

    void editTask(int id, const string& name, int priority, const string& dueDate, const string& category) {
        for (Task& task : tasks) {
            if (task.id == id && task.owner == currentUser) {
                if (!name.empty()) task.name = name;
                if (isValidPriority(priority)) task.priority = priority;
                if (!dueDate.empty() && dueDate != "01-01-1970") {
                    Date date = Date::fromString(dueDate);
                    if (date.isValid()) {
                        task.dueDate = date.toString();
                    } else {
                        cout << RED << "[ERROR] Invalid due date format. Use DD-MM-YYYY." << RESET << endl;
                        return;
                    }
                }
                if (!category.empty()) task.category = category;
                saveToFile();
                cout << GREEN << "[INFO] Task ID " << id << " updated successfully." << RESET << endl;
                return;
            }
        }
        cout << RED << "[ERROR] Task not found or you lack permission." << RESET << endl;
    }

    void checkOverdueTasks() {
        vector<Task> overdueTasks;
        for (const Task& task : tasks) {
            if (!task.done && (isAdmin || task.owner == currentUser)) {
                Date date = Date::fromString(task.dueDate);
                if (date.isValid() && date.isOverdue()) {
                    overdueTasks.push_back(task);
                }
            }
        }

        if (!overdueTasks.empty()) {
            cout << YELLOW << "\n[WARNING] You have " << overdueTasks.size() << " overdue task(s):" << RESET << endl;
            cout << CYAN << string(100, '=') << RESET << "\n";
            cout << CYAN << "| " << left << setw(6) << "ID" << setw(26) << "Task Name" << setw(11) << "Priority"
                 << setw(13) << "Due Date" << setw(11) << "Status" << setw(12) << "Category"
                 << setw(18) << (isAdmin ? "Owner" : "") << "|" << RESET << "\n";
            cout << CYAN << string(100, '=') << RESET << "\n";

            for (const Task& task : overdueTasks) {
                string status = YELLOW + string("Overdue") + RESET;
                cout << "| " << left << setw(6) << task.id << setw(26) << task.name.substr(0, 25)
                     << setw(11) << task.priority << setw(13) << task.dueDate
                     << setw(20) << status << setw(12) << task.category.substr(0, 11)
                     << setw(18) << (isAdmin ? task.owner.substr(0, 14) : "") << "|\n";
            }
            cout << CYAN << string(100, '=') << RESET << "\n";
        }
    }

    void markAsDoneById(int id) {
        for (Task& task : tasks) {
            if (task.id == id && task.owner == currentUser && !task.done) {
                task.done = true;
                saveToFile();
                cout << GREEN << "[INFO] Task ID " << id << " marked as done." << RESET << endl;
                return;
            }
        }
        cout << RED << "[ERROR] Task not found, already done, or you lack permission." << RESET << endl;
    }

    void unmarkTaskById(int id) {
        for (Task& task : tasks) {
            if (task.id == id && task.owner == currentUser && task.done) {
                task.done = false;
                saveToFile();
                cout << GREEN << "[INFO] Task ID " << id << " unmarked as done." << RESET << endl;
                return;
            }
        }
        cout << RED << "[ERROR] Task not found, not done, or you lack permission." << RESET << endl;
    }

    void deleteTaskById(int id) {
        auto it = remove_if(tasks.begin(), tasks.end(), [this, id](const Task& task) {
            return task.id == id && task.owner == currentUser;
        });
        if (it == tasks.end()) {
            cout << RED << "[ERROR] Task not found or you lack permission." << RESET << endl;
            return;
        }
        tasks.erase(it, tasks.end());
        saveToFile();
        cout << GREEN << "[INFO] Task deleted successfully." << RESET << endl;
    }

    void clearAllTask() {
        cout << YELLOW << "[WARNING] Are you sure? [Y/N]: " << RESET;
        char response;
        cin >> response;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');  // Clear input buffer

        if (toupper(response) == 'Y') {
            tasks.clear();
            nextId = 1;
            saveToFile();
            cout << GREEN << "[SUCCESS] All tasks have been cleared successfully!" << RESET << endl;
        } else {
            cout << RED << "[FAILED] Cancel the process" << endl;
        }
    }

    void sortTasks(const string& criterion) {
        if (criterion == "priority") {
            sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
                return a.priority < b.priority;
            });
        } else if (criterion == "date") {
            sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
                return a.dueDate < b.dueDate;
            });
        } else if (criterion == "name") {
            sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
                return a.name < b.name;
            });
        } else if (criterion == "owner" && isAdmin) {
            sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
                return a.owner < b.owner;
            });
        } else {
            cout << RED << "[ERROR] Invalid sort criterion. Use 'priority', 'date', 'name'"
                 << (isAdmin ? ", or 'owner'." : ".") << RESET << endl;
            return;
        }
        if (!isAdmin) saveToFile();
        else for (const auto& task : tasks) saveToFile(task.owner);
        cout << GREEN << "[INFO] Tasks sorted by " << criterion << "." << RESET << endl;
    }

    void showTasks(const string& filter = "all", const string& category = "", const string& owner = "") {
        vector<Task> filteredTasks;
        for (const Task& task : tasks) {
            if ((filter == "all" || (filter == "completed" && task.done) || (filter == "incomplete" && !task.done)) &&
                (category.empty() || task.category == category) &&
                (owner.empty() || task.owner == owner) &&
                (isAdmin || task.owner == currentUser)) {
                filteredTasks.push_back(task);
            }
        }

        if (filteredTasks.empty()) {
            cout << YELLOW << "[INFO] No tasks to show." << RESET << endl;
            return;
        }

        size_t total = isAdmin ? tasks.size() : count_if(tasks.begin(), tasks.end(),
            [this](const Task& t) { return t.owner == currentUser; });
        size_t completed = count_if(tasks.begin(), tasks.end(),
            [this](const Task& t) { return t.done && (isAdmin || t.owner == currentUser); });
        double progress = total > 0 ? (static_cast<double>(completed) / total) * 100 : 0;

        cout << "\n" << CYAN << string(100, '=') << RESET << "\n";
        cout << CYAN << "| " << left << setw(6) << "ID" << setw(26) << "Task Name" << setw(11) << "Priority"
             << setw(13) << "Due Date" << setw(11) << "Status" << setw(12) << "Category"
             << setw(18) << (isAdmin ? "Owner" : "") << "|" << RESET << "\n";
        cout << CYAN << string(100, '=') << RESET << "\n";

        for (const Task& task : filteredTasks) {
            string status;
            if (task.done) {
                status = GREEN + string("Done") + RESET;
            } else {
                Date date = Date::fromString(task.dueDate);
                status = date.isValid() && date.isOverdue() ? (YELLOW + string("Overdue") + RESET) : (YELLOW + string("Not Done") + RESET);
            }
            cout << "| " << left << setw(6) << task.id << setw(26) << task.name.substr(0, 25)
                 << setw(11) << task.priority << setw(13) << task.dueDate
                 << setw(20) << status << setw(12) << task.category.substr(0, 11)
                 << setw(18) << (isAdmin ? task.owner.substr(0, 14) : "") << "|\n";
        }

        cout << CYAN << string(100, '=') << RESET << "\n";
        cout << BLUE << "Progress: " << fixed << setprecision(2) << progress << "% completed ("
             << completed << " of " << total << " tasks)" << RESET << "\n";
    }

    void searchTasks(const string& query, const string& owner = "") {
        vector<Task> results;
        string queryLower = query;
        transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

        for (const Task& task : tasks) {
            if (isAdmin || task.owner == currentUser) {
                string nameLower = task.name;
                string categoryLower = task.category;
                transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                transform(categoryLower.begin(), categoryLower.end(), categoryLower.begin(), ::tolower);

                // Contains search in name or category
                if (nameLower.find(queryLower) != string::npos ||
                    categoryLower.find(queryLower) != string::npos) {
                    if (owner.empty() || task.owner == owner) {
                        results.push_back(task);
                    }
                }
            }
        }


        if (results.empty()) {
            cout << YELLOW << "[INFO] No tasks match the query '" << query << "'." << RESET << endl;
            return;
        }

        cout << "\n" << CYAN << string(100, '=') << RESET << "\n";
        cout << CYAN << "| " << left << setw(6) << "ID" << setw(26) << "Task Name" << setw(11) << "Priority"
             << setw(13) << "Due Date" << setw(11) << "Status" << setw(12) << "Category"
             << setw(18) << (isAdmin ? "Owner" : "") << "|" << RESET << "\n";
        cout << CYAN << string(100, '=') << RESET << "\n";

        for (const Task& task : results) {
            string status;
            if (task.done) {
                status = GREEN + string("Done") + RESET;
            } else {
                Date date = Date::fromString(task.dueDate);
                status = date.isValid() && date.isOverdue() ? (YELLOW + string("Overdue") + RESET) : (YELLOW + string("Not Done") + RESET);
            }
            cout << "|" << left << setw(6) << task.id << setw(26) << task.name.substr(0, 25)
                 << setw(11) << task.priority << setw(14) << task.dueDate
                 << setw(20) << status << setw(12) << task.category.substr(0, 11)
                 << setw(18) << (isAdmin ? task.owner.substr(0, 14) : "") << "|\n";
        }

        cout << CYAN << string(100, '=') << RESET << "\n";
    }

    void listAllUsers() {
        ifstream inFile("users.txt");
        if (!inFile.is_open()) {
            cout << RED << "[ERROR] Cannot open users file." << RESET << endl;
            return;
        }

        vector<string> users;
        string line;
        while (getline(inFile, line)) {
            line = trim(line);
            if (line.empty()) continue;

            size_t commaPos = line.find(',');
            if (commaPos == string::npos) continue;

            string username = trim(line.substr(0, commaPos));
            if (username.front() == '"' && username.back() == '"') {
                username = username.substr(1, username.size() - 2);
            }

            if (username != "admin") users.push_back(username);
        }
        inFile.close();

        if (users.empty()) {
            cout << YELLOW << "[INFO] No users found." << RESET << endl;
            return;
        }

        cout << "\n" << CYAN << string(30, '=') << RESET << "\n";
        cout << CYAN << "|" << left << setw(28) << "Username" << "|" << RESET << "\n";
        cout << CYAN << string(30, '=') << RESET << "\n";
        for (const auto& user : users) {
            cout << "|" << left << setw(28) << user << "|\n";
        }
        cout << CYAN << string(30, '=') << RESET << "\n";
    }

    void removeUser(const string& username) {
        if (username == "admin") {
            cout << RED << "[ERROR] Cannot remove the admin account." << RESET << endl;
            return;
        }

        ifstream inFile("users.txt");
        if (!inFile.is_open()) {
            cout << RED << "[ERROR] Cannot open users file." << RESET << endl;
            return;
        }

        vector<string> lines;
        string line;
        bool found = false;
        while (getline(inFile, line)) {
            line = trim(line);
            if (line.empty()) continue;

            size_t commaPos = line.find(',');
            if (commaPos == string::npos) continue;

            string storedUser = trim(line.substr(0, commaPos));
            if (storedUser.front() == '"' && storedUser.back() == '"') {
                storedUser = storedUser.substr(1, storedUser.size() - 2);
            }

            if (storedUser != username) {
                lines.push_back(line);
            } else {
                found = true;
            }
        }
        inFile.close();

        if (!found) {
            cout << RED << "[ERROR] User '" << username << "' not found." << RESET << endl;
            return;
        }

        ofstream outFile("users.txt");
        if (!outFile.is_open()) {
            cout << RED << "[ERROR] Cannot open users file for writing." << RESET << endl;
            return;
        }
        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        outFile.close();

        string taskFile = "tasks_" + username + ".txt";
        if (filesystem::exists(taskFile)) {
            filesystem::remove(taskFile);
        }

        tasks.erase(
            remove_if(tasks.begin(), tasks.end(),
                [username](const Task& task) { return task.owner == username; }),
            tasks.end()
        );

        cout << GREEN << "[INFO] User '" << username << "' and their tasks removed successfully." << RESET << endl;
    }
};

bool userExists(const string& username) {
    string trimmedUsername = trim(username);
    ifstream inFile("users.txt");
    if (!inFile.is_open()) return false;

    string line;
    while (getline(inFile, line)) {
        line = trim(line);
        if (line.empty()) continue;

        size_t commaPos = line.find(',');
        if (commaPos == string::npos) continue;

        string storedUser = trim(line.substr(0, commaPos));
        if (storedUser.front() == '"' && storedUser.back() == '"') {
            storedUser = storedUser.substr(1, storedUser.size() - 2);
        }

        if (storedUser == trimmedUsername) {
            inFile.close();
            return true;
        }
    }
    inFile.close();
    return false;
}

bool registerUser(const string& username, const string& password) {
    string trimmedUsername = trim(username);
    string trimmedPassword = trim(password);

    if (trimmedUsername.empty() || trimmedPassword.empty()) {
        cout << RED << "[ERROR] Username and password cannot be empty." << RESET << endl;
        return false;
    }

    if (userExists(trimmedUsername)) {
        cout << RED << "[ERROR] Username already exists." << RESET << endl;
        return false;
    }

    ofstream outFile("users.txt", ios::app);
    if (!outFile.is_open()) {
        cout << RED << "[ERROR] Cannot open users file for writing." << RESET << endl;
        return false;
    }

    string formattedUsername = trimmedUsername;
    if (trimmedUsername.find(' ') != string::npos) {
        formattedUsername = "\"" + trimmedUsername + "\"";
    }

    outFile << formattedUsername << "," << trimmedPassword << "\n";
    outFile.close();
    cout << GREEN << "[INFO] Registration successful." << RESET << endl;
    return true;
}

bool loginUser(const string& username, const string& password) {
    string trimmedUsername = trim(username);
    string trimmedPassword = trim(password);

    if (trimmedUsername == "admin" && trimmedPassword == "admin123") {
        cout << GREEN << "[SUCCESS] Admin login successful." << RESET << endl;
        return true;
    }

    if (trimmedUsername.empty() || trimmedPassword.empty()) {
        cout << RED << "[ERROR] Username and password cannot be empty." << RESET << endl;
        return false;
    }

    ifstream inFile("users.txt");
    if (!inFile.is_open()) {
        cout << RED << "[ERROR] Cannot open users file. Admin login still available." << RESET << endl;
        return false;
    }

    string line;
    while (getline(inFile, line)) {
        line = trim(line);
        if (line.empty()) continue;

        size_t commaPos = line.find(',');
        if (commaPos == string::npos) continue;

        string storedUser = trim(line.substr(0, commaPos));
        string storedPass = trim(line.substr(commaPos + 1));

        if (storedUser.front() == '"' && storedUser.back() == '"') {
            storedUser = storedUser.substr(1, storedUser.size() - 2);
        }

        if (storedUser == trimmedUsername && storedPass == trimmedPassword) {
            inFile.close();
            cout << GREEN << "[SUCCESS] User login successful." << RESET << endl;
            return true;
        }
    }
    inFile.close();
    cout << RED << "[ERROR] User login failed." << RESET << endl;
    return false;
}

void runToDoApp(ToDoList& todo) {
    string choice, name, dueDate, category, query, sortCriterion, owner, input;
    int id, priority;

    vector<pair<string, string>> userMenuOptions = {
        {"1", "Add Task"}, {"2", "Edit Task"}, {"3", "Delete Task"},
        {"4", "Mark Task as Done"}, {"5", "Unmark Task"}, {"6", "View All Tasks"},
        {"7", "View Completed Tasks"}, {"8", "View Incomplete Tasks"},
        {"9", "Sort Tasks"}, {"10", "Search Tasks"}, {"11", "Filter by Category"},
        {"12", "Clear All Tasks"},{"13", "Logout"}
    };

    vector<pair<string, string>> adminMenuOptions = {
        {"1", "View All Tasks"}, {"2", "View Completed Tasks"}, {"3", "View Incomplete Tasks"},
        {"4", "Sort Tasks"}, {"5", "Search Tasks"}, {"6", "Filter by Category"},
        {"7", "List All Users"}, {"8", "Remove User"}, {"9", "Clear All Tasks"},{"10", "Logout"}
    };

    const auto& menuOptions = todo.getIsAdmin() ? adminMenuOptions : userMenuOptions;

    while (true) {
        cout << "\n" << CYAN << string(60, '=') << RESET << "\n";
        cout << CYAN << "|" << BOLD << setw(58) << left << " To-Do List Menu" << RESET << CYAN << "|" << RESET << "\n";
        cout << CYAN << string(60, '=') << RESET << "\n";

        for (size_t i = 0; i < menuOptions.size(); i += 2) {
            cout << CYAN << "| " << left << setw(5) << menuOptions[i].first << RESET << setw(25) << menuOptions[i].second;
            if (i + 1 < menuOptions.size()) {
                cout << CYAN << setw(5) << menuOptions[i + 1].first << RESET << setw(22) << menuOptions[i + 1].second;
            } else {
                cout << string(27, ' ');
            }
            cout << CYAN << "|" << RESET << "\n";
        }
        cout << CYAN << string(60, '=') << RESET << "\n";
        cout << BLUE << "Enter your choice: " << RESET;
        getline(cin, choice);

        if (todo.getIsAdmin()) {
            if (choice == "1") {
                cout << BLUE << "Owner (leave blank for all users): " << RESET;
                getline(cin, owner);
                todo.showTasks("all", "", owner);
            } else if (choice == "2") {
                cout << BLUE << "Owner (leave blank for all users): " << RESET;
                getline(cin, owner);
                todo.showTasks("completed", "", owner);
            } else if (choice == "3") {
                cout << BLUE << "Owner (leave blank for all users): " << RESET;
                getline(cin, owner);
                todo.showTasks("incomplete", "", owner);
            } else if (choice == "4") {
                cout << BLUE << "Sort by (priority, date, name, owner): " << RESET;
                getline(cin, sortCriterion);
                todo.sortTasks(sortCriterion);
            } else if (choice == "5") {
                cout << BLUE << "Search task: " << RESET;
                getline(cin, query);
                cout << BLUE << "Owner (leave blank for all users): " << RESET;
                getline(cin, owner);
                todo.searchTasks(query, owner);
            } else if (choice == "6") {
                cout << BLUE << "Category to filter (e.g., Work, Personal, General): " << RESET;
                getline(cin, category);
                cout << BLUE << "Owner (leave blank for all users): " << RESET;
                getline(cin, owner);
                todo.showTasks("all", category, owner);
            } else if (choice == "7") {
                todo.listAllUsers();
            } else if (choice == "8") {
                todo.listAllUsers();
                cout << BLUE << "Username to remove: " << RESET;
                getline(cin, owner);
                todo.removeUser(owner);
            } else if (choice == "9"){
                todo.clearAllTask();
            } else if (choice == "10") {
                cout << GREEN << "[INFO] Logged out successfully." << RESET << endl;
                break;
            } else {
                cout << RED << "[ERROR] Invalid choice. Please select a valid option." << RESET << endl;
            }
        } else {
            if (choice == "1") {
                cout << BLUE << "Task Name: " << RESET;
                getline(cin, name);
                cout << BLUE << "Priority (1-5): " << RESET;
                getline(cin, input);
                if (!parseInt(input, priority) || !isValidPriority(priority)) {
                    cout << RED << "[ERROR] Invalid priority. Must be between 1 and 5." << RESET << endl;
                    continue;
                }
                cout << BLUE << "Due Date (DD-MM-YYYY): " << RESET;
                getline(cin, dueDate);
                cout << BLUE << "Category (e.g., Work, Personal, General): " << RESET;
                getline(cin, category);
                if (category.empty()) category = "General";
                todo.addTask(name, priority, dueDate, category);
            } else if (choice == "2") {
                todo.showTasks();
                cout << BLUE << "Task ID: " << RESET;
                getline(cin, input);
                if (!parseInt(input, id)) {
                    cout << RED << "[ERROR] Invalid ID." << RESET << endl;
                    continue;
                }
                cout << BLUE << "New Task Name (leave blank to keep unchanged): " << RESET;
                getline(cin, name);
                cout << BLUE << "New Priority (1-5, enter 0 to keep unchanged): " << RESET;
                getline(cin, input);
                if (!parseInt(input, priority)) {
                    cout << RED << "[ERROR] Invalid priority." << RESET << endl;
                    continue;
                }
                cout << BLUE << "New Due Date (DD-MM-YYYY, leave blank to keep unchanged): " << RESET;
                getline(cin, dueDate);
                if (dueDate.empty()) dueDate = "01-01-1970";
                cout << BLUE << "New Category (leave blank to keep unchanged): " << RESET;
                getline(cin, category);
                todo.editTask(id, name, priority, dueDate, category);
            } else if (choice == "3") {
                todo.showTasks();
                cout << BLUE << "Task ID: " << RESET;
                getline(cin, input);
                if (!parseInt(input, id)) {
                    cout << RED << "[ERROR] Invalid ID." << RESET << endl;
                    continue;
                }
                todo.deleteTaskById(id);
            } else if (choice == "4") {
                todo.showTasks("incomplete");
                cout << BLUE << "Task ID: " << RESET;
                getline(cin, input);
                if (!parseInt(input, id)) {
                    cout << RED << "[ERROR] Invalid ID." << RESET << endl;
                    continue;
                }
                todo.markAsDoneById(id);
            } else if (choice == "5") {
                todo.showTasks("completed");
                cout << BLUE << "Task ID: " << RESET;
                getline(cin, input);
                if (!parseInt(input, id)) {
                    cout << RED << "[ERROR] Invalid ID." << RESET << endl;
                    continue;
                }
                todo.unmarkTaskById(id);
            } else if (choice == "6") {
                todo.showTasks("all");
            } else if (choice == "7") {
                todo.showTasks("completed");
            } else if (choice == "8") {
                todo.showTasks("incomplete");
            } else if (choice == "9") {
                todo.showTasks();
                cout << BLUE << "Sort by (priority, date, name): " << RESET;
                getline(cin, sortCriterion);
                todo.sortTasks(sortCriterion);
            } else if (choice == "10") {
                cout << BLUE << "Search task: " << RESET;
                getline(cin, query);
                todo.searchTasks(query);
            } else if (choice == "11") {
                cout << BLUE << "Category to filter (e.g., Work, Personal, General): " << RESET;
                getline(cin, category);
                todo.showTasks("all", category);
            }else if (choice == "12"){
                todo.clearAllTask();
            } else if (choice == "13") {
                cout << GREEN << "[INFO] Logged out successfully." << RESET << endl;
                break;
            } else {
                cout << RED << "[ERROR] Invalid choice. Please select a valid option." << RESET << endl;
            }
        }
    }
}

void showAuthMenu() {
    string choice, username, password;
    vector<pair<string, string>> authOptions = {
        {"1", "Login"}, {"2", "Register"}, {"3", "Exit"}
    };

    while (true) {
        cout << "\n" << CYAN << string(50, '=') << RESET << "\n";
        for (size_t i = 0; i < authOptions.size(); ++i) {
            cout << CYAN << "| " << left << setw(5) << authOptions[i].first << RESET << setw(42) << authOptions[i].second << CYAN << "|" << RESET << "\n";
        }
        cout << CYAN << string(50, '=') << RESET << "\n";
        cout << BLUE << "Enter your choice: " << RESET;
        getline(cin, choice);

        if (choice == "1") {
            cout << BLUE << "Username: " << RESET;
            getline(cin, username);
            cout << BLUE << "Password: " << RESET;
            getline(cin, password);
            if (loginUser(username, password)) {
                ToDoList todo(trim(username), trim(username) == "admin");
                runToDoApp(todo);
            }
        } else if (choice == "2") {
            cout << BLUE << "Username: " << RESET;
            getline(cin, username);
            cout << BLUE << "Password: " << RESET;
            getline(cin, password);
            registerUser(trim(username), trim(password));
        } else if (choice == "3") {
            cout << GREEN << "[INFO] Goodbye! Thank you for using the To-Do List App." << RESET << endl;
            break;
        } else {
            cout << RED << "[ERROR] Invalid choice. Please select 1, 2, or 3." << RESET << endl;
        }
    }
}

void greeting() {
    cout << "\n" << CYAN << string(50, '=') << RESET << "\n";
    cout << CYAN << "|" << BOLD << setw(48) << left << " Welcome to the To-Do List App" << RESET << CYAN << "|" << RESET << "\n";
    cout << CYAN << string(50, '=') << RESET << "\n";
}

int main() {
    greeting();
    showAuthMenu();
    return 0;
}

string simpleHash(const string& input) {
    return input;
}