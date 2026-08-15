#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

// ============================================================
// MODGAMES - ESP32 DevKit V1
// WiFi Access Point + microSD + HTTP Web Server
// ============================================================

// ============================================================
// CONFIGURACIÓN WIFI
// ============================================================

const char* AP_SSID     = "MODGAMES-SLOPKIT";
const char* AP_PASSWORD = "12345678";

IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);

// ============================================================
// CONFIGURACIÓN MICROSD
// ============================================================
//
// ESP32 DevKit V1 - SPI VSPI
// SCK  -> GPIO 18
// MISO -> GPIO 19
// MOSI -> GPIO 23
// CS   -> GPIO 5
//

#define SD_SCK   18
#define SD_MISO  19
#define SD_MOSI  23
#define SD_CS    5

// ============================================================
// OBJETOS
// ============================================================

SPIClass sdSPI(VSPI);
WebServer server(80);

// Indicador de que la SD está operativa
bool sdOK = false;

// ============================================================
// OBTENER MIME TYPE
// ============================================================

String getContentType(String path) {
    path.toLowerCase();

    if (path.endsWith(".html")) return "text/html; charset=utf-8";
    if (path.endsWith(".htm"))  return "text/html; charset=utf-8";
    if (path.endsWith(".css"))  return "text/css; charset=utf-8";
    if (path.endsWith(".js"))   return "application/javascript; charset=utf-8";
    if (path.endsWith(".json")) return "application/json; charset=utf-8";
    if (path.endsWith(".txt"))  return "text/plain; charset=utf-8";
    if (path.endsWith(".xml"))  return "application/xml; charset=utf-8";

    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".jpg"))  return "image/jpeg";
    if (path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".gif"))  return "image/gif";
    if (path.endsWith(".webp")) return "image/webp";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".ico"))  return "image/x-icon";

    if (path.endsWith(".bin"))  return "application/octet-stream";
    if (path.endsWith(".elf"))  return "application/octet-stream";

    return "application/octet-stream";
}

// ============================================================
// SEGURIDAD DE RUTA
// ============================================================

bool isSafePath(String path) {
    if (!path.startsWith("/")) return false;
    if (path.indexOf("..") >= 0) return false;
    if (path.indexOf("\\") >= 0) return false;
    return true;
}

// ============================================================
// HEADERS HTTP (comunes)
// ============================================================

void addCommonHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
}

// ============================================================
// SERVIR ARCHIVO DESDE LA SD
// ============================================================

bool serveFile(String path) {
    if (!isSafePath(path)) {
        addCommonHeaders();
        server.send(400, "text/plain; charset=utf-8", "Bad request");
        return true;
    }

    if (!sdOK || !SD.exists(path)) {
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return false;
    }

    addCommonHeaders();
    server.streamFile(file, getContentType(path));
    file.close();
    return true;
}

// ============================================================
// MANEJADOR DE ARCHIVOS ESTÁTICOS
// ============================================================

void handleStaticFile() {
    String path = server.uri();

    if (path == "/" || path.length() == 0) {
        path = "/index.html";
    }

    // Eliminar query string
    int qm = path.indexOf('?');
    if (qm >= 0) path = path.substring(0, qm);

    Serial.print("[HTTP] ");
    Serial.print(server.method() == HTTP_GET ? "GET " : "REQUEST ");
    Serial.println(path);

    if (serveFile(path)) return;

    // Si termina en "/", probar con index.html
    if (path.endsWith("/")) {
        String indexPath = path + "index.html";
        if (serveFile(indexPath)) return;
    }

    addCommonHeaders();
    server.send(404, "text/plain; charset=utf-8", "404 - File not found");
}

// ============================================================
// API STATUS
// ============================================================

void handleStatus() {
    String json = "{";
    json += "\"status\":\"online\",";
    json += "\"wifi_ssid\":\"" + String(AP_SSID) + "\",";
    json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";

    // Verificar SD de forma segura
    if (!sdOK || SD.cardType() == CARD_NONE) {
        json += "\"sd\":\"error\",";
        json += "\"sd_size_mb\":0,";
        json += "\"sd_used_mb\":0,";
        json += "\"sd_total_mb\":0";
    } else {
        json += "\"sd\":\"ok\",";
        uint64_t cardSize = SD.cardSize();
        uint64_t used = SD.usedBytes();
        uint64_t total = SD.totalBytes();
        json += "\"sd_size_mb\":" + String((uint32_t)(cardSize / (1024ULL * 1024ULL))) + ",";
        json += "\"sd_used_mb\":"  + String((uint32_t)(used    / (1024ULL * 1024ULL))) + ",";
        json += "\"sd_total_mb\":" + String((uint32_t)(total   / (1024ULL * 1024ULL)));
    }

    json += "}";

    addCommonHeaders();
    server.send(200, "application/json; charset=utf-8", json);
}

// ============================================================
// MANEJO DE OPTIONS (CORS)
// ============================================================

void handleOptions() {
    addCommonHeaders();
    server.send(204);
}

// ============================================================
// 404 (redirige a estáticos)
// ============================================================

void handleNotFound() {
    handleStaticFile();
}

// ============================================================
// INICIALIZAR MICROSD
// ============================================================

bool initSD() {
    Serial.println();
    Serial.println("[SD] Initializing...");

    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, sdSPI, 20000000)) {
        Serial.println("[SD] ERROR: SD.begin() failed.");
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] ERROR: No card detected.");
        return false;
    }

    Serial.print("[SD] Card type: ");
    if (cardType == CARD_MMC)      Serial.println("MMC");
    else if (cardType == CARD_SD)  Serial.println("SDSC");
    else if (cardType == CARD_SDHC)Serial.println("SDHC");
    else                           Serial.println("UNKNOWN");

    Serial.print("[SD] Size: ");
    Serial.print((uint32_t)(SD.cardSize() / (1024ULL * 1024ULL)));
    Serial.println(" MB");

    Serial.print("[SD] Used: ");
    Serial.print((uint32_t)(SD.usedBytes() / (1024ULL * 1024ULL)));
    Serial.println(" MB");

    Serial.print("[SD] Total filesystem: ");
    Serial.print((uint32_t)(SD.totalBytes() / (1024ULL * 1024ULL)));
    Serial.println(" MB");

    if (!SD.exists("/index.html")) {
        Serial.println("[SD] WARNING: /index.html not found.");
        Serial.println("[SD] Copy the SlopKit files to the SD root.");
    } else {
        Serial.println("[SD] /index.html OK.");
    }

    return true;
}

// ============================================================
// INICIALIZAR WIFI ACCESS POINT
// ============================================================

void initWiFiAP() {
    Serial.println();
    Serial.println("[WiFi] Starting Access Point...");

    WiFi.mode(WIFI_AP);

    if (!WiFi.softAPConfig(apIP, apGateway, apSubnet)) {
        Serial.println("[WiFi] WARNING: softAPConfig failed.");
    }

    if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
        Serial.println("[WiFi] ERROR: Failed to start AP.");
        return;
    }

    delay(500);

    Serial.print("[WiFi] SSID: ");
    Serial.println(AP_SSID);
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("[WiFi] MAC: ");
    Serial.println(WiFi.softAPmacAddress());
}

// ============================================================
// INICIALIZAR WEB SERVER
// ============================================================

void initWebServer() {
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/status", HTTP_OPTIONS, handleOptions);
    server.onNotFound(handleNotFound);

    server.begin();

    Serial.println();
    Serial.println("[HTTP] Server started on port 80.");
    Serial.println("[HTTP] Open: http://192.168.4.1/");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("       MODGAMES ESP32 SD WEB SERVER");
    Serial.println("==============================================");

    sdOK = initSD();
    initWiFiAP();
    initWebServer();

    Serial.println();
    Serial.println("==============================================");
    if (sdOK) {
        Serial.println("STATUS: READY");
    } else {
        Serial.println("STATUS: WIFI READY / SD ERROR");
    }
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("PASSWORD: ");
    Serial.println(AP_PASSWORD);
    Serial.println("WEB: http://192.168.4.1/");
    Serial.println("STATUS: http://192.168.4.1/api/status");
    Serial.println("==============================================");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    server.handleClient();
    delay(1);
}