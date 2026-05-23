// ================================================================
// owner.h - Besitzer-/Kontakt-Info (Anzeige im Besitzer-Screen)
// ================================================================
#pragma once

struct OwnerInfo {
  const char* title;        // z.B. "THW OV Berlin"
  const char* subtitle;     // z.B. "Friedrichshain-Kreuzberg"
  const char* addr_line1;   // Strasse + Hausnummer
  const char* addr_line2;   // PLZ + Ort
  const char* phone;
  const char* fax;
  const char* email;
};

extern const OwnerInfo OWNER;
