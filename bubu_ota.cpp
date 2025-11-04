#include "bubu_ota.h"
#include "ota_anim.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_wifi.h>

#define BUBU_FW_VERSION "0.3.0"
static const char* MANIFEST_URL_DEFAULT = "https://raw.githubusercontent.com/hoangtalu/Bubu-OTA/main/latest.json";

// --- internals ---
namespace {
  Adafruit_GC9A01A* tft = nullptr;
  WebServer server(80);
  DNSServer dns;
  Preferences prefs;

  bool uiActive = false;        // owns screen
  bool configMode = false;      // AP portal
  bool autoOtaRan = false;

  String otaUrl;
  String m_version, m_fwUrl, m_sha256;

  uint32_t lastUiTick = 0;
  uint32_t lastStatusTick = 0;

  // HTML: single big button
  const char CONFIG_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BUBU Update</title>
<style>
body{font-family:system-ui,Arial;margin:0;background:#0e0f12;color:#fff;display:flex;min-height:100vh;align-items:center;justify-content:center}
.btn{width:240px;height:240px;border-radius:999px;background:radial-gradient(circle at 30% 30%,#1be6c0,#119b86);display:flex;align-items:center;justify-content:center;font-weight:800;font-size:26px;letter-spacing:1px;border:none;box-shadow:0 20px 60px rgba(0,0,0,.4);cursor:pointer}
.btn:active{transform:translateY(1px)}
.help{position:fixed;bottom:14px;left:0;right:0;text-align:center;color:#9aa; font:12px system-ui}
.hidden{display:none}
</style></head><body>
<button class="btn" id="go">UPDATE</button>
<div class="help">If mobile data isn’t available while connected to BUBU, use <b>Upload</b> below.</div>
<form id="upForm" class="hidden" action="/upload" method="POST" enctype="multipart/form-data">
  <input type="file" name="bin" accept=".bin" />
  <input type="submit" value="Upload & Install">
</form>
<script>
document.getElementById('go').addEventListener('click', async ()=>{
  try{
    const r = await fetch('/fetch', {method:'POST'});
    const t = await r.text();
    if (!r.ok) throw new Error(t);
    alert('Downloading + installing… keep this page open.');
  }catch(e){
    alert('Direct fetch failed. Try the Upload below.');
    document.getElementById('upForm').classList.remove('hidden');
  }
});
</script>
</body></html>
)HTML";

  // -------- helpers --------
  static void anim(OtaPhase p){ OTAAnim_start(p); uiActive = true; }
  static void setProgress(float p){ OTAAnim_setProgress(p); }

  String jsonGet(const String& json, const char* key){
    String pat = String("\"")+key+"\"";
    int k = json.indexOf(pat); if (k<0) return "";
    int c = json.indexOf(':',k); if (c<0) return "";
    int q1= json.indexOf('"',c+1); if (q1<0) return "";
    int q2= json.indexOf('"',q1+1); if (q2<0) return "";
    return json.substring(q1+1,q2);
  }

  bool httpsGet(const String& url, String& body, int& contentLen){
    WiFiClientSecure cli; cli.setInsecure();
    HTTPClient http; http.setTimeout(20000);
#if defined(HTTPC_STRICT_FOLLOW_REDIRECTS)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
    if (!http.begin(cli,url)) return false;
    int code = http.GET();
    if (code!=HTTP_CODE_OK){ http.end(); return false; }
    contentLen = http.getSize();
    body = http.getString();
    http.end();
    return true;
  }

  bool fetchManifest(){
    if (otaUrl.isEmpty()) return false;
    anim(OtaPhase::CHECKING);
    String body; int len=-1;
    if (!httpsGet(otaUrl, body, len)) return false;
    m_version = jsonGet(body,"version");
    m_fwUrl   = jsonGet(body,"url");
    m_sha256  = jsonGet(body,"sha256");
    return !m_version.isEmpty() && !m_fwUrl.isEmpty();
  }

  bool installFromUrl(const String& url){
    WiFiClientSecure cli; cli.setInsecure();
    HTTPClient http; http.setTimeout(30000);
#if defined(HTTPC_STRICT_FOLLOW_REDIRECTS)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
#endif
    if (!http.begin(cli,url)) return false;
    int code = http.GET();
    if (code!=HTTP_CODE_OK){ http.end(); return false; }

    int contentLen = http.getSize();
    size_t beginSize = (contentLen>0) ? (size_t)contentLen : UPDATE_SIZE_UNKNOWN;

    anim(OtaPhase::FLASHING);
    if (!Update.begin(beginSize)){ http.end(); return false; }

    WiFiClient* s = http.getStreamPtr();
    const size_t BUFSZ = 4096;
    uint8_t buf[BUFSZ];
    size_t written = 0, total = 0;
    uint32_t lastProg = millis();

    while (true){
      int n = s->readBytes(buf, BUFSZ);
      if (n<=0) break;
      if (Update.write(buf, n) != (size_t)n){ http.end(); Update.abort(); return false; }
      total += n;
      if (contentLen>0){
        float p = (float)total / (float)contentLen;
        setProgress(p);
      }
      uint32_t now=millis();
      if (now-lastProg>80){ lastProg=now; OTAAnim_drawFrame(); }
      yield();
    }
    http.end();

    if (Update.hasError()){ Update.abort(); return false; }
    if (!Update.end(true)) return false;

    anim(OtaPhase::OK);
    uint32_t t0=millis(); while (millis()-t0<600){ OTAAnim_drawFrame(); }
    ESP.restart();
    return true;
  }

  // -------- web handlers --------
  void handleRoot(){ server.send(200, "text/html", CONFIG_HTML); uiActive=true; }

  void handleFetch(){
    if (!WiFi.isConnected()){ server.send(503,"text/plain","No internet"); return; }
    if (!fetchManifest()){ server.send(500,"text/plain","Manifest error"); return; }
    anim(OtaPhase::DOWNLOADING);
    OTAAnim_drawFrame();
    bool ok = installFromUrl(m_fwUrl);
    if (!ok){ anim(OtaPhase::ERROR_); server.send(500,"text/plain","Install failed"); return; }
    server.send(200,"text/plain","OK");
  }

  // multipart upload -> /upload
  File uploadFile;
  void handleUpload(){
    HTTPUpload& up = server.upload();
    if (up.status == UPLOAD_FILE_START){
      anim(OtaPhase::FLASHING);
      OTAAnim_drawFrame();
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)){ server.send(500,"text/plain","Begin failed"); return; }
    } else if (up.status == UPLOAD_FILE_WRITE){
      if (Update.write(up.buf, up.currentSize) != up.currentSize){
        Update.abort(); server.send(500,"text/plain","Write failed"); return;
      }
      setProgress(0.5f); OTAAnim_drawFrame(); // coarse pulse
    } else if (up.status == UPLOAD_FILE_END){
      if (Update.hasError() || !Update.end(true)){ server.send(500,"text/plain","End failed"); return; }
      anim(OtaPhase::OK);
      server.send(200,"text/plain","OK");
      uint32_t t0=millis(); while (millis()-t0<600){ OTAAnim_drawFrame(); }
      ESP.restart();
    }
  }

  void startPortalInternal(){
    if (configMode) return;
    WiFi.mode(WIFI_AP_STA);
    uint8_t mac[6]; WiFi.macAddress(mac);
    char apName[32]; snprintf(apName,sizeof(apName),"BUBU-Setup-%02X%02X%02X",mac[3],mac[4],mac[5]);
    WiFi.softAP(apName);

    // captive DNS
    IPAddress ip = WiFi.softAPIP();
    dns.setTTL(1);
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", ip);

    server.on("/", handleRoot);
    server.on("/fetch", HTTP_POST, handleFetch);
    server.on(
      "/upload", HTTP_POST,
      [](){ server.send(200,"text/plain","Upload done"); },
      handleUpload
    );
    server.onNotFound(handleRoot);
    server.begin();

    configMode = true;
    uiActive = true;
    OTAAnim_start(OtaPhase::WIFI_CONNECTING);
  }

  void runAutoOtaOnce(){
    if (autoOtaRan) return;
    autoOtaRan = true;

    prefs.begin("sys", true);
    otaUrl = prefs.getString("ota_url", "");
    prefs.end();
    if (otaUrl.isEmpty()) otaUrl = MANIFEST_URL_DEFAULT;

    if (!WiFi.isConnected()) return;

    // auto-OTA path
    OTAAnim_start(OtaPhase::CHECKING);
    if (!fetchManifest()) return;

    if (String(BUBU_FW_VERSION) == m_version) return; // already latest
    OTAAnim_start(OtaPhase::DOWNLOADING);
    OTAAnim_drawFrame();
    (void)installFromUrl(m_fwUrl);
  }
} // anon

// ------- public API -------
namespace BubuOTA {
  void setManifestURL(const char* url){ otaUrl = url ? url : ""; }

  void begin(Adafruit_GC9A01A* display, int pinSCK, int pinMOSI, int pinCS){
    tft = display;

    // start the animation engine and show something right away
    OTAAnim_begin(tft, 240, 240);
    uiActive = true;
    OTAAnim_start(OtaPhase::WIFI_CONNECTING);

    // If already online (e.g., your main code connected Wi-Fi), do auto-OTA.
    // Otherwise, bring up the portal AP immediately so you see “Sign in to Wi-Fi…”
    if (WiFi.isConnected()) {
      runAutoOtaOnce();
    } else {
      startPortalInternal();   // creates BUBU-Setup-XXXX AP and serves the UPDATE page
    }
  }

  void startPortal(){ startPortalInternal(); }

  bool busy(){ return uiActive; }

  void loop(){
    // draw animation when active
    if (uiActive){
      OTAAnim_drawFrame();
    }

    // handle portal
    if (configMode){
      dns.processNextRequest();
      server.handleClient();
    }

    // if Wi-Fi connected, try once
    static bool tried = false;
    if (WiFi.isConnected() && !tried){
      tried = true;
      runAutoOtaOnce();
    }
  }
}