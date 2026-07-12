# Projektbericht

In diesem Projekt, wurde eine Secure Memory Unit implementiert, die dazu dient:
- Daten verschlüsselt abzuspeichern
- Angriffe durch Address Scrambling abzuwehren
- Die Korrektheit von gespeicherten Daten zu prüfen

## Funktionsweise von Secure Memory Unit

### Secure Memory Unit
Bei einem Schreibzugriff erhält das Modul die vier verschlüsselten Bytes vom und die vier gescrambleten Subadressen vom Secure-Submodul. Es speichert die Bytes jeweils in einer gescrambleten bzw. physischen Subadresse des internen Speichers, wobei die Speicherung der Daten von der Endianness des Systems abhängt. Danach erfolgt die Berechnung der Parity der Daten mit Hilfe des Parity-Submodules.

Bei einem Lesezugriff erhält das Modul die vier gescrambleten Subadressen vom Secure-Submodul, holt die vier verschlüsselten Bytes aus dem Speicher (unter Berücksichtigung der Endianness) und sendet sie an das Secure-Submodul, das sie entschlüsselt. Danach berechnet das Parity-Submodul die Paritäten der entschlüsselten Bytes und überprüft, ob diese mit der gespeicherten Paritätswerte übereinstimmen. Abschließend werden die entsprechenden Daten in rdata geschrieben.

Zudem kann das System in jedem Taktzyklus eine Fault-Injection durchführen – entweder durch Invertieren des Paritätsbits oder eines zufälligen Datenbits – um die Korrektheit der gespeicherten Daten zu überprüfen.

### Secure
Das Secure-Submodul bildet die zentrale Sicherheitskomponente für den Speicherzugriff durch zwei Kernfunktionen:
1. **Address-Scrambling** (start_scramble-Flag)
   Bei aktivem start_scramble-Flag generiert das Modul einen 32-Bit-Scramble-Key. Dieser Schlüssel wird in einen internen Map zusammen mit der logischen Adresse gespeichert. Für Schreib- und Lesezugriffe werden vier physische Adressen erzeugt, indem jede Byte-Adresse mit dem Schlüssel Xored wird. Bei Lesezugriffen wird ein vorhandener Key wiederverwendet oder bei Bedarf neu generiert.
2. **Datenverschlüsselung** (start_read_write-Flag)
   Bei Schreibzugriffen (w_oder_r=1) generiert das Modul einen 8-Bit-Datenschlüssel, speichert ihn in einer separaten internen Map und verschlüsselt die Datenbytes via XOR.
   Bei Lesezugriffen (w_oder_r=0) werden die Bytes mit dem gespeicherten Schlüssel wieder entschlüsselt. Die Steuerung erfolgt durch das start_read_write-Flag.
   Das Modul verwendet einen Linear Congruential Generator (LCG-PRNG) zur Erzeugung aller Schlüssel. [1]

### Parity
Dieses Submodul gewährleistet Datenintegrität durch eine einfache Paritätsprüfung. Bei Schreiboperationen berechnet es das Paritätsbit der Daten und speichert diese zur jeweiligen physischen Adresse. Während Lesevorgängen vergleicht es das aktuell berechnete mit dem gespeicherten Paritätsbit - bei Abweichung wird das parity_error-Signal ausgelöst. Zur Testunterstützung ermöglicht das Modul die gezielte Injektion von Paritätsfehlern durch manuelles Invertieren gespeicherter Paritätsbits.
Die Steuerung erfolgt über die Signale write_enable (Schreibmodus), read_enable (Lesemodus) und start (Auslöser der Operation).





## Literature Recherche

### Begriffe

- **Symmetrische Verschlüsselung**: Der symmetrische Verschlüsselungsalgorithmus ist eine Verschlüsselungsmethode, die einen einzigen Schlüssel zum Verschlüsseln und Entschlüsseln von Daten verwendet. Obwohl sie im Allgemeinen weniger sicher als die asymmetrische Verschlüsselung ist, wird sie oft als effizienter angesehen, da sie weniger Rechenleistung erfordert. [2]
- **XOR-Verschlüsselung**: Die XOR-Verschlüsselung ist eine symmetrische Verschlüsselungsmethode, bei der jedes Bit des Klartextes mit dem entsprechenden Bit des Schlüssels per XOR verknüpft wird. Zur Entschlüsselung wird der gleiche Schlüssel erneut angewendet: Da zweimaliges XOR den Originaltext wiederherstellt, ist die Methode einfach und effizient. [3]
- **Address Scrambling**: Address Scrambling ist die Abbildung sequenzieller logischer Adressen auf nicht-sequenzielle physische Adressen. [4]
- **Pseudo-Random Number Generator**: Pseudo-Random Number Generator ist ein Algorithmus, der mathematische Formeln verwendet, um Sequenzen von Zufallszahlen zu erzeugen. [5]
- **Paritätsbit**: Das Paritätsbit ergänzt Binärdaten um ein Prüfbit, das durch Zählen der '1'-Bits einfache Fehler bei der Datenübertragung aufdeckt. Ein Fehler in der Kommunikation tritt auf, wenn sich eine 1 unerwartet in eine 0 verwandelt oder andersherum. [6]

### Fragen

**Kann mit dem Parity Check jeder Fehler gefunden werden?**
Paritätsbits können nur Fehler erkennen, bei denen eine ungerade Anzahl an Bits verfälscht wird. Wenn eine gerade Anzahl an Bits fehlerhaft ist, bleibt die Parität unverändert, was zu unentdeckten Fehlern führen kann. [7]
*z.B.: Unentdeckte Zwei-Bit-Fehler:*
Sender (Original): 1 0 1 1 0 0 0 1 | 1
Empfänger: 1 0 1 0 1 0 0 1 | 1

**Wie sicher sind die verwendeten Sicherheitsmechanismen? Gibt es mögliche Angriffe dagegen?**

1. Der lineare Kongruenzgenerator (LCG) ist kryptografisch unsicher, weil seine einfache Formel (Xₙ₊₁ = (aXₙ + c) mod m) Angreifern ermöglicht, aus wenigen Werten alle zukünftigen Schlüssel vorherzusagen [8]. Da die Secure-Memory-Unit einen LCG verwendet, können Angreifer über Speicherzugriffe oder bekannte Daten die Verschlüsselungs- und Address-Scrambling-Schlüssel rekonstruieren.

2. Ein 32-Bit-Schlüsselraum (4,3 Mrd. Möglichkeiten) ist zwar groß, bleibt aber bei modernen Angriffsmethoden ebenfalls verwundbar und kann durch Brute-Force leicht geknackt werden.

3. Parity-Bits erkennen zwar Einzelbitfehler, bieten jedoch keinen Schutz gegen geradzahlige Bitfehler. Angreifer können diese Schwachstelle durch gezielte Mehrbit-Manipulation oder Fault-Injection ausnutzen. Für zuverlässige Fehlererkennung und -korrektur sind daher robustere Methoden notwendig:
- Hamming-Codes [9] (erkennen Doppelbitfehler und korrigieren Einzelbitfehler)
- ECC (Error-Correcting Code) [10] (korrigiert Einzelbitfehler, erkennt Mehrbitfehler)
- Reed-Solomon-Codes [11] (korrigieren Mehrbitfehler)



## Mitwirkende
### Mohamed Amine Chouchene
War für die Submodules  verantwortlich.
### Nour Marzouki
War für  run_simulation und die Tests verantwortlich.
### Omayma Yaich
War für das rahmenprogramm verantwortlich.
### Hinweis
Alle haben gleichermaßen zur Implementierung der secure_memory_unit beigetragen.
Diese Aufteilung stellt die Organisation auf dem Papier dar – in der Realität haben wir bei jeder Implementierung zusammengearbeitet und jede Idee gemeinsam diskutiert. Codezeilen wurden erst dann geschrieben, wenn wir uns alle auf den Ansatz geeinigt hatten.




## Quellen
[1] https://en.wikipedia.org/wiki/Linear_congruential_generator
[2] https://www.ibm.com/de-de/think/topics/symmetric-encryption
[3] https://ofi.gbsl.website/26e/Kryptologie/Symmetrisch/xor
[4] https://patents.google.com/patent/US5943283A/en
[5] https://www.geeksforgeeks.org/dsa/pseudo-random-number-generator-prng/
[6] https://newhavendisplay.com/de/blog/parity-bit/
[7] https://www.geeksforgeeks.org/digital-logic/error-detection-codes-parity-bit-method/
[8] https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=ecde7b66d6855f0f0dc530f2b4858c04e9aa94f8 (2.1 Predictableness of LCGs)
[9] https://www.geeksforgeeks.org/computer-networks/hamming-code-in-computer-network/
[10] https://www.vpnunlimited.com/de/help/cybersecurity/error-correcting-code?srsltid=AfmBOopoQrCpqriUUg7tsdn-iTRKkjxjH_f_u5fhcIF3bOmD8FKGwRrZ
[11] https://tu-dresden.de/ing/elektrotechnik/ifn/itml/ressourcen/dateien/lehre/vl/codth/A2_RSC.pdf?lang=en
