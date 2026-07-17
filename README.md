# NFC / EMV Card Reader — ESP8266 + PN532

A learning-oriented firmware that turns an **ESP8266 (NodeMCU)** and a **PN532** NFC module into a dual-mode contactless reader. It can:

- Talk to **EMV payment cards** (physical cards *and* Apple Pay) over ISO 14443-4 / APDU, and parse the account number and expiry out of the card's records.
- Read plain **ISO 14443A tags** (MIFARE / NTAG21x) — grabbing the UID from tag memory.

The point of this project is to **understand the protocol plumbing** behind contactless cards — APDUs, BER-TLV parsing, the PPSE → SELECT AID → GPO → READ RECORD flow, and how payment tokenization (Apple Pay) changes what you actually see on the wire.

---

## ⚠️ Responsible use — please read first

This firmware reads data from contactless payment cards. Before you use it:

- **Only read cards you own or have explicit permission to test.** Reading someone else's card without consent is unlawful in most jurisdictions.
- This is an **educational / research project**, not a payment terminal and not a security product.
- It **cannot** be used to make payments, clone a card, or move money. The card's cryptographic keys never leave its secure element; a genuine transaction requires a per-tap cryptogram (ARQC) that this code neither has nor requests. What you can read here is limited to data the card exposes in the clear.
- If you fork or extend this, keep it lawful and keep it ethical.

*This repository was published by its author with the data they were permitted to share; see the security notes below for what was intentionally left out.*

---

## 🔒 Security notes — what is **not** safe for production

This code is deliberately simple so the protocol is easy to follow. Several parts are **fine for a bench demo but must not be treated as secure**:

| Area | What the code does | Why it's not production-safe |
|------|--------------------|------------------------------|
| **Handling of card data (PAN/DPAN)** | Reads the account number into a `String`, compares it, and can publish it over MQTT | Real cardholder-data handling falls under **PCI-DSS**. You should not store or transmit a full account number in the clear. Treat any capture here as sensitive and ephemeral. |
| **Hard-coded credential** | `StoredPAN` holds a reference number the reader matches against | Baking a real card/device number into source is a data-exposure risk. **Keep this a placeholder**, never a real value, in any public copy. |
| **MQTT publishing** | Sends the parsed result as a plaintext message | No transport encryption, no auth shown. Anything published this way is readable on the wire. Use TLS + auth if you ever wire this up for real. |
| **UID-based identity** | Uses the tag UID as an identifier | UIDs are not secret and can be spoofed on programmable tags. Never use a bare UID as an access credential in anything that matters. |

If you adapt this into an actual access-control system, the identity layer needs rotating tokens / challenge-response and an authenticated backend — none of which is implemented here.

---

## 💳 A note on Apple Pay & tokenization (the interesting part)

When you tap a **physical card**, you read its real account number (the **PAN**, matching the number printed on the plastic).

When you tap the **same card via Apple Pay**, you get a completely different 16-digit number — the **DPAN** (Device Primary Account Number, a.k.a. the device token). It's valid but maps back to the real card only inside the network/issuer's secure vault. This is tokenization, and it's the core security feature of mobile wallets: a skimmed contactless transaction yields a device-specific token that's useless elsewhere, and the real card number is never transmitted.

Consequences this firmware demonstrates:

- You **cannot** recover the printed card number from an Apple Pay tap — it simply isn't there.
- The DPAN is **stable per-card-per-device**, so it works as a consistent identifier for that phone.
- Apple Pay's tokenized applet exposes the account number in a **non-standard Track 2 field (tag `56`, ASCII)** rather than the usual BCD tag `5A` — which is why the parser handles both.
- Any expiry you read from a DPAN is the **token's** expiry, not the card's.

---

## 🔌 Hardware

| Component | Notes |
|-----------|-------|
| **ESP8266 NodeMCU** | Prototyping target |
| **PN532 NFC module** | Chosen over MFRC522 because it supports **ISO 14443-4 / APDU**, required for smartphone (HCE / Apple Pay) reads |

---

## 🛠️ Build & flash

Built with **[PlatformIO](https://platformio.org/)** (Arduino framework for ESP8266).

**Dependencies**

- `Adafruit PN532` library

---

## 🙏 Acknowledgements

The EMV read approach is based on the excellent write-up by **Werner Rothschopf** on Arduino/ESP8266 NFC EMV reading, and the **Adafruit PN532** library. This project extends that foundation with AFL-driven record reading and Apple Pay (DPAN / Track 2) handling.

---

*Built as a learning project to understand contactless card protocols. Not affiliated with any card network, and not intended for any use involving cards you don't own or aren't authorized to test.*
