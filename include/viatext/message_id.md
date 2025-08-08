# message_id.BLUEPRINT.md

MODULE MessageID

  ## DOC
  ## @description: Compact 5-byte message header for ViaText mesh routing and delivery.
  ## @role: Encodes unique ID, fragment info, hop count, and delivery flags for every message.
  ## @usage: Always prepended to messages for LoRa, serial, sneakernet. Used on Linux, Arduino, etc.
  ## @note: Structure is fixed-length, binary-packed for MCU and cross-platform compatibility.
  ## @author: Leo, ChatGPT

  # --- Data Structure

  TYPE MessageID {
    sequence: Number,    # 16 bits; unique ID for message (all fragments share)
    part: Number,        # 8 bits; index of this fragment (0 = first)
    total: Number,       # 8 bits; total number of fragments (1 = not fragmented)
    hops: Number,        # 4 bits (upper nibble); mesh hop count (0–15), TTL
    flags: Number        # 4 bits (lower nibble); bitfield: ACK, request, encryption
  } END TYPE

  # --- Byte Layout
  # Bytes 0–1: sequence (big endian)
  # Byte 2: part
  # Byte 3: total
  # Byte 4: hops:4 (high nibble) + flags:4 (low nibble)

  ENUM MessageFlags {
    REQUEST_ACK = 0x1,   # Sender requests ACK
    ACK         = 0x2,   # This is an ACK
    ENCRYPTED   = 0x4,   # Payload encrypted
    UNUSED      = 0x8    # Reserved/future
  } END ENUM

  # --- Core Functions

  ## DOC
  ## @description: Pack struct fields into 5-byte buffer (for wire/transmission).
  ## @param out_buf: List<Number> — output buffer, length ≥ 5
  DEFINE pack(self: MessageID, out_buf: List<Number>): Void
    # out_buf[0] = (sequence >> 8) & 0xFF
    # out_buf[1] = sequence & 0xFF
    # out_buf[2] = part
    # out_buf[3] = total
    # out_buf[4] = ((hops & 0x0F) << 4) | (flags & 0x0F)
  END DEFINE

  ## DOC
  ## @description: Unpack fields from 5-byte buffer (after receive).
  ## @param in_buf: List<Number> — input buffer, length ≥ 5
  DEFINE unpack(self: MessageID, in_buf: List<Number>): Void
    # sequence = (in_buf[0] << 8) | in_buf[1]
    # part = in_buf[2]
    # total = in_buf[3]
    # hops = (in_buf[4] >> 4) & 0x0F
    # flags = in_buf[4] & 0x0F
  END DEFINE

  ## DOC
  ## @description: Return human-readable string summary of the MessageID.
  ## @return: String, e.g. "SEQ:28 PART:5/7 HOPS:10 FLAGS:0xC"
  DEFINE to_string(self: MessageID): String
    # Converts all fields to text; MCU-safe, fixed buffer
  END DEFINE

  # --- Constructors

  ## DOC
  ## @description: Default zero-initialized constructor.
  DEFINE new(): MessageID
    RETURN MessageID(sequence=0, part=0, total=0, hops=0, flags=0)
  END DEFINE

  ## DOC
  ## @description: Construct from packed 5-byte integer (big endian, lowest 40 bits).
  DEFINE from_int(five_byte_value: Number): MessageID
    # Extract 5 bytes, then unpack
  END DEFINE

  ## DOC
  ## @description: Construct from 10-char hex string (e.g., "4F2B000131").
  DEFINE from_hex(hex_str: String): MessageID
    # Parse hex_str → buffer → unpack
  END DEFINE

  ## DOC
  ## @description: Construct from 5-byte buffer.
  DEFINE from_bytes(buf: List<Number>): MessageID
    # Unpack directly from bytes
  END DEFINE

  ## DOC
  ## @description: Construct from explicit fields (all values).
  DEFINE from_fields(sequence: Number, part: Number, total: Number, hops: Number, flags: Number): MessageID
    # Mask hops and flags to 4 bits each
  END DEFINE

  ## DOC
  ## @description: Construct from fields, with explicit flag booleans.
  DEFINE from_flags(sequence: Number, part: Number, total: Number, hops: Number,
                    request_ack: Boolean, is_ack: Boolean, encrypted: Boolean, unused: Boolean): MessageID
    # Compose flags bitfield from booleans
  END DEFINE

  # --- Field/Flag Accessors

  ## DOC
  ## @description: Returns true if ACK requested flag set.
  DEFINE requests_acknowledgment(self: MessageID): Boolean
    RETURN (self.flags & 0x1) != 0
  END DEFINE

  ## DOC
  ## @description: Returns true if this is an ACK.
  DEFINE is_acknowledgment(self: MessageID): Boolean
    RETURN (self.flags & 0x2) != 0
  END DEFINE

  ## DOC
  ## @description: Returns true if message is encrypted.
  DEFINE is_encrypted(self: MessageID): Boolean
    RETURN (self.flags & 0x4) != 0
  END DEFINE

  ## DOC
  ## @description: Set or clear ACK request flag.
  DEFINE set_request_acknowledgment(self: MessageID, enable: Boolean): Void
    IF enable
      self.flags = self.flags | 0x1
    ELSE
      self.flags = self.flags & (~0x1)
    END IF
  END DEFINE

  ## DOC
  ## @description: Set or clear acknowledgment flag.
  DEFINE set_is_acknowledgment(self: MessageID, enable: Boolean): Void
    IF enable
      self.flags = self.flags | 0x2
    ELSE
      self.flags = self.flags & (~0x2)
    END IF
  END DEFINE

  ## DOC
  ## @description: Set or clear encryption flag.
  DEFINE set_is_encrypted(self: MessageID, enable: Boolean): Void
    IF enable
      self.flags = self.flags | 0x4
    ELSE
      self.flags = self.flags & (~0x4)
    END IF
  END DEFINE

  # --- Hex String Conversion (for debug)

  ## DOC
  ## @description: Convert to 10-char hex string, e.g. "4F2B000131".
  DEFINE to_hex_string(self: MessageID): String
    # Converts packed bytes to hex chars, upper-case, no prefix.
  END DEFINE

  ## DOC
  ## @description: Parse hex string to byte array (static).
  DEFINE hex_str_to_bytes(hex_str: String, bytes_needed: Number): List<Number>
    # Handles optional "0x" prefix, checks length, parses pairs to bytes.
  END DEFINE

END MODULE
