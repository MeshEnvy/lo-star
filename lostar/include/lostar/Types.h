#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Host-agnostic, POD-only vocabulary for mesh events. Every member is fixed-layout, so these
 * structs are safe to pass across translation units compiled with different preprocessor flags
 * (in particular, different nanopb flavors in each fork). Never embed host-native or nanopb-
 * generated types inside a `lostar_*` POD.
 */

/** Protocol discriminator for ingested adverts / DMs. Fork adapters stamp this on ingress. */
typedef enum {
  LOSTAR_PROTOCOL_UNKNOWN    = 0,
  LOSTAR_PROTOCOL_MESHTASTIC = 1,
  LOSTAR_PROTOCOL_MESHCORE   = 2,
} lostar_protocol;

/** Normalized advert role. Fork adapters map native role codes to these constants. */
typedef enum {
  LOSTAR_ADVERT_TYPE_UNKNOWN  = 0,
  LOSTAR_ADVERT_TYPE_CHAT     = 1,
  LOSTAR_ADVERT_TYPE_REPEATER = 2,
  LOSTAR_ADVERT_TYPE_ROOM     = 3,
  LOSTAR_ADVERT_TYPE_SENSOR   = 4,
} lostar_advert_type;

/**
 * Incoming direct-message text event. Payload pointer is only valid for the duration of the
 * ingress call; consumers must copy if they need to outlive that. `from_pubkey` carries a 32-
 * byte stable public identity when the host has one (meshcore always; meshtastic when the sender
 * has advertised a key); otherwise `from_pubkey_len` is 0.
 */
typedef struct {
  uint32_t        from;             /**< sender NodeId (canonical low bits). */
  uint32_t        to;               /**< receiver NodeId (host's self). */
  uint32_t        rx_time;          /**< host-chosen timestamp (unix seconds or packet timestamp). */
  uint8_t         from_pubkey_len;  /**< 0 or 32 */
  uint8_t         _reserved0[3];    /**< padding — keep zero */
  uint8_t         from_pubkey[32];
  const uint8_t  *payload;          /**< raw text bytes; not NUL-terminated. */
  uint32_t        payload_len;
} lostar_TextDm;

/**
 * Node advertisement / nodeinfo event. Superset carrier for both meshtastic and meshcore
 * shapes — lotato discriminates on `protocol`. Fixed layout so the adapter TU and the lostar /
 * lotato TUs see identical offsets regardless of nanopb flags.
 */
typedef struct {
  uint8_t  protocol;            /**< lostar_protocol */
  uint8_t  advert_type;         /**< lostar_advert_type */
  uint8_t  public_key_len;      /**< 0 or 32 */
  uint8_t  _reserved0;
  uint32_t num;                 /**< canonical NodeId (meshcore: first 4 bytes of pub_key). */
  uint32_t last_heard;          /**< unix seconds; 0 if unknown. */
  int32_t  latitude_i;          /**< integer lat in fork-native scaling (meshtastic 1e-7, meshcore 1e-6). */
  int32_t  longitude_i;         /**< integer lon in fork-native scaling. */
  uint16_t hw_model;            /**< meshtastic hw_model; 0 on meshcore. */
  uint16_t _reserved1;
  uint8_t  public_key[32];
  char     long_name[40];       /**< meshtastic long_name or meshcore name. NUL-terminated. */
  char     short_name[8];       /**< meshtastic short_name; empty on meshcore. NUL-terminated. */
} lostar_NodeAdvert;

#ifdef __cplusplus
}  // extern "C"
#endif
