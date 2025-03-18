#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "Huawei";  // Имя Wi-Fi сети
const char* password = "fynjy15456";  // Пароль от сети Wi-Fi

ESP8266WebServer server(80);  // Создаем HTTP сервер на порту 80

// Данные для управления матрицей
bool grid[16][16] = {};  // Матрица 16x16 для хранения состояния светодиодов
int speed = 5;  // Значение скорости
int bright = 2;  // Значение скорости
bool running = false;  // Флаг состояния

#include <GyverMAX7219.h>
MAX7219 < 2, 2, 5 > mtrx;   // одна матрица (1х1), пин CS на D5
#define N 16

// HTML страница для управления
const char* htmlPage = R"(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Управление светодиодной матрицей 16x16</title>
    <style>
        .grid {
            display: grid;
            grid-template-columns: repeat(16, 30px);
            grid-template-rows: repeat(16, 30px);
            gap: 1px;
        }
        .grid-cell {
            width: 30px;
            height: 30px;
            background-color: white;
            border: 1px solid #ddd;
            cursor: pointer;
        }
        .active {
            background-color: black;
        }
        .button {
            padding: 10px;
            margin: 5px;
            cursor: pointer;
        }
    </style>
</head>
<body>
   
    <!-- Ползунок -->
    <label for="speed">Скорость:</label>
    <input type="range" id="speed" min="1" max="10" value="5" />
    <span id="speedValue">5</span>

    <label for="bright">Яркость:</label>
    <input type="range" id="bright" min="1" max="15" value="2" />
    <span id="brightValue">2</span>

    <!-- Сетка -->
    <div class="grid" id="grid"></div>

    <!-- Кнопки -->
    <br>
    <button class="button" id="startStopButton">Обновить</button>
    <button class="button" id="clearButton">Очистить</button>

    <script>
        // Создание сетки 16x16
        const gridContainer = document.getElementById('grid');
        let gridData = Array(16).fill().map(() => Array(16).fill(false));

        let drawing = false;
        let lastCell = null;

        for (let i = 0; i < 16; i++) {
            for (let j = 0; j < 16; j++) {
                const cell = document.createElement('div');
                cell.classList.add('grid-cell');
                cell.dataset.row = i;
                cell.dataset.col = j;
//                cell.addEventListener('click', toggleCell);
                cell.addEventListener('mousedown', startDrawing);
                cell.addEventListener('mousemove', drawLine);
                cell.addEventListener('mouseup', stopDrawing);
                gridContainer.appendChild(cell);
            }
        }

        // Функция для начала рисования
        function startDrawing(event) {
            drawing = true;
            lastCell = event.target;
            toggleCell(event);  // Обрабатываем текущую ячейку
            
        }

        // Функция для рисования линии
        function drawLine(event) {
            if (drawing) {
                const cell = event.target;

                // Проверяем, что на ячейке ещё не было изменений
                if (cell !== lastCell) {
                    toggleCell(event);  // Обрабатываем текущую ячейку
                    lastCell = cell;  // Запоминаем текущую ячейку
                }
            }
        }

        // Функция для остановки рисования
        function stopDrawing() {
            drawing = false;
            lastCell = null;  // Сбрасываем сохранение ячейки
        }

        // Функция для переключения состояния ячейки
        function toggleCell(event) {
            const cell = event.target;
            const row = cell.dataset.row;
            const col = cell.dataset.col;

            gridData[row][col] = !gridData[row][col];
            cell.classList.toggle('active', gridData[row][col]);
        }

        // Ползунок
        const speedSlider = document.getElementById('speed');
        const speedValue = document.getElementById('speedValue');
        speedSlider.addEventListener('input', () => {
            speedValue.textContent = speedSlider.value;
        });

        const brightSlider = document.getElementById('bright');
        const brightValue = document.getElementById('brightValue');
        brightSlider.addEventListener('input', () => {
            brightValue.textContent = brightSlider.value;
        });

        // Старт / Стоп
        let isRunning = false;
        const startStopButton = document.getElementById('startStopButton');
        startStopButton.addEventListener('click', () => {
            isRunning = !isRunning;
//            startStopButton.textContent = isRunning ? 'Стоп' : 'Старт';
            sendData();
        });

        // Очистка
        const clearButton = document.getElementById('clearButton');
        clearButton.addEventListener('click', () => {
            gridData = Array(16).fill().map(() => Array(16).fill(false));
            const cells = gridContainer.getElementsByClassName('grid-cell');
            for (let cell of cells) {
                cell.classList.remove('active');
            }
        });

        function convertBooleanArrayToNumbers(boolArray) {
          // Проверяем, что входной аргумент является массивом
          if (!Array.isArray(boolArray)) {
              throw new Error("Input must be a 2D array");
          }
      
          // Преобразуем каждый элемент массива
          return boolArray.map(row => {
              // Проверяем, что каждая строка тоже является массивом
              if (!Array.isArray(row)) {
                  throw new Error("Each row in the 2D array must be an array");
              }
              // Преобразуем значения в строке: true -> 1, false -> 0
              return row.map(value => (value === true ? 1 : 0));
          });
      }

        // Функция для отправки данных на ESP8266
        function sendData() {

            const url = `/update`;

            // Создание объекта с данными
            const data = {
                grid: convertBooleanArrayToNumbers(gridData),
                speed: speedSlider.value,
                bright: brightSlider.value,
                running: isRunning
            };

            // Отправка данных на ESP8266
            fetch(url, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(data)
            });
        }
    </script>
</body>
</html>
)";

// Функция для обработки запроса на главную страницу
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Обработчик POST-запроса для обновления данных
void handleUpdate() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Парсим данные
    int gridIndex = body.indexOf("\"grid\":");
    int speedIndex = body.indexOf("\"speed\":");
    int brightIndex = body.indexOf("\"bright\":");
    int runningIndex = body.indexOf("\"running\":");

    if (gridIndex != -1 && speedIndex != -1 && runningIndex != -1) {
      String gridData = body.substring(gridIndex + 8, speedIndex - 1);  // Получаем часть данных о матрице
      for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
          grid[i][15-j] = (gridData.charAt(i * 34 + j*2+1) == '1');  // Преобразуем символы в bool
        }
      }

      // Извлекаем скорость
      speed = body.substring(speedIndex + 9, brightIndex-2).toInt();
      bright = body.substring(brightIndex + 10, runningIndex-2).toInt();
      mtrx.setBright(bright);
      // Извлекаем флаг работы
      running = (body.substring(runningIndex + 10, body.indexOf('}', runningIndex)) == "true");

      server.send(200, "application/json", "{\"status\":\"OK\"}");
      Serial.println("Data updated");
    } else {
      server.send(400, "text/plain", "Invalid data format.");
    }
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

void calc_life(bool cur[N][N]){
  bool next[N][N];
  
  for (int i=0; i<N; i++){
    for (int j=0; j<N; j++){
      int cnt = 0;

      for (int k=i-1; k<=i+1; k++){
        for (int p=j-1; p<=j+1; p++){
          cnt+=cur[(k+N)%N][(p+N)%N];
        }
      }
      cnt-=cur[i][j];

      if (cnt == 3) next[i][j]=1;
      else if (cnt == 2 && cur[i][j] == 1) next[i][j]=1;
      else next[i][j]=0;
      
    }
  }

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      grid[i][j] = next[i][j];
    }
  }
}

void fill_mtrx(bool cur[N][N]){
  mtrx.clear();
  
  for (int i=0; i<N; i++){
    for (int j=0; j<N; j++){
      if(cur[i][j]) mtrx.dot(i, j);
    }
  }
  
  mtrx.update();
}

void setup() {
  mtrx.begin();       // запускаем
  mtrx.setBright(2);  // яркость 0..15
  mtrx.setConnection(GM_LEFT_BOTTOM_UP);
  
  Serial.begin(115200);

  // Подключение к Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // Печатает IP адрес ESP8266

  // Определяем маршруты
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_POST, handleUpdate);

  // Запуск сервера
  server.begin();
}

void loop() {
  server.handleClient();  // Обработка входящих запросов
  yield();
//  if (running){
    fill_mtrx(grid);
    calc_life(grid);
    delay(100 * (11-speed));
//  }
}
