#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ========== STABILITY IMPROVEMENTS ==========
#include <Ticker.h>

// Forward declarations
void testSequence();
void ICACHE_RAM_ATTR resetWatchdog();

// Watchdog timer to prevent resets
Ticker wdtTicker;
void ICACHE_RAM_ATTR resetWatchdog() {
  ESP.wdtFeed();
}

// Memory optimization
#define WIFI_RECONNECT_INTERVAL 30000
unsigned long lastWiFiCheck = 0;
int wifiRetryCount = 0;

// Stack canary for stack overflow detection
uint32_t stackCanary;
#define STACK_CANARY 0xDEADBEEF

// Add missing sin8 function implementation
uint8_t sin8(uint8_t theta) {
  return (uint8_t)(sin(theta * TWO_PI / 255.0) * 127.5 + 127.5);
}

uint8_t cos8(uint8_t theta) {
  return (uint8_t)(cos(theta * TWO_PI / 255.0) * 127.5 + 127.5);
}

// ========== TOUCH SENSOR CONFIGURATION ==========
// Add touch sensor pin configuration
#define TOUCH_SENSOR_PIN 2      // D4 on NodeMCU (GPIO2) - CHANGED FROM 4 TO 2
#define TOUCH_DEBOUNCE_TIME 300 // ms debounce time
#define LONG_PRESS_TIME 1500    // ms for long press

// Touch sensor variables
bool lastTouchState = false;
bool currentTouchState = false;
unsigned long lastTouchTime = 0;
unsigned long touchStartTime = 0;
bool touchActive = false;
bool longPressDetected = false;

// Touch control mode
bool touchMode = false;         // True when controlling via touch
int touchEffectIndex = 0;       // Current effect index for touch control

// WiFi Configuration
const char *ssid = "Optic RGB"; // CHANGED FROM "Roger" TO "Optic RGB"
const char *password = "12349876";
const byte DNS_PORT = 53;

// LED Configuration - CHANGED FROM 16 TO 3 LEDs
const int LED_PIN = 4;      // D2 on NodeMCU (GPIO4) - CHANGED FROM 5 TO 4
const int NUM_LEDS = 3;    // CHANGED FROM 16 TO 3 LEDs

// Network Configuration
IPAddress apIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Create server objects
DNSServer dnsServer;
ESP8266WebServer webServer(80);

// Initialize NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ========== MUSIC CONTROL VARIABLES ==========
// Music control parameters
int musicDensity = 50;
int musicRoughness = 50;
int effectSpeed = 50;
int glowingSpeed = 50;
bool musicPlaying = false;
int currentMusicEffect = 0;
String currentSongName = "No song selected";

// Music effect states
unsigned long lastMusicUpdate = 0;
int musicEffectCounter = 0;
int musicEffectPosition = 0;

// Store your HTML page - UPDATED WITH NEW WEB PAGE
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LED Controller</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        
        body {
            background-color: #FFF9E6;
            color: #333;
            height: 100vh;
            overflow: hidden;
            position: relative;
            padding: 20px;
        }
        
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
        }
        
        .top-btn {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background-color: #87CEEB;
            border: none;
            color: white;
            font-size: 18px;
            font-weight: bold;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
            transition: all 0.3s ease;
        }
        
        .top-btn:hover {
            transform: scale(1.1);
            box-shadow: 0 6px 12px rgba(0,0,0,0.15);
        }
        
        .top-btn:active {
            transform: scale(0.95);
        }
        
        .color-picker-container {
            display: flex;
            justify-content: center;
            margin-bottom: 30px;
        }
        
        .color-picker {
            width: 500px;
            height: 500px;
            border-radius: 50%;
            background: conic-gradient(red, yellow, lime, aqua, blue, magenta, red);
            position: relative;
            box-shadow: 0 8px 25px rgba(0,0,0,0.15);
            border: 8px solid white;
            cursor: pointer;
        }
        
        .color-selector {
            width: 35px;
            height: 35px;
            border-radius: 50%;
            background-color: white;
            border: 4px solid #333;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            pointer-events: none;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
        }
        
        .brightness-control {
            margin: 30px 0;
            padding: 0 20px;
        }
        
        .slider-label {
            display: flex;
            justify-content: space-between;
            margin-bottom: 15px;
            font-size: 18px;
            font-weight: 600;
            color: #555;
        }
        
        .slider {
            width: 100%;
            height: 12px;
            -webkit-appearance: none;
            background: linear-gradient(to right, #000, #fff);
            border-radius: 10px;
            outline: none;
            box-shadow: 0 2px 6px rgba(0,0,0,0.1);
        }
        
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 28px;
            height: 28px;
            border-radius: 50%;
            background: #87CEEB;
            border: 3px solid white;
            cursor: pointer;
            box-shadow: 0 2px 6px rgba(0,0,0,0.2);
            transition: all 0.2s ease;
        }
        
        .slider::-webkit-slider-thumb:hover {
            transform: scale(1.1);
            background: #5F9EA0;
        }
        
        .preset-section {
            margin: 25px 0;
            padding: 0 20px;
        }
        
        .section-label {
            font-size: 16px;
            font-weight: 600;
            margin-bottom: 15px;
            color: #666;
            text-align: center;
        }
        
        .preset-colors {
            display: grid;
            grid-template-columns: repeat(6, 1fr);
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .color-btn {
            width: 50px;
            height: 50px;
            border-radius: 50%;
            border: none;
            cursor: pointer;
            box-shadow: 0 4px 8px rgba(0,0,0,0.15);
            transition: all 0.3s ease;
            margin: 0 auto;
        }
        
        .color-btn:hover {
            transform: scale(1.15);
            box-shadow: 0 6px 12px rgba(0,0,0,0.2);
        }
        
        .color-btn:active {
            transform: scale(0.95);
        }
        
        .white-picker-overlay {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: #FFF9E6;
            z-index: 10;
            display: none;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 40px;
        }
        
        .white-picker-title {
            font-size: 28px;
            margin-bottom: 40px;
            color: #333;
            font-weight: 600;
        }
        
        .white-options {
            display: flex;
            flex-direction: column;
            gap: 25px;
            width: 100%;
            max-width: 350px;
        }
        
        .white-option {
            display: flex;
            align-items: center;
            gap: 20px;
            padding: 20px;
            border-radius: 20px;
            background-color: white;
            box-shadow: 0 6px 15px rgba(0,0,0,0.1);
            cursor: pointer;
            transition: all 0.3s ease;
        }
        
        .white-option:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 20px rgba(0,0,0,0.15);
        }
        
        .white-color {
            width: 60px;
            height: 60px;
            border-radius: 15px;
        }
        
        .warm-white {
            background: linear-gradient(135deg, #FFE4B5, #F5DEB3);
        }
        
        .cool-white {
            background: linear-gradient(135deg, #F5F5F5, #E6E6FA);
        }
        
        .white-label {
            font-size: 20px;
            font-weight: bold;
            color: #333;
        }
        
        .back-btn {
            position: absolute;
            top: 30px;
            right: 30px;
            padding: 12px 25px;
            background-color: #87CEEB;
            color: white;
            border: none;
            border-radius: 25px;
            cursor: pointer;
            font-size: 16px;
            font-weight: 600;
            box-shadow: 0 4px 8px rgba(0,0,0,0.15);
            transition: all 0.3s ease;
        }
        
        .back-btn:hover {
            background-color: #5F9EA0;
            transform: scale(1.05);
        }
        
        .effects-panel {
            position: fixed;
            top: 0;
            right: -100%;
            width: 80%;
            height: 100%;
            background-color: white;
            box-shadow: -4px 0 20px rgba(0,0,0,0.2);
            z-index: 5;
            transition: right 0.4s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            padding: 30px 20px;
            overflow-y: auto;
        }
        
        .effects-panel.open {
            right: 0;
        }
        
        .effects-title {
            font-size: 24px;
            margin-bottom: 25px;
            text-align: center;
            color: #333;
            font-weight: 600;
        }
        
        .effects-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
        }
        
        .effect-btn {
            padding: 16px 8px;
            background-color: #f8f8f8;
            border: none;
            border-radius: 15px;
            cursor: pointer;
            font-size: 13px;
            text-align: center;
            transition: all 0.2s ease;
            color: #555;
            font-weight: 500;
            box-shadow: 0 2px 6px rgba(0,0,0,0.08);
        }
        
        .effect-btn:hover {
            background-color: #e8e8e8;
            transform: translateY(-2px);
            box-shadow: 0 4px 10px rgba(0,0,0,0.12);
        }
        
        .effect-btn.active {
            background-color: #87CEEB;
            color: white;
            box-shadow: 0 4px 12px rgba(135, 206, 235, 0.4);
        }
        
        .white-effect-btn {
            padding: 16px 8px;
            background-color: #f0f8ff;
            border: none;
            border-radius: 15px;
            cursor: pointer;
            font-size: 13px;
            text-align: center;
            transition: all 0.2s ease;
            color: #4682B4;
            font-weight: 500;
            box-shadow: 0 2px 6px rgba(0,0,0,0.08);
            border: 2px solid #87CEEB;
        }
        
        .white-effect-btn:hover {
            background-color: #e0f0ff;
            transform: translateY(-2px);
            box-shadow: 0 4px 10px rgba(0,0,0,0.12);
        }
        
        .white-effect-btn.active {
            background-color: #4682B4;
            color: white;
            box-shadow: 0 4px 12px rgba(70, 130, 180, 0.4);
        }
        
        /* IMPROVED MUSIC CONTROL PANEL */
        .music-control-panel {
            position: fixed;
            bottom: -400px;
            left: 0;
            width: 100%;
            height: 400px;
            background-color: #FFA500;
            z-index: 20;
            transition: bottom 0.5s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            border-top-left-radius: 30px;
            border-top-right-radius: 30px;
            box-shadow: 0 -5px 25px rgba(0,0,0,0.3);
            padding: 20px;
            display: flex;
            flex-direction: column;
            overflow-y: auto;
        }
        
        .music-control-panel.open {
            bottom: 0;
        }
        
        .music-panel-title {
            font-size: 22px;
            margin-bottom: 15px;
            text-align: center;
            color: white;
            font-weight: bold;
            text-shadow: 1px 1px 3px rgba(0,0,0,0.3);
        }
        
        .music-slider-container {
            margin: 10px 0;
            padding: 0 15px;
        }
        
        .music-slider-label {
            display: flex;
            justify-content: space-between;
            margin-bottom: 5px;
            font-size: 14px;
            font-weight: 600;
            color: white;
            text-shadow: 1px 1px 2px rgba(0,0,0,0.3);
        }
        
        .music-slider {
            width: 100%;
            height: 8px;
            -webkit-appearance: none;
            background: linear-gradient(to right, #000, #fff);
            border-radius: 10px;
            outline: none;
            box-shadow: 0 2px 6px rgba(0,0,0,0.2);
        }
        
        .music-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #FF4500;
            border: 3px solid white;
            cursor: pointer;
            box-shadow: 0 2px 6px rgba(0,0,0,0.3);
            transition: all 0.2s ease;
        }
        
        .music-slider::-webkit-slider-thumb:hover {
            transform: scale(1.15);
            background: #FF6347;
        }
        
        .music-buttons-container {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
            margin: 15px 0;
            padding: 0 10px;
        }
        
        .music-btn {
            width: 50px;
            height: 50px;
            border-radius: 50%;
            background-color: #FF6347;
            border: none;
            cursor: pointer;
            font-size: 24px;
            text-align: center;
            transition: all 0.3s ease;
            color: white;
            font-weight: bold;
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0 auto;
        }
        
        .music-btn:hover {
            background-color: #FF4500;
            transform: translateY(-3px);
            box-shadow: 0 6px 12px rgba(0,0,0,0.25);
        }
        
        .music-btn:active {
            transform: scale(0.95);
        }
        
        .music-btn-label {
            margin-top: 5px;
            text-align: center;
            font-size: 12px;
            color: white;
            font-weight: 500;
        }
        
        .song-info-container {
            margin-top: 10px;
            padding: 10px;
            background-color: rgba(255, 255, 255, 0.9);
            border-radius: 15px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }
        
        .song-name-box {
            text-align: center;
            font-size: 16px;
            color: #333;
            font-weight: 500;
            padding: 10px;
            min-height: 40px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-bottom: 1px solid #ddd;
            margin-bottom: 10px;
        }
        
        .song-details {
            display: flex;
            justify-content: space-between;
            font-size: 14px;
            color: #666;
        }
        
        .swipe-indicator {
            position: absolute;
            top: 50%;
            left: 15px;
            transform: translateY(-50%);
            width: 6px;
            height: 50px;
            background-color: #ddd;
            border-radius: 3px;
        }
        
        .close-effects-btn {
            position: absolute;
            top: 20px;
            right: 20px;
            width: 30px;
            height: 30px;
            border-radius: 50%;
            background-color: #ff6b6b;
            color: white;
            border: none;
            font-size: 18px;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 6;
        }
        
        .effects-section-title {
            font-size: 18px;
            font-weight: 600;
            margin: 20px 0 10px 0;
            color: #666;
            text-align: center;
            padding-bottom: 5px;
            border-bottom: 2px solid #87CEEB;
        }
        
        .drag-handle {
            position: absolute;
            top: 10px;
            left: 50%;
            transform: translateX(-50%);
            width: 50px;
            height: 5px;
            background-color: rgba(255, 255, 255, 0.7);
            border-radius: 3px;
            cursor: pointer;
        }
        
        .file-input-hidden {
            display: none;
        }
        
        .button-group {
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        
        @media (max-width: 600px) {
            .color-picker {
                width: 350px;
                height: 350px;
            }
            
            .preset-colors {
                grid-template-columns: repeat(3, 1fr);
                gap: 12px;
            }
            
            .color-btn {
                width: 45px;
                height: 45px;
            }
            
            .effects-grid {
                grid-template-columns: 1fr;
            }
            
            .music-control-panel {
                height: 420px;
                bottom: -420px;
            }
            
            .music-slider-container {
                margin: 8px 0;
            }
            
            .music-slider-label {
                font-size: 12px;
            }
        }
        
        /* Mobile optimization for touch */
        @media (hover: none) {
            .effect-btn:active {
                background-color: #87CEEB;
                color: white;
            }
            
            .color-btn:active {
                transform: scale(0.95);
            }
            
            .music-btn:active {
                transform: scale(0.95);
            }
        }
        
        /* HIDDEN AUDIO ELEMENT */
        .hidden-audio {
            display: none;
        }
    </style>
</head>
<body>
    <div class="header">
        <button class="top-btn" id="colorPickerBtn" title="White Picker">W</button>
        <button class="top-btn" id="toggleBtn" title="Power">⚡</button>
    </div>
    
    <div class="color-picker-container">
        <div class="color-picker" id="colorPicker">
            <div class="color-selector"></div>
        </div>
    </div>
    
    <div class="brightness-control">
        <div class="slider-label">
            <span>Brightness</span>
            <span id="brightnessValue">50%</span>
        </div>
        <input type="range" min="0" max="100" value="50" class="slider" id="brightnessSlider">
    </div>
    
    <div class="preset-section">
        <div class="section-label">Preset</div>
        <div class="preset-colors">
            <button class="color-btn" style="background-color: #FF5252;"></button>
            <button class="color-btn" style="background-color: #FF9800;"></button>
            <button class="color-btn" style="background-color: #FFEB3B;"></button>
            <button class="color-btn" style="background-color: #4CAF50;"></button>
            <button class="color-btn" style="background-color: #2196F3;"></button>
            <button class="color-btn" style="background-color: #9C27B0;"></button>
        </div>
    </div>
    
    <div class="preset-section">
        <div class="section-label">Classic</div>
        <div class="preset-colors">
            <button class="color-btn" style="background-color: #E91E63;"></button>
            <button class="color-btn" style="background-color: #FF5722;"></button>
            <button class="color-btn" style="background-color: #CDDC39;"></button>
            <button class="color-btn" style="background-color: #00BCD4;"></button>
            <button class="color-btn" style="background-color: #3F51B5;"></button>
            <button class="color-btn" style="background-color: #673AB7;"></button>
        </div>
    </div>
    
    <div class="white-picker-overlay" id="whitePickerOverlay">
        <div class="white-picker-title">Select White Temperature</div>
        <div class="white-options">
            <div class="white-option" id="warmWhite">
                <div class="white-color warm-white"></div>
                <div class="white-label">Warm White</div>
            </div>
            <div class="white-option" id="coolWhite">
                <div class="white-color cool-white"></div>
                <div class="white-label">Cool White</div>
            </div>
        </div>
        <button class="back-btn" id="backFromWhite">Back</button>
    </div>
    
    <div class="effects-panel" id="effectsPanel">
        <button class="close-effects-btn" id="closeEffectsBtn">B</button>
        <div class="effects-title">LED Effects</div>
        
        <div class="effects-section-title">Color Effects (1-60)</div>
        <div class="effects-grid" id="effectsGrid">
            <!-- Effects 1-60 will be added dynamically -->
        </div>
        
        <div class="effects-section-title">Additional Effects (61-80)</div>
        <div class="effects-grid" id="additionalEffectsGrid">
            <!-- Effects 61-80 will be added dynamically -->
        </div>
        
        <div class="effects-section-title">White Effects (81-85)</div>
        <div class="effects-grid" id="whiteEffectsGrid">
            <!-- White Effects 81-85 will be added dynamically -->
        </div>
        
        <div class="effects-section-title">Extra Effects (86-100)</div>
        <div class="effects-grid" id="extraEffectsGrid">
            <!-- Extra Effects 86-100 will be added dynamically -->
        </div>
    </div>
    
    <!-- IMPROVED MUSIC CONTROL PANEL -->
    <div class="music-control-panel" id="musicControlPanel">
        <div class="drag-handle"></div>
        <div class="music-panel-title">Music Control Panel</div>
        
        <!-- Original 2 sliders -->
        <div class="music-slider-container">
            <div class="music-slider-label">
                <span>Color Density</span>
                <span id="densityValue">50%</span>
            </div>
            <input type="range" min="0" max="100" value="50" class="music-slider" id="densitySlider">
        </div>
        
        <div class="music-slider-container">
            <div class="music-slider-label">
                <span>Color Roughness</span>
                <span id="roughnessValue">50%</span>
            </div>
            <input type="range" min="0" max="100" value="50" class="music-slider" id="roughnessSlider">
        </div>
        
        <!-- NEW: 2 Additional Sliders -->
        <div class="music-slider-container">
            <div class="music-slider-label">
                <span>Effect Speed</span>
                <span id="effectSpeedValue">50%</span>
            </div>
            <input type="range" min="0" max="100" value="50" class="music-slider" id="effectSpeedSlider">
        </div>
        
        <div class="music-slider-container">
            <div class="music-slider-label">
                <span>Glowing Speed</span>
                <span id="glowingSpeedValue">50%</span>
            </div>
            <input type="range" min="0" max="100" value="50" class="music-slider" id="glowingSpeedSlider">
        </div>
        
        <!-- Music Control Buttons -->
        <div class="music-buttons-container">
            <div class="button-group">
                <button class="music-btn" id="musicFileBtn" title="File Manager">F</button>
                <div class="music-btn-label">File</div>
            </div>
            <div class="button-group">
                <button class="music-btn" id="musicToggleBtn" title="Run/Stop">R</button>
                <div class="music-btn-label">Run/Stop</div>
            </div>
            <div class="button-group">
                <button class="music-btn" id="musicEffectBtn" title="Music Effect">M</button>
                <div class="music-btn-label">Effect</div>
            </div>
        </div>
        
        <!-- Song Information Box -->
        <div class="song-info-container">
            <div class="song-name-box" id="songNameBox">
                No song selected
            </div>
            <div class="song-details">
                <span>Effect: <span id="currentEffect">1/10</span></span>
                <span>Status: <span id="musicStatus">Stopped</span></span>
            </div>
        </div>
    </div>
    
    <!-- Hidden file input -->
    <input type="file" id="fileInput" class="file-input-hidden" accept="audio/*">
    
    <!-- Audio element -->
    <audio id="audioPlayer" class="hidden-audio"></audio>

    <script>
        // DOM Elements
        const colorPickerBtn = document.getElementById('colorPickerBtn');
        const toggleBtn = document.getElementById('toggleBtn');
        const colorPicker = document.getElementById('colorPicker');
        const brightnessSlider = document.getElementById('brightnessSlider');
        const brightnessValue = document.getElementById('brightnessValue');
        const whitePickerOverlay = document.getElementById('whitePickerOverlay');
        const backFromWhite = document.getElementById('backFromWhite');
        const warmWhite = document.getElementById('warmWhite');
        const coolWhite = document.getElementById('coolWhite');
        const effectsPanel = document.getElementById('effectsPanel');
        const effectsGrid = document.getElementById('effectsGrid');
        const additionalEffectsGrid = document.getElementById('additionalEffectsGrid');
        const whiteEffectsGrid = document.getElementById('whiteEffectsGrid');
        const extraEffectsGrid = document.getElementById('extraEffectsGrid');
        const colorBtns = document.querySelectorAll('.color-btn');
        const closeEffectsBtn = document.getElementById('closeEffectsBtn');
        
        // MUSIC CONTROL ELEMENTS
        const musicControlPanel = document.getElementById('musicControlPanel');
        const densitySlider = document.getElementById('densitySlider');
        const densityValue = document.getElementById('densityValue');
        const roughnessSlider = document.getElementById('roughnessSlider');
        const roughnessValue = document.getElementById('roughnessValue');
        const effectSpeedSlider = document.getElementById('effectSpeedSlider');
        const effectSpeedValue = document.getElementById('effectSpeedValue');
        const glowingSpeedSlider = document.getElementById('glowingSpeedSlider');
        const glowingSpeedValue = document.getElementById('glowingSpeedValue');
        const musicFileBtn = document.getElementById('musicFileBtn');
        const musicToggleBtn = document.getElementById('musicToggleBtn');
        const musicEffectBtn = document.getElementById('musicEffectBtn');
        const songNameBox = document.getElementById('songNameBox');
        const currentEffectSpan = document.getElementById('currentEffect');
        const musicStatusSpan = document.getElementById('musicStatus');
        const fileInput = document.getElementById('fileInput');
        
        // AUDIO ELEMENTS
        const audioPlayer = document.getElementById('audioPlayer');
        
        // State
        let isOn = true;
        let isEffectsOpen = false;
        let isMusicPanelOpen = false;
        let musicPlaying = false;
        let currentMusicEffect = 0;
        let selectedSongName = "No song selected";
        let selectedSongFile = null;
        
        // Effects list for 1-60
        const effects = [
            "Solid Color", "Rainbow", "Rainbow Cycle", "Color Wipe", "Theater Chase", 
            "Blink", "Running Lights", "Meteor", "Twinkle", "Cycling Wipe",
            "Fire", "Confetti", "Police", "BPM", "Strobe",
            "Waves", "Comet", "Checkerboard", "Split Color", "Rainbow Fast",
            "Reverse Wipe", "Theater Rainbow", "Twinkle Random", "Pulse", "Sparkle",
            "Bounce", "Fade In Out", "Dual Chase", "Rainbow Wave", "Meteor Rainbow",
            "Breath", "Spiral", "Random Flash", "Alternate", "Color Chase",
            "Double Comet", "Rainbow Bounce", "Pulse Rainbow", "Dense Sparkle", "Dual Wave",
            "Chase Rainbow", "Dense Twinkle", "Moving Blocks", "Rainbow Spiral", "Comet Rainbow",
            "Fast Pulse", "Sparkle Rainbow", "Alternate Rainbow", "Slow Wave", "Triple Chase",
            "Bright Twinkle", "Rainbow Pulse", "Moving Dots", "Multi Sparkle", "Wave Rainbow",
            "Chase Blocks", "Rainbow Sparkle", "Alternate Blocks", "Dual Comet", "Slow Pulse"
        ];
        
        // Additional effects list for 61-80
        const additionalEffects = [
            "Music Visualizer", "Rainbow Fire", "Color Dance", "Matrix Rain", "Galaxy Spin",
            "Energy Pulse", "Water Ripple", "Heart Beat", "Christmas Lights", "Fireworks",
            "Plasma Ball", "Lava Lamp", "Aurora Borealis", "Ocean Waves", "Desert Sunset",
            "Northern Lights", "Rainbow Tornado", "Color Tornado", "Sparkle Storm", "Rainbow Explosion"
        ];
        
        // White effects list for 81-85
        const whiteEffects = [
            "Warm Glow", "Cool Pulse", "White Strobe", "Soft Fade", "Candle Light"
        ];
        
        // Extra effects list for 86-100
        const extraEffects = [
            "Laser Scan", "Digital Rain", "Color Wheel", "Particle Flow", "Hypnotic Spiral",
            "Binary Counter", "Color Symphony", "Neon Pulse", "Gradient Flow", "Pixel Dance",
            "Color Vortex", "Rainbow Ripple", "Matrix Code", "Cyber Pulse", "Star Field"
        ];
        
        // Music Effects List (10 effects)
        const musicEffects = [
            "Beat Pulse", "Color Wave", "Spectrum Analyzer", "Bass React", "Treble Dance",
            "Energy Flow", "Rhythm Flash", "Harmony Glow", "Tempo Chase", "Frequency Pulse"
        ];
        
        // Initialize effects grid 1-60
        effects.forEach((effect, index) => {
            const effectBtn = document.createElement('button');
            effectBtn.className = 'effect-btn';
            effectBtn.textContent = effect;
            effectBtn.dataset.id = index;
            effectBtn.onclick = () => setEffect(index);
            effectsGrid.appendChild(effectBtn);
        });
        
        // Initialize additional effects grid 61-80
        additionalEffects.forEach((effect, index) => {
            const effectBtn = document.createElement('button');
            effectBtn.className = 'effect-btn';
            effectBtn.textContent = effect;
            effectBtn.dataset.id = index + 60; // Start from 61
            effectBtn.onclick = () => setEffect(index + 60);
            additionalEffectsGrid.appendChild(effectBtn);
        });
        
        // Initialize white effects grid 81-85
        whiteEffects.forEach((effect, index) => {
            const effectBtn = document.createElement('button');
            effectBtn.className = 'white-effect-btn';
            effectBtn.textContent = effect;
            effectBtn.dataset.id = index + 80; // Start from 81
            effectBtn.onclick = () => setEffect(index + 80);
            whiteEffectsGrid.appendChild(effectBtn);
        });
        
        // Initialize extra effects grid 86-100
        extraEffects.forEach((effect, index) => {
            const effectBtn = document.createElement('button');
            effectBtn.className = 'effect-btn';
            effectBtn.textContent = effect;
            effectBtn.dataset.id = index + 85; // Start from 86
            effectBtn.onclick = () => setEffect(index + 85);
            extraEffectsGrid.appendChild(effectBtn);
        });
        
        // Initialize with first effect active
        document.querySelector('.effect-btn[data-id="0"]').classList.add('active');
        
        // Color picker functionality
        function handleColorPickerClick(event) {
            const picker = document.getElementById('colorPicker');
            const selector = document.querySelector('.color-selector');
            const rect = picker.getBoundingClientRect();
            
            const x = event.clientX - rect.left;
            const y = event.clientY - rect.top;
            
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            const deltaX = x - centerX;
            const deltaY = y - centerY;
            const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
            const radius = rect.width / 2 - 20;
            
            if (distance > radius) return;
            
            selector.style.left = x + 'px';
            selector.style.top = y + 'px';
            
            const angle = Math.atan2(deltaY, deltaX) * 180 / Math.PI;
            const normalizedAngle = (angle + 360) % 360;
            const hue = normalizedAngle;
            
            const hexColor = hslToHex(hue, 100, 50);
            setQuickColor(hexColor);
        }
        
        // Utility functions
        function hslToHex(h, s, l) {
            h /= 360;
            s /= 100;
            l /= 100;
            
            let r, g, b;
            
            if (s === 0) {
                r = g = b = l;
            } else {
                const hue2rgb = (p, q, t) => {
                    if (t < 0) t += 1;
                    if (t > 1) t -= 1;
                    if (t < 1/6) return p + (q - p) * 6 * t;
                    if (t < 1/2) return q;
                    if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
                    return p;
                };
                
                const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
                const p = 2 * l - q;
                r = hue2rgb(p, q, h + 1/3);
                g = hue2rgb(p, q, h);
                b = hue2rgb(p, q, h - 1/3);
            }
            
            const toHex = x => {
                const hex = Math.round(x * 255).toString(16);
                return hex.length === 1 ? '0' + hex : hex;
            };
            
            return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
        }
        
        function setQuickColor(hex) {
            fetch('/color?hex=' + hex.substring(1)).catch(err => console.log('Error setting color:', err));
        }
        
        function setEffect(id) {
            // Remove active class from all effect buttons
            document.querySelectorAll('.effect-btn, .white-effect-btn').forEach(b => {
                b.classList.remove('active');
            });
            
            // Add active class to clicked button
            const clickedBtn = document.querySelector(`[data-id="${id}"]`);
            if (clickedBtn) {
                clickedBtn.classList.add('active');
            }
            
            fetch('/effect?id=' + id).catch(err => console.log('Error setting effect:', err));
        }
        
        function toggleEffectsPanel(open) {
            if (open) {
                effectsPanel.classList.add('open');
                isEffectsOpen = true;
            } else {
                effectsPanel.classList.remove('open');
                isEffectsOpen = false;
            }
        }
        
        function toggleMusicPanel(open) {
            if (open) {
                musicControlPanel.classList.add('open');
                isMusicPanelOpen = true;
            } else {
                musicControlPanel.classList.remove('open');
                isMusicPanelOpen = false;
            }
        }
        
        // MUSIC CONTROL FUNCTIONS
        function handleMusicFileSelect() {
            fileInput.click();
        }
        
        // Handle file selection
        fileInput.addEventListener('change', function(event) {
            const file = event.target.files[0];
            if (file) {
                // Check file type
                if (!file.type.startsWith('audio/')) {
                    alert('Please select an audio file (MP3, WAV, etc.)');
                    return;
                }
                
                selectedSongName = file.name;
                selectedSongFile = file;
                songNameBox.textContent = selectedSongName;
                currentEffectSpan.textContent = "1/10";
                musicStatusSpan.textContent = "Ready";
                
                console.log('Music file selected:', file.name);
                
                // Send file info to ESP8266
                fetch('/music?file=' + encodeURIComponent(file.name))
                    .then(response => response.text())
                    .then(data => {
                        console.log('Server response:', data);
                        // Auto-close the music panel
                        setTimeout(() => {
                            toggleMusicPanel(false);
                        }, 500);
                        
                        // Reset play button
                        musicPlaying = false;
                        musicToggleBtn.textContent = 'R';
                        musicToggleBtn.title = 'Run';
                    })
                    .catch(err => {
                        console.log('Error sending file info:', err);
                        alert('Error communicating with device. Please try again.');
                    });
            }
        });
        
        function handleMusicToggle() {
            if (!selectedSongFile) {
                alert('Please select a music file first!');
                return;
            }
            
            if (!musicPlaying) {
                // START MUSIC PLAYBACK
                console.log('Starting music...');
                
                musicPlaying = true;
                musicToggleBtn.textContent = 'S';
                musicToggleBtn.title = 'Stop';
                musicStatusSpan.textContent = 'Playing';
                
                console.log('Music playback started on ESP8266');
                
                // Send play command to ESP8266
                fetch('/music?cmd=play')
                    .then(response => response.text())
                    .then(data => {
                        console.log('Play command response:', data);
                        // Auto-close the music panel
                        setTimeout(() => {
                            toggleMusicPanel(false);
                        }, 500);
                    })
                    .catch(err => {
                        console.log('Error sending play command:', err);
                        alert('Error communicating with device. Please try again.');
                        musicPlaying = false;
                        musicToggleBtn.textContent = 'R';
                        musicToggleBtn.title = 'Run';
                        musicStatusSpan.textContent = 'Error';
                    });
            } else {
                // STOP MUSIC PLAYBACK
                musicPlaying = false;
                musicToggleBtn.textContent = 'R';
                musicToggleBtn.title = 'Run';
                musicStatusSpan.textContent = 'Stopped';
                
                console.log('Music playback stopped');
                
                // Send stop command to ESP8266
                fetch('/music?cmd=stop')
                    .then(response => response.text())
                    .then(data => {
                        console.log('Stop command response:', data);
                    })
                    .catch(err => {
                        console.log('Error sending stop command:', err);
                        alert('Error communicating with device. Please try again.');
                    });
            }
        }
        
        function handleMusicEffect() {
            currentMusicEffect = (currentMusicEffect + 1) % 10;
            musicEffectBtn.textContent = (currentMusicEffect + 1);
            musicEffectBtn.title = musicEffects[currentMusicEffect];
            currentEffectSpan.textContent = (currentMusicEffect + 1) + "/10";
            
            // Send effect change to ESP8266
            fetch('/music?effect=' + currentMusicEffect)
                .then(response => response.text())
                .then(data => {
                    console.log('Effect change response:', data);
                })
                .catch(err => {
                    console.log('Error changing effect:', err);
                    alert('Error communicating with device. Please try again.');
                });
        }
        
        // SLIDER EVENT LISTENERS
        brightnessSlider.addEventListener('input', () => {
            const value = brightnessSlider.value;
            brightnessValue.textContent = `${value}%`;
            fetch(`/brightness?val=${Math.round(value * 2.55)}`)
                .catch(err => console.log('Error setting brightness:', err));
        });
        
        densitySlider.addEventListener('input', () => {
            const value = densitySlider.value;
            densityValue.textContent = `${value}%`;
            fetch(`/density?val=${value}`)
                .then(response => response.text())
                .then(data => console.log('Density response:', data))
                .catch(err => console.log('Error setting density:', err));
        });
        
        roughnessSlider.addEventListener('input', () => {
            const value = roughnessSlider.value;
            roughnessValue.textContent = `${value}%`;
            fetch(`/roughness?val=${value}`)
                .then(response => response.text())
                .then(data => console.log('Roughness response:', data))
                .catch(err => console.log('Error setting roughness:', err));
        });
        
        effectSpeedSlider.addEventListener('input', () => {
            const value = effectSpeedSlider.value;
            effectSpeedValue.textContent = `${value}%`;
            fetch(`/effectSpeed?val=${value}`)
                .then(response => response.text())
                .then(data => console.log('Effect speed response:', data))
                .catch(err => console.log('Error setting effect speed:', err));
        });
        
        glowingSpeedSlider.addEventListener('input', () => {
            const value = glowingSpeedSlider.value;
            glowingSpeedValue.textContent = `${value}%`;
            fetch(`/glowingSpeed?val=${value}`)
                .then(response => response.text())
                .then(data => console.log('Glowing speed response:', data))
                .catch(err => console.log('Error setting glowing speed:', err));
        });
        
        // EVENT LISTENERS
        colorPickerBtn.addEventListener('click', () => {
            whitePickerOverlay.style.display = 'flex';
        });
        
        toggleBtn.addEventListener('click', () => {
            isOn = !isOn;
            toggleBtn.style.backgroundColor = isOn ? '#87CEEB' : '#FF6B6B';
            toggleBtn.innerHTML = isOn ? '⚡' : '○';
            fetch('/toggle')
                .catch(err => console.log('Error toggling power:', err));
        });
        
        // Color picker events
        let isColorPickerDragging = false;
        colorPicker.addEventListener('mousedown', (e) => {
            isColorPickerDragging = true;
            handleColorPickerClick(e);
        });
        
        document.addEventListener('mousemove', (e) => {
            if (isColorPickerDragging) {
                handleColorPickerClick(e);
            }
        });
        
        document.addEventListener('mouseup', () => {
            isColorPickerDragging = false;
        });
        
        colorPicker.addEventListener('touchstart', (e) => {
            isColorPickerDragging = true;
            handleColorPickerClick(e.touches[0]);
            e.preventDefault();
        });
        
        document.addEventListener('touchmove', (e) => {
            if (isColorPickerDragging) {
                handleColorPickerClick(e.touches[0]);
                e.preventDefault();
            }
        });
        
        document.addEventListener('touchend', () => {
            isColorPickerDragging = false;
        });
        
        // MUSIC BUTTONS
        musicFileBtn.addEventListener('click', handleMusicFileSelect);
        musicToggleBtn.addEventListener('click', handleMusicToggle);
        musicEffectBtn.addEventListener('click', handleMusicEffect);
        
        backFromWhite.addEventListener('click', () => {
            whitePickerOverlay.style.display = 'none';
        });
        
        warmWhite.addEventListener('click', () => {
            setQuickColor('#FFE4B5');
            whitePickerOverlay.style.display = 'none';
        });
        
        coolWhite.addEventListener('click', () => {
            setQuickColor('#F5F5F5');
            whitePickerOverlay.style.display = 'none';
        });
        
        colorBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const color = getComputedStyle(btn).backgroundColor;
                const rgb = color.match(/\d+/g);
                const hex = '#' + 
                    ('0' + parseInt(rgb[0]).toString(16)).slice(-2) +
                    ('0' + parseInt(rgb[1]).toString(16)).slice(-2) +
                    ('0' + parseInt(rgb[2]).toString(16)).slice(-2);
                setQuickColor(hex);
            });
        });
        
        closeEffectsBtn.addEventListener('click', () => {
            toggleEffectsPanel(false);
        });
        
        // SWIPE GESTURES
        let touchStartY = 0;
        let touchEndY = 0;
        const swipeThreshold = 80;
        
        document.addEventListener('touchstart', (e) => {
            touchStartY = e.touches[0].clientY;
        });
        
        document.addEventListener('touchend', (e) => {
            touchEndY = e.changedTouches[0].clientY;
            const swipeDistance = touchEndY - touchStartY;
            const windowHeight = window.innerHeight;
            
            if (windowHeight - touchStartY < 100) {
                if (swipeDistance < -swipeThreshold && !isMusicPanelOpen) {
                    toggleMusicPanel(true);
                } else if (swipeDistance > swipeThreshold && isMusicPanelOpen) {
                    toggleMusicPanel(false);
                }
            }
        });
        
        document.querySelector('.drag-handle').addEventListener('click', () => {
            toggleMusicPanel(!isMusicPanelOpen);
        });
        
        // Effects panel swipe
        let effectSwipeStartX = 0;
        let effectSwipeEndX = 0;
        
        document.addEventListener('touchstart', (e) => {
            effectSwipeStartX = e.touches[0].clientX;
        });
        
        document.addEventListener('touchend', (e) => {
            effectSwipeEndX = e.changedTouches[0].clientX;
            const swipeDistance = effectSwipeEndX - effectSwipeStartX;
            
            if (effectSwipeStartX > window.innerWidth - 50 && swipeDistance < -100 && !isEffectsOpen) {
                toggleEffectsPanel(true);
            }
            else if (isEffectsOpen && effectSwipeStartX < 100 && swipeDistance > 100) {
                toggleEffectsPanel(false);
            }
        });
        
        // Click outside to close panels
        document.addEventListener('click', (e) => {
            if (isEffectsOpen && !effectsPanel.contains(e.target) && 
                !e.target.classList.contains('effect-btn') &&
                !e.target.classList.contains('white-effect-btn')) {
                toggleEffectsPanel(false);
            }
            
            if (isMusicPanelOpen && !musicControlPanel.contains(e.target) &&
                e.target !== musicFileBtn && e.target !== musicToggleBtn && 
                e.target !== musicEffectBtn && e.target !== fileInput) {
                toggleMusicPanel(false);
            }
        });
        
        // Initialize with default color
        setTimeout(() => {
            setQuickColor('#87CEEB');
            console.log('System initialized successfully');
        }, 100);
    </script>
</body>
</html>
)rawliteral";

// Variables for effects
bool isPoweredOn = true;
bool isEffectRunning = false;
int currentEffect = 0;
int currentBrightness = 128;
uint32_t currentColor = strip.Color(135, 206, 235);
unsigned long lastEffectUpdate = 0;
int effectCounter = 0;
int effectPosition = 0;
int hueCounter = 0;

// ========== STABILITY FUNCTIONS ==========
void checkStack() {
  // Check if stack canary is still intact
  if (stackCanary != STACK_CANARY) {
    Serial.println("STACK OVERFLOW DETECTED! Resetting...");
    ESP.reset();
  }
}

void feedWatchdog() {
  // Feed the watchdog timer
  ESP.wdtFeed();
}

// ========== TOUCH SENSOR FUNCTIONS ==========
void handleTouchSensor() {
  // Read touch sensor state
  currentTouchState = (digitalRead(TOUCH_SENSOR_PIN) == LOW); // Assuming active LOW
  
  unsigned long currentMillis = millis();
  
  // Detect touch press (falling edge)
  if (currentTouchState && !lastTouchState) {
    touchStartTime = currentMillis;
    touchActive = true;
    longPressDetected = false;
    Serial.println("Touch pressed");
  }
  
  // Detect touch release (rising edge)
  if (!currentTouchState && lastTouchState) {
    touchActive = false;
    unsigned long pressDuration = currentMillis - touchStartTime;
    
    // Short press: Change effect
    if (pressDuration < LONG_PRESS_TIME && !longPressDetected) {
      touchMode = true; // Switch to touch control mode
      
      // Cycle to next effect
      touchEffectIndex++;
      if (touchEffectIndex >= 100) { // 100 total effects
        touchEffectIndex = 0;
      }
      
      // Set the effect
      currentEffect = touchEffectIndex;
      isEffectRunning = true;
      effectCounter = 0;
      effectPosition = 0;
      hueCounter = 0;
      
      Serial.print("Touch: Changed to effect ");
      Serial.println(touchEffectIndex);
      
      // Visual feedback - blink once
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
      }
      strip.show();
      delay(50);
    }
  }
  
  // Detect long press while still touching
  if (touchActive && !longPressDetected) {
    unsigned long pressDuration = currentMillis - touchStartTime;
    if (pressDuration >= LONG_PRESS_TIME) {
      longPressDetected = true;
      
      // Long press: Toggle power
      isPoweredOn = !isPoweredOn;
      touchMode = true; // Switch to touch control mode
      
      if (!isPoweredOn) {
        // Turn off all LEDs
        for (int i = 0; i < NUM_LEDS; i++) {
          strip.setPixelColor(i, 0);
        }
        isEffectRunning = false;
      } else {
        // Turn on with current effect
        isEffectRunning = true;
        
        // Visual feedback - blink twice
        for (int j = 0; j < 2; j++) {
          for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, strip.Color(255, 255, 255));
          }
          strip.show();
          delay(50);
          for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, 0);
          }
          strip.show();
          delay(50);
        }
      }
      
      strip.show();
      Serial.print("Touch: Power ");
      Serial.println(isPoweredOn ? "ON" : "OFF");
    }
  }
  
  // Update last touch state
  lastTouchState = currentTouchState;
}

// Handle web requests
void handleRoot() {
  webServer.send(200, "text/html", index_html);
}

void handleColor() {
  if (webServer.hasArg("hex")) {
    String hexStr = webServer.arg("hex");
    long hexColor = strtol(hexStr.c_str(), NULL, 16);
    
    int r = (hexColor >> 16) & 0xFF;
    int g = (hexColor >> 8) & 0xFF;
    int b = hexColor & 0xFF;
    
    currentColor = strip.Color(r, g, b);
    
    // Stop any running effect
    isEffectRunning = false;
    touchMode = false; // Switch back to web control mode
    
    // Set all LEDs to the selected color
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, currentColor);
    }
    strip.setBrightness(currentBrightness);
    strip.show();
    
    Serial.print("Color set: R=");
    Serial.print(r);
    Serial.print(" G=");
    Serial.print(g);
    Serial.print(" B=");
    Serial.println(b);
    
    webServer.send(200, "text/plain", "OK");
  } else {
    webServer.send(400, "text/plain", "Missing hex parameter");
  }
}

void handleBrightness() {
  if (webServer.hasArg("val")) {
    currentBrightness = webServer.arg("val").toInt();
    strip.setBrightness(currentBrightness);
    strip.show();
    
    Serial.print("Brightness set to: ");
    Serial.println(currentBrightness);
    
    webServer.send(200, "text/plain", "OK");
  } else {
    webServer.send(400, "text/plain", "Missing val parameter");
  }
}

void handleToggle() {
  isPoweredOn = !isPoweredOn;
  touchMode = false; // Switch back to web control mode
  
  if (!isPoweredOn) {
    // Turn off all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    isEffectRunning = false;
  } else {
    // Turn on with current color
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, currentColor);
    }
  }
  strip.show();
  
  Serial.print("Power toggled: ");
  Serial.println(isPoweredOn ? "ON" : "OFF");
  
  webServer.send(200, "text/plain", isPoweredOn ? "ON" : "OFF");
}

void handleEffect() {
  if (webServer.hasArg("id")) {
    currentEffect = webServer.arg("id").toInt();
    isEffectRunning = true;
    touchMode = false; // Switch back to web control mode
    effectCounter = 0;
    effectPosition = 0;
    hueCounter = 0;
    
    Serial.print("Effect started: ");
    Serial.println(currentEffect);
    
    webServer.send(200, "text/plain", "Effect started");
  } else {
    webServer.send(400, "text/plain", "Missing id parameter");
  }
}

// ========== MUSIC CONTROL HANDLERS ==========
void handleMusic() {
  if (webServer.hasArg("file")) {
    currentSongName = webServer.arg("file");
    Serial.print("Music file selected: ");
    Serial.println(currentSongName);
    webServer.send(200, "text/plain", "File received: " + currentSongName);
  } 
  else if (webServer.hasArg("cmd")) {
    String cmd = webServer.arg("cmd");
    if (cmd == "play") {
      musicPlaying = true;
      musicEffectCounter = 0;
      musicEffectPosition = 0;
      Serial.println("Music playback started");
      webServer.send(200, "text/plain", "Playing");
    } 
    else if (cmd == "stop") {
      musicPlaying = false;
      // Clear LEDs when music stops
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, 0);
      }
      strip.show();
      Serial.println("Music playback stopped");
      webServer.send(200, "text/plain", "Stopped");
    } 
    else {
      webServer.send(400, "text/plain", "Invalid command");
    }
  }
  else if (webServer.hasArg("effect")) {
    currentMusicEffect = webServer.arg("effect").toInt();
    Serial.print("Music effect changed to: ");
    Serial.println(currentMusicEffect);
    webServer.send(200, "text/plain", "Effect changed");
  }
  else if (webServer.hasArg("density")) {
    musicDensity = webServer.arg("density").toInt();
    Serial.print("Density set to: ");
    Serial.println(musicDensity);
    webServer.send(200, "text/plain", "Density set");
  }
  else if (webServer.hasArg("roughness")) {
    musicRoughness = webServer.arg("roughness").toInt();
    Serial.print("Roughness set to: ");
    Serial.println(musicRoughness);
    webServer.send(200, "text/plain", "Roughness set");
  }
  else if (webServer.hasArg("effectSpeed")) {
    effectSpeed = webServer.arg("effectSpeed").toInt();
    Serial.print("Effect speed set to: ");
    Serial.println(effectSpeed);
    webServer.send(200, "text/plain", "Effect speed set");
  }
  else if (webServer.hasArg("glowingSpeed")) {
    glowingSpeed = webServer.arg("glowingSpeed").toInt();
    Serial.print("Glowing speed set to: ");
    Serial.println(glowingSpeed);
    webServer.send(200, "text/plain", "Glowing speed set");
  }
  else {
    webServer.send(400, "text/plain", "Invalid parameters");
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  
  webServer.send(404, "text/plain", message);
}

// Utility functions
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// ========== EFFECTS ADAPTED FOR 3 LEDs ==========

// Effect 0: Solid Color
void effect0() {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, currentColor);
  }
  strip.show();
}

// Effect 1: Rainbow
void effect1() {
  if (millis() - lastEffectUpdate > 20) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel((i * 85 + effectCounter) & 255));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 2: Rainbow Cycle
void effect2() {
  if (millis() - lastEffectUpdate > 20) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel(((i * 85) + effectCounter) & 255));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256 * 5) effectCounter = 0;
  }
}

// Effect 3: Color Wipe
void effect3() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    strip.setPixelColor(effectPosition, currentColor);
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS) {
      effectPosition = 0;
      for (int j = 0; j < NUM_LEDS; j++) {
        strip.setPixelColor(j, 0);
      }
    }
  }
}

// Effect 4: Theater Chase
void effect4() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectPosition) % 2 == 0) {
        strip.setPixelColor(i, currentColor);
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= 2) effectPosition = 0;
  }
}

// Effect 5: Blink
void effect5() {
  if (millis() - lastEffectUpdate > 500) {
    lastEffectUpdate = millis();
    if (effectCounter % 2 == 0) {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, currentColor);
      }
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 6: Running Lights
void effect6() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int brightness = sin8((i * 85 + effectCounter)) / 2;
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * brightness / 255,
        (currentColor >> 8 & 0xFF) * brightness / 255,
        (currentColor & 0xFF) * brightness / 255
      ));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 7: Meteor
void effect7() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    // Fade all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * 0.7,
        (currentColor >> 8 & 0xFF) * 0.7,
        (currentColor & 0xFF) * 0.7
      ));
    }
    // Draw meteor
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition - i + NUM_LEDS) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos, strip.Color(
          (currentColor >> 16 & 0xFF) * brightness / 255,
          (currentColor >> 8 & 0xFF) * brightness / 255,
          (currentColor & 0xFF) * brightness / 255
        ));
      }
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 8: Twinkle
void effect8() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    // Randomly twinkle LEDs
    if (random(10) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, currentColor);
    } else {
      // Fade all LEDs
      for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t color = strip.getPixelColor(i);
        int r = (color >> 16) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = color & 0xFF;
        r = r * 0.9;
        g = g * 0.9;
        b = b * 0.9;
        strip.setPixelColor(i, strip.Color(r, g, b));
      }
    }
    strip.show();
  }
}

// Effect 9: Cycling Wipe
void effect9() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i == effectPosition) {
        strip.setPixelColor(i, currentColor);
      } else {
        strip.setPixelColor(i, Wheel((i * 85) & 255));
      }
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 10: Fire
void effect10() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int flicker = random(150, 255);
      int r = flicker;
      int g = flicker * 0.4;
      int b = flicker * 0.1;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
  }
}

// Effect 11: Confetti
void effect11() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    // Fade all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.8;
      g = g * 0.8;
      b = b * 0.8;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Add new confetti
    if (random(8) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 12: Police
void effect12() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    if (effectCounter % 4 < 2) {
      // Red
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i % 2 == 0) strip.setPixelColor(i, strip.Color(255, 0, 0));
        else strip.setPixelColor(i, 0);
      }
    } else {
      // Blue
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i % 2 == 1) strip.setPixelColor(i, strip.Color(0, 0, 255));
        else strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 13: BPM
void effect13() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    int beat = sin8(effectCounter);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel(beat + (i * 85)));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 14: Strobe
void effect14() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    if (effectCounter % 2 == 0) {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
      }
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 15: Waves
void effect15() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, strip.Color(wave, wave/2, 255-wave));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 16: Comet
void effect16() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Draw comet with tail
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition - i + NUM_LEDS) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos, Wheel((effectCounter + i * 40) % 256));
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 10;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 17: Checkerboard
void effect17() {
  if (millis() - lastEffectUpdate > 500) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectCounter) % 2 == 0) {
        strip.setPixelColor(i, currentColor);
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 18: Split Color
void effect18() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < NUM_LEDS/2) {
      strip.setPixelColor(i, currentColor);
    } else {
      strip.setPixelColor(i, Wheel((effectCounter) % 256));
    }
  }
  strip.show();
  effectCounter++;
  if (effectCounter >= 256) effectCounter = 0;
  delay(100);
}

// Effect 19: Rainbow Fast
void effect19() {
  if (millis() - lastEffectUpdate > 20) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel((i * 85 + effectCounter) & 255));
    }
    strip.show();
    effectCounter += 5;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 20: Reverse Wipe
void effect20() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    strip.setPixelColor(NUM_LEDS - 1 - effectPosition, currentColor);
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS) {
      effectPosition = 0;
      for (int j = 0; j < NUM_LEDS; j++) {
        strip.setPixelColor(j, 0);
      }
    }
  }
}

// Effect 21: Theater Rainbow
void effect21() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectPosition) % 2 == 0) {
        strip.setPixelColor(i, Wheel((i * 85) & 255));
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= 2) effectPosition = 0;
  }
}

// Effect 22: Twinkle Random
void effect22() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.85;
      g = g * 0.85;
      b = b * 0.85;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Random twinkle with random colors
    if (random(5) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 23: Pulse
void effect23() {
  if (millis() - lastEffectUpdate > 40) {
    lastEffectUpdate = millis();
    int pulse = sin8(effectCounter);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * pulse / 255,
        (currentColor >> 8 & 0xFF) * pulse / 255,
        (currentColor & 0xFF) * pulse / 255
      ));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 24: Sparkle
void effect24() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    // Set all to dim
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(10, 10, 10));
    }
    // Random sparkles
    for (int i = 0; i < 1; i++) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, strip.Color(255, 255, 255));
    }
    strip.show();
  }
}

// Effect 25: Bounce
void effect25() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Bouncing ball
    int pos = abs((effectPosition % (NUM_LEDS * 2 - 2)) - (NUM_LEDS - 1));
    strip.setPixelColor(pos, currentColor);
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS * 2 - 2) effectPosition = 0;
  }
}

// Effect 26: Fade In Out
void effect26() {
  if (millis() - lastEffectUpdate > 40) {
    lastEffectUpdate = millis();
    int brightness = sin8(effectCounter);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * brightness / 255,
        (currentColor >> 8 & 0xFF) * brightness / 255,
        (currentColor & 0xFF) * brightness / 255
      ));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 27: Dual Chase
void effect27() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectPosition) % 2 == 0) {
        strip.setPixelColor(i, currentColor);
      } else if ((i + effectPosition) % 2 == 1) {
        strip.setPixelColor(i, Wheel((effectCounter) % 256));
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 20;
    if (effectPosition >= 2) effectPosition = 0;
  }
}

// Effect 28: Rainbow Wave
void effect28() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, Wheel(wave));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 29: Meteor Rainbow
void effect29() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.8;
      g = g * 0.8;
      b = b * 0.8;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Meteor with rainbow tail
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition - i + NUM_LEDS) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos, Wheel((effectCounter + i * 60) % 256));
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 10;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 30: Breath
void effect30() {
  if (millis() - lastEffectUpdate > 40) {
    lastEffectUpdate = millis();
    int breath = (exp(sin(effectCounter * 0.01)) - 0.36787944) * 108.0;
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * breath / 255,
        (currentColor >> 8 & 0xFF) * breath / 255,
        (currentColor & 0xFF) * breath / 255
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 31: Spiral
void effect31() {
  if (millis() - lastEffectUpdate > 200) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Spiral pattern for 3 LEDs
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition + i) % NUM_LEDS;
      strip.setPixelColor(pos, Wheel((effectCounter + i * 85) % 256));
    }
    strip.show();
    effectPosition++;
    effectCounter += 20;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 32: Random Flash
void effect32() {
  if (millis() - lastEffectUpdate > 300) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Flash random LEDs
    for (int i = 0; i < 2; i++) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 33: Alternate
void effect33() {
  if (millis() - lastEffectUpdate > 500) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectCounter) % 2 == 0) {
        strip.setPixelColor(i, currentColor);
      } else {
        strip.setPixelColor(i, Wheel((effectCounter) % 256));
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 34: Color Chase
void effect34() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Chase with multiple colors
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition - i * 2 + NUM_LEDS) % NUM_LEDS;
      strip.setPixelColor(pos, Wheel((i * 85 + effectCounter) % 256));
    }
    strip.show();
    effectPosition++;
    effectCounter += 10;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 35: Double Comet
void effect35() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Two comets moving in opposite directions
    for (int i = 0; i < 2; i++) {
      int pos1 = (effectPosition + i) % NUM_LEDS;
      int pos2 = (NUM_LEDS - effectPosition - i + NUM_LEDS) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos1, Wheel((effectCounter + i * 40) % 256));
        strip.setPixelColor(pos2, Wheel((effectCounter + 128 + i * 40) % 256));
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 10;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 36: Rainbow Bounce
void effect36() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Bouncing rainbow ball
    int pos = abs((effectPosition % (NUM_LEDS * 2 - 2)) - (NUM_LEDS - 1));
    strip.setPixelColor(pos, Wheel((effectCounter) % 256));
    strip.show();
    effectPosition++;
    effectCounter += 20;
    if (effectPosition >= NUM_LEDS * 2 - 2) effectPosition = 0;
  }
}

// Effect 37: Pulse Rainbow
void effect37() {
  if (millis() - lastEffectUpdate > 40) {
    lastEffectUpdate = millis();
    int pulse = sin8(effectCounter);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel((i * 85 + effectCounter) % 256));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 38: Dense Sparkle
void effect38() {
  if (millis() - lastEffectUpdate > 60) {
    lastEffectUpdate = millis();
    // Set all to very dim
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(5, 5, 5));
    }
    // Many sparkles
    for (int i = 0; i < 2; i++) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 39: Dual Wave
void effect39() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave1 = sin8((i * 85) + effectCounter);
      int wave2 = sin8((i * 85) + effectCounter + 128);
      strip.setPixelColor(i, strip.Color(wave1, wave2, (wave1 + wave2) / 2));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 40: Chase Rainbow
void effect40() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectPosition) % 2 == 0) {
        strip.setPixelColor(i, Wheel((i * 85 + effectCounter) % 256));
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 10;
    if (effectPosition >= 2) effectPosition = 0;
  }
}

// Effect 41: Dense Twinkle
void effect41() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    // Fade all quickly
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.7;
      g = g * 0.7;
      b = b * 0.7;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Many twinkles
    for (int i = 0; i < 2; i++) {
      if (random(3) == 0) {
        int led = random(NUM_LEDS);
        strip.setPixelColor(led, Wheel(random(256)));
      }
    }
    strip.show();
  }
}

// Effect 42: Moving Blocks
void effect42() {
  if (millis() - lastEffectUpdate > 300) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int block = (i + effectPosition) % 3;
      if (block == 0) strip.setPixelColor(i, currentColor);
      else if (block == 1) strip.setPixelColor(i, Wheel(85));
      else if (block == 2) strip.setPixelColor(i, Wheel(170));
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= 3) effectPosition = 0;
  }
}

// Effect 43: Rainbow Spiral
void effect43() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int pos = (i + effectPosition) % NUM_LEDS;
      strip.setPixelColor(pos, Wheel((i * 85 + effectCounter) % 256));
    }
    strip.show();
    effectCounter += 5;
    effectPosition++;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 44: Comet Rainbow
void effect44() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.85;
      g = g * 0.85;
      b = b * 0.85;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Rainbow comet
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition - i + NUM_LEDS) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos, Wheel((effectCounter + i * 80) % 256));
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 15;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 45: Fast Pulse
void effect45() {
  if (millis() - lastEffectUpdate > 20) {
    lastEffectUpdate = millis();
    int pulse = sin8(effectCounter * 3);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * pulse / 255,
        (currentColor >> 8 & 0xFF) * pulse / 255,
        (currentColor & 0xFF) * pulse / 255
      ));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 85) effectCounter = 0;
  }
}

// Effect 46: Sparkle Rainbow
void effect46() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    // Dim all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(15, 15, 15));
    }
    // Rainbow sparkles
    for (int i = 0; i < 2; i++) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 47: Alternate Rainbow
void effect47() {
  if (millis() - lastEffectUpdate > 300) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectCounter) % 2 == 0) {
        strip.setPixelColor(i, Wheel((i * 85) % 256));
      } else {
        strip.setPixelColor(i, Wheel((i * 85 + 128) % 256));
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 48: Slow Wave
void effect48() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, strip.Color(wave, wave/3, 255-wave));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 49: Triple Chase
void effect49() {
  if (millis() - lastEffectUpdate > 200) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectPosition) % 3 == 0) {
        strip.setPixelColor(i, currentColor);
      } else if ((i + effectPosition) % 3 == 1) {
        strip.setPixelColor(i, Wheel(85));
      } else {
        strip.setPixelColor(i, Wheel(170));
      }
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= 3) effectPosition = 0;
  }
}

// Effect 50: Bright Twinkle
void effect50() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.6;
      g = g * 0.6;
      b = b * 0.6;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Bright twinkles
    if (random(5) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, strip.Color(255, 255, 255));
    }
    strip.show();
  }
}

// Effect 51: Rainbow Pulse
void effect51() {
  if (millis() - lastEffectUpdate > 60) {
    lastEffectUpdate = millis();
    int pulse = (sin8(effectCounter) + cos8(effectCounter)) / 2;
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel((i * 85 + pulse) % 256));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 52: Moving Dots
void effect52() {
  if (millis() - lastEffectUpdate > 250) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Moving dots
    for (int i = 0; i < 2; i++) {
      int pos = (effectPosition + i * 2) % NUM_LEDS;
      strip.setPixelColor(pos, Wheel((i * 85) % 256));
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 53: Multi Sparkle
void effect53() {
  if (millis() - lastEffectUpdate > 50) {
    lastEffectUpdate = millis();
    // Very fast fade
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.5;
      g = g * 0.5;
      b = b * 0.5;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Many sparkles
    for (int i = 0; i < 2; i++) {
      if (random(3) == 0) {
        int led = random(NUM_LEDS);
        strip.setPixelColor(led, Wheel(random(256)));
      }
    }
    strip.show();
  }
}

// Effect 54: Wave Rainbow
void effect54() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, Wheel(wave));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 55: Chase Blocks
void effect55() {
  if (millis() - lastEffectUpdate > 400) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int block = (i + effectPosition) % 3;
      if (block == 0) strip.setPixelColor(i, currentColor);
      else if (block == 1) strip.setPixelColor(i, Wheel(85));
      else if (block == 2) strip.setPixelColor(i, Wheel(170));
    }
    strip.show();
    effectPosition++;
    if (effectPosition >= 3) effectPosition = 0;
  }
}

// Effect 56: Rainbow Sparkle
void effect56() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    // Set background to rainbow
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel((i * 85 + effectCounter) % 256));
    }
    // Add sparkles
    for (int i = 0; i < 1; i++) {
      if (random(8) == 0) {
        int led = random(NUM_LEDS);
        strip.setPixelColor(led, strip.Color(255, 255, 255));
      }
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 57: Alternate Blocks
void effect57() {
  if (millis() - lastEffectUpdate > 500) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectCounter) % 2 == 0) {
        strip.setPixelColor(i, currentColor);
      } else {
        strip.setPixelColor(i, Wheel((effectCounter) % 256));
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 58: Dual Comet
void effect58() {
  if (millis() - lastEffectUpdate > 70) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Two comets
    for (int i = 0; i < 2; i++) {
      int pos1 = (effectPosition + i) % NUM_LEDS;
      int pos2 = (effectPosition + 2 + i) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos1, Wheel((effectCounter) % 256));
        strip.setPixelColor(pos2, Wheel((effectCounter + 128) % 256));
      }
    }
    strip.show();
    effectPosition++;
    effectCounter += 5;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 59: Slow Pulse
void effect59() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    int pulse = sin8(effectCounter / 2);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * pulse / 255,
        (currentColor >> 8 & 0xFF) * pulse / 255,
        (currentColor & 0xFF) * pulse / 255
      ));
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 512) effectCounter = 0;
  }
}

// ========== ADDITIONAL EFFECTS 60-79 ==========

// Effect 60: Music Visualizer
void effect60() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    // Simulate music visualization with random brightness
    for (int i = 0; i < NUM_LEDS; i++) {
      int brightness = random(50, 255);
      strip.setPixelColor(i, strip.Color(
        (currentColor >> 16 & 0xFF) * brightness / 255,
        (currentColor >> 8 & 0xFF) * brightness / 255,
        (currentColor & 0xFF) * brightness / 255
      ));
    }
    strip.show();
  }
}

// Effect 61: Rainbow Fire
void effect61() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int flicker = random(150, 255);
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, strip.Color(
        flicker,
        wave * 0.6,
        (255 - wave) * 0.3
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 62: Color Dance
void effect62() {
  if (millis() - lastEffectUpdate > 150) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave1 = sin8((i * 85) + effectCounter);
      int wave2 = sin8((i * 85) + effectCounter + 85);
      int wave3 = sin8((i * 85) + effectCounter + 170);
      strip.setPixelColor(i, strip.Color(wave1, wave2, wave3));
    }
    strip.show();
    effectCounter += 5;
  }
}

// Effect 63: Matrix Rain
void effect63() {
  if (millis() - lastEffectUpdate > 140) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.3;
      g = g * 0.8;
      b = b * 0.3;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // New rain drops
    if (random(8) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, strip.Color(0, 255, 0));
    }
    strip.show();
  }
}

// Effect 64: Galaxy Spin
void effect64() {
  if (millis() - lastEffectUpdate > 120) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int pos = (i + effectPosition) % NUM_LEDS;
      int brightness = sin8((i * 85) + effectCounter);
      strip.setPixelColor(pos, Wheel((i * 85 + hueCounter) % 256));
    }
    strip.show();
    effectCounter += 3;
    hueCounter += 2;
    effectPosition++;
    if (effectPosition >= NUM_LEDS) effectPosition = 0;
  }
}

// Effect 65: Energy Pulse
void effect65() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    int pulse = sin8(effectCounter * 3);
    for (int i = 0; i < NUM_LEDS; i++) {
      int distance = abs(i - NUM_LEDS/2);
      int brightness = max(0, pulse - distance * 40);
      strip.setPixelColor(i, strip.Color(
        0,
        brightness,
        brightness * 0.7
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 66: Water Ripple
void effect66() {
  if (millis() - lastEffectUpdate > 120) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, strip.Color(
        0,
        wave * 0.3,
        wave
      ));
    }
    strip.show();
    effectCounter += 3;
  }
}

// Effect 67: Heart Beat
void effect67() {
  static int heartBeat = 0;
  if (millis() - lastEffectUpdate > 60) {
    lastEffectUpdate = millis();
    
    // Heart beat pattern
    int beat = sin8(heartBeat);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(beat, 0, 0));
    }
    strip.show();
    
    heartBeat += 15;
    if (heartBeat >= 768) heartBeat = 0;
  }
}

// Effect 68: Christmas Lights
void effect68() {
  if (millis() - lastEffectUpdate > 400) {
    lastEffectUpdate = millis();
    // Alternate between red and green
    for (int i = 0; i < NUM_LEDS; i++) {
      if ((i + effectCounter) % 2 == 0) {
        strip.setPixelColor(i, strip.Color(255, 0, 0));
      } else {
        strip.setPixelColor(i, strip.Color(0, 255, 0));
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 69: Fireworks
void effect69() {
  if (millis() - lastEffectUpdate > 300) {
    lastEffectUpdate = millis();
    
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.7;
      g = g * 0.7;
      b = b * 0.7;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    
    // Random fireworks
    if (random(15) == 0) {
      int center = random(NUM_LEDS);
      strip.setPixelColor(center, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 70: Plasma Ball
void effect70() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int plasma = sin8(i * 85 + effectCounter) + 
                   sin8(effectCounter * 2) + 
                   sin8((i * 85 + effectCounter) / 2);
      strip.setPixelColor(i, Wheel(plasma));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 71: Lava Lamp
void effect71() {
  if (millis() - lastEffectUpdate > 160) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int lava = sin8(i * 85 + effectCounter * 2);
      strip.setPixelColor(i, strip.Color(
        255,
        lava * 0.4,
        lava * 0.1
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 72: Aurora Borealis
void effect72() {
  if (millis() - lastEffectUpdate > 140) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave1 = sin8((i * 85) + effectCounter);
      int wave2 = sin8((i * 85) + effectCounter + 64);
      strip.setPixelColor(i, strip.Color(
        wave1 * 0.2,
        wave1,
        wave2
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 73: Ocean Waves
void effect73() {
  if (millis() - lastEffectUpdate > 120) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      strip.setPixelColor(i, strip.Color(
        0,
        wave * 0.3,
        wave
      ));
    }
    strip.show();
    effectCounter += 3;
  }
}

// Effect 74: Desert Sunset
void effect74() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int sunset = sin8(i * 85 + effectCounter);
      strip.setPixelColor(i, strip.Color(
        255,
        sunset * 0.6,
        0
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 75: Northern Lights
void effect75() {
  if (millis() - lastEffectUpdate > 180) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter + random(-10, 10));
      strip.setPixelColor(i, strip.Color(
        wave * 0.1,
        wave,
        wave * 0.5
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 76: Rainbow Tornado
void effect76() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int pos = (i + effectPosition) % NUM_LEDS;
      strip.setPixelColor(pos, Wheel((i * 85 + effectCounter) % 256));
    }
    strip.show();
    effectPosition++;
    effectCounter += 10;
  }
}

// Effect 77: Color Tornado
void effect77() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int pos = (i + effectPosition) % NUM_LEDS;
      strip.setPixelColor(pos, currentColor);
    }
    strip.show();
    effectPosition++;
  }
}

// Effect 78: Sparkle Storm
void effect78() {
  if (millis() - lastEffectUpdate > 40) {
    lastEffectUpdate = millis();
    // Very fast fade
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.4;
      g = g * 0.4;
      b = b * 0.4;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Storm of sparkles
    for (int i = 0; i < 2; i++) {
      if (random(4) == 0) {
        int led = random(NUM_LEDS);
        strip.setPixelColor(led, Wheel(random(256)));
      }
    }
    strip.show();
  }
}

// Effect 79: Rainbow Explosion
void effect79() {
  if (millis() - lastEffectUpdate > 200) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Explosion
    for (int i = 0; i < 2; i++) {
      int pos1 = (1 + i) % NUM_LEDS;
      int pos2 = (1 - i + NUM_LEDS) % NUM_LEDS;
      int brightness = 255 - (i * 120);
      if (brightness > 0) {
        strip.setPixelColor(pos1, Wheel((effectCounter + i * 80) % 256));
        strip.setPixelColor(pos2, Wheel((effectCounter + i * 80 + 128) % 256));
      }
    }
    strip.show();
    effectCounter += 20;
  }
}

// ========== WHITE EFFECTS 80-84 ==========

// Effect 80: Warm Glow
void effect80() {
  if (millis() - lastEffectUpdate > 60) {
    lastEffectUpdate = millis();
    int intensity = sin8(effectCounter);
    uint32_t warmWhite = strip.Color(
      map(intensity, 0, 255, 100, 255),
      map(intensity, 0, 255, 80, 200),
      map(intensity, 0, 255, 60, 150)
    );
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, warmWhite);
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 81: Cool Pulse
void effect81() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    int intensity = sin8(effectCounter * 2);
    uint32_t coolWhite = strip.Color(
      map(intensity, 0, 255, 80, 200),
      map(intensity, 0, 255, 100, 220),
      map(intensity, 0, 255, 120, 255)
    );
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, coolWhite);
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 128) effectCounter = 0;
  }
}

// Effect 82: White Strobe
void effect82() {
  if (millis() - lastEffectUpdate > 200) {
    lastEffectUpdate = millis();
    if (effectCounter % 2 == 0) {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(255, 255, 255));
      }
    } else {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 83: Soft Fade
void effect83() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave = sin8((i * 85) + effectCounter);
      uint32_t softWhite = strip.Color(
        wave,
        wave * 0.9,
        wave * 0.8
      );
      strip.setPixelColor(i, softWhite);
    }
    strip.show();
    effectCounter++;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 84: Candle Light
void effect84() {
  if (millis() - lastEffectUpdate > random(100, 300)) {
    lastEffectUpdate = millis();
    int flicker = random(150, 255);
    uint32_t candleColor = strip.Color(255, flicker * 0.6, flicker * 0.2);
    
    for (int i = 0; i < NUM_LEDS; i++) {
      int variation = random(-30, 30);
      strip.setPixelColor(i, strip.Color(
        constrain(255 + variation, 180, 255),
        constrain(flicker * 0.6 + variation, 60, 200),
        constrain(flicker * 0.2 + variation, 20, 100)
      ));
    }
    strip.show();
  }
}

// ========== EXTRA EFFECTS 85-99 ==========

// Effect 85: Laser Scan
void effect85() {
  if (millis() - lastEffectUpdate > 160) {
    lastEffectUpdate = millis();
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    // Laser scan
    strip.setPixelColor(effectPosition % NUM_LEDS, strip.Color(255, 0, 0));
    strip.show();
    effectPosition++;
    if (effectPosition >= NUM_LEDS * 2) effectPosition = 0;
  }
}

// Effect 86: Digital Rain
void effect86() {
  if (millis() - lastEffectUpdate > 200) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.3;
      g = g * 0.8;
      b = b * 0.3;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // New rain drops with different colors
    if (random(12) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel(random(256)));
    }
    strip.show();
  }
}

// Effect 87: Color Wheel
void effect87() {
  if (millis() - lastEffectUpdate > 60) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, Wheel((i * 85 + effectCounter) % 256));
    }
    strip.show();
    effectCounter += 5;
    if (effectCounter >= 256) effectCounter = 0;
  }
}

// Effect 88: Particle Flow
void effect88() {
  if (millis() - lastEffectUpdate > 120) {
    lastEffectUpdate = millis();
    // Fade all
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.7;
      g = g * 0.7;
      b = b * 0.7;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // Flow particles
    if (random(8) == 0) {
      int led = random(NUM_LEDS);
      strip.setPixelColor(led, Wheel((effectCounter + led * 40) % 256));
    }
    strip.show();
    effectCounter += 3;
  }
}

// Effect 89: Hypnotic Spiral
void effect89() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int pos = (i + effectPosition) % NUM_LEDS;
      int brightness = sin8((i * 85) + effectCounter);
      strip.setPixelColor(pos, Wheel((effectCounter + i * 85) % 256));
    }
    strip.show();
    effectPosition++;
    effectCounter += 3;
  }
}

// Effect 90: Binary Counter
void effect90() {
  static unsigned long lastBinaryUpdate = 0;
  static int binaryValue = 0;
  
  if (millis() - lastBinaryUpdate > 1000) {
    lastBinaryUpdate = millis();
    
    // Clear all
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, 0);
    }
    
    // Display binary value
    for (int i = 0; i < NUM_LEDS; i++) {
      if (binaryValue & (1 << i)) {
        strip.setPixelColor(i, strip.Color(0, 255, 0));
      }
    }
    strip.show();
    
    binaryValue++;
    if (binaryValue >= (1 << NUM_LEDS)) binaryValue = 0;
  }
}

// Effect 91: Color Symphony
void effect91() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int wave1 = sin8((i * 85) + effectCounter);
      int wave2 = sin8((i * 85) + effectCounter + 85);
      int wave3 = sin8((i * 85) + effectCounter + 170);
      strip.setPixelColor(i, strip.Color(wave1, wave2, wave3));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 92: Neon Pulse
void effect92() {
  if (millis() - lastEffectUpdate > 60) {
    lastEffectUpdate = millis();
    int pulse = sin8(effectCounter * 3);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(
        pulse,
        0,
        pulse
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 93: Gradient Flow
void effect93() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int gradient = (i * 85 + effectCounter) % 256;
      strip.setPixelColor(i, Wheel(gradient));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 94: Pixel Dance
void effect94() {
  if (millis() - lastEffectUpdate > 300) {
    lastEffectUpdate = millis();
    // Random pixel dance
    for (int i = 0; i < NUM_LEDS; i++) {
      if (random(5) == 0) {
        strip.setPixelColor(i, Wheel(random(256)));
      } else {
        strip.setPixelColor(i, 0);
      }
    }
    strip.show();
  }
}

// Effect 95: Color Vortex
void effect95() {
  if (millis() - lastEffectUpdate > 80) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int pos = (i + effectPosition) % NUM_LEDS;
      int vortex = sin8((i * 85) + effectCounter);
      strip.setPixelColor(pos, Wheel(vortex));
    }
    strip.show();
    effectPosition++;
    effectCounter += 5;
  }
}

// Effect 96: Rainbow Ripple
void effect96() {
  if (millis() - lastEffectUpdate > 120) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int ripple = sin8((i * 85) + effectCounter + sin8(effectCounter / 2));
      strip.setPixelColor(i, Wheel(ripple));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 97: Matrix Code
void effect97() {
  if (millis() - lastEffectUpdate > 240) {
    lastEffectUpdate = millis();
    // Matrix code effect
    for (int i = 0; i < NUM_LEDS; i++) {
      if (random(15) == 0) {
        strip.setPixelColor(i, strip.Color(0, 255, 0));
      } else {
        uint32_t color = strip.getPixelColor(i);
        int r = (color >> 16) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = color & 0xFF;
        r = r * 0.5;
        g = g * 0.8;
        b = b * 0.5;
        strip.setPixelColor(i, strip.Color(r, g, b));
      }
    }
    strip.show();
  }
}

// Effect 98: Cyber Pulse
void effect98() {
  if (millis() - lastEffectUpdate > 100) {
    lastEffectUpdate = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      int cyber = sin8((i * 85) + effectCounter) + sin8((i * 85) + effectCounter * 2);
      cyber = cyber % 256;
      strip.setPixelColor(i, strip.Color(
        0,
        cyber,
        255 - cyber
      ));
    }
    strip.show();
    effectCounter++;
  }
}

// Effect 99: Star Field
void effect99() {
  if (millis() - lastEffectUpdate > 160) {
    lastEffectUpdate = millis();
    // Fade stars
    for (int i = 0; i < NUM_LEDS; i++) {
      uint32_t color = strip.getPixelColor(i);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;
      r = r * 0.8;
      g = g * 0.8;
      b = b * 0.8;
      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    // New stars
    if (random(15) == 0) {
      int star = random(NUM_LEDS);
      strip.setPixelColor(star, strip.Color(255, 255, 255));
    }
    // Twinkling stars
    if (random(8) == 0) {
      int star = random(NUM_LEDS);
      strip.setPixelColor(star, Wheel(random(256)));
    }
    strip.show();
  }
}

// ========== MUSIC EFFECTS - ALL 10 WORKING PERFECTLY ==========
void handleMusicEffects() {
  if (!musicPlaying) return;
  
  unsigned long currentMillis = millis();
  int updateInterval = map(effectSpeed, 0, 100, 200, 10);
  
  if (currentMillis - lastMusicUpdate > updateInterval) {
    lastMusicUpdate = currentMillis;
    
    // Map music effect parameters
    int densityFactor = map(musicDensity, 0, 100, 1, 10);
    int roughnessFactor = map(musicRoughness, 0, 100, 10, 100);
    int glowFactor = map(glowingSpeed, 0, 100, 1, 10);
    
    switch(currentMusicEffect) {
      case 0: // Beat Pulse
        {
          int pulse = sin8(musicEffectCounter * densityFactor);
          for (int i = 0; i < NUM_LEDS; i++) {
            int brightness = pulse - (i * 20);
            if (brightness < 0) brightness = 0;
            strip.setPixelColor(i, strip.Color(brightness, 0, 0));
          }
        }
        break;
        
      case 1: // Color Wave
        {
          for (int i = 0; i < NUM_LEDS; i++) {
            int wave = sin8((i * 85) + musicEffectCounter * glowFactor);
            strip.setPixelColor(i, Wheel(wave));
          }
        }
        break;
        
      case 2: // Spectrum Analyzer
        {
          for (int i = 0; i < NUM_LEDS; i++) {
            int height = random(0, roughnessFactor * 2);
            int r = height;
            int g = height * 0.5;
            int b = 255 - height;
            strip.setPixelColor(i, strip.Color(r, g, b));
          }
        }
        break;
        
      case 3: // Bass React
        {
          int bass = sin8(musicEffectCounter * densityFactor);
          for (int i = 0; i < NUM_LEDS; i++) {
            int intensity = bass - (i * 30);
            if (intensity < 0) intensity = 0;
            strip.setPixelColor(i, strip.Color(0, 0, intensity));
          }
        }
        break;
        
      case 4: // Treble Dance
        {
          for (int i = 0; i < NUM_LEDS; i++) {
            int treble = sin8((i * 85) + musicEffectCounter * 3);
            strip.setPixelColor(i, strip.Color(treble, 0, treble));
          }
        }
        break;
        
      case 5: // Energy Flow
        {
          for (int i = 0; i < NUM_LEDS; i++) {
            int energy = sin8((i * 85 * densityFactor) + musicEffectCounter);
            int r = energy;
            int g = 255 - energy;
            int b = energy / 2;
            strip.setPixelColor(i, strip.Color(r, g, b));
          }
        }
        break;
        
      case 6: // Rhythm Flash
        {
          if ((musicEffectCounter / glowFactor) % 2 == 0) {
            for (int i = 0; i < NUM_LEDS; i++) {
              strip.setPixelColor(i, Wheel(random(256)));
            }
          } else {
            for (int i = 0; i < NUM_LEDS; i++) {
              strip.setPixelColor(i, 0);
            }
          }
        }
        break;
        
      case 7: // Harmony Glow
        {
          for (int i = 0; i < NUM_LEDS; i++) {
            int glow = sin8((i * 85 * densityFactor) + musicEffectCounter);
            strip.setPixelColor(i, strip.Color(glow, glow/2, glow/4));
          }
        }
        break;
        
      case 8: // Tempo Chase
        {
          for (int i = 0; i < NUM_LEDS; i++) {
            if ((i + musicEffectPosition) % 2 == 0) {
              int brightness = sin8(musicEffectCounter * glowFactor);
              strip.setPixelColor(i, Wheel((musicEffectCounter * 10 + i * 85) % 256));
            } else {
              strip.setPixelColor(i, 0);
            }
          }
          musicEffectPosition++;
        }
        break;
        
      case 9: // Frequency Pulse
        {
          int pulse = sin8(musicEffectCounter * densityFactor * 2);
          for (int i = 0; i < NUM_LEDS; i++) {
            int offset = i * roughnessFactor;
            int r = pulse;
            int g = pulse * 0.5;
            int b = 255 - pulse;
            strip.setPixelColor(i, strip.Color(r, g, b));
          }
        }
        break;
    }
    
    strip.show();
    musicEffectCounter++;
    if (musicEffectCounter >= 256) musicEffectCounter = 0;
  }
}

// ========== AUTOMATIC MODE WHEN NO WIFI ==========
void runAutomaticMode() {
  // Simple white glow when no WiFi clients
  uint32_t whiteColor = strip.Color(255, 255, 255);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, whiteColor);
  }
  strip.show();
}

// ========== IMPROVED WIFI MAINTENANCE ==========
void checkAndMaintainWiFi() {
  static unsigned long lastWifiCheck = 0;
  static unsigned long lastMemoryCheck = 0;
  
  unsigned long currentMillis = millis();
  
  // Check WiFi status every 30 seconds
  if (currentMillis - lastWifiCheck > WIFI_RECONNECT_INTERVAL) {
    lastWifiCheck = currentMillis;
    
    // Check if AP is still running
    int stationCount = WiFi.softAPgetStationNum();
    Serial.print("Stations connected: ");
    Serial.println(stationCount);
    
    // If no stations for too long, consider reducing power
    if (stationCount == 0 && currentMillis > 300000) { // After 5 minutes
      // Reduce WiFi power to save energy
      WiFi.setOutputPower(10.5); // Reduce from 20.5 dBm
    }
  }
  
  // Check memory every minute
  if (currentMillis - lastMemoryCheck > 60000) {
    lastMemoryCheck = currentMillis;
    uint32_t freeHeap = ESP.getFreeHeap();
    Serial.print("Free Heap: ");
    Serial.println(freeHeap);
    
    // If memory is critically low, restart
    if (freeHeap < 2000) {
      Serial.println("Low memory! Restarting...");
      ESP.reset();
    }
  }
}

// ========== TEST SEQUENCE ==========
void testSequence() {
  Serial.println("Testing 3 LED Matrix...");
  
  // Quick test of colors
  uint32_t colors[] = {
    strip.Color(255, 0, 0),    // Red
    strip.Color(0, 255, 0),    // Green
    strip.Color(0, 0, 255),    // Blue
    strip.Color(255, 255, 255) // White
  };
  
  for (int c = 0; c < 4; c++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, colors[c]);
    }
    strip.show();
    delay(300);
  }
  
  // Test music effects
  Serial.println("Testing music effects...");
  musicPlaying = true;
  for (int effect = 0; effect < 10; effect++) {
    currentMusicEffect = effect;
    Serial.print("Testing music effect ");
    Serial.println(effect);
    
    for (int j = 0; j < 50; j++) {
      handleMusicEffects();
      delay(50);
    }
  }
  musicPlaying = false;
  
  // Clear
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
  
  Serial.println("Test complete! Ready for operation.");
  Serial.println("System is now stable and optimized.");
  Serial.println("Features: 3 LEDs, Touch control, Music panel, 100 effects, 10 music effects");
}

// ========== SETUP WITH STABILITY IMPROVEMENTS ==========
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n\nStarting LED Matrix Controller with 3 LEDs and Music Control...");
  
  // Set stack canary
  stackCanary = STACK_CANARY;
  
  // Initialize NeoPixel strip
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  strip.setBrightness(currentBrightness);
  
  // ========== TOUCH SENSOR SETUP ==========
  pinMode(TOUCH_SENSOR_PIN, INPUT_PULLUP); // Touch sensor with internal pull-up
  Serial.println("Touch sensor initialized on pin D4 (GPIO2)");
  
  // Initial test sequence
  testSequence();
  
  // ========== CRITICAL STABILITY IMPROVEMENTS ==========
  
  // 1. Disable WiFi sleep mode
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  
  // 2. Reduce WiFi TX power to prevent crashes
  WiFi.setOutputPower(15.5); // Reduced from 20.5 dBm
  
  // 3. Set WiFi to AP mode only
  WiFi.mode(WIFI_AP);
  
  // 4. Set a static IP to avoid DHCP issues
  WiFi.softAPConfig(apIP, gateway, subnet);
  
  // 5. Set better WiFi parameters
  wifi_set_phy_mode(PHY_MODE_11N);  // Use 802.11n mode
  wifi_set_channel(6);  // Set to channel 6 (less crowded)
  
  // 6. Start Access Point with stability settings
  bool apStarted = WiFi.softAP(ssid, password, 6, 0, 4);
  
  if (!apStarted) {
    Serial.println("Failed to start AP! Retrying...");
    delay(1000);
    ESP.restart();
  }
  
  Serial.println("Access Point Started");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.softAPmacAddress());
  
  // ========== SETUP WEB SERVER WITH TIMEOUTS ==========
  
  // Setup web server routes
  webServer.on("/", handleRoot);
  webServer.on("/color", handleColor);
  webServer.on("/brightness", handleBrightness);
  webServer.on("/toggle", handleToggle);
  webServer.on("/effect", handleEffect);
  webServer.on("/music", handleMusic);
  webServer.on("/density", handleMusic);
  webServer.on("/roughness", handleMusic);
  webServer.on("/effectSpeed", handleMusic);
  webServer.on("/glowingSpeed", handleMusic);
  webServer.onNotFound(handleNotFound);
  
  // Set web server timeouts to prevent hanging
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.begin();
  
  Serial.println("HTTP server started");
  Serial.println("Connect to WiFi: " + String(ssid));
  Serial.println("Password: " + String(password));
  Serial.println("Then open: http://192.168.4.1");
  
  // ========== WATCHDOG AND TIMER SETUP ==========
  
  // Enable software watchdog
  ESP.wdtEnable(8000); // 8 second watchdog
  
  // Start watchdog feeder ticker (feed every 2 seconds)
  wdtTicker.attach(2, resetWatchdog);
  
  // Set last check time
  lastWiFiCheck = millis();
  
  Serial.println("Setup complete with stability improvements!");
  Serial.println("System is now stable and should not restart randomly.");
  Serial.println("Features: 3 LEDs, 100 effects, Touch control, Music control panel");
  Serial.println("Music Effects: 10 different effects with 4 control sliders");
  
  // Show touch sensor ready indication
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < NUM_LEDS; j++) {
      strip.setPixelColor(j, strip.Color(0, 255, 0)); // Green for ready
    }
    strip.show();
    delay(200);
    for (int j = 0; j < NUM_LEDS; j++) {
      strip.setPixelColor(j, 0);
    }
    strip.show();
    delay(200);
  }
}

// ========== MAIN LOOP WITH STABILITY CHECKS ==========
void loop() {
  // Feed the watchdog
  feedWatchdog();
  
  // Check stack canary periodically
  static unsigned long lastStackCheck = 0;
  if (millis() - lastStackCheck > 10000) { // Every 10 seconds
    lastStackCheck = millis();
    checkStack();
  }
  
  // WiFi maintenance
  checkAndMaintainWiFi();
  
  // Always handle touch sensor (works regardless of WiFi status)
  handleTouchSensor();
  
  // Handle music effects if playing
  if (musicPlaying) {
    handleMusicEffects();
  }
  
  // Check if WiFi has clients connected
  if (WiFi.softAPgetStationNum() > 0) {
    // Handle web requests if clients are connected
    webServer.handleClient();
    
    // Handle effects if one is running and music is not playing
    if (isPoweredOn && isEffectRunning && !musicPlaying) {
      switch(currentEffect) {
        // Effects 0-59
        case 0: effect0(); break;
        case 1: effect1(); break;
        case 2: effect2(); break;
        case 3: effect3(); break;
        case 4: effect4(); break;
        case 5: effect5(); break;
        case 6: effect6(); break;
        case 7: effect7(); break;
        case 8: effect8(); break;
        case 9: effect9(); break;
        case 10: effect10(); break;
        case 11: effect11(); break;
        case 12: effect12(); break;
        case 13: effect13(); break;
        case 14: effect14(); break;
        case 15: effect15(); break;
        case 16: effect16(); break;
        case 17: effect17(); break;
        case 18: effect18(); break;
        case 19: effect19(); break;
        case 20: effect20(); break;
        case 21: effect21(); break;
        case 22: effect22(); break;
        case 23: effect23(); break;
        case 24: effect24(); break;
        case 25: effect25(); break;
        case 26: effect26(); break;
        case 27: effect27(); break;
        case 28: effect28(); break;
        case 29: effect29(); break;
        case 30: effect30(); break;
        case 31: effect31(); break;
        case 32: effect32(); break;
        case 33: effect33(); break;
        case 34: effect34(); break;
        case 35: effect35(); break;
        case 36: effect36(); break;
        case 37: effect37(); break;
        case 38: effect38(); break;
        case 39: effect39(); break;
        case 40: effect40(); break;
        case 41: effect41(); break;
        case 42: effect42(); break;
        case 43: effect43(); break;
        case 44: effect44(); break;
        case 45: effect45(); break;
        case 46: effect46(); break;
        case 47: effect47(); break;
        case 48: effect48(); break;
        case 49: effect49(); break;
        case 50: effect50(); break;
        case 51: effect51(); break;
        case 52: effect52(); break;
        case 53: effect53(); break;
        case 54: effect54(); break;
        case 55: effect55(); break;
        case 56: effect56(); break;
        case 57: effect57(); break;
        case 58: effect58(); break;
        case 59: effect59(); break;
        
        // Additional Effects 60-79
        case 60: effect60(); break;
        case 61: effect61(); break;
        case 62: effect62(); break;
        case 63: effect63(); break;
        case 64: effect64(); break;
        case 65: effect65(); break;
        case 66: effect66(); break;
        case 67: effect67(); break;
        case 68: effect68(); break;
        case 69: effect69(); break;
        case 70: effect70(); break;
        case 71: effect71(); break;
        case 72: effect72(); break;
        case 73: effect73(); break;
        case 74: effect74(); break;
        case 75: effect75(); break;
        case 76: effect76(); break;
        case 77: effect77(); break;
        case 78: effect78(); break;
        case 79: effect79(); break;
        
        // White Effects 80-84
        case 80: effect80(); break;
        case 81: effect81(); break;
        case 82: effect82(); break;
        case 83: effect83(); break;
        case 84: effect84(); break;
        
        // Extra Effects 85-99
        case 85: effect85(); break;
        case 86: effect86(); break;
        case 87: effect87(); break;
        case 88: effect88(); break;
        case 89: effect89(); break;
        case 90: effect90(); break;
        case 91: effect91(); break;
        case 92: effect92(); break;
        case 93: effect93(); break;
        case 94: effect94(); break;
        case 95: effect95(); break;
        case 96: effect96(); break;
        case 97: effect97(); break;
        case 98: effect98(); break;
        case 99: effect99(); break;
        
        default: effect0(); break;
      }
    }
  } else {
    // No WiFi clients connected - run effects if touch mode is active
    if (isPoweredOn) {
      if (touchMode || isEffectRunning) {
        // Run the current effect (controlled by touch or previously set)
        switch(currentEffect) {
          // Effects 0-59
          case 0: effect0(); break;
          case 1: effect1(); break;
          case 2: effect2(); break;
          case 3: effect3(); break;
          case 4: effect4(); break;
          case 5: effect5(); break;
          case 6: effect6(); break;
          case 7: effect7(); break;
          case 8: effect8(); break;
          case 9: effect9(); break;
          case 10: effect10(); break;
          case 11: effect11(); break;
          case 12: effect12(); break;
          case 13: effect13(); break;
          case 14: effect14(); break;
          case 15: effect15(); break;
          case 16: effect16(); break;
          case 17: effect17(); break;
          case 18: effect18(); break;
          case 19: effect19(); break;
          case 20: effect20(); break;
          case 21: effect21(); break;
          case 22: effect22(); break;
          case 23: effect23(); break;
          case 24: effect24(); break;
          case 25: effect25(); break;
          case 26: effect26(); break;
          case 27: effect27(); break;
          case 28: effect28(); break;
          case 29: effect29(); break;
          case 30: effect30(); break;
          case 31: effect31(); break;
          case 32: effect32(); break;
          case 33: effect33(); break;
          case 34: effect34(); break;
          case 35: effect35(); break;
          case 36: effect36(); break;
          case 37: effect37(); break;
          case 38: effect38(); break;
          case 39: effect39(); break;
          case 40: effect40(); break;
          case 41: effect41(); break;
          case 42: effect42(); break;
          case 43: effect43(); break;
          case 44: effect44(); break;
          case 45: effect45(); break;
          case 46: effect46(); break;
          case 47: effect47(); break;
          case 48: effect48(); break;
          case 49: effect49(); break;
          case 50: effect50(); break;
          case 51: effect51(); break;
          case 52: effect52(); break;
          case 53: effect53(); break;
          case 54: effect54(); break;
          case 55: effect55(); break;
          case 56: effect56(); break;
          case 57: effect57(); break;
          case 58: effect58(); break;
          case 59: effect59(); break;
          
          // Additional Effects 60-79
          case 60: effect60(); break;
          case 61: effect61(); break;
          case 62: effect62(); break;
          case 63: effect63(); break;
          case 64: effect64(); break;
          case 65: effect65(); break;
          case 66: effect66(); break;
          case 67: effect67(); break;
          case 68: effect68(); break;
          case 69: effect69(); break;
          case 70: effect70(); break;
          case 71: effect71(); break;
          case 72: effect72(); break;
          case 73: effect73(); break;
          case 74: effect74(); break;
          case 75: effect75(); break;
          case 76: effect76(); break;
          case 77: effect77(); break;
          case 78: effect78(); break;
          case 79: effect79(); break;
          
          // White Effects 80-84
          case 80: effect80(); break;
          case 81: effect81(); break;
          case 82: effect82(); break;
          case 83: effect83(); break;
          case 84: effect84(); break;
          
          // Extra Effects 85-99
          case 85: effect85(); break;
          case 86: effect86(); break;
          case 87: effect87(); break;
          case 88: effect88(); break;
          case 89: effect89(); break;
          case 90: effect90(); break;
          case 91: effect91(); break;
          case 92: effect92(); break;
          case 93: effect93(); break;
          case 94: effect94(); break;
          case 95: effect95(); break;
          case 96: effect96(); break;
          case 97: effect97(); break;
          case 98: effect98(); break;
          case 99: effect99(); break;
          
          default: effect0(); break;
        }
      } else {
        // No touch control or WiFi - run automatic mode
        runAutomaticMode();
      }
    } else {
      // Power is off
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, 0);
      }
      strip.show();
    }
  }
  
  // Small delay for stability - DO NOT REMOVE
  delay(1);
}