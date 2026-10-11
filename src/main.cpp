#include "raylib.h"
#include <vector>
#include <map>
#include <cmath>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <functional>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

// Типы станций и пассажиров
enum class ShapeType {
    CIRCLE,
    SQUARE,
    TRIANGLE
};

// Структура пассажира
struct Passenger {
    ShapeType type;
    int destinationStationId; // ID станции назначения (-1 если без назначения)
};

// Структура станции
struct Station {
    Vector2 position;
    ShapeType shape;
    std::vector<Passenger> passengers;
    std::vector<int> connectedStations; // индексы соединенных станций
    int id;
    std::map<int, int> lineConnectionsCount; // количество соединений на каждой линии (индекс линии -> количество)
    
    // Подсчет пассажиров по типам
    int getPassengerCount(ShapeType type) const {
        int count = 0;
        for (const auto& p : passengers) {
            if (p.type == type) {
                count++;
            }
        }
        return count;
    }
    
    int getTotalPassengers() const {
        return passengers.size();
    }
    
    // Проверка, можно ли добавить соединение на линию
    bool canAddConnectionToLine(int lineIndex) const {
        auto it = lineConnectionsCount.find(lineIndex);
        if (it != lineConnectionsCount.end()) {
            return it->second < 2; // не более 2 соединений на линию
        }
        return true; // пока нет соединений на этой линии
    }
    
    // Добавление соединения на линию
    void addConnectionToLine(int lineIndex) {
        lineConnectionsCount[lineIndex]++;
    }
};

// Структура вагона
struct Train {
    int currentLineIndex;     // индекс линии, на которой едет вагон
    int currentNodeIndex;     // индекс текущей станции в маршруте
    int nextNodeIndex;        // индекс следующей станции в маршруте
    float t;                  // прогресс между станциями (0-1)
    Vector2 position;         // текущая позиция
    bool direction;           // true - вперед, false - назад
    std::vector<Passenger> passengers; // пассажиры в вагоне
    float speed;              // скорость движения
    std::vector<int> route;   // маршрут следования (порядок станций)
    
    Train() : currentLineIndex(-1), currentNodeIndex(0), nextNodeIndex(1), t(0), 
              direction(true), speed(0.3f) {}
};

// Структура линии метро (ветки)
struct MetroLine {
    Color color;
    std::vector<std::pair<int, int>> connections; // пары индексов станций
    std::vector<Train> trains; // поезда на этой линии
    bool isActive; // активна ли линия для построения
    std::vector<int> stationsOrder; // порядок станций на линии (маршрут)
};

class MiniMetro {
private:
    std::vector<Station> stations;
    std::vector<MetroLine> metroLines;
    int nextStationId;
    bool gameOver;
    bool isDragging;
    int startStationIndex;
    Vector2 dragEndPos;
    float spawnTimer;
    float trainSpawnTimer;
    int activeLineIndex; // индекс активной линии (0 или 1)
    
    // Цвета для веток
    const Color RED_COLOR = RED;
    const Color GREEN_COLOR = GREEN;
    
    // Функция для получения расстояния между точками
    float getDistance(Vector2 a, Vector2 b) {
        return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    }
    
    // Проверка, можно ли создать соединение (не более 2 ребер одного цвета у каждой станции)
    bool canCreateConnection(int station1, int station2, int lineIndex) {
        // Проверяем, не превысит ли лимит соединений на линии для каждой станции
        return stations[station1].canAddConnectionToLine(lineIndex) && 
               stations[station2].canAddConnectionToLine(lineIndex);
    }
    
    // Построение маршрута для линии (обход всех станций в порядке соединения)
    void buildRouteForLine(int lineIndex) {
        if (metroLines[lineIndex].connections.empty()) {
            metroLines[lineIndex].stationsOrder.clear();
            return;
        }
        
        std::vector<int> order;
        std::vector<bool> visited(stations.size(), false);
        
        // Находим концевую станцию (с одной связью на этой линии)
        std::map<int, int> connectionCount;
        for (const auto& conn : metroLines[lineIndex].connections) {
            connectionCount[conn.first]++;
            connectionCount[conn.second]++;
        }
        
        int startStation = -1;
        for (const auto& pair : connectionCount) {
            if (pair.second == 1) {
                startStation = pair.first;
                break;
            }
        }
        
        // Если нет концевой (замкнутый цикл), берем первую станцию
        if (startStation == -1 && !metroLines[lineIndex].connections.empty()) {
            startStation = metroLines[lineIndex].connections[0].first;
        }
        
        if (startStation == -1) return;
        
        // Обход в глубину для построения маршрута
        std::vector<int> route;
        std::vector<bool> visitedStation(stations.size(), false);
        
        // Рекурсивная функция для DFS
        std::function<void(int, int)> dfs = [&](int current, int parent) {
            visitedStation[current] = true;
            route.push_back(current);
            
            for (const auto& conn : metroLines[lineIndex].connections) {
                int neighbor = -1;
                if (conn.first == current && conn.second != parent) {
                    neighbor = conn.second;
                } else if (conn.second == current && conn.first != parent) {
                    neighbor = conn.first;
                }
                
                if (neighbor != -1 && !visitedStation[neighbor]) {
                    dfs(neighbor, current);
                }
            }
        };
        
        dfs(startStation, -1);
        metroLines[lineIndex].stationsOrder = route;
    }
    
    // Получение позиции на линии между двумя станциями
    Vector2 getPositionBetweenStations(int station1, int station2, float t) {
        Vector2 start = stations[station1].position;
        Vector2 end = stations[station2].position;
        
        return {
            start.x + (end.x - start.x) * t,
            start.y + (end.y - start.y) * t
        };
    }
    
    // Получение текущей позиции поезда
    Vector2 getTrainPosition(const Train& train) {
        if (train.route.size() < 2) return {0, 0};
        
        int fromStation = train.route[train.currentNodeIndex];
        int toStation = train.route[train.nextNodeIndex];
        
        return getPositionBetweenStations(fromStation, toStation, train.t);
    }
    
    // Создание новой станции
    void addNewStation() {
        Vector2 newPos;
        bool validPosition = false;
        
        // Находим позицию, не слишком близко к другим станциям
        while (!validPosition) {
            newPos = {(float)(50 + rand() % (SCREEN_WIDTH - 100)), 
                     (float)(50 + rand() % (SCREEN_HEIGHT - 100))};
            validPosition = true;
            for (const auto& station : stations) {
                if (getDistance(newPos, station.position) < 80) {
                    validPosition = false;
                    break;
                }
            }
        }
        
        ShapeType randomShape = static_cast<ShapeType>(rand() % 3);
        
        stations.push_back({newPos, randomShape, {}, {}, nextStationId++, {}});
    }
    
    // Спавн пассажиров
    void spawnPassengers() {
        if (stations.empty()) return;
        
        int stationIndex = rand() % stations.size();
        ShapeType randomType = static_cast<ShapeType>(rand() % 3);
        
        // Ищем станцию назначения того же типа
        int destinationId = -1;
        std::vector<int> sameTypeStations;
        for (const auto& station : stations) {
            if (station.shape == randomType && station.id != stations[stationIndex].id) {
                sameTypeStations.push_back(station.id);
            }
        }
        
        if (!sameTypeStations.empty()) {
            destinationId = sameTypeStations[rand() % sameTypeStations.size()];
        }
        
        stations[stationIndex].passengers.push_back({randomType, destinationId});
        
        // Проверка на окончание игры
        if (stations[stationIndex].getTotalPassengers() > 10) {
            gameOver = true;
        }
    }
    
    // Создание соединения между станциями
    bool createConnection(int station1, int station2) {
        if (station1 == station2) return false;
        if (areConnected(station1, station2)) return false;
        
        // Проверяем, можно ли добавить соединение на активную линию
        if (!canCreateConnection(station1, station2, activeLineIndex)) {
            return false; // Нельзя добавить - превышен лимит соединений
        }
        
        // Используем активную линию
        if (activeLineIndex >= (int)metroLines.size()) {
            Color lineColor = (metroLines.empty()) ? RED_COLOR : GREEN_COLOR;
            metroLines.push_back({lineColor, {}, {}, false, {}});
        }
        
        // Добавляем соединение в активную линию
        metroLines[activeLineIndex].connections.push_back({station1, station2});
        
        stations[station1].connectedStations.push_back(station2);
        stations[station2].connectedStations.push_back(station1);
        
        // Обновляем счетчики соединений на линии для станций
        stations[station1].addConnectionToLine(activeLineIndex);
        stations[station2].addConnectionToLine(activeLineIndex);
        
        // Перестраиваем маршрут для линии
        buildRouteForLine(activeLineIndex);
        
        // Обновляем маршруты для существующих поездов на этой линии
        for (auto& train : metroLines[activeLineIndex].trains) {
            train.route = metroLines[activeLineIndex].stationsOrder;
            if (train.route.size() >= 2) {
                train.currentNodeIndex = 0;
                train.nextNodeIndex = 1;
                train.direction = true;
                train.t = 0;
            }
        }
        
        return true;
    }
    
    // Спавн поезда на линии
    void spawnTrainOnLine(int lineIndex) {
        if (metroLines[lineIndex].stationsOrder.size() < 2) return;
        
        Train newTrain;
        newTrain.currentLineIndex = lineIndex;
        newTrain.route = metroLines[lineIndex].stationsOrder;
        newTrain.currentNodeIndex = 0;
        newTrain.nextNodeIndex = 1;
        newTrain.t = 0;
        newTrain.direction = true;
        newTrain.speed = 0.3f;
        
        // Устанавливаем начальную позицию
        newTrain.position = getTrainPosition(newTrain);
        
        metroLines[lineIndex].trains.push_back(newTrain);
    }
    
    // Обновление позиций поездов и перевозка пассажиров
    void updateTrains(float dt) {
        for (auto& line : metroLines) {
            for (auto& train : line.trains) {
                if (train.route.size() < 2) continue;
                
                // Движение поезда
                train.t += train.speed * dt * (train.direction ? 1 : -1);
                
                // Проверка достижения станции
                if (train.t >= 1.0f) {
                    train.t = 0;
                    
                    // Прибытие на станцию
                    int stationId = train.route[train.nextNodeIndex];
                    int stationIndex = getStationIndexById(stationId);
                    
                    if (stationIndex != -1) {
                        // Высадка пассажиров
                        auto it = train.passengers.begin();
                        while (it != train.passengers.end()) {
                            if (it->destinationStationId == stationId) {
                                it = train.passengers.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        
                        // Посадка пассажиров
                        auto& station = stations[stationIndex];
                        auto passengerIt = station.passengers.begin();
                        while (passengerIt != station.passengers.end()) {
                            if (passengerIt->destinationStationId != -1 && 
                                train.passengers.size() < 20) {
                                train.passengers.push_back(*passengerIt);
                                passengerIt = station.passengers.erase(passengerIt);
                            } else {
                                ++passengerIt;
                            }
                        }
                    }
                    
                    // Переход к следующей станции
                    if (train.direction) {
                        train.currentNodeIndex++;
                        train.nextNodeIndex++;
                        
                        // Если достигли конца маршрута, разворачиваемся
                        if (train.nextNodeIndex >= (int)train.route.size()) {
                            train.direction = false;
                            train.currentNodeIndex = train.route.size() - 1;
                            train.nextNodeIndex = train.route.size() - 2;
                        }
                    } else {
                        train.currentNodeIndex--;
                        train.nextNodeIndex--;
                        
                        // Если достигли начала маршрута, разворачиваемся
                        if (train.nextNodeIndex < 0) {
                            train.direction = true;
                            train.currentNodeIndex = 0;
                            train.nextNodeIndex = 1;
                        }
                    }
                } else if (train.t <= 0) {
                    train.t = 0;
                    
                    // Прибытие на станцию при движении назад
                    int stationId = train.route[train.currentNodeIndex];
                    int stationIndex = getStationIndexById(stationId);
                    
                    if (stationIndex != -1) {
                        // Высадка пассажиров
                        auto it = train.passengers.begin();
                        while (it != train.passengers.end()) {
                            if (it->destinationStationId == stationId) {
                                it = train.passengers.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        
                        // Посадка пассажиров
                        auto& station = stations[stationIndex];
                        auto passengerIt = station.passengers.begin();
                        while (passengerIt != station.passengers.end()) {
                            if (passengerIt->destinationStationId != -1 && 
                                train.passengers.size() < 20) {
                                train.passengers.push_back(*passengerIt);
                                passengerIt = station.passengers.erase(passengerIt);
                            } else {
                                ++passengerIt;
                            }
                        }
                    }
                    
                    // Переход к следующей станции
                    if (!train.direction) {
                        train.currentNodeIndex--;
                        train.nextNodeIndex--;
                        
                        if (train.nextNodeIndex < 0) {
                            train.direction = true;
                            train.currentNodeIndex = 0;
                            train.nextNodeIndex = 1;
                        }
                    } else {
                        train.currentNodeIndex++;
                        train.nextNodeIndex++;
                        
                        if (train.nextNodeIndex >= (int)train.route.size()) {
                            train.direction = false;
                            train.currentNodeIndex = train.route.size() - 1;
                            train.nextNodeIndex = train.route.size() - 2;
                        }
                    }
                }
                
                // Обновление позиции
                train.position = getTrainPosition(train);
            }
        }
    }
    
    // Получение индекса станции по ID
    int getStationIndexById(int id) const {
        for (size_t i = 0; i < stations.size(); i++) {
            if (stations[i].id == id) {
                return i;
            }
        }
        return -1;
    }
    
    // Проверка, соединены ли две станции
    bool areConnected(int station1, int station2) const {
        for (const auto& line : metroLines) {
            for (const auto& conn : line.connections) {
                if ((conn.first == station1 && conn.second == station2) ||
                    (conn.first == station2 && conn.second == station1)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    // Проверка, все ли станции имеют хотя бы одно соединение
    bool allStationsConnected() const {
        for (const auto& station : stations) {
            if (station.connectedStations.empty()) {
                return false;
            }
        }
        return true;
    }
    
    // Отрисовка станции
    void drawStation(const Station& station) {
        Vector2 pos = station.position;
        
        // Отрисовка формы станции
        switch (station.shape) {
            case ShapeType::CIRCLE:
                DrawCircleLines(pos.x, pos.y, 20, BLACK);
                break;
            case ShapeType::SQUARE:
                DrawRectangleLines(pos.x - 20, pos.y - 20, 40, 40, BLACK);
                break;
            case ShapeType::TRIANGLE:
                DrawTriangleLines({pos.x, pos.y - 20}, 
                                 {pos.x - 17.3f, pos.y + 10}, 
                                 {pos.x + 17.3f, pos.y + 10}, BLACK);
                break;
        }
        
        // Отображение количества соединений на каждой линии
        int redConnections = 0;
        int greenConnections = 0;
        for (const auto& pair : station.lineConnectionsCount) {
            if (pair.first == 0) redConnections = pair.second;
            else if (pair.first == 1) greenConnections = pair.second;
        }
        
        if (redConnections > 0) {
            DrawText(TextFormat("R:%d", redConnections), pos.x + 25, pos.y - 20, 12, RED);
        }
        if (greenConnections > 0) {
            DrawText(TextFormat("G:%d", greenConnections), pos.x + 25, pos.y - 8, 12, GREEN);
        }
        
        // Отрисовка пассажиров над станцией
        float xOffset = -40;
        int yOffset = -35;
        
        // Круги
        int circleCount = station.getPassengerCount(ShapeType::CIRCLE);
        if (circleCount > 0) {
            DrawCircle(pos.x + xOffset, pos.y + yOffset, 8, DARKGRAY);
            DrawText(TextFormat("%d", circleCount), 
                    pos.x + xOffset - 5, pos.y + yOffset - 8, 12, BLACK);
            xOffset += 25;
        }
        
        // Квадраты
        int squareCount = station.getPassengerCount(ShapeType::SQUARE);
        if (squareCount > 0) {
            DrawRectangle(pos.x + xOffset - 8, pos.y + yOffset - 8, 16, 16, DARKGRAY);
            DrawText(TextFormat("%d", squareCount), 
                    pos.x + xOffset - 5, pos.y + yOffset - 8, 12, BLACK);
            xOffset += 25;
        }
        
        // Треугольники
        int triangleCount = station.getPassengerCount(ShapeType::TRIANGLE);
        if (triangleCount > 0) {
            DrawTriangle({pos.x + xOffset, pos.y + yOffset - 8},
                        {pos.x + xOffset - 8, pos.y + yOffset + 4},
                        {pos.x + xOffset + 8, pos.y + yOffset + 4}, DARKGRAY);
            DrawText(TextFormat("%d", triangleCount), 
                    pos.x + xOffset - 5, pos.y + yOffset - 8, 12, BLACK);
        }
        
        // Если игра окончена, показываем красный крест
        if (gameOver && station.getTotalPassengers() > 10) {
            DrawLine(pos.x - 15, pos.y - 15, pos.x + 15, pos.y + 15, RED);
            DrawLine(pos.x + 15, pos.y - 15, pos.x - 15, pos.y + 15, RED);
        }
    }
    
    // Отрисовка поездов
    void drawTrains() {
        for (const auto& line : metroLines) {
            for (const auto& train : line.trains) {
                // Рисуем вагон
                DrawRectangle(train.position.x - 8, train.position.y - 5, 16, 10, line.color);
                DrawRectangleLines(train.position.x - 8, train.position.y - 5, 16, 10, BLACK);
                
                // Рисуем маленькие окошки
                DrawRectangle(train.position.x - 4, train.position.y - 3, 3, 3, WHITE);
                DrawRectangle(train.position.x + 1, train.position.y - 3, 3, 3, WHITE);
            }
        }
    }
    
    // Отрисовка линий метро
    void drawMetroLines() {
        for (size_t i = 0; i < metroLines.size(); i++) {
            const auto& line = metroLines[i];
            float lineWidth = (i == activeLineIndex && !gameOver) ? 6.0f : 4.0f;
            
            for (const auto& conn : line.connections) {
                Vector2 start = stations[conn.first].position;
                Vector2 end = stations[conn.second].position;
                DrawLineEx(start, end, lineWidth, line.color);
            }
        }
    }
    
public:
    MiniMetro() : nextStationId(0), gameOver(false), isDragging(false), 
                  startStationIndex(-1), spawnTimer(0), trainSpawnTimer(0), activeLineIndex(0) {
        InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MiniMetro");
        SetTargetFPS(60);
        srand(time(nullptr));
        
        // Создаем две начальные станции
        addNewStation();
        addNewStation();
        
        // Размещаем их на разумном расстоянии
        stations[0].position = {SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2};
        stations[1].position = {SCREEN_WIDTH / 2 + 100, SCREEN_HEIGHT / 2};
        
        // Создаем две линии метро
        metroLines.push_back({RED_COLOR, {}, {}, false, {}});
        metroLines.push_back({GREEN_COLOR, {}, {}, false, {}});
    }
    
    ~MiniMetro() {
        CloseWindow();
    }
    
    void run() {
        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            handleInput();
            update(dt);
            draw();
        }
    }
    
    void handleInput() {
        Vector2 mousePos = GetMousePosition();
        
        // Выбор активной линии клавишами 1 и 2
        if (IsKeyPressed(KEY_ONE)) {
            activeLineIndex = 0;
        }
        if (IsKeyPressed(KEY_TWO)) {
            activeLineIndex = 1;
        }
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !gameOver) {
            // Проверяем, нажали ли на станцию
            for (size_t i = 0; i < stations.size(); i++) {
                if (CheckCollisionPointCircle(mousePos, stations[i].position, 20)) {
                    startStationIndex = i;
                    isDragging = true;
                    break;
                }
            }
        }
        
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && isDragging && !gameOver) {
            // Проверяем, отпустили ли на станции
            for (size_t i = 0; i < stations.size(); i++) {
                if (CheckCollisionPointCircle(mousePos, stations[i].position, 20) && 
                    i != startStationIndex) {
                    if (!createConnection(startStationIndex, i)) {
                        // Если соединение не удалось, показываем сообщение
                        DrawText("Cannot connect! Max 2 connections per line!", 
                                SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/2, 20, RED);
                    }
                    break;
                }
            }
            isDragging = false;
            startStationIndex = -1;
        }
        
        if (isDragging) {
            dragEndPos = mousePos;
        }
    }
    
    void update(float dt) {
        if (gameOver) return;
        
        // Спавн пассажиров
        spawnTimer += dt;
        if (spawnTimer >= 1.0f) {
            spawnTimer = 0;
            spawnPassengers();
        }
        
        // Спавн поездов на линиях
        trainSpawnTimer += dt;
        if (trainSpawnTimer >= 5.0f) {
            trainSpawnTimer = 0;
            for (size_t i = 0; i < metroLines.size(); i++) {
                if (!metroLines[i].connections.empty() && metroLines[i].trains.size() < 3) {
                    spawnTrainOnLine(i);
                }
            }
        }
        
        // Обновление поездов
        updateTrains(dt);
        
        // Проверяем, все ли станции соединены
        if (allStationsConnected() && stations.size() < 20) {
            addNewStation();
        }
    }
    
    void draw() {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        drawMetroLines();
        
        // Отрисовка линии при перетаскивании
        if (isDragging && startStationIndex != -1) {
            Color dragColor = (activeLineIndex < (int)metroLines.size()) ? 
                             metroLines[activeLineIndex].color : GRAY;
            DrawLineEx(stations[startStationIndex].position, dragEndPos, 4, dragColor);
        }
        
        for (const auto& station : stations) {
            drawStation(station);
        }
        
        drawTrains();
        
        // Отрисовка UI
        DrawText("MiniMetro", 10, 10, 20, DARKGRAY);
        DrawText("Press 1 or 2 to select active line", 10, 35, 15, DARKGRAY);
        
        if (activeLineIndex < (int)metroLines.size()) {
            Color activeColor = metroLines[activeLineIndex].color;
            DrawText(TextFormat("Active line: %s", 
                   activeLineIndex == 0 ? "RED" : "GREEN"), 10, 55, 15, activeColor);
        }
        
        DrawText("Drag from station to station to create metro lines", 10, 75, 15, DARKGRAY);
        DrawText("Each station: max 2 connections per line!", 10, 95, 15, DARKGRAY);
        DrawText(TextFormat("Stations: %d", (int)stations.size()), 10, 115, 15, DARKGRAY);
        DrawText(TextFormat("Trains: %d", getTotalTrains()), 10, 135, 15, DARKGRAY);
        
        // Отображение количества соединений на каждой линии
        if (metroLines.size() >= 2) {
            DrawText(TextFormat("Line 1 (RED): %d connections", 
                   (int)metroLines[0].connections.size()), 10, SCREEN_HEIGHT - 50, 15, RED);
            DrawText(TextFormat("Line 2 (GREEN): %d connections", 
                   (int)metroLines[1].connections.size()), 10, SCREEN_HEIGHT - 30, 15, GREEN);
        }
        
        if (gameOver) {
            const char* gameOverText = "GAME OVER! Press R to restart";
            int textWidth = MeasureText(gameOverText, 30);
            DrawText(gameOverText, SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/2, 30, RED);
            
            if (IsKeyPressed(KEY_R)) {
                restart();
            }
        }
        
        EndDrawing();
    }
    
    int getTotalTrains() {
        int total = 0;
        for (const auto& line : metroLines) {
            total += line.trains.size();
        }
        return total;
    }
    
    void restart() {
        stations.clear();
        metroLines.clear();
        nextStationId = 0;
        gameOver = false;
        isDragging = false;
        startStationIndex = -1;
        spawnTimer = 0;
        trainSpawnTimer = 0;
        activeLineIndex = 0;
        
        // Создаем две начальные станции
        addNewStation();
        addNewStation();
        stations[0].position = {SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2};
        stations[1].position = {SCREEN_WIDTH / 2 + 100, SCREEN_HEIGHT / 2};
        
        // Создаем две линии метро
        metroLines.push_back({RED_COLOR, {}, {}, false, {}});
        metroLines.push_back({GREEN_COLOR, {}, {}, false, {}});
    }
};

int main() {
    MiniMetro game;
    game.run();
    return 0;
}