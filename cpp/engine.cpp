#include <iostream>
#include <string>
#include <limits>
#include <stdexcept>
using namespace std;

// ==================================================
// PATIENT: stores one patient's details
// ==================================================
class Patient {
private:
    int id;
    string name;
    int age;

public:
    Patient(int patientId, string patientName, int patientAge)
        : id(patientId), name(patientName), age(patientAge) {}

    int getId() const {
        return id;
    }

    string getName() const {
        return name;
    }

    void display() const {
        cout << "Token: " << id
             << " | Name: " << name
             << " | Age: " << age << '\n';
    }
};

// ==================================================
// 1. LINKED LIST: stores all registered patients
// ==================================================
class PatientList {
private:
    struct Node {
        Patient data;
        Node* next;

        Node(int id, string name, int age)
            : data(id, name, age), next(nullptr) {}
    };

    Node* head;
    Node* tail;

public:
    PatientList() : head(nullptr), tail(nullptr) {}

    // This class owns allocated nodes, so prevent copying.
    PatientList(const PatientList&) = delete;
    PatientList& operator=(const PatientList&) = delete;

    void addPatient(int id, string name, int age) {
        Node* newNode = new Node(id, name, age);

        if (head == nullptr) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
    }

    void displayAll() const {
        if (head == nullptr) {
            cout << "No patients registered.\n";
            return;
        }

        Node* current = head;

        while (current != nullptr) {
            current->data.display();
            current = current->next;
        }
    }

    // ==============================================
    // 2. SEARCHING: linear search through the list
    // Function overloading: same name, different inputs
    // ==============================================
    const Patient* searchPatient(int token) const {
        Node* current = head;

        while (current != nullptr) {
            if (current->data.getId() == token) {
                return &current->data;
            }

            current = current->next;
        }

        return nullptr;
    }

    const Patient* searchPatient(string name) const {
        Node* current = head;

        while (current != nullptr) {
            if (current->data.getName() == name) {
                return &current->data;
            }

            current = current->next;
        }

        return nullptr;
    }

    ~PatientList() {
        while (head != nullptr) {
            Node* oldNode = head;
            head = head->next;
            delete oldNode;
        }
    }
};

// ==================================================
// 3. QUEUE: waiting patients, first in first out
// ==================================================
class ConsultationQueue {
private:
    static const int CAPACITY = 100;
    int tokens[CAPACITY];
    int frontIndex;
    int rearIndex;
    int count;

public:
    ConsultationQueue()
        : frontIndex(0), rearIndex(0), count(0) {}

    bool isFull() const {
        return count == CAPACITY;
    }

    int size() const {
        return count;
    }

    bool enqueue(int token) {
        if (isFull()) {
            return false;
        }

        tokens[rearIndex] = token;
        rearIndex = (rearIndex + 1) % CAPACITY;
        count++;
        return true;
    }

    int dequeue() {
        if (count == 0) {
            return -1;
        }

        int token = tokens[frontIndex];
        frontIndex = (frontIndex + 1) % CAPACITY;
        count--;
        return token;
    }

    // Special operation used only to undo a call.
    // Restore the patient at the FRONT, not the rear.
    bool restoreFront(int token) {
        if (isFull()) {
            return false;
        }

        frontIndex = (frontIndex - 1 + CAPACITY) % CAPACITY;
        tokens[frontIndex] = token;
        count++;
        return true;
    }

    void display() const {
        if (count == 0) {
            cout << "Waiting queue is empty.\n";
            return;
        }

        cout << "Waiting tokens: ";

        for (int i = 0; i < count; i++) {
            int index = (frontIndex + i) % CAPACITY;
            cout << tokens[index] << ' ';
        }

        cout << "\nTotal waiting: " << count << '\n';
    }
};

// ==================================================
// 4. STACK: remembers calls, last in first out
// ==================================================
class ActionStack {
private:
    static const int CAPACITY = 100;
    int tokens[CAPACITY];
    int topIndex;

public:
    ActionStack() : topIndex(-1) {}

    bool isFull() const {
        return topIndex == CAPACITY - 1;
    }

    bool isEmpty() const {
        return topIndex == -1;
    }

    bool push(int token) {
        if (isFull()) {
            return false;
        }

        tokens[++topIndex] = token;
        return true;
    }

    int pop() {
        if (isEmpty()) {
            return -1;
        }

        return tokens[topIndex--];
    }
};

// ==================================================
// INPUT HELPER: handles invalid numeric input
// ==================================================
int readNumber(const string& prompt, int minimum, int maximum) {
    int value;

    while (true) {
        cout << prompt;

        if (cin >> value) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (value >= minimum && value <= maximum) {
                return value;
            }
        } else {
            if (cin.eof()) {
                // Exit cleanly if the input stream is closed.
                throw runtime_error("Input closed.");
            }

            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        cout << "Enter a number from "
             << minimum << " to " << maximum << ".\n";
    }
}

int runBackendMode() {
    PatientList patients;
    ConsultationQueue waiting;
    ActionStack history;

    int nextToken = 1;
    string command;

    while (getline(cin, command)) {
        if (command == "ADD") {
            // ADD is followed by name and age on separate lines.
            string name;
            string ageText;

            if (!getline(cin, name) || !getline(cin, ageText)) {
                break;
            }

            try {
                size_t used;
                int age = stoi(ageText, &used);

                if (used != ageText.size() || age < 0 || age > 120) {
                    cout << "ERROR Invalid age\n";
                } else if (name.find_first_not_of(" \t\r") == string::npos) {
                    cout << "ERROR Name is required\n";
                } else if (waiting.isFull()) {
                    cout << "ERROR Waiting queue is full\n";
                } else {
                    patients.addPatient(nextToken, name, age);
                    waiting.enqueue(nextToken);

                    cout << "OK Registered token " << nextToken << '\n';
                    nextToken++;
                }
            } catch (const invalid_argument&) {
                cout << "ERROR Invalid age\n";
            } catch (const out_of_range&) {
                cout << "ERROR Invalid age\n";
            }
        } else if (command == "LIST") {
            patients.displayAll();

        } else if (command == "WAITING") {
            waiting.display();

        } else if (command == "CALL") {
            if (waiting.size() == 0) {
                cout << "ERROR No patients waiting\n";
            } else if (history.isFull()) {
                cout << "ERROR Call history is full\n";
            } else {
                int token = waiting.dequeue();
                history.push(token);

                cout << "OK Called token " << token << '\n';
                patients.searchPatient(token)->display();
            }

        } else if (command == "UNDO") {
            if (history.isEmpty()) {
                cout << "ERROR No calls to undo\n";
            } else if (waiting.isFull()) {
                cout << "ERROR Waiting queue is full\n";
            } else {
                int token = history.pop();
                waiting.restoreFront(token);

                cout << "OK Restored token " << token << '\n';
            }

        } else if (command == "SEARCH") {
            string name;

            if (!getline(cin, name)) {
                break;
            }

            const Patient* patient = patients.searchPatient(name);

            if (patient != nullptr) {
                patient->display();
            } else {
                cout << "ERROR Patient not found\n";
            }

        } else if (command == "EXIT") {
            cout << "OK Closing\nEND" << endl;
            break;

        } else {
            cout << "ERROR Unknown command\n";
        }

        // endl flushes the response immediately to Python.
        cout << "END" << endl;
    }

    return 0;
}

// ==================================================
// APPLICATION
// ==================================================
int main(int argc, char* argv[]) {
    if (argc > 1 && string(argv[1]) == "--api") {
        return runBackendMode();
    }
    PatientList patients;
    ConsultationQueue waiting;
    ActionStack history;

    int nextToken = 1;

    try {
        while (true) {
            cout << "\n========== QUEUECARE ==========\n"
                 << "1. Register patient\n"
                 << "2. Show all patients\n"
                 << "3. Show waiting queue\n"
                 << "4. Call next patient\n"
                 << "5. Undo latest call\n"
                 << "6. Search by token\n"
                 << "7. Search by name\n"
                 << "0. Exit\n";

            int choice = readNumber("Choose: ", 0, 7);

            if (choice == 0) {
                cout << "QueueCare closed.\n";
                break;
            }

            switch (choice) {
                case 1: {
                    if (waiting.isFull()) {
                        cout << "Waiting queue is full.\n";
                        break;
                    }

                    string name;

                    do {
                        cout << "Patient name: ";
                        if (!getline(cin, name)) {
                            throw runtime_error("Input closed.");
                        }
                    } while (name.find_first_not_of(" \t\r") == string::npos);

                    int age = readNumber("Age: ", 0, 120);

                    patients.addPatient(nextToken, name, age);
                    waiting.enqueue(nextToken);

                    cout << "Registered! Token: " << nextToken << '\n';
                    nextToken++;
                    break;
                }

                case 2:
                    patients.displayAll();
                    break;

                case 3:
                    waiting.display();
                    break;

                case 4: {
                    if (waiting.size() == 0) {
                        cout << "No patients waiting.\n";
                        break;
                    }

                    if (history.isFull()) {
                        cout << "Call history is full; cannot record another call.\n";
                        break;
                    }

                    int token = waiting.dequeue();
                    history.push(token);

                    cout << "Calling patient:\n";

                    const Patient* patient = patients.searchPatient(token);
                    if (patient != nullptr) {
                        patient->display();
                    }

                    break;
                }

                case 5: {
                    if (history.isEmpty()) {
                        cout << "No calls to undo.\n";
                        break;
                    }

                    if (waiting.isFull()) {
                        cout << "Queue is full; cannot restore patient.\n";
                        break;
                    }

                    int token = history.pop();
                    waiting.restoreFront(token);

                    cout << "Call undone. Token " << token
                         << " restored to the front.\n";
                    break;
                }

                case 6: {
                    int token = readNumber(
                        "Enter token: ", 1, numeric_limits<int>::max()
                    );

                    const Patient* patient = patients.searchPatient(token);

                    if (patient != nullptr) {
                        patient->display();
                    } else {
                        cout << "Patient not found.\n";
                    }

                    break;
                }

                case 7: {
                    string name;
                    cout << "Enter exact name: ";

                    if (!getline(cin, name)) {
                        throw runtime_error("Input closed.");
                    }

                    const Patient* patient = patients.searchPatient(name);

                    if (patient != nullptr) {
                        patient->display();
                    } else {
                        cout << "Patient not found.\n";
                    }

                    break;
                }
            }
        }
    } catch (const exception& error) {
        cout << '\n' << error.what() << '\n';
    }

    return 0;
}