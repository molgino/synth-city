// https fuer die Handy-App (siehe webtls.h).
// Die App laeuft auf https (GitHub Pages) und darf nur per https mit dem Geraet sprechen. Das Geraet erzeugt dazu
// beim ersten Start eine eigene Zertifizierungsstelle (CA, gilt per nameConstraints nur fuer private Netze und *.local)
// und damit ein Server-Zertifikat fuer seine Adressen. Das iPhone vertraut der CA nach einmaligem Installieren des
// Profils (/cert). Port 443 (sonst 8443) nimmt TLS an und reicht jede Verbindung an den normalen Webserver weiter.
// OpenSSL wird zur Laufzeit geladen (libssl.so.3 Groovebox-OS, .1.1 ArkOS): kein Build-Abhaengigkeit, fehlt es,
// gibt es eben kein https.
#include "webtls.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef void* P;
static struct {
    // libcrypto
    P (*EC_KEY_new_by_curve_name)(int); int (*EC_KEY_generate_key)(P); void (*EC_KEY_set_asn1_flag)(P, int);
    P (*EVP_PKEY_new)(); int (*EVP_PKEY_assign)(P, int, P); void (*EVP_PKEY_free)(P);
    P (*X509_new)(); void (*X509_free)(P); int (*X509_set_version)(P, long);
    P (*X509_get_serialNumber)(P); int (*ASN1_INTEGER_set)(P, long); P (*ASN1_TIME_set)(P, time_t);
    P (*X509_getm_notBefore)(P); P (*X509_getm_notAfter)(P);
    int (*X509_set_pubkey)(P, P); P (*X509_get_subject_name)(P); int (*X509_set_issuer_name)(P, P);
    int (*X509_NAME_add_entry_by_txt)(P, const char*, int, const unsigned char*, int, int, int);
    void (*X509V3_set_ctx)(P, P, P, P, P, int); P (*X509V3_EXT_conf_nid)(P, P, int, const char*);
    int (*X509_add_ext)(P, P, int); void (*X509_EXTENSION_free)(P);
    int (*X509_sign)(P, P, P); P (*EVP_sha256)();
    P (*BIO_new_file)(const char*, const char*); int (*BIO_free)(P);
    int (*PEM_write_bio_X509)(P, P); int (*PEM_write_bio_PrivateKey)(P, P, P, unsigned char*, int, P, P);
    P (*PEM_read_bio_X509)(P, P*, P, P); P (*PEM_read_bio_PrivateKey)(P, P*, P, P);
    int (*i2d_X509)(P, unsigned char**);
    // libssl
    P (*TLS_server_method)(); P (*SSL_CTX_new)(P); void (*SSL_CTX_free)(P); long (*SSL_CTX_ctrl)(P, int, long, P);
    int (*SSL_CTX_use_certificate)(P, P); int (*SSL_CTX_use_PrivateKey)(P, P); int (*SSL_CTX_check_private_key)(P);
    P (*SSL_new)(P); int (*SSL_set_fd)(P, int); int (*SSL_accept)(P); int (*SSL_read)(P, void*, int);
    int (*SSL_write)(P, const void*, int); int (*SSL_pending)(P); int (*SSL_shutdown)(P); void (*SSL_free)(P);
} O;

enum { NID_P256 = 415, PKEY_EC = 408, MBSTR_ASC = 0x1001, NID_BASIC = 87, NID_KEYUSE = 83, NID_EKU = 126, NID_SAN = 85,
       NID_SKI = 82, NID_AKI = 90, NID_NAMECONS = 666, CTRL_CHAIN_CERT = 89, CTRL_MIN_PROTO = 123, TLS12 = 0x0303 };

static char g_status[160] = "https: startet ...";
static volatile int g_port = 0;
static char g_dir[600];
static int g_plain = 80;
static P g_ctx = nullptr, g_caCert = nullptr, g_caKey = nullptr;
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static char g_san[600] = "";
static time_t g_leafUntil = 0;

const char* webTlsStatus() { return g_status; }
int webTlsPort() { return g_port; }
const char* webTlsDir() { return g_dir; }

static bool loadOpenSSL() {
    static const char* const ssl[] = {"libssl.so.3", "libssl.so.1.1", "libssl.so"};
    static const char* const cry[] = {"libcrypto.so.3", "libcrypto.so.1.1", "libcrypto.so"};
    P hs = nullptr, hc = nullptr;
    for (int i = 0; i < 3 && !hs; i++) { hc = dlopen(cry[i], RTLD_NOW | RTLD_GLOBAL); if (hc) hs = dlopen(ssl[i], RTLD_NOW | RTLD_GLOBAL); }
    if (!hs || !hc) return false;
    bool ok = true;
#define C(n) ok = (*(P*)&O.n = dlsym(hc, #n)) && ok; if (!O.n) fprintf(stderr, "https: %s fehlt\n", #n);
#define S(n) ok = (*(P*)&O.n = dlsym(hs, #n)) && ok; if (!O.n) fprintf(stderr, "https: %s fehlt\n", #n);
    C(EC_KEY_new_by_curve_name) C(EC_KEY_generate_key) C(EC_KEY_set_asn1_flag) C(EVP_PKEY_new) C(EVP_PKEY_assign) C(EVP_PKEY_free)
    C(X509_new) C(X509_free) C(X509_set_version) C(X509_get_serialNumber) C(ASN1_INTEGER_set) C(ASN1_TIME_set)
    C(X509_getm_notBefore) C(X509_getm_notAfter) C(X509_set_pubkey) C(X509_get_subject_name) C(X509_set_issuer_name)
    C(X509_NAME_add_entry_by_txt) C(X509V3_set_ctx) C(X509V3_EXT_conf_nid) C(X509_add_ext) C(X509_EXTENSION_free)
    C(X509_sign) C(EVP_sha256) C(BIO_new_file) C(BIO_free) C(PEM_write_bio_X509) C(PEM_write_bio_PrivateKey)
    C(PEM_read_bio_X509) C(PEM_read_bio_PrivateKey) C(i2d_X509)
    S(TLS_server_method) S(SSL_CTX_new) S(SSL_CTX_free) S(SSL_CTX_ctrl) S(SSL_CTX_use_certificate) S(SSL_CTX_use_PrivateKey)
    S(SSL_CTX_check_private_key) S(SSL_new) S(SSL_set_fd) S(SSL_accept) S(SSL_read) S(SSL_write) S(SSL_pending) S(SSL_shutdown) S(SSL_free)
#undef C
#undef S
    return ok;
}

// ---------------------------------------------------------------- Zertifikate
static time_t buildTime() {   // Untergrenze fuer "jetzt": die Uhr des R36S kann ohne Netz falsch stehen
    static const char* const mon = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char m[4] = {0}; int d = 1, y = 2026;
    sscanf(__DATE__, "%3s %d %d", m, &d, &y);
    struct tm t; memset(&t, 0, sizeof t);
    t.tm_mon = (int)((strstr(mon, m) ? strstr(mon, m) - mon : 0) / 3); t.tm_mday = d; t.tm_year = y - 1900; t.tm_hour = 12;
    return timegm(&t);
}
static time_t nowSane() { time_t n = time(nullptr), b = buildTime(); return n > b ? n : b; }

static P genKey() {
    P ec = O.EC_KEY_new_by_curve_name(NID_P256);
    if (!ec) return nullptr;
    O.EC_KEY_set_asn1_flag(ec, 1);   // benannte Kurve
    if (!O.EC_KEY_generate_key(ec)) return nullptr;
    P k = O.EVP_PKEY_new();
    O.EVP_PKEY_assign(k, PKEY_EC, ec);
    return k;
}
static bool addExt(P x, P issuer, int nid, const char* v) {
    alignas(16) unsigned char ctx[1024]; memset(ctx, 0, sizeof ctx);   // X509V3_CTX (Groesse je Version verschieden, hier reichlich)
    O.X509V3_set_ctx(ctx, issuer, x, nullptr, nullptr, 0);
    P e = O.X509V3_EXT_conf_nid(nullptr, ctx, nid, v);
    if (!e) { fprintf(stderr, "https: Erweiterung %d '%s' abgelehnt\n", nid, v); return false; }
    O.X509_add_ext(x, e, -1); O.X509_EXTENSION_free(e);
    return true;
}
// ca = true: CA (issuer = selbst); sonst Server-Zertifikat mit san, unterschrieben von g_caCert/g_caKey
static P makeCert(P key, bool ca, const char* cn, const char* san, time_t from, time_t until) {
    P x = O.X509_new();
    O.X509_set_version(x, 2);
    long serial = 0; FILE* r = fopen("/dev/urandom", "rb"); if (r) { if (fread(&serial, sizeof serial, 1, r) != 1) serial = (long)time(nullptr); fclose(r); }
    O.ASN1_INTEGER_set(O.X509_get_serialNumber(x), (serial & 0x7fffffffL) | 1);
    O.ASN1_TIME_set(O.X509_getm_notBefore(x), from);
    O.ASN1_TIME_set(O.X509_getm_notAfter(x), until);
    O.X509_set_pubkey(x, key);
    P nm = O.X509_get_subject_name(x);
    O.X509_NAME_add_entry_by_txt(nm, "O", MBSTR_ASC, (const unsigned char*)"SYNTH CITY", -1, -1, 0);
    O.X509_NAME_add_entry_by_txt(nm, "CN", MBSTR_ASC, (const unsigned char*)cn, -1, -1, 0);
    P iss = ca ? x : g_caCert;
    O.X509_set_issuer_name(x, O.X509_get_subject_name(iss));
    bool ok = true;
    if (ca) {
        ok = addExt(x, iss, NID_BASIC, "critical,CA:TRUE,pathlen:0") && ok;
        ok = addExt(x, iss, NID_KEYUSE, "critical,keyCertSign,cRLSign") && ok;
        ok = addExt(x, iss, NID_SKI, "hash") && ok;
        // nur private Netze und .local: eine eingeschleuste Webseite koennte mit dieser CA sonst beliebige Seiten vortaeuschen
        ok = addExt(x, iss, NID_NAMECONS, "critical,permitted;IP:10.0.0.0/255.0.0.0,permitted;IP:172.16.0.0/255.240.0.0,"
                                          "permitted;IP:192.168.0.0/255.255.0.0,permitted;IP:127.0.0.0/255.0.0.0,permitted;DNS:local") && ok;
    } else {
        ok = addExt(x, iss, NID_BASIC, "critical,CA:FALSE") && ok;
        ok = addExt(x, iss, NID_KEYUSE, "critical,digitalSignature,keyEncipherment") && ok;
        ok = addExt(x, iss, NID_EKU, "serverAuth") && ok;
        ok = addExt(x, iss, NID_SKI, "hash") && ok;
        ok = addExt(x, iss, NID_AKI, "keyid") && ok;
        ok = addExt(x, iss, NID_SAN, san) && ok;
    }
    if (!ok || !O.X509_sign(x, ca ? key : g_caKey, O.EVP_sha256())) { O.X509_free(x); return nullptr; }
    return x;
}
static bool writePem(const char* name, P x, P key) {
    char p[700]; snprintf(p, sizeof p, "%s/%s", g_dir, name);
    P b = O.BIO_new_file(p, "w");
    if (!b) return false;
    bool ok = x ? O.PEM_write_bio_X509(b, x) == 1 : O.PEM_write_bio_PrivateKey(b, key, nullptr, nullptr, 0, nullptr, nullptr) == 1;
    O.BIO_free(b);
    if (key) chmod(p, 0600);
    return ok;
}
static P readPem(const char* name, bool key) {
    char p[700]; snprintf(p, sizeof p, "%s/%s", g_dir, name);
    P b = O.BIO_new_file(p, "r");
    if (!b) return nullptr;
    P r = key ? O.PEM_read_bio_PrivateKey(b, nullptr, nullptr, nullptr) : O.PEM_read_bio_X509(b, nullptr, nullptr, nullptr);
    O.BIO_free(b);
    return r;
}
static bool writeDer(P x) {   // fuer das iPhone-Profil (/cert)
    unsigned char* der = nullptr;
    int n = O.i2d_X509(x, &der);
    if (n <= 0) return false;
    char p[700]; snprintf(p, sizeof p, "%s/ca.der", g_dir);
    FILE* f = fopen(p, "wb");
    bool ok = f && fwrite(der, 1, (size_t)n, f) == (size_t)n;
    if (f) fclose(f);
    free(der);   // OpenSSL nutzt malloc (OPENSSL_free) - bei Standard-Allokator gleich
    return ok;
}
static bool loadOrMakeCa() {
    g_caKey = readPem("ca.key", true);
    g_caCert = readPem("ca.pem", false);
    if (g_caKey && g_caCert) return true;
    printf("https: erzeuge die Zertifizierungsstelle des Geraets\n");
    g_caKey = genKey();
    if (!g_caKey) return false;
    time_t b = buildTime();
    g_caCert = makeCert(g_caKey, true, "SYNTH CITY Geraet (R36S)", nullptr, b - 400L * 86400, b + 20L * 365 * 86400);
    if (!g_caCert) return false;
    return writePem("ca.key", nullptr, g_caKey) && writePem("ca.pem", g_caCert, nullptr) && writeDer(g_caCert);
}

// Adressen des Geraets (USB 192.168.77.1 immer, dazu WLAN/LAN) -> SAN
static void currentSan(char* out, int n) {
    char ips[16][INET_ADDRSTRLEN]; int k = 0;
    snprintf(ips[k++], INET_ADDRSTRLEN, "192.168.77.1");
    struct ifaddrs* ia = nullptr;
    if (getifaddrs(&ia) == 0) {
        for (struct ifaddrs* a = ia; a && k < 16; a = a->ifa_next) {
            if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
            uint32_t ip = ntohl(((struct sockaddr_in*)a->ifa_addr)->sin_addr.s_addr);
            bool priv = (ip >> 24) == 10 || (ip >> 20) == (172u << 4 | 1) || (ip >> 16) == (192u << 8 | 168) || (ip >> 24) == 127;
            if (!priv) continue;   // nur private Netze (so beschraenkt die CA), oeffentliche Adressen wuerden die Kette ungueltig machen
            char s[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &((struct sockaddr_in*)a->ifa_addr)->sin_addr, s, sizeof s);
            bool dup = false; for (int i = 0; i < k; i++) if (!strcmp(ips[i], s)) dup = true;
            if (!dup) snprintf(ips[k++], INET_ADDRSTRLEN, "%s", s);
        }
        freeifaddrs(ia);
    }
    for (int i = 1; i < k; i++) for (int j = i + 1; j < k; j++) if (strcmp(ips[i], ips[j]) > 0) { char t[INET_ADDRSTRLEN]; memcpy(t, ips[i], sizeof t); memcpy(ips[i], ips[j], sizeof t); memcpy(ips[j], t, sizeof t); }
    int l = snprintf(out, n, "DNS:synth-city.local");
    for (int i = 0; i < k && l < n; i++) l += snprintf(out + l, n - l, ",IP:%s", ips[i]);
}

// Server-Zertifikat (neu, wenn sich die Adressen geaendert haben oder es bald ablaeuft) + TLS-Kontext
static bool refreshLeaf() {
    char san[600]; currentSan(san, sizeof san);
    time_t now = nowSane();
    if (g_ctx && !strcmp(san, g_san) && now < g_leafUntil - 60L * 86400) return true;
    P key = nullptr, x = nullptr;
    char saved[600] = "";
    {   // gespeichertes Zertifikat passt noch?
        char p[700]; snprintf(p, sizeof p, "%s/server.san", g_dir);
        FILE* f = fopen(p, "r"); long until = 0;
        if (f) { if (fscanf(f, "%ld %599s", &until, saved) != 2) saved[0] = 0; fclose(f); }
        if (!strcmp(saved, san) && now < until - 60L * 86400) { key = readPem("server.key", true); x = readPem("server.pem", false); g_leafUntil = until; }
    }
    if (!key || !x) {
        key = genKey();
        if (!key) return false;
        time_t from = now - 2L * 86400, until = now + 800L * 86400;   // iOS: hoechstens 825 Tage
        x = makeCert(key, false, "SYNTH CITY R36S", san, from, until);
        if (!x) return false;
        writePem("server.key", nullptr, key); writePem("server.pem", x, nullptr);
        char p[700]; snprintf(p, sizeof p, "%s/server.san", g_dir);
        FILE* f = fopen(p, "w"); if (f) { fprintf(f, "%ld %s\n", (long)until, san); fclose(f); }
        g_leafUntil = until;
        printf("https: neues Zertifikat fuer %s\n", san);
    }
    P ctx = O.SSL_CTX_new(O.TLS_server_method());
    if (!ctx) return false;
    O.SSL_CTX_ctrl(ctx, CTRL_MIN_PROTO, TLS12, nullptr);
    bool ok = O.SSL_CTX_use_certificate(ctx, x) == 1 && O.SSL_CTX_use_PrivateKey(ctx, key) == 1 && O.SSL_CTX_check_private_key(ctx) == 1;
    if (ok) O.SSL_CTX_ctrl(ctx, CTRL_CHAIN_CERT, 1, g_caCert);   // CA mitschicken (add1: zaehlt selbst hoch)
    O.X509_free(x); O.EVP_PKEY_free(key);   // der Kontext haelt eigene Referenzen
    if (!ok) { O.SSL_CTX_free(ctx); return false; }
    pthread_mutex_lock(&g_mx);
    P old = g_ctx; g_ctx = ctx;
    snprintf(g_san, sizeof g_san, "%s", san);
    pthread_mutex_unlock(&g_mx);
    if (old) O.SSL_CTX_free(old);   // laufende Verbindungen halten eigene Referenzen
    return true;
}

// ---------------------------------------------------------------- Verbindungen: TLS <-> Webserver (127.0.0.1)
static volatile int g_conns = 0;
static bool sendAllFd(int fd, const char* p, int n) {
    while (n > 0) { ssize_t w = send(fd, p, (size_t)n, MSG_NOSIGNAL); if (w <= 0) return false; p += w; n -= (int)w; }
    return true;
}
static void* connThread(void* arg) {
    int fd = (int)(intptr_t)arg;
    struct timeval tv = {20, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    pthread_mutex_lock(&g_mx);
    P s = g_ctx ? O.SSL_new(g_ctx) : nullptr;
    pthread_mutex_unlock(&g_mx);
    int up = -1;
    if (s && O.SSL_set_fd(s, fd) == 1 && O.SSL_accept(s) == 1) {
        up = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons((uint16_t)g_plain); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (up >= 0 && connect(up, (struct sockaddr*)&a, sizeof a) == 0) {
            static __thread char buf[16384];
            for (;;) {
                bool tlsReady = O.SSL_pending(s) > 0;
                struct pollfd pf[2] = {{fd, POLLIN, 0}, {up, POLLIN, 0}};
                if (!tlsReady && poll(pf, 2, 60000) <= 0) break;
                if (tlsReady || (pf[0].revents & (POLLIN | POLLHUP | POLLERR))) {
                    int n = O.SSL_read(s, buf, sizeof buf);
                    if (n <= 0 || !sendAllFd(up, buf, n)) break;
                }
                if (pf[1].revents & (POLLIN | POLLHUP | POLLERR)) {
                    ssize_t n = recv(up, buf, sizeof buf, 0);
                    if (n <= 0) break;
                    int off = 0;
                    while (off < n) { int w = O.SSL_write(s, buf + off, (int)n - off); if (w <= 0) { off = -1; break; } off += w; }
                    if (off < 0) break;
                }
            }
        }
    }
    if (s) { O.SSL_shutdown(s); O.SSL_free(s); }
    if (up >= 0) close(up);
    close(fd);
    __sync_fetch_and_sub(&g_conns, 1);
    return nullptr;
}

static void* tlsMain(void*) {
    mkdir(g_dir, 0700);
    if (!loadOpenSSL()) { snprintf(g_status, sizeof g_status, "https: OpenSSL fehlt"); printf("%s\n", g_status); return nullptr; }
    if (!loadOrMakeCa() || !refreshLeaf()) { snprintf(g_status, sizeof g_status, "https: Zertifikat fehlgeschlagen"); printf("%s\n", g_status); return nullptr; }
    int ls = -1, port = 0;
    static const int ports[2] = {443, 8443};
    for (int i = 0; i < 2 && ls < 0; i++) {
        ls = socket(AF_INET, SOCK_STREAM, 0);
        int one = 1; setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        struct sockaddr_in a; memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons((uint16_t)ports[i]); a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(ls, (struct sockaddr*)&a, sizeof a) || listen(ls, 16)) { close(ls); ls = -1; } else port = ports[i];
    }
    if (ls < 0) { snprintf(g_status, sizeof g_status, "https: kein Port frei"); printf("%s\n", g_status); return nullptr; }
    g_port = port;
    snprintf(g_status, sizeof g_status, "https aktiv, Port %d", port);
    printf("%s (%s)\n", g_status, g_san); fflush(stdout);
    time_t lastCheck = time(nullptr);
    for (;;) {
        struct pollfd pf = {ls, POLLIN, 0};
        int r = poll(&pf, 1, 15000);
        if (time(nullptr) - lastCheck >= 15) { lastCheck = time(nullptr); refreshLeaf(); }   // WLAN-Adresse neu? -> neues Zertifikat
        if (r <= 0) continue;
        int fd = accept(ls, nullptr, nullptr);
        if (fd < 0) continue;
        if (g_conns >= 12) { close(fd); continue; }
        __sync_fetch_and_add(&g_conns, 1);
        pthread_t t; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256 * 1024);
        pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&t, &at, connThread, (void*)(intptr_t)fd)) { close(fd); __sync_fetch_and_sub(&g_conns, 1); }
        pthread_attr_destroy(&at);
    }
    return nullptr;
}

bool webTlsStart(const char* basePath, int plainPort) {
    if (plainPort <= 0) return false;
    struct stat st;   // Groovebox-OS: bei den Nutzerdaten (/data/config), uebersteht App-Updates
    if (stat("/data/config", &st) == 0 && S_ISDIR(st.st_mode)) snprintf(g_dir, sizeof g_dir, "/data/config/tls");
    else snprintf(g_dir, sizeof g_dir, "%stls", basePath);
    g_plain = plainPort;
    pthread_t t;
    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    bool ok = pthread_create(&t, &at, tlsMain, nullptr) == 0;
    pthread_attr_destroy(&at);
    return ok;
}
