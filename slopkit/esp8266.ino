#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <SPI.h>
#include <SD.h>


// ============================================================
// MODGAMES - ESP8266 SD WEB SERVER
// ============================================================
//
// Hardware:
// NodeMCU ESP8266 / ESP-12E
//
// MICRO SD SPI:
//
// SD SCK  -> D5 / GPIO14
// SD MISO -> D6 / GPIO12
// SD MOSI -> D7 / GPIO13
// SD CS   -> D8 / GPIO15   <--- USAMOS EL NÚMERO GPIO 15
// SD VCC  -> 3.3V
// SD GND  -> GND
//
// ============================================================


// ============================================================
// WIFI ACCESS POINT
// ============================================================

const char* AP_SSID = "MODGAMES-SLOPKIT";
const char* AP_PASSWORD = "12345678";


// ============================================================
// WIFI IP
// ============================================================

IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);


// ============================================================
// MICRO SD
// ============================================================

// NodeMCU:
// D5 = GPIO14 = SCK
// D6 = GPIO12 = MISO
// D7 = GPIO13 = MOSI
// D8 = GPIO15 = CS   <--- Definimos directamente el GPIO 15

#define SD_CS 15   // Antes era D8, ahora usamos el número de GPIO


// ============================================================
// WEB SERVER
// ============================================================

ESP8266WebServer server(80);


// ============================================================
// ESTADO SD
// ============================================================

bool sdAvailable = false;


// ============================================================
// OBTENER MIME TYPE
// ============================================================

String getContentType(String path)
{
    path.toLowerCase();


    // HTML
    if (path.endsWith(".html"))
    {
        return "text/html; charset=utf-8";
    }

    if (path.endsWith(".htm"))
    {
        return "text/html; charset=utf-8";
    }


    // CSS
    if (path.endsWith(".css"))
    {
        return "text/css; charset=utf-8";
    }


    // JavaScript
    if (path.endsWith(".js"))
    {
        return "application/javascript; charset=utf-8";
    }


    // JSON
    if (path.endsWith(".json"))
    {
        return "application/json; charset=utf-8";
    }


    // TXT
    if (path.endsWith(".txt"))
    {
        return "text/plain; charset=utf-8";
    }


    // XML
    if (path.endsWith(".xml"))
    {
        return "application/xml; charset=utf-8";
    }


    // PNG
    if (path.endsWith(".png"))
    {
        return "image/png";
    }


    // JPG
    if (path.endsWith(".jpg"))
    {
        return "image/jpeg";
    }


    // JPEG
    if (path.endsWith(".jpeg"))
    {
        return "image/jpeg";
    }


    // GIF
    if (path.endsWith(".gif"))
    {
        return "image/gif";
    }


    // WEBP
    if (path.endsWith(".webp"))
    {
        return "image/webp";
    }


    // SVG
    if (path.endsWith(".svg"))
    {
        return "image/svg+xml";
    }


    // ICO
    if (path.endsWith(".ico"))
    {
        return "image/x-icon";
    }


    // Fuentes
    if (path.endsWith(".woff"))
    {
        return "font/woff";
    }

    if (path.endsWith(".woff2"))
    {
        return "font/woff2";
    }

    if (path.endsWith(".ttf"))
    {
        return "font/ttf";
    }


    // Archivos binarios
    if (path.endsWith(".bin"))
    {
        return "application/octet-stream";
    }

    if (path.endsWith(".elf"))
    {
        return "application/octet-stream";
    }

    if (path.endsWith(".dat"))
    {
        return "application/octet-stream";
    }

    if (path.endsWith(".img"))
    {
        return "application/octet-stream";
    }


    // Default
    return "application/octet-stream";
}


// ============================================================
// VALIDAR RUTA
// ============================================================

bool isSafePath(String path)
{
    // Debe comenzar con /
    if (!path.startsWith("/"))
    {
        return false;
    }


    // Evitar directory traversal
    if (path.indexOf("..") >= 0)
    {
        return false;
    }


    // Evitar backslash
    if (path.indexOf("\\") >= 0)
    {
        return false;
    }


    return true;
}


// ============================================================
// HEADERS HTTP
// ============================================================

void addCommonHeaders()
{
    server.sendHeader(
        "Access-Control-Allow-Origin",
        "*"
    );

    server.sendHeader(
        "Access-Control-Allow-Methods",
        "GET, OPTIONS"
    );

    server.sendHeader(
        "Access-Control-Allow-Headers",
        "*"
    );

    server.sendHeader(
        "Cache-Control",
        "no-cache, no-store, must-revalidate"
    );

    server.sendHeader(
        "Pragma",
        "no-cache"
    );

    server.sendHeader(
        "Expires",
        "0"
    );
}


// ============================================================
// SERVIR ARCHIVO DESDE SD
// ============================================================

bool serveFile(String path)
{
    // --------------------------------------------------------
    // Seguridad
    // --------------------------------------------------------

    if (!isSafePath(path))
    {
        addCommonHeaders();

        server.send(
            400,
            "text/plain; charset=utf-8",
            "Bad Request"
        );

        return true;
    }


    // --------------------------------------------------------
    // Comprobar SD
    // --------------------------------------------------------

    if (!sdAvailable)
    {
        addCommonHeaders();

        server.send(
            503,
            "text/plain; charset=utf-8",
            "SD card unavailable"
        );

        return true;
    }


    // --------------------------------------------------------
    // Comprobar existencia
    // --------------------------------------------------------

    if (!SD.exists(path))
    {
        return false;
    }


    // --------------------------------------------------------
    // Abrir archivo
    // --------------------------------------------------------

    File file = SD.open(
        path,
        FILE_READ
    );


    if (!file)
    {
        return false;
    }


    // --------------------------------------------------------
    // Verificar que no sea directorio
    // --------------------------------------------------------

    if (file.isDirectory())
    {
        file.close();

        return false;
    }


    // --------------------------------------------------------
    // MIME
    // --------------------------------------------------------

    String contentType =
        getContentType(path);


    // --------------------------------------------------------
    // DEBUG
    // --------------------------------------------------------

    Serial.print(
        "[HTTP] Serving: "
    );

    Serial.print(
        path
    );

    Serial.print(
        " | Size: "
    );

    Serial.print(
        file.size()
    );

    Serial.println(
        " bytes"
    );


    // --------------------------------------------------------
    // HEADERS
    // --------------------------------------------------------

    addCommonHeaders();


    // --------------------------------------------------------
    // STREAMING
    // --------------------------------------------------------
    //
    // No cargamos el archivo completo en RAM.
    //
    // Esto es importante para ESP8266.
    //

    server.streamFile(
        file,
        contentType
    );


    // --------------------------------------------------------
    // Cerrar archivo
    // --------------------------------------------------------

    file.close();


    return true;
}


// ============================================================
// SERVIDOR DE ARCHIVOS
// ============================================================

void handleStaticFile()
{
    // --------------------------------------------------------
    // Obtener URI
    // --------------------------------------------------------

    String path =
        server.uri();


    // --------------------------------------------------------
    // ROOT
    // --------------------------------------------------------

    if (
        path == "/" ||
        path.length() == 0
    )
    {
        path = "/index.html";
    }


    // --------------------------------------------------------
    // Eliminar query string
    // --------------------------------------------------------

    int questionMark =
        path.indexOf('?');


    if (questionMark >= 0)
    {
        path = path.substring(
            0,
            questionMark
        );
    }


    // --------------------------------------------------------
    // DEBUG
    // --------------------------------------------------------

    Serial.print(
        "[HTTP] Request: "
    );

    Serial.println(
        path
    );


    // --------------------------------------------------------
    // Intentar servir archivo
    // --------------------------------------------------------

    if (serveFile(path))
    {
        return;
    }


    // --------------------------------------------------------
    // Si es directorio intentar index.html
    // --------------------------------------------------------

    if (path.endsWith("/"))
    {
        String indexPath =
            path + "index.html";


        if (serveFile(indexPath))
        {
            return;
        }
    }


    // --------------------------------------------------------
    // 404
    // --------------------------------------------------------

    addCommonHeaders();

    server.send(
        404,
        "text/plain; charset=utf-8",
        "404 - File not found"
    );


    Serial.print(
        "[HTTP] 404: "
    );

    Serial.println(
        path
    );
}


// ============================================================
// API STATUS
// ============================================================

void handleStatus()
{
    String json = "{";


    // Estado
    json += "\"status\":\"online\",";


    // Dispositivo
    json += "\"device\":\"ESP8266\",";


    // SSID
    json += "\"wifi_ssid\":\"";

    json += AP_SSID;

    json += "\",";


    // IP
    json += "\"ip\":\"";

    json += WiFi.softAPIP().toString();

    json += "\",";


    // SD
    json += "\"sd\":\"";

    if (sdAvailable)
    {
        json += "ok";
    }
    else
    {
        json += "error";
    }

    json += "\"";


    json += "}";


    // Respuesta
    addCommonHeaders();

    server.send(
        200,
        "application/json; charset=utf-8",
        json
    );
}


// ============================================================
// PÁGINA DE INFORMACIÓN
// ============================================================

void handleInfo()
{
    String html;


    html += "<!DOCTYPE html>";

    html += "<html>";

    html += "<head>";

    html +=
        "<meta charset='UTF-8'>";

    html +=
        "<meta name='viewport' content='width=device-width,initial-scale=1'>";

    html +=
        "<title>MODGAMES ESP8266</title>";

    html += "</head>";


    html += "<body>";


    html +=
        "<h1>MODGAMES ESP8266</h1>";


    html +=
        "<p>Servidor web activo.</p>";


    html +=
        "<p>IP: ";

    html +=
        WiFi.softAPIP().toString();

    html +=
        "</p>";


    html +=
        "<p>SSID: ";

    html +=
        AP_SSID;

    html +=
        "</p>";


    html +=
        "<p>MicroSD: ";


    if (sdAvailable)
    {
        html += "OK";
    }
    else
    {
        html += "ERROR";
    }


    html += "</p>";


    html +=
        "<p>";

    html +=
        "<a href='/api/status'>";

    html +=
        "API Status";

    html +=
        "</a>";

    html +=
        "</p>";


    html +=
        "<p>";

    html +=
        "<a href='/'>";

    html +=
        "Inicio";

    html +=
        "</a>";

    html +=
        "</p>";


    html += "</body>";

    html += "</html>";


    addCommonHeaders();


    server.send(
        200,
        "text/html; charset=utf-8",
        html
    );
}


// ============================================================
// OPTIONS
// ============================================================

void handleOptions()
{
    addCommonHeaders();

    server.send(
        204
    );
}


// ============================================================
// 404
// ============================================================

void handleNotFound()
{
    handleStaticFile();
}


// ============================================================
// INICIALIZAR MICROSD
// ============================================================

bool initSD()
{
    Serial.println();

    Serial.println(
        "================================"
    );

    Serial.println(
        "[SD] Initializing microSD..."
    );

    Serial.println(
        "================================"
    );


    // --------------------------------------------------------
    // SPI
    // --------------------------------------------------------
    //
    // ESP8266 NodeMCU:
    //
    // SCK  = D5 / GPIO14
    // MISO = D6 / GPIO12
    // MOSI = D7 / GPIO13
    //
    // SD CS = D8 / GPIO15
    //

    SPI.begin();


    // --------------------------------------------------------
    // Inicializar SD
    // --------------------------------------------------------

    if (!SD.begin(SD_CS))
    {
        Serial.println(
            "[SD] ERROR: SD.begin() failed."
        );

        Serial.println(
            "[SD] Check SD wiring."
        );

        return false;
    }


    // --------------------------------------------------------
    // SD OK
    // --------------------------------------------------------

    Serial.println(
        "[SD] SD initialization OK."
    );


    // --------------------------------------------------------
    // Buscar index.html
    // --------------------------------------------------------

    if (SD.exists("/index.html"))
    {
        Serial.println(
            "[SD] /index.html FOUND"
        );
    }
    else
    {
        Serial.println(
            "[SD] WARNING:"
        );

        Serial.println(
            "[SD] /index.html NOT FOUND"
        );
    }


    return true;
}


// ============================================================
// INICIALIZAR WIFI
// ============================================================

void initWiFi()
{
    Serial.println();

    Serial.println(
        "================================"
    );

    Serial.println(
        "[WiFi] Starting Access Point..."
    );

    Serial.println(
        "================================"
    );


    // --------------------------------------------------------
    // Modo AP
    // --------------------------------------------------------

    WiFi.mode(
        WIFI_AP
    );


    // --------------------------------------------------------
    // Configuración IP
    // --------------------------------------------------------

    bool configOK =
        WiFi.softAPConfig(
            apIP,
            apGateway,
            apSubnet
        );


    if (!configOK)
    {
        Serial.println(
            "[WiFi] WARNING:"
        );

        Serial.println(
            "[WiFi] softAPConfig failed."
        );
    }


    // --------------------------------------------------------
    // Crear Access Point
    // --------------------------------------------------------

    bool apOK =
        WiFi.softAP(
            AP_SSID,
            AP_PASSWORD
        );


    if (!apOK)
    {
        Serial.println(
            "[WiFi] ERROR:"
        );

        Serial.println(
            "[WiFi] Failed to start AP."
        );

        return;
    }


    delay(500);


    // --------------------------------------------------------
    // Información WiFi
    // --------------------------------------------------------

    Serial.println();

    Serial.print(
        "[WiFi] SSID: "
    );

    Serial.println(
        AP_SSID
    );


    Serial.print(
        "[WiFi] Password: "
    );

    Serial.println(
        AP_PASSWORD
    );


    Serial.print(
        "[WiFi] IP: "
    );

    Serial.println(
        WiFi.softAPIP()
    );


    Serial.print(
        "[WiFi] MAC: "
    );

    Serial.println(
        WiFi.softAPmacAddress()
    );
}


// ============================================================
// INICIALIZAR WEB SERVER
// ============================================================

void initWebServer()
{
    // --------------------------------------------------------
    // API STATUS
    // --------------------------------------------------------

    server.on(
        "/api/status",
        HTTP_GET,
        handleStatus
    );


    // --------------------------------------------------------
    // INFO
    // --------------------------------------------------------

    server.on(
        "/info",
        HTTP_GET,
        handleInfo
    );


    // --------------------------------------------------------
    // OPTIONS
    // --------------------------------------------------------

    server.on(
        "/api/status",
        HTTP_OPTIONS,
        handleOptions
    );


    // --------------------------------------------------------
    // Archivos estáticos
    // --------------------------------------------------------

    server.onNotFound(
        handleNotFound
    );


    // --------------------------------------------------------
    // Iniciar servidor
    // --------------------------------------------------------

    server.begin();


    Serial.println();

    Serial.println(
        "[HTTP] Web server started."
    );

    Serial.println(
        "[HTTP] Port: 80"
    );

    Serial.println(
        "[HTTP] URL:"
    );

    Serial.println(
        "http://192.168.4.1/"
    );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // --------------------------------------------------------
    // SERIAL
    // --------------------------------------------------------

    Serial.begin(
        115200
    );


    delay(1000);


    // --------------------------------------------------------
    // MENSAJE INICIAL
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "================================================"
    );

    Serial.println(
        "       MODGAMES ESP8266 SD WEB SERVER"
    );

    Serial.println(
        "================================================"
    );

    Serial.println();


    // --------------------------------------------------------
    // INICIALIZAR SD
    // --------------------------------------------------------

    sdAvailable =
        initSD();


    // --------------------------------------------------------
    // INICIALIZAR WIFI
    // --------------------------------------------------------

    initWiFi();


    // --------------------------------------------------------
    // INICIALIZAR WEB SERVER
    // --------------------------------------------------------

    initWebServer();


    // --------------------------------------------------------
    // ESTADO FINAL
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "================================================"
    );


    if (sdAvailable)
    {
        Serial.println(
            "STATUS: READY"
        );
    }
    else
    {
        Serial.println(
            "STATUS: WIFI READY / SD ERROR"
        );
    }


    Serial.println();


    Serial.print(
        "SSID: "
    );

    Serial.println(
        AP_SSID
    );


    Serial.print(
        "PASSWORD: "
    );

    Serial.println(
        AP_PASSWORD
    );


    Serial.println(
        "IP: 192.168.4.1"
    );


    Serial.println(
        "WEB: http://192.168.4.1/"
    );


    Serial.println(
        "INFO: http://192.168.4.1/info"
    );


    Serial.println(
        "STATUS: http://192.168.4.1/api/status"
    );


    Serial.println(
        "================================================"
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // Procesar peticiones HTTP
    server.handleClient();


    // Pequeña pausa
    delay(1);
}