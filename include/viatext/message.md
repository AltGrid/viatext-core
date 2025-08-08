# message.BLUEPRINT.md

MODULE Message

  ## DOC
  ## @description: Universal, heap-free protocol message for ViaText; wraps routing, addressing, and payload in a single MCU-safe container.
  ## @role: Standard envelope for all messages in mesh; supports wire parsing, fragmentation, validation, and zero-overhead MCU/LINUX use.
  ## @integration: Used by Core, CLI, LoRa, Station; compatible with MessageID, TextFragments.
  ## @author: Leo, ChatGPT

  # --- Type and Limits

  CONST FROM_LEN: Number = 6      # Max chars for sender ID
  CONST TO_LEN: Number = 6        # Max chars for recipient ID
  CONST DATA_FRAGS: Number = 8    # Max number of payload fragments
  CONST FRAG_SIZE: Number = 32    # Max size per fragment (bytes)

  TYPE Message {
    id: MessageID,                                          # Routing header (5-byte binary, see message_id.BLUEPRINT.md)
    from: String (max FROM_LEN),                            # Sender node ID
    to: String (max TO_LEN),                                # Recipient node ID
    data: TextFragments<DATA_FRAGS, FRAG_SIZE>,             # Fixed-fragment payload (8×32 bytes)
    error: Number                                           # Error code (see below)
  } END TYPE

  ENUM MessageError {
    OK = 0,           # Success
    PARSE = 1,        # Invalid format
    OVERFLOW = 2,     # Payload/data too large
    EMPTY = 3,        # Uninitialized/blank
    FRAGMENT = 4      # Fragment/parsing error
  } END ENUM

  # --- Wire Format
  # "<id>~<from>~<to>~<data>"
  # Example: "0x4F2B000131~shrek~donkey~Shut Up"

  # --- Constructors

  ## DOC
  ## @description: Zero/empty constructor; error=EMPTY
  DEFINE new(): Message
    RETURN Message(id=MessageID.new(), from="", to="", data=TextFragments.new(), error=3)
  END DEFINE

  ## DOC
  ## @description: Construct from all fields (routing, sender, recipient, data)
  DEFINE from_fields(id: MessageID, from_: String, to_: String, data_: String): Message
    # Sets all fields, splits data_ into fragments, sets error if overflow
  END DEFINE

  ## DOC
  ## @description: Construct by parsing a wire-format string (see above)
  DEFINE from_wire_string(wire_str: String): Message
    # Splits on '~', parses header, sender, recipient, data; sets error on failure
  END DEFINE

  # --- Setters

  ## DOC
  ## @description: Set routing header
  DEFINE set_id(self: Message, id_: MessageID): Void
    self.id = id_
  END DEFINE

  ## DOC
  ## @description: Set sender node ID (truncate if needed)
  DEFINE set_from(self: Message, from_: String): Void
    self.from = Substr(from_, 0, FROM_LEN)
  END DEFINE

  ## DOC
  ## @description: Set recipient node ID (truncate if needed)
  DEFINE set_to(self: Message, to_: String): Void
    self.to = Substr(to_, 0, TO_LEN)
  END DEFINE

  ## DOC
  ## @description: Set message payload; splits to fixed fragments, sets error on overflow
  DEFINE set_data(self: Message, data_: String): Void
    CALL self.data.set(data_)
    IF self.data.error != 0
      self.error = 2
    ELSE
      self.error = 0
    END IF
  END DEFINE

  ## DOC
  ## @description: Clear all fields to default state (id zeroed, from/to/data empty, error=3)
  DEFINE clear(self: Message): Void
    self.id = MessageID.new()
    self.from = ""
    self.to = ""
    CALL self.data.clear()
    self.error = 3
  END DEFINE

  # --- Getters

  DEFINE get_id(self: Message): MessageID
    RETURN self.id
  END DEFINE

  DEFINE get_from(self: Message): String
    RETURN self.from
  END DEFINE

  DEFINE get_to(self: Message): String
    RETURN self.to
  END DEFINE

  DEFINE get_data(self: Message): TextFragments<DATA_FRAGS, FRAG_SIZE>
    RETURN self.data
  END DEFINE

  # --- Serialization

  ## DOC
  ## @description: Serialize message to wire string ("<id>~<from>~<to>~<data>")
  DEFINE to_wire_string(self: Message): String
    # Join all fragments in self.data, combine all fields with '~' delimiter
    # id encoded as 10-char hex string
  END DEFINE

  ## DOC
  ## @description: Parse wire string into message fields; sets error on failure
  DEFINE from_wire_string(self: Message, wire_str: String): Boolean
    # Splits, parses, loads fields, sets error if invalid; returns true on success
  END DEFINE

  # --- Validation and State

  ## DOC
  ## @description: Returns true if message has valid structure and no errors
  DEFINE is_valid(self: Message): Boolean
    RETURN (Length(self.from) > 0) AND (Length(self.to) > 0) AND (self.data.used_fragments > 0) AND (self.id.sequence > 0) AND (self.error == 0)
  END DEFINE

  ## DOC
  ## @description: Returns true if message is part of a multi-part series (fragmented)
  DEFINE is_fragmented(self: Message): Boolean
    RETURN self.id.total > 1
  END DEFINE

  ## DOC
  ## @description: Returns true if message is a complete single-part
  DEFINE is_complete(self: Message): Boolean
    RETURN self.id.part == 0 AND self.id.total == 1
  END DEFINE

  # --- Debug

  ## DOC
  ## @description: Format message as debug string: "<ID> FROM:<from> TO:<to> DATA:<first fragment>"
  DEFINE to_string(self: Message): String
    # Uses self.id.to_string(), shows only first data fragment, truncates as needed
  END DEFINE

END MODULE
