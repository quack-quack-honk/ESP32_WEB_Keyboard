#include <WiFi.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include "USBHIDConsumerControl.h"
#include <DNSServer.h>
#include <WebServer.h>
#include <SD_MMC.h>
#include "pin_config.h"

const char* ssid = "ESP32-Interface";
const char* password = "12345678"; // Consider making this stronger
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1); // Access point IP address
IPAddress netMsk(255, 255, 255, 0); // Network mask

DNSServer dnsServer;
WebServer server(80);
USBHIDKeyboard keyboard;
USBHIDMouse mouse;
USBHIDConsumerControl consumerControl;
unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_MS = 5000;

// MIME types for different file extensions
struct {
    const char* extension;
    const char* mimeType;
} mimeTypes[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".mjs", "application/javascript"},
    {".javascript", "application/javascript"},
    {".json", "application/json"},
    {".ico", "image/x-icon"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".xml", "application/xml"},
    {".txt", "text/plain"}
};

void handleNotFound() {
    String requestedFile = server.uri();
    String hostHeader = server.hostHeader();
    
    Serial.print("Requested URI: ");
    Serial.println(requestedFile);
    Serial.print("Host header: ");
    Serial.println(hostHeader);

    // First try to serve the actual requested file
    if (handleFileRead(requestedFile)) {
        return;
    }

    // For captive portal detection and unknown hosts
    if (requestedFile == "/generate_204" || 
        requestedFile == "/ncsi.txt" ||
        requestedFile == "/mobile/status.php" ||
        requestedFile == "/success.txt" ||
        requestedFile.endsWith(".php") ||
        hostHeader != apIP.toString() ||
        requestedFile == "/favicon.ico" ||
        requestedFile == "/") {
        
        if (handleFileRead("/index.html")) {
            return;
        }
    }

    // If we get here, no file was served
    server.send(404, "text/plain", "File not found");
}

String getContentType(String filename) {
    for (auto& mt : mimeTypes) {
        if (filename.endsWith(mt.extension)) {
            return mt.mimeType;
        }
    }
    return "text/plain";
}

bool handleFileRead(String path) {
    Serial.print("handleFileRead: ");
    Serial.println(path);

    if (path.endsWith("/")) {
        path += "index.html";
    }
    
    // Remove any query parameters
    int queryIndex = path.indexOf('?');
    if (queryIndex != -1) {
        path = path.substring(0, queryIndex);
    }
    
    // Make sure the path starts with /
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    
    String contentType = getContentType(path);
    Serial.print("Content-Type: ");
    Serial.println(contentType);

    // Try to open the file
    if (!SD_MMC.exists(path.c_str())) {
        Serial.println("File does not exist: " + path);
        return false;
    }

    File file = SD_MMC.open(path.c_str(), "r");
    if (!file) {
        Serial.println("Failed to open file: " + path);
        return false;
    }

    // Set headers for proper file serving
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    
    Serial.print("Streaming file: ");
    Serial.println(path);
    
    size_t sent = server.streamFile(file, contentType);
    file.close();
    
    Serial.print("Sent bytes: ");
    Serial.println(sent);
    
    return true;
}

void handleCommand() {
    String command = server.arg("plain");
    if (command.length() < 3) {
        server.send(400, "text/plain", "error:Invalid command format\n");
        return;
    }

    char type = command.charAt(0);
    int firstColon = command.indexOf(':');
    int secondColon = command.indexOf(':', firstColon + 1);

    if (firstColon == -1 || firstColon >= command.length() - 1) {
        server.send(400, "text/plain", "error:Missing first colon or value\n");
        return;
    }

    String data = command.substring(firstColon + 1);
    String response;    switch (type) {
        case 'k': { // Keyboard command
            if (secondColon == -1 || secondColon >= command.length() - 1) {
                server.send(400, "text/plain", "error:Invalid keyboard command\n");
                return;
            }            int keyCode = data.substring(0, secondColon - firstColon - 1).toInt();
            bool pressed = data.substring(secondColon - firstColon).toInt() == 1;
            
            // Handle special keys and modifiers
            if (keyCode >= 128 && keyCode <= 135) { // Modifier keys (Left Ctrl to Right GUI)
                if (pressed) {
                    keyboard.press(keyCode);
                } else {
                    keyboard.release(keyCode);
                }
            } else if (keyCode >= 0x20 && keyCode <= 0x7E) { // ASCII printable characters
                // This includes all letters, numbers, and symbols
                if (pressed) {
                    keyboard.write(keyCode);
                }            } else if (keyCode >= 194 && keyCode <= 205) { // F1-F12
                if (pressed) {
                    keyboard.press(KEY_F1 + (keyCode - 194));
                } else {
                    keyboard.release(KEY_F1 + (keyCode - 194));
                }            } else if (keyCode >= 215 && keyCode <= 219) { // Arrow keys
                uint8_t arrowKey;
                switch (keyCode) {
                    case 216: arrowKey = KEY_LEFT_ARROW; break;  // Left
                    case 215: arrowKey = KEY_RIGHT_ARROW; break; // Right
                    case 218: arrowKey = KEY_UP_ARROW; break;    // Up
                    case 219: arrowKey = KEY_DOWN_ARROW; break;  // Down
                    default: return;
                }
                if (pressed) {
                    keyboard.press(arrowKey);
                } else {
                    keyboard.release(arrowKey);
                }
            } else if (keyCode == 209) { // Insert
                if (pressed) keyboard.press(KEY_INSERT);
                else keyboard.release(KEY_INSERT);
            } else if (keyCode == 210) { // Home
                if (pressed) keyboard.press(KEY_HOME);
                else keyboard.release(KEY_HOME);
            } else if (keyCode == 211) { // Page Up
                if (pressed) keyboard.press(KEY_PAGE_UP);
                else keyboard.release(KEY_PAGE_UP);
            } else if (keyCode == 212) { // Delete
                if (pressed) keyboard.press(KEY_DELETE);
                else keyboard.release(KEY_DELETE);
            } else if (keyCode == 213) { // End
                if (pressed) keyboard.press(KEY_END);
                else keyboard.release(KEY_END);
            } else if (keyCode == 214) { // Page Down
                if (pressed) keyboard.press(KEY_PAGE_DOWN);
                else keyboard.release(KEY_PAGE_DOWN);
            } else if (keyCode >= 112 && keyCode <= 123) { // Old F1-F12 codes (keeping for compatibility)
                if (pressed) keyboard.press(KEY_F1 + (keyCode - 112));
                else keyboard.release(KEY_F1 + (keyCode - 112));    
            } else if (keyCode == 8) { // Backspace
                if (pressed) keyboard.write(KEY_BACKSPACE);
            } else if (keyCode == 9) { // Tab
                if (pressed) keyboard.write(KEY_TAB);
            } else if (keyCode == 13) { // Enter
                if (pressed) keyboard.write(KEY_RETURN);
            } else if (keyCode == 27) { // ESC
                if (pressed) keyboard.write(KEY_ESC);
            } else if (keyCode == 20) { // Caps Lock
                if (pressed) keyboard.write(KEY_CAPS_LOCK);
                if (pressed) {
                    keyboard.write(keyCode);
                }            } else if (keyCode == 173) { // Mute
                if (pressed) consumerControl.press(CONSUMER_CONTROL_MUTE);
                else consumerControl.release();
            } else if (keyCode == 174) { // Volume Down
                if (pressed) consumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
                else consumerControl.release();
            } else if (keyCode == 175) { // Volume Up
                if (pressed) consumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
                else consumerControl.release();
            } else if (keyCode == 176) { // Next Track
                if (pressed) consumerControl.press(CONSUMER_CONTROL_SCAN_NEXT);
                else consumerControl.release();
            } else if (keyCode == 177) { // Previous Track
                if (pressed) consumerControl.press(CONSUMER_CONTROL_SCAN_PREVIOUS);
                else consumerControl.release();
            } else if (keyCode == 178) { // Stop
                if (pressed) consumerControl.press(CONSUMER_CONTROL_STOP);
                else consumerControl.release();
            } else if (keyCode == 179) { // Play/Pause
                if (pressed) consumerControl.press(CONSUMER_CONTROL_PLAY_PAUSE);
                else consumerControl.release();            } else if (keyCode == 220) { // Mouse Scroll Up
                if (pressed) mouse.move(0, 0, 1);
            } else if (keyCode == 221) { // Mouse Scroll Down
                if (pressed) mouse.move(0, 0, -1);} else if (keyCode >= 0x20 && keyCode <= 0x7E) { // ASCII printable characters
                if (pressed) {
                    keyboard.write(keyCode);
                }
            } else { // Other keys like Enter, Backspace, etc.
                if (pressed) {
                    keyboard.press(keyCode);
                } else {
                    keyboard.release(keyCode);
                }
            }
            response = "ok:key:" + String(keyCode) + ":" + String(pressed) + "\n";
            break;
        }
        case 'm': { // Mouse move
            if (secondColon == -1 || secondColon >= command.length() - 1) {
                server.send(400, "text/plain", "error:Invalid mouse move command\n");
                return;
            }
            int x = data.substring(0, secondColon - firstColon - 1).toInt();
            int y = data.substring(secondColon - firstColon).toInt();
            mouse.move(x, y);
            response = "ok:move:" + String(x) + ":" + String(y) + "\n";
            break;
        }
        case 'b': { // Mouse button
            if (secondColon == -1 || secondColon >= command.length() - 1) {
                server.send(400, "text/plain", "error:Invalid mouse button command\n");
                return;
            }
            int button = data.substring(0, secondColon - firstColon - 1).toInt();
            bool pressed = data.substring(secondColon - firstColon).toInt() == 1;
            if (pressed) {
                mouse.press(button);
            } else {
                mouse.release(button);
            }
            response = "ok:button:" + String(button) + ":" + String(pressed) + "\n";
            break;
        }
        default:
            server.send(400, "text/plain", "error:Unknown command type\n");
            return;
    }
    
    server.send(200, "text/plain", response);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting WiFi AP, SD Card, and HID");
      // Initialize USB HID
    USB.begin();
    keyboard.begin();
    mouse.begin();
    consumerControl.begin();

    // Initialize SD Card
    SD_MMC.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);
    if (!SD_MMC.begin()) {
        Serial.println("SD Card Mount Failed");
        return;
    }
    
    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD Card attached");
        return;
    }    // Set up Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(ssid, password);
    // Disable WIFI power saving
    WiFi.setSleep(false);
    
    // Print network info
    Serial.println("WiFi AP Started");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
      // Configure web server
    server.on("/api/command", HTTP_POST, handleCommand);
    
    // Use handleNotFound for all file requests
    server.onNotFound(handleNotFound);

    server.begin();
    
    // Start DNS server
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS Server started");
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
}
