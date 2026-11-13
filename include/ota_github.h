#pragma once

// Controleert de nieuwste GitHub Release en installeert die als ze verschilt
// van de huidige firmwareversie. Blokkeert tijdens de download en herstart
// de ESP32 na een geslaagde update.
void otaCheckForUpdate();
