/**
 * ============================================================================
 * UTILITARIOS DE TEMPO
 * ============================================================================
 *
 * Funcoes thread-safe para manipulacao e formatacao de tempo.
 *
 * ============================================================================
 */

#include "utils/time_utils.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// FUNCOES DE TEMPO
// ============================================================================

uint32_t time_millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

uint64_t time_micros(void) {
    return esp_timer_get_time();
}

uint32_t time_seconds(void) {
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

// ============================================================================
// FORMATACAO DE TEMPO (THREAD-SAFE)
// ============================================================================

void time_format_ms(uint32_t timeMs, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < TIME_FORMAT_MIN_BUFFER) {
        return;
    }

    uint32_t totalSeconds = timeMs / 1000;
    uint32_t hours = totalSeconds / 3600;
    uint32_t minutes = (totalSeconds % 3600) / 60;
    uint32_t seconds = totalSeconds % 60;

    if (hours > 0) {
        snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu",
                 (unsigned long)hours,
                 (unsigned long)minutes,
                 (unsigned long)seconds);
    } else {
        snprintf(buffer, bufferSize, "%02lu:%02lu",
                 (unsigned long)minutes,
                 (unsigned long)seconds);
    }
}

void time_format_ms_full(uint32_t timeMs, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < TIME_FORMAT_MIN_BUFFER) {
        return;
    }

    uint32_t totalSeconds = timeMs / 1000;
    uint32_t hours = totalSeconds / 3600;
    uint32_t minutes = (totalSeconds % 3600) / 60;
    uint32_t seconds = totalSeconds % 60;
    uint32_t millis = timeMs % 1000;

    snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu.%03lu",
             (unsigned long)hours,
             (unsigned long)minutes,
             (unsigned long)seconds,
             (unsigned long)millis);
}

void time_format_seconds(uint32_t totalSeconds, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < TIME_FORMAT_MIN_BUFFER) {
        return;
    }

    uint32_t hours = totalSeconds / 3600;
    uint32_t minutes = (totalSeconds % 3600) / 60;
    uint32_t seconds = totalSeconds % 60;

    if (hours > 0) {
        snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu",
                 (unsigned long)hours,
                 (unsigned long)minutes,
                 (unsigned long)seconds);
    } else {
        snprintf(buffer, bufferSize, "%02lu:%02lu",
                 (unsigned long)minutes,
                 (unsigned long)seconds);
    }
}

void time_format_duration(uint32_t startMs, uint32_t endMs, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < TIME_FORMAT_MIN_BUFFER) {
        return;
    }

    uint32_t duration = (endMs > startMs) ? (endMs - startMs) : 0;
    time_format_ms(duration, buffer, bufferSize);
}

// ============================================================================
// CONVERSOES
// ============================================================================

uint32_t time_hours_to_ms(uint32_t hours) {
    return hours * 3600000UL;
}

uint32_t time_minutes_to_ms(uint32_t minutes) {
    return minutes * 60000UL;
}

uint32_t time_seconds_to_ms(uint32_t seconds) {
    return seconds * 1000UL;
}

uint32_t time_ms_to_hours(uint32_t ms) {
    return ms / 3600000UL;
}

uint32_t time_ms_to_minutes(uint32_t ms) {
    return ms / 60000UL;
}

uint32_t time_ms_to_seconds(uint32_t ms) {
    return ms / 1000UL;
}

// ============================================================================
// COMPARACOES
// ============================================================================

bool time_has_elapsed(uint32_t startMs, uint32_t durationMs) {
    return (time_millis() - startMs) >= durationMs;
}

uint32_t time_elapsed_since(uint32_t startMs) {
    uint32_t now = time_millis();
    return (now >= startMs) ? (now - startMs) : 0;
}

uint32_t time_remaining(uint32_t startMs, uint32_t durationMs) {
    uint32_t elapsed = time_elapsed_since(startMs);
    return (elapsed < durationMs) ? (durationMs - elapsed) : 0;
}

// ============================================================================
// CLASSE TimeFormatter (C++)
// ============================================================================

#ifdef __cplusplus

TimeFormatter::TimeFormatter() {
    buffer[0] = '\0';
}

const char* TimeFormatter::formatMs(uint32_t timeMs) {
    time_format_ms(timeMs, buffer, sizeof(buffer));
    return buffer;
}

const char* TimeFormatter::formatMsFull(uint32_t timeMs) {
    time_format_ms_full(timeMs, buffer, sizeof(buffer));
    return buffer;
}

const char* TimeFormatter::formatSeconds(uint32_t totalSeconds) {
    time_format_seconds(totalSeconds, buffer, sizeof(buffer));
    return buffer;
}

const char* TimeFormatter::formatDuration(uint32_t startMs, uint32_t endMs) {
    time_format_duration(startMs, endMs, buffer, sizeof(buffer));
    return buffer;
}

const char* TimeFormatter::formatElapsed(uint32_t startMs) {
    uint32_t elapsed = time_elapsed_since(startMs);
    time_format_ms(elapsed, buffer, sizeof(buffer));
    return buffer;
}

#endif // __cplusplus
