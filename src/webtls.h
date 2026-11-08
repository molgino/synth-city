// https fuer die Handy-App: eigene Zertifizierungsstelle des Geraets + TLS-Weiterleitung auf den Webserver
#pragma once
bool webTlsStart(const char* basePath, int plainPort);   // im Hintergrund: OpenSSL laden, Zertifikate, Port 443 (sonst 8443)
int webTlsPort();                                        // 0 = laeuft (noch) nicht
const char* webTlsStatus();                              // kurzer Zustandstext (Einstellungen, /cert)
const char* webTlsDir();                                 // Ordner mit ca.der/ca.pem (fuer /cert)
