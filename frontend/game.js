// Game variables
let socket = null;
let isConnected = false;
let gameActive = false;
let gameInterval = null;
let lastSensorUpdate = 0;

// Game state
let carX = 400;
let carY = 350;
let roadOffset = 0;
let speed = 0;
let score = 0;
let distance = 0;
let obstacles = [];
let lastObstacleTime = 0;
let sensorData = "00,0.00";

// Game constants
const SCREEN_WIDTH = 800;
const SCREEN_HEIGHT = 400;
const ROAD_WIDTH = 400;
const CAR_WIDTH = 50;
const CAR_HEIGHT = 80;
const OBSTACLE_WIDTH = 50;
const OBSTACLE_HEIGHT = 50;
const MAX_SPEED = 10;
const ACCELERATION = 0.2;
const DECELERATION = 0.1;
const ROAD_SPEED = 5;

// Canvas elements
const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d');
const gameOverlay = document.getElementById('gameOverlay');

// Initialize SocketIO connection
function initSocketIO() {
    socket = io();
    
    socket.on('connect', function() {
        console.log("Connected to server");
        isConnected = true;
        updateConnectionStatus(true);
        addToConsole(">> System connected and ready");
    });
    
    socket.on('disconnect', function() {
        console.log("Disconnected from server");
        isConnected = false;
        updateConnectionStatus(false);
        addToConsole(">> Connection lost. Reconnecting...");
    });
    
    socket.on('sensor_data', function(data) {
        sensorData = data;
        updateSensorDisplay();
    });
    
    socket.on('console_output', function(msg) {
        addToConsole(msg);
    });
    
    socket.on('game_state', function(data) {
        if (data.game_active != gameActive) {
            gameActive = data.game_active;
            if (!gameActive && data.score > 0) {
                showGameOver(data.score);
            }
        }
    });
}

function updateConnectionStatus(connected) {
    const indicator = document.getElementById('statusIndicator');
    const statusText = document.getElementById('connectionStatus');
    
    if (connected) {
        indicator.className = 'status-indicator connected';
        statusText.textContent = 'Connected to Arduino';
    } else {
        indicator.className = 'status-indicator disconnected';
        statusText.textContent = 'Disconnected - Reconnecting';
    }
}

function addToConsole(text) {
    const consoleElement = document.getElementById('console');
    const line = document.createElement('div');
    line.className = 'console-line';
    line.textContent = text;
    consoleElement.appendChild(line);
    consoleElement.scrollTop = consoleElement.scrollHeight;
}

function updateSensorDisplay() {
    try {
        const [hands, steering] = sensorData.split(',');
        const leftHand = hands[0] === '1';
        const rightHand = hands[1] === '1';
        const steeringVal = parseFloat(steering).toFixed(2);
        
        document.getElementById('leftSensor').classList.toggle('active', leftHand);
        document.getElementById('rightSensor').classList.toggle('active', rightHand);
        document.getElementById('leftValue').textContent = leftHand ? "ON" : "OFF";
        document.getElementById('rightValue').textContent = rightHand ? "ON" : "OFF";
        document.getElementById('steeringValue').textContent = steeringVal;
    } catch (e) {
        console.error("Error parsing sensor data:", e);
    }
}

function switchTab(tab) {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.content-section').forEach(s => s.classList.remove('active'));
    
    if (tab === 'test') {
        document.querySelector('.tab:nth-child(1)').classList.add('active');
        document.getElementById('test-section').classList.add('active');
    } else if (tab === 'game') {
        document.querySelector('.tab:nth-child(2)').classList.add('active');
        document.getElementById('game-section').classList.add('active');
    }
}

function runTest(testType) {
    if (!isConnected) {
        addToConsole(">> Waiting for connection...");
        return;
    }
    socket.emit('command', {type: 'test', command: testType});
}

function resetGameState() {
    score = 0;
    distance = 0;
    speed = 0;
    obstacles = [];
    document.getElementById('scoreValue').textContent = '0';
    document.getElementById('speedValue').textContent = '0';
    document.getElementById('distanceValue').textContent = '0';
    carX = 400;
    roadOffset = 0;
}

function startGame() {
    stopGame(); // Cleanup previous game
    socket.emit('command', {type: 'game', command: 'start'});
    gameActive = true;
    gameOverlay.style.display = 'none';
    resetGameState();
    gameInterval = setInterval(updateGame, 50);
}

function stopGame() {
    if (gameInterval) {
        clearInterval(gameInterval);
        gameInterval = null;
    }
    if (isConnected) socket.emit('command', {type: 'game', command: 'stop'});
    gameActive = false;
    gameOverlay.style.display = 'flex';
}

function showGameOver(finalScore) {
    gameOverlay.style.display = 'flex';
    gameOverlay.innerHTML = `
        <h2>GAME OVER</h2>
        <p>Your Score: ${finalScore}</p>
        <button class="btn btn-game" onclick="startGame()">
            <i class="fas fa-redo"></i> Play Again
        </button>
    `;
}

function updateGame() {
    if (!gameActive) return;
    
    try {
        const [hands, steering] = sensorData.split(',');
        const leftHand = hands[0] === '1';
        const rightHand = hands[1] === '1';
        const steeringVal = parseFloat(steering) || 0;
        
        // Update speed based on hand detection
        if (leftHand && rightHand) {
            speed = Math.min(speed + ACCELERATION, MAX_SPEED);
        } else {
            speed = Math.max(speed - DECELERATION, 0);
        }
        
        // FIX: Invert steering direction by subtracting instead of adding
        carX = Math.max(
            Math.min(carX - steeringVal * 10, SCREEN_WIDTH/2 + ROAD_WIDTH/2 - CAR_WIDTH/2),
            SCREEN_WIDTH/2 - ROAD_WIDTH/2 + CAR_WIDTH/2
        );
        
        // Update road position
        roadOffset = (roadOffset + speed) % 100;
        
        // Generate obstacles
        const currentTime = Date.now();
        if (currentTime - lastObstacleTime > 1500 && speed > 0) {
            lastObstacleTime = currentTime;
            const lane = Math.floor(Math.random() * 3);
            const xPos = SCREEN_WIDTH/2 - ROAD_WIDTH/2 + ROAD_WIDTH/4 + lane * (ROAD_WIDTH/4) - OBSTACLE_WIDTH/2;
            obstacles.push({
                x: xPos,
                y: -OBSTACLE_HEIGHT,
                width: OBSTACLE_WIDTH,
                height: OBSTACLE_HEIGHT,
                passed: false
            });
        }
        
        // Update obstacles
        for (let i = obstacles.length - 1; i >= 0; i--) {
            obstacles[i].y += speed + ROAD_SPEED;
            
            // Check collision (using simple bounding box)
            const carLeft = carX - CAR_WIDTH/2;
            const carRight = carX + CAR_WIDTH/2;
            const carTop = carY - CAR_HEIGHT/2;
            const carBottom = carY + CAR_HEIGHT/2;
            
            const obsLeft = obstacles[i].x;
            const obsRight = obstacles[i].x + OBSTACLE_WIDTH;
            const obsTop = obstacles[i].y;
            const obsBottom = obstacles[i].y + OBSTACLE_HEIGHT;
            
            if (carRight > obsLeft && 
                carLeft < obsRight && 
                carBottom > obsTop && 
                carTop < obsBottom) {
                stopGame();
                showGameOver(score);
                return;
            }
            
            // Remove off-screen obstacles and update score
            if (obstacles[i].y > SCREEN_HEIGHT) {
                if (!obstacles[i].passed) {
                    score += 10;
                    obstacles[i].passed = true;
                    document.getElementById('scoreValue').textContent = score;
                }
                obstacles.splice(i, 1);
            }
        }
        
        // Update distance
        distance += speed;
        
        // Update display
        document.getElementById('speedValue').textContent = Math.round(speed);
        document.getElementById('distanceValue').textContent = Math.round(distance);
        
        // Render game
        renderGame();
    } catch (e) {
        console.error("Error updating game:", e);
    }
}

function renderGame() {
    // Clear canvas
    ctx.clearRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    // Draw road
    ctx.fillStyle = '#2c3e50';
    ctx.fillRect(SCREEN_WIDTH/2 - ROAD_WIDTH/2, 0, ROAD_WIDTH, SCREEN_HEIGHT);
    
    // Draw road markings
    ctx.strokeStyle = '#f1c40f';
    ctx.lineWidth = 3;
    ctx.setLineDash([30, 30]);
    ctx.lineDashOffset = -roadOffset;
    ctx.beginPath();
    ctx.moveTo(SCREEN_WIDTH/2, 0);
    ctx.lineTo(SCREEN_WIDTH/2, SCREEN_HEIGHT);
    ctx.stroke();
    ctx.setLineDash([]);
    
    // Draw road borders
    ctx.strokeStyle = '#ecf0f1';
    ctx.lineWidth = 5;
    ctx.beginPath();
    ctx.moveTo(SCREEN_WIDTH/2 - ROAD_WIDTH/2, 0);
    ctx.lineTo(SCREEN_WIDTH/2 - ROAD_WIDTH/2, SCREEN_HEIGHT);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(SCREEN_WIDTH/2 + ROAD_WIDTH/2, 0);
    ctx.lineTo(SCREEN_WIDTH/2 + ROAD_WIDTH/2, SCREEN_HEIGHT);
    ctx.stroke();
    
    // Draw car
    ctx.fillStyle = '#e74c3c';
    ctx.fillRect(carX - CAR_WIDTH/2, carY - CAR_HEIGHT/2, CAR_WIDTH, CAR_HEIGHT);
    
    // Draw car details
    ctx.fillStyle = '#3498db';
    ctx.fillRect(carX - CAR_WIDTH/4, carY - CAR_HEIGHT/4, CAR_WIDTH/2, CAR_HEIGHT/4);
    
    // Draw wheels
    ctx.fillStyle = '#2c3e50';
    ctx.fillRect(carX - CAR_WIDTH/2 - 5, carY - CAR_HEIGHT/4, 5, CAR_HEIGHT/2);
    ctx.fillRect(carX + CAR_WIDTH/2, carY - CAR_HEIGHT/4, 5, CAR_HEIGHT/2);
    
    // Draw obstacles
    ctx.fillStyle = '#9b59b6';
    for (const obstacle of obstacles) {
        ctx.fillRect(obstacle.x, obstacle.y, obstacle.width, obstacle.height);
        
        // Draw obstacle details
        ctx.fillStyle = '#8e44ad';
        ctx.fillRect(obstacle.x + 10, obstacle.y + 10, obstacle.width - 20, obstacle.height - 20);
        ctx.fillStyle = '#9b59b6';
    }
}

// Initialize when page loads
window.onload = function() {
    initSocketIO();
    // Start with test tab active
    switchTab('test');
};